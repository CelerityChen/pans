#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string_view>
#include <vector>

#if defined(_WIN32)
// 如果尚未定义 WIN32_LEAN_AND_MEAN，则定义它以减少 Windows 头文件的包含内容
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// 如果尚未定义 NOMINMAX，则定义它以避免与 Windows 头文件的冲突
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#elif defined(__linux__)
#include <cxxabi.h>
#include <elfutils/libdwfl.h>
#include <execinfo.h>
#include <unistd.h>
#elif defined(__APPLE__)
// macOS 没有 elfutils（不支持 Mach-O）：backtrace 取调用栈，dladdr 按地址查符号名和所属镜像，
// 均由 libSystem 提供；__cxa_demangle 由 libc++abi 提供；
// 文件名和行号没有进程内公开接口，通过 posix_spawn 调用开发者工具中的 atos 解析
#include <crt_externs.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <map>
#include <string>
#endif

#include <utils/system_utils.h>

namespace pans
{
    std::chrono::steady_clock::duration GetElapsedTime() noexcept
    {
        static const auto START_TIME = std::chrono::steady_clock::now();
        return std::chrono::steady_clock::now() - START_TIME;
    }

    u64 GetFiberId() noexcept
    {
        return 0;
    }

    namespace
    {
#if defined(__linux__) || defined(__APPLE__)
        /**
         * @brief 去掉项目根目录前缀，与 redefine_file_macro 重定义的 __FILE__ 一致
         * @note PANS_SOURCE_DIR 由 pans/CMakeLists.txt 定义为 "<项目根目录>/"；
         *       项目外的文件（系统头文件、作为依赖被其他项目使用时调用方的源码）保留绝对路径
         */
        std::string MakeProjectRelative(std::string path)
        {
#if defined(PANS_SOURCE_DIR)
            constexpr std::string_view SOURCE_DIR = PANS_SOURCE_DIR;
            if (path.size() > SOURCE_DIR.size() && std::string_view(path).starts_with(SOURCE_DIR))
            {
                path.erase(0, SOURCE_DIR.size());
            }
#endif
            return path;
        }
#endif

#if defined(_WIN32)
        // defbugif is not thread-safe, so we use a mutex to protect it
        std::mutex &GetDbgHelpMutex()
        {
            static std::mutex mutex;
            return mutex;
        }

        class DbgHelpContext final
        {
            // RAII 管理 SymInitialize 和 SymCleanup
        public:
            DbgHelpContext()
                : m_process(GetCurrentProcess())
            {
                // 配置符号处理选项：
                // SYMOPT_DEFERRED_LOADS   —— 延迟加载模块符号，直到需要时再加载（加快初始化速度）
                // SYMOPT_LOAD_LINES       —— 加载源文件和行号信息（可用于栈回溯显示源码行）
                // SYMOPT_UNDNAME          —— 自动对符号名做反混淆（demangle），获得人类可读的名称
                SymSetOptions(SymGetOptions() | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
                m_initialized = SymInitialize(m_process, nullptr, TRUE) == TRUE; // 初始化符号引擎
            }
            ~DbgHelpContext()
            {
                if (m_initialized)
                {
                    SymCleanup(m_process);
                }
            }

            DbgHelpContext(const DbgHelpContext &) = delete;
            DbgHelpContext &operator=(const DbgHelpContext &) = delete;

            [[nodiscard]] HANDLE process() const noexcept { return m_process; }
            [[nodiscard]] bool initialized() const noexcept { return m_initialized; }

        private:
            HANDLE m_process;
            bool m_initialized;
        };

        DbgHelpContext &GetDbgHelpContext()
        {
            static DbgHelpContext context;
            return context;
        }
        // 严格来说，DbgHelpContext 应实现为单例，因为 SymInitialize 和 SymCleanup 是进程级操作，多个 dbghelp 实例之间会相互影响。此处为简化处理。

