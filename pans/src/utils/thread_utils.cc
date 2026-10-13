#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>

#if defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#endif

#if defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#endif

#include <utils/thread_utils.h>
#include <utils/string_utils.h>
#include <pans/macros.h>

namespace pans
{
    namespace
    {
#if defined(_WIN32)
        struct LocalFreeDeleter
        {
            void operator()(wchar_t *value) const noexcept
            {
                if (value != nullptr)
                {
                    (void)::LocalFree(value);
                }
            }
        };
#endif

        [[nodiscard]] std::string GetNativeThreadName() noexcept
        {
            try
            {
#if defined(__linux__) || defined(__APPLE__)
                // Linux / macOS: int pthread_getname_np(pthread_t, char*, size_t); 签名一致
                // Linux 内核名通常 ≤15；macOS 可更长，用 64 避免截断
                constexpr std::size_t kNameCap =
#if defined(__APPLE__)
                    64;
#else
                    16;
#endif
                std::array<char, kNameCap> native_name{};
                if (pthread_getname_np(pthread_self(), native_name.data(), native_name.size()) != 0)
                {
                    return {};
                }
                return std::string(native_name.data());
#elif defined(_WIN32)
                PWSTR raw_name = nullptr;
                if (FAILED(::GetThreadDescription(GetCurrentThread(), &raw_name)))
                {
                    return {};
                }
                const std::unique_ptr<wchar_t, LocalFreeDeleter> native_name{raw_name};
                if (native_name == nullptr)
                {
                    return {};
                }
                return StringUtils::WStringToString(native_name.get());
#else
                return {};
#endif
            }
            catch (...)
            {
                return {};
            }
        }

        void SetNativeThreadName(std::string_view name) noexcept
        {
#if defined(__linux__)
            // Linux: int pthread_setname_np(pthread_t thread, const char* name);
            // 内核线程名最长 15 字符 + '\0'
            std::array<char, 16> native_name{};
            const std::size_t name_size = std::min(name.size(), native_name.size() - 1);
            std::memcpy(native_name.data(), name.data(), name_size);
            (void)pthread_setname_np(pthread_self(), native_name.data());
#elif defined(__APPLE__)
            // macOS: int pthread_setname_np(const char* name); 只能设置当前线程
            std::array<char, 64> native_name{};
            const std::size_t name_size = std::min(name.size(), native_name.size() - 1);
            std::memcpy(native_name.data(), name.data(), name_size);
            (void)pthread_setname_np(native_name.data());
#elif defined(_WIN32)
            try
            {
                const std::wstring wide_name = StringUtils::StringToWString(name);
                if (!wide_name.empty())
                {
                    (void)::SetThreadDescription(::GetCurrentThread(), wide_name.c_str());
                }
            }
            catch (...)
            {
            }
#else
            (void)name;
#endif
        }

        constexpr std::string_view DEFAULT_THREAD_NAME = "UNKNOWN";
        thread_local std::string t_thread_name = GetNativeThreadName();
    } // namespace

    u64 GetThreadId() noexcept
    {
#if defined(__linux__)
        static thread_local u64 THREAD_ID = static_cast<u64>(syscall(SYS_gettid));
#elif defined(__APPLE__)
        // pthread_threadid_np：系统范围唯一的 64 位线程 ID，与 lldb、Instruments 中显示的一致；
        // pthread_mach_thread_np 返回的是进程内的 Mach 端口名，不适合作为日志中的线程标识
        static thread_local u64 THREAD_ID = []() noexcept {
            u64 thread_id = 0;
            (void)pthread_threadid_np(nullptr, &thread_id);
            return thread_id;
        }();
#elif defined(_WIN32)
        static thread_local u64 THREAD_ID = static_cast<u64>(::GetCurrentThreadId());
#else
        static thread_local u64 THREAD_ID = static_cast<u64>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif
        return THREAD_ID;
    }

    void SetThreadName(std::string name)
    {
        if (name.empty())
        {
            ASSERT_NO_EFFECT_INFO(false, "You passed empty thread name");
            std::string native_name = GetNativeThreadName();
            t_thread_name = native_name.empty() ? std::string(DEFAULT_THREAD_NAME) : std::move(native_name);
            return;
        }
        t_thread_name = std::move(name);
        SetNativeThreadName(t_thread_name);
    }

    std::string_view GetThreadName() noexcept
    {
        return t_thread_name;
    }
} // namespace pans
