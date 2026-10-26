#ifndef PANS_INCLUDE_LOGGER_APPENDER_H_
#define PANS_INCLUDE_LOGGER_APPENDER_H_

#include <memory>
#include <string>

#include <pans/export.h>
#include <logger/log_level.h>

namespace pans{

    namespace detail{
        // 代理内部访问权限，采用接口和实现隔离的实现方式
        class AppenderAccess;
    }

    class PANS_API Appender final{
        public:
            class Impl;

            /**
             * @brief 销毁输出器。
             */
            ~Appender() = default;

            /**
             * @brief 禁止复制构造。
             */
            Appender(const Appender&) = delete;

            /**
             * @brief 禁止移动构造。
             */
            Appender(Appender&&) = delete;

            /**
             * @brief 禁止复制赋值。
             */
            Appender& operator=(const Appender&) = delete;

            /**
             * @brief 禁止移动赋值。
             */
            Appender& operator=(Appender&&) = delete;

            /**
             * @brief 设置输出器的最低日志级别。
             * @param level 新的最低日志级别。
             */
            void setLevel(LogLevel::Level level) noexcept;

            /**
             * @brief 获取输出器的最低日志级别。
             * @return 当前最低日志级别。
             */
            [[nodiscard]] LogLevel::Level getLevel() const noexcept;

            /**
             * @brief 刷新输出器中的缓存数据。
             */
            void flush();

            /**
             * @brief 将输出器中的缓存同步到目标设备。
             */
            void sync();
        private:
            /**
             * @brief 使用内部实现对象构造输出器。
             * @param impl 输出器的内部实现对象。
             */
            explicit Appender(std::unique_ptr<Impl> impl) noexcept;
            std::unique_ptr<Impl> m_impl;
            friend class detail::AppenderAccess;
    };

    using AppenderPtr = std::shared_ptr<Appender>;

    /**
     * @brief 创建输出到标准输出的日志输出器。
     * @return 新创建的输出器。
     */
    [[nodiscard]] PANS_API AppenderPtr MakeStdoutAppender();

    /**
     * @brief 创建输出到文件的日志输出器。
     * @param file_name 日志文件路径。
     * @return 新创建的输出器。
     */
    [[nodiscard]] PANS_API AppenderPtr MakeFileAppender(std::string file_name);
}
#endif
