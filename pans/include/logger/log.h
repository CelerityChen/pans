#ifndef PANS_INCLUDE_LOGGER_LOG_H_
#define PANS_INCLUDE_LOGGER_LOG_H_

#include <cstddef>
#include <iosfwd>
#include <string_view>

#include <logger/log_level.h>
#include <logger/appender.h>
#include <logger/logger.h>
#include <pans/macros.h>

// pans::detail 命名空间
// 不推荐调用者直接使用；
// 对编译器可见，让编译器能够处理正确的语法、编译依赖
namespace pans::detail
{

    // PANS_API 表示从动态库中导出
    /**
     * 只服务于日志宏
     */
    class PANS_API LogLine final
    {
    public:
        LogLine(Logger &logger, LogLevel::Level level, u32 line, std::string_view file_name);
        ~LogLine() noexcept;

        LogLine(const LogLine &) = delete;
        LogLine operator=(const LogLine &) = delete;
        LogLine(LogLine &&) = delete;
        LogLine &operator=(LogLine &&) = delete;

        [[nodiscard]] std::ostream &stream() noexcept;

    private:
        struct Impl;

        static constexpr std::size_t LOG_LINE_IMPL_SIZE = 1024;
        alignas(std::max_align_t) std::byte m_implStorage[LOG_LINE_IMPL_SIZE];

        [[nodiscard]] Impl &getImpl() noexcept;
    };

    PANS_API void LogPrintf(Logger &logger, LogLevel::Level level, u32 line, std::string_view file_name, const char *format, ...);
}

#define PANS_LOG_LEVEL(logger, level)                                                          \
    if (auto pans_log_logger = (logger); !pans_log_logger)                                     \
    {                                                                                          \
    }                                                                                          \
    else if (const auto pans_log_level = (level); !pans_log_logger->shouldLog(pans_log_level)) \
    {                                                                                          \
    }                                                                                          \
    else                                                                                       \
        pans::detail::LogLine(*pans_log_logger, pans_log_level, __LINE__, __FILE__).stream()

#define PANS_LOG_DEBUG(logger) PANS_LOG_LEVEL((logger), pans::LogLevel::Level::LOG_LV_DEBUG)
#define PANS_LOG_INFO(logger) PANS_LOG_LEVEL((logger), pans::LogLevel::Level::LOG_LV_INFO)
#define PANS_LOG_WARN(logger) PANS_LOG_LEVEL((logger), pans::LogLevel::Level::LOG_LV_WARN)
#define PANS_LOG_ERROR(logger) PANS_LOG_LEVEL((logger), pans::LogLevel::Level::LOG_LV_ERROR)
#define PANS_LOG_FATAL(logger) PANS_LOG_LEVEL((logger), pans::LogLevel::Level::LOG_LV_FATAL)

#define PANS_LOG_FMT_LEVEL(logger, level, format, ...)                                         \
    if (auto pans_log_logger = (logger); !pans_log_logger)                                     \
    {                                                                                          \
    }                                                                                          \
    else if (const auto pans_log_level = (level); !pans_log_logger->shouldLog(pans_log_level)) \
    {                                                                                          \
    }                                                                                          \
    else                                                                                       \
        pans::detail::LogPrintf(*pans_log_logger, pans_log_level, __LINE__, __FILE__, (format)__VA_OPT__(, ) __VA_ARGS__)

#define PANS_LOG_FMT_DEBUG(logger, format, ...) \
    PANS_LOG_FMT_LEVEL((logger), pans::LogLevel::Level::LOG_LV_DEBUG, (format) __VA_OPT__(, ) __VA_ARGS__)
#define PANS_LOG_FMT_INFO(logger, format, ...) \
    PANS_LOG_FMT_LEVEL((logger), pans::LogLevel::Level::LOG_LV_INFO, (format) __VA_OPT__(, ) __VA_ARGS__)
#define PANS_LOG_FMT_WARN(logger, format, ...) \
    PANS_LOG_FMT_LEVEL((logger), pans::LogLevel::Level::LOG_LV_WARN, (format) __VA_OPT__(, ) __VA_ARGS__)
#define PANS_LOG_FMT_ERROR(logger, format, ...) \
    PANS_LOG_FMT_LEVEL((logger), pans::LogLevel::Level::LOG_LV_ERROR, (format) __VA_OPT__(, ) __VA_ARGS__)
#define PANS_LOG_FMT_FATAL(logger, format, ...) \
    PANS_LOG_FMT_LEVEL((logger), pans::LogLevel::Level::LOG_LV_FATAL, (format) __VA_OPT__(, ) __VA_ARGS__)

#define PANS_LOG_ROOT() pans::GetRootLogger()
#define PANS_LOG_NAME(name) pans::GetLogger(name)

#define LOG_DEBUG PANS_LOG_DEBUG(g_logger)
#define LOG_INFO PANS_LOG_INFO(g_logger)
#define LOG_WARN PANS_LOG_WARN(g_logger)
#define LOG_ERROR PANS_LOG_ERROR(g_logger)
#define LOG_FATAL PANS_LOG_FATAL(g_logger)

#endif