        /**
         * @brief 将 Windows 栈帧信息写入流
         * @param stream 输出流
         * @param context DbgHelpContext 实例
         * @param frame 栈帧地址
         * @param is_return_address 是否是返回地址
         */
        void WriteWindowsFrame(std::ostringstream &stream, const DbgHelpContext &context, void *frame, bool is_return_address)
        {
            auto address = static_cast<DWORD64>(reinterpret_cast<std::uintptr_t>(frame));
            // 如果返回地址，则减1，因为返回地址是调用函数的返回地址，而不是当前函数的返回地址
            if (is_return_address && address > 0)
            {
                --address;
            }
            bool symbol_found = false;
            if (context.initialized())
            {
                SYMBOL_INFO_PACKAGE symbol_package{};
                symbol_package.si.SizeOfStruct = sizeof(SYMBOL_INFO);
                symbol_package.si.MaxNameLen = MAX_SYM_NAME;

                DWORD64 displacement = 0;
                // SymFromAddr 是 Windows dbghelp 库提供的 API，用于根据内存地址查找对应的符号信息（如函数名），成功时返回 true
                if (SymFromAddr(context.process(), address, &displacement, &symbol_package.si) == true)
                {
                    stream << symbol_package.si.Name;
                    if (displacement != 0)
                    {
                        stream << "+0x" << std::hex << displacement << std::dec;
                    }
                    symbol_found = true;
                }
                if (context.initialized())
                {
                    IMAGEHLP_LINE64 line{};
                    line.SizeOfStruct = sizeof(line);
                    DWORD displacement = 0;
                    if (SymGetLineFromAddr64(context.process(), address, &displacement, &line) == true && line.FileName != nullptr)
                    {
                        stream << " at " << line.FileName << ":" << line.LineNumber;
                    }
                }
            }
            if (!symbol_found)
            {
                stream << "0x" << std::hex << address << std::dec;
            }
        }
#elif defined(__linux__) || defined(__APPLE__)
        // backtrace_symbols、__cxa_demangle 返回的内存需用 free 释放
        struct FreeDeleter final
        {
            void operator()(void *ptr) const noexcept
            {
                std::free(ptr);
            }
        };
#endif

#if defined(__linux__)
        struct DwflDeleter final
        {
            void operator()(Dwfl *dwfl) const noexcept
            {
                dwfl_end(dwfl);
            }
        };

        const Dwfl_Callbacks DWFL_CALLBACKS{
            dwfl_linux_proc_find_elf,     // 查找 ELF 文件回调
            dwfl_standard_find_debuginfo, // 标准调试信息查找回调
            nullptr,
            nullptr,
        };

        class SourceLocationParser final
        {
        public:
            SourceLocationParser()
                : m_dwfl(dwfl_begin(&DWFL_CALLBACKS))
            {
                if (!m_dwfl || dwfl_linux_proc_report(m_dwfl.get(), getpid()) || dwfl_report_end(m_dwfl.get(), nullptr, nullptr))
                {
                    m_dwfl.reset();
                }
            }

            [[nodiscard]] std::string parse(void *frame, bool is_return_address) const
            {
                if (!m_dwfl)
                {
                    return {};
                }
                auto address = static_cast<Dwarf_Addr>(reinterpret_cast<std::uintptr_t>(frame));
                if (is_return_address && address > 0)
                {
                    --address;
                }

                Dwfl_line *line = dwfl_getsrc(m_dwfl.get(), address);
                if (line == nullptr)
                {
                    return {};
                }
                int line_number = 0;
                const char *file_name = dwfl_lineinfo(line, nullptr, &line_number, nullptr, nullptr, nullptr);
                if (file_name == nullptr || line_number <= 0)
                {
                    return {};
                }
                return MakeProjectRelative(file_name) + ":" + std::to_string(line_number);
            }

        private:
            std::unique_ptr<Dwfl, DwflDeleter> m_dwfl;
        };

        std::string DemangleBacktraceSymbol(const char *symbol)
        {
            // 模块名（符号名+偏移）[地址]
            const std::string_view text(symbol == nullptr ? "" : symbol);
            const auto name_begin = text.find('(');
            if (name_begin == std::string_view::npos)
            {
                return std::string(symbol);
            }
            const auto mangled_begin = name_begin + 1;
            auto name_end = text.find('+', mangled_begin);
            if (name_end == std::string_view::npos)
            {
                name_end = text.find(')', mangled_begin);
            }
            if (name_end == std::string_view::npos || name_end == mangled_begin)
            {
                return std::string(symbol);
            }
            const std::string mangled_name(text.substr(mangled_begin, name_end - mangled_begin));
            int status = 0;
            std::unique_ptr<char, FreeDeleter> demangled(abi::__cxa_demangle(mangled_name.c_str(), nullptr, nullptr, &status));
            if (status == 0 && demangled)
            {
                return demangled.get();
            }
            return mangled_name;
        }
#elif defined(__APPLE__)
        struct AppleFrame final
        {
            std::uintptr_t address = 0;        // 原始地址，用于显示和计算符号偏移
            std::uintptr_t lookup_address = 0; // 查询地址：返回地址减 1，落在调用指令内
            Dl_info info{};
            bool has_info = false;
            std::string source_location;       // "文件:行号"，解析失败时为空
        };

