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
            ~Appender() = default;
            Appender(const Appender&) = delete;
            Appender(Appender&&) = delete;
            Appender& operator=(const Appender&) = delete;
            Appender& operator=(Appender&&) = delete;

            void setLevel(LogLevel::Level level) noexcept;
            [[nodiscard]] LogLevel::Level getLevel() const noexcept;

            void flush();
            void sync();
        private:
            explicit Appender(std::unique_ptr<Impl> impl) noexcept;
            std::unique_ptr<Impl> m_impl;
            friend class detail::AppenderAccess;
    };

    using AppenderPtr = std::shared_ptr<Appender>;

    [[nodiscard]] PANS_API AppenderPtr MakeStdoutAppender();
    [[nodiscard]] PANS_API AppenderPtr MakeFileAppender(std::string file_name);
}
#endif