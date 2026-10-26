#ifndef PANS_INCLUDE_PANS_LOGGER_LOGGER_H
#define PANS_INCLUDE_PANS_LOGGER_LOGGER_H

#include <memory>
#include <string>
#include <string_view>

#include <pans/export.h>
#include <logger/log_level.h>
#include <logger/appender.h>

namespace pans{
    namespace detail{
        class LoggerAccess;
    }

    class PANS_API Logger final{
        public:
        class Impl;

        /**
         * @brief 创建日志器。
         * @param name 日志器名称，默认为 root。
         */
        explicit Logger(std::string name = "root");

        /**
         * @brief 销毁日志器。
         */
        ~Logger();

        /**
         * @brief 禁止复制构造。
         */
        Logger(const Logger&) = delete;

        /**
         * @brief 禁止复制赋值。
         */
        Logger& operator=(const Logger&) = delete;

        /**
         * @brief 禁止移动构造。
         */
        Logger(Logger&&) = delete;

        /**
         * @brief 禁止移动赋值。
         */
        Logger& operator=(Logger&&) = delete;

        /**
         * @brief 判断指定级别的日志是否会被记录。
         * @param level 待检查的日志级别。
         * @return 当前日志器会记录该级别时返回 true。
         */
        [[nodiscard]] bool shouldLog(LogLevel::Level level) const noexcept;

        /**
         * @brief 设置日志器的最低日志级别。
         * @param level 新的最低日志级别。
         */
        void setLevel(LogLevel::Level level) noexcept;

        /**
         * @brief 获取日志器的最低日志级别。
         * @return 当前最低日志级别。
         */
        [[nodiscard]] LogLevel::Level getLevel() const noexcept;

        /**
         * @brief 获取日志器名称。
         * @return 日志器名称视图。
         */
        [[nodiscard]] std::string_view getName() const noexcept;

        /**
         * @brief 设置日志格式化模板。
         * @param pattern 格式化模板。
         */
        void setFormatter(std::string_view pattern);

        /**
         * @brief 获取当前日志格式化模板。
         * @return 格式化模板字符串。
         */
        [[nodiscard]] std::string getFormatterPattern() const;

        /**
         * @brief 添加日志输出器。
         * @param appender 要添加的输出器。
         */
        void addAppender(AppenderPtr appender);

        /**
         * @brief 移除指定日志输出器。
         * @param appender 要移除的输出器。
         */
        void removeAppender(const AppenderPtr& appender);

        /**
         * @brief 移除所有日志输出器。
         */
        void clearAppenders();

        /**
         * @brief 刷新所有日志输出器。
         */
        void flush();

        /**
         * @brief 同步所有日志输出器。
         */
        void sync();

        private:
        std::unique_ptr<Impl> m_impl;
        friend class detail::LoggerAccess;
    };

    using LoggerPtr = std::shared_ptr<Logger>;

    /**
     * @brief 获取全局根日志器。
     * @return 根日志器的共享指针。
     */
    [[nodiscard]] PANS_API LoggerPtr GetRootLogger();

    /**
     * @brief 获取指定名称的日志器。
     * @param name 日志器名称。
     * @return 指定名称的日志器共享指针。
     */
    [[nodiscard]] PANS_API LoggerPtr GetLogger(std::string_view name);
}
#endif