        std::string ToHex(std::uintptr_t value)
        {
            std::ostringstream stream;
            stream << "0x" << std::hex << value;
            return stream.str();
        }

        /**
         * @brief 查找开发者工具中真正的 atos，结果缓存
         * @note 不使用 /usr/bin/atos：它只是按 xcode-select 转发的壳，未安装开发者工具时会弹出安装对话框；
         *       找不到时返回空串，调用栈退化为只有符号名
         */
        const std::string &GetAtosPath()
        {
            static const std::string ATOS_PATH = []() -> std::string {
                std::vector<std::string> developer_dirs;
                if (const char *developer_dir = std::getenv("DEVELOPER_DIR"); developer_dir != nullptr && *developer_dir != '\0')
                {
                    developer_dirs.emplace_back(developer_dir);
                }
                // xcode-select -p 的结果就是这个符号链接的目标
                char resolved[PATH_MAX];
                if (::realpath("/var/db/xcode_select_link", resolved) != nullptr)
                {
                    developer_dirs.emplace_back(resolved);
                }
                developer_dirs.emplace_back("/Library/Developer/CommandLineTools");
                developer_dirs.emplace_back("/Applications/Xcode.app/Contents/Developer");
                for (const auto &dir : developer_dirs)
                {
                    std::string candidate = dir + "/usr/bin/atos";
                    if (::access(candidate.c_str(), X_OK) == 0)
                    {
                        return candidate;
                    }
                }
                return {};
            }();
            return ATOS_PATH;
        }

        /**
         * @brief 启动子进程并读取其标准输出，标准错误丢弃
         * @note 使用 posix_spawn 而非 fork/popen：多线程程序中更安全，也不经过 shell；
         *       POSIX_SPAWN_CLOEXEC_DEFAULT 使子进程只继承这里显式设置的文件描述符
         */
        bool RunAndCapture(const std::vector<std::string> &args, std::string &output)
        {
            int pipe_fds[2];
            if (::pipe(pipe_fds) != 0)
            {
                return false;
            }

            posix_spawn_file_actions_t actions;
            ::posix_spawn_file_actions_init(&actions);
            ::posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
            ::posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

            posix_spawnattr_t attributes;
            ::posix_spawnattr_init(&attributes);
            ::posix_spawnattr_setflags(&attributes, POSIX_SPAWN_CLOEXEC_DEFAULT);

            std::vector<char *> argv;
            argv.reserve(args.size() + 1);
            for (const auto &arg : args)
            {
                argv.push_back(const_cast<char *>(arg.c_str()));
            }
            argv.push_back(nullptr);

            pid_t pid = 0;
            const int spawn_result = ::posix_spawn(&pid, argv[0], &actions, &attributes, argv.data(), *::_NSGetEnviron());
            ::posix_spawnattr_destroy(&attributes);
            ::posix_spawn_file_actions_destroy(&actions);
            ::close(pipe_fds[1]);
            if (spawn_result != 0)
            {
                ::close(pipe_fds[0]);
                return false;
            }

            char buffer[4096];
            for (;;)
            {
                const ssize_t read_size = ::read(pipe_fds[0], buffer, sizeof(buffer));
                if (read_size > 0)
                {
                    output.append(buffer, static_cast<std::size_t>(read_size));
                }
                else if (read_size < 0 && errno == EINTR)
                {
                    continue;
                }
                else
                {
                    break;
                }
            }
            ::close(pipe_fds[0]);

            int status = 0;
            while (::waitpid(pid, &status, 0) < 0 && errno == EINTR)
            {
            }
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }

        /**
         * @brief 从 atos 的一行输出中提取 "文件:行号"
         * @note 有调试信息时形如 "main (in app) (main.cpp:14)"；
         *       无调试信息时形如 "main (in app) + 12"，或行号为 0，均返回空
         */
        std::string ParseAtosLine(std::string_view line)
        {
            if (line.empty() || line.back() != ')')
            {
                return {};
            }
            const auto open = line.rfind('(');
            if (open == std::string_view::npos)
            {
                return {};
            }
            const std::string_view location = line.substr(open + 1, line.size() - open - 2);
            const auto colon = location.rfind(':');
            if (colon == std::string_view::npos || colon == 0 || colon + 1 == location.size())
            {
                return {};
            }
            const std::string_view line_number = location.substr(colon + 1);
            if (!std::all_of(line_number.begin(), line_number.end(), [](char c) { return c >= '0' && c <= '9'; }) ||
                line_number.find_first_not_of('0') == std::string_view::npos)
            {
                return {};
            }
            return MakeProjectRelative(std::string(location));
        }

        /**
         * @brief 调用 atos 为各帧补充文件名和行号
         * @note 同一镜像的地址合并为一次 atos 调用；系统库没有调试信息，直接跳过。
         *       需要可执行文件带调试信息（-g），且构建产生的 .o 文件或 .dSYM 仍在原位置，
         *       因为 macOS 的链接器不会把 DWARF 复制进可执行文件，atos 要据此找回调试信息。
         *       每个镜像启动一次子进程，耗时约几十到几百毫秒，仅适合断言、FATAL 等低频场景，
         *       且不能在信号处理函数中使用。
         */
        void ResolveSourceLocations(std::vector<AppleFrame> &frames)
        {
            const std::string &atos_path = GetAtosPath();
            if (atos_path.empty())
            {
                return;
            }

            std::map<const void *, std::vector<std::size_t>> frames_by_image;
            for (std::size_t i = 0; i < frames.size(); ++i)
            {
                const AppleFrame &frame = frames[i];
                if (!frame.has_info || frame.info.dli_fname == nullptr || frame.info.dli_fbase == nullptr)
                {
                    continue;
                }
                const std::string_view image_path(frame.info.dli_fname);
                if (image_path.starts_with("/usr/lib/") || image_path.starts_with("/System/"))
                {
                    continue;
                }
                frames_by_image[frame.info.dli_fbase].push_back(i);
            }

            for (const auto &[image_base, indices] : frames_by_image)
            {
                // -l：镜像的实际加载地址（含 ASLR 偏移）；-arch：通用二进制中选择当前运行的架构
                std::vector<std::string> args{
                    atos_path,
                    "--fullPath",
                    "-o", frames[indices.front()].info.dli_fname,
                    "-l", ToHex(reinterpret_cast<std::uintptr_t>(image_base)),
#if defined(__arm64__) || defined(__aarch64__)
                    "-arch", "arm64",
#elif defined(__x86_64__)
                    "-arch", "x86_64",
#endif
                };
                for (const std::size_t index : indices)
                {
                    args.push_back(ToHex(frames[index].lookup_address));
                }

                std::string output;
                if (!RunAndCapture(args, output))
                {
                    continue;
                }
                // atos 按输入顺序每个地址输出一行
                std::istringstream lines(output);
                std::string line;
                for (const std::size_t index : indices)
                {
                    if (!std::getline(lines, line))
                    {
                        break;
                    }
                    frames[index].source_location = ParseAtosLine(line);
                }
            }
        }

        /**
         * @brief 将 macOS 栈帧信息写入流，格式：符号名+0x偏移 (镜像名) [0x地址] at 文件:行号
         * @note macOS 的 backtrace_symbols 输出格式与 glibc 不同（无括号），因此直接用 dladdr 解析；
         *       dladdr 只能查到导出符号，static / 匿名命名空间中的函数会显示为最近的导出符号或仅显示地址
         */
        void WriteAppleFrame(std::ostringstream &stream, const AppleFrame &frame)
        {
            if (frame.has_info && frame.info.dli_sname != nullptr)
            {
                int status = 0;
                std::unique_ptr<char, FreeDeleter> demangled(abi::__cxa_demangle(frame.info.dli_sname, nullptr, nullptr, &status));
                stream << (status == 0 && demangled ? demangled.get() : frame.info.dli_sname);
                const auto symbol_address = reinterpret_cast<std::uintptr_t>(frame.info.dli_saddr);
                if (frame.address > symbol_address)
                {
                    stream << "+0x" << std::hex << (frame.address - symbol_address) << std::dec;
                }
            }
            else
            {
                stream << "???";
            }
            if (frame.has_info && frame.info.dli_fname != nullptr)
            {
                const std::string_view image_path(frame.info.dli_fname);
                const auto slash = image_path.find_last_of('/');
                stream << " (" << (slash == std::string_view::npos ? image_path : image_path.substr(slash + 1)) << ")";
            }
            stream << " [0x" << std::hex << frame.address << std::dec << "]";
            if (!frame.source_location.empty())
            {
                stream << " at " << frame.source_location;
            }
        }
#endif
    }

    std::string GetBacktrace(int size, int skip, const std::string &prefix)
    {
        if (size <= 0)
        {
            return {};
        }
#if defined(_WIN32)
        const int capture_size = std::min(size, static_cast<int>(std::numeric_limits<USHORT>::max()));
        std::vector<void *> frames(static_cast<std::size_t>(capture_size));
        const int frame_count = static_cast<int>(CaptureStackBackTrace(0, static_cast<DWORD>(frames.size()), frames.data(), nullptr));
        if (frame_count <= 0)
        {
            return {};
        }
        std::ostringstream stream;
        // std::clamp 是 C++17 标准库中的一个函数，用于将一个值限制在指定的范围内。
        const int first_frame = std::clamp(skip, 0, frame_count);
        std::lock_guard<std::mutex> lock(GetDbgHelpMutex());
        const DbgHelpContext &context = GetDbgHelpContext();
        for (int i = first_frame; i < frame_count; ++i)
        {
            stream << prefix << '#' << i - first_frame << " ";
            WriteWindowsFrame(stream, context, frames[i], i > 0);
            stream << '\n';
        }
        return stream.str();

#elif defined(__linux__)
        std::vector<void *> frames(static_cast<std::size_t>(size));
        const int frame_count = ::backtrace(frames.data(), size);
        if (frame_count <= 0)
        {
            return {};
        }
        std::unique_ptr<char *, FreeDeleter> symbols(::backtrace_symbols(frames.data(), frame_count));
        if (!symbols)
        {
            return {};
        }
        std::ostringstream stream;
        const SourceLocationParser source_location_parser;
        const int first_frame = std::clamp(skip, 0, frame_count);
        for (int i = first_frame; i < frame_count; ++i)
        {
            stream << prefix << '#' << i - first_frame << " " << DemangleBacktraceSymbol(symbols.get()[i]);
            const std::string source_location = source_location_parser.parse(frames[i], i > 0);
            if (!source_location.empty())
            {
                stream << " at " << source_location;
            }
            stream << '\n';
        }
        return stream.str();

#elif defined(__APPLE__)
        std::vector<void *> frames(static_cast<std::size_t>(size));
        const int frame_count = ::backtrace(frames.data(), size);
        if(frame_count <= 0)
        {
            return {};
        }
        const int first_frame = std::clamp(skip, 0, frame_count);
        std::vector<AppleFrame> apple_frames;
        apple_frames.reserve(static_cast<std::size_t>(frame_count - first_frame));
        for(int i = first_frame; i < frame_count; ++i)
        {
            AppleFrame frame;
            frame.address = reinterpret_cast<std::uintptr_t>(frames[i]);
            // 第 0 帧是当前执行位置，其余帧是返回地址
            frame.lookup_address = (i > 0 && frame.address > 0) ? frame.address - 1 : frame.address;
            frame.has_info = ::dladdr(reinterpret_cast<const void *>(frame.lookup_address), &frame.info) != 0;
            apple_frames.push_back(std::move(frame));
        }
        ResolveSourceLocations(apple_frames);

        std::ostringstream stream;
        for(std::size_t i = 0; i < apple_frames.size(); ++i)
        {
            stream << prefix << '#' << i << " ";
            WriteAppleFrame(stream, apple_frames[i]);
            stream << '\n';
        }
        return stream.str();
#else
        (void)skip;
        (void)prefix;
        return {};
#endif
    }
}
