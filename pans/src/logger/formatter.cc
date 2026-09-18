#include "logger/formatter.h"
#include <unordered_map>

namespace pans::detail{
    constexpr std::string_view DEFAULT_DATA_FORMAT = "%Y-%m-%d %H:%M:%S";

    Formatter::Formatter(std::string_view pattern)
    : m_pattern(pattern)
    {
        if(parse())
        {
            throw std::invalid_argument("Invalid format pattern");
        }
    }

    void Formatter::format(const LogRecordView& record, FormattedRecordBuffer& output) const
    {
        for(const auto& item : m_items)
        {
            item->format(record, output);
        }
    }

    // 直接转换到栈缓冲区，再按实际长度追加，避免创建临时字符串。
    template<typename T>
    void appendInteger(FormattedRecordBuffer& output, T value)
    {
        std::array<char, 24> buffer{};
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        ASSERT_RET_NONE_INFO(result.ec == std::errc(), "trans " << value << " to chars failed.");
        output.append(buffer.data(), static_cast<size_t>(result.ptr - buffer.data()));
    }

    class LiteralFormatItem final : public Formatter::FormatItem
    {
        public:
        explicit LiteralFormatItem(std::string value)
        : m_value(std::move(value))
        {}

        void format([[maybe_unused]] const LogRecordView& record, FormattedRecordBuffer& output) const
        {
            output.append(m_value);
        }
        private:
        std::string m_value;
    };

    class LevelFormatItem final : public Formatter::FormatItem
    {
        void format(const LogRecordView& record, FormattedRecordBuffer& output) const
        {
            output.append(LogLevel::ToString(record.m_level));
        }
    };

    class MessageFormatItem final : public Formatter::FormatItem
    {
        public:
        void format([[maybe_unused]] const LogRecordView& record, FormattedRecordBuffer& output) const
        {
            output.append(record.m_message);
        }
    };

    class ElapsedFormatItem final : public Formatter::FormatItem
    {
        void format(const LogRecordView& record, FormattedRecordBuffer& output) const
        {
            appendInteger(output, std::chrono::duration_cast<std::chrono::milliseconds>(record.m_elapsed).count());
        }
    };

    class LoggerNameFormatItem final : public Formatter::FormatItem
    {
        void format(const LogRecordView& record, FormattedRecordBuffer& output) const
        {
            output.append(record.m_logger_name);
        }
    };

    class ThreadIdFormatItem final : public Formatter::FormatItem
    {
        void format(const LogRecordView& record, FormattedRecordBuffer& output) const
        {
            appendInteger(output, record.m_thread_id);
        }
    };

    class NewLineFormatItem final : public Formatter::FormatItem
    {
        void format([[maybe_unused]] const LogRecordView& record, FormattedRecordBuffer& output) const
        {
            output.append('\n');
        }
    };

    class DateTimeFormatItem final : public Formatter::FormatItem
    {
        public:
        explicit DateTimeFormatItem(std::string_view format)
        : m_format(format.empty() ? DEFAULT_DATA_FORMAT : format)
        {}

        // 同一秒复用日期字符串；微秒由独立格式项逐条输出。
        // TODO: 函数内 TLS 被同线程所有实例共享，缓存键还需区分格式。
        // 20 字节仅容纳默认格式；自定义长格式和首次 epoch 秒仍需处理。
        void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
        {
            static thread_local time_t last_second = 0;
            static thread_local char cached_date_time[20] = {'\0'};
            const auto duration = record.m_timestamp.time_since_epoch();
            const time_t current_second = static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(duration).count());
            if(current_second != last_second)
            {
                std::tm buffer{};
                #if defined(_WIN32)
                const errno_t result = localtime_s(&buffer, &current_second);
                ASSERT_RET_NONE_INFO(result == 0, "failed to convert log time to local time.");
                #else
                const std::tm* result = localtime_r(&current_second, &buffer);  // 性能隐形消耗
                ASSERT_RET_NONE_INFO(result != nullptr, "failed to convert log time to local time.");
                #endif
                const std::size_t size = std::strftime(cached_date_time, sizeof(cached_date_time), m_format.c_str(), &buffer);
                ASSERT_RET_NONE_INFO(size != 0, "failed to format log time.");
                last_second = current_second;
            }
            output.append(cached_date_time);
        }

        private:
            std::string m_format;   // %Y-%m-%d %H:%M:%S

    };

    class MicrosecondsFormatItem final : public Formatter::FormatItem
    {
        public:
        void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
        {
            const auto total_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(record.m_timestamp.time_since_epoch()).count();
            // C++ 负数取余可能为负，归一化到一秒内的非负微秒部分。
            s64 microseconds = total_microseconds % 1000000;
            if(microseconds < 0)
            {
                microseconds += 1000000;
            }
            // 要补齐6位，不足前面补0
            std::array<char, MICROSECONDS_WIDTH> digits{};
            auto remaining = static_cast<u32>(microseconds);
            for(std::size_t index = digits.size(); index-- > 0; )
            {
                digits[index] = static_cast<char>('0' + remaining % 10);
                remaining /= 10;
            }
            output.append(digits.data(), digits.size());
        }
        private:
        static constexpr std::size_t MICROSECONDS_WIDTH = 6;
    };

    class FileNameFormatItem final : public Formatter::FormatItem
    {
        public:
        void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
        {
            output.append(record.m_file_name);
        }
    };

    class LineNumberFormatItem final : public Formatter::FormatItem
    {
        public:
        void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
        {
            appendInteger(output, record.m_line);
        }
    };

    class TabFormatItem final : public Formatter::FormatItem
    {
        public:
        void format([[maybe_unused]] const LogRecordView& record, FormattedRecordBuffer& output) const override
        {
            output.append('\t');
        }
    };

    class FiberIdFormatItem final : public Formatter::FormatItem
    {
        public:
        void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
        {
            appendInteger(output, record.m_fiber_id);
        }
    };


    class ThreadNameFormatItem final : public Formatter::FormatItem
    {
        public:
        void format(const LogRecordView& record, FormattedRecordBuffer& output) const override
        {
            output.append(record.m_thread_name);
        }
    };

    using FormatItemFactory = std::unique_ptr<Formatter::FormatItem>(*)(std::string_view);

    template<typename Item>
    [[nodiscard]] std::unique_ptr<Formatter::FormatItem> CreateSimpleFormatItem(std::string_view)
    {
        return std::make_unique<Item>();
    }

    template<typename Item>
    [[nodiscard]] std::unique_ptr<Formatter::FormatItem> CreateConfiguredFormatItem(std::string_view format)
    {
        return std::make_unique<Item>(format);
    }

    // format: %d%T%t%T%F%T%N%T[%p]%T%f:%l%T%m%n
    // 占位符表示
    [[nodiscard]] const std::unordered_map<char, FormatItemFactory>& GetFormatItemFactories()
    {
        static const std::unordered_map<char, FormatItemFactory> FORMAT_ITEM_FACTORIES{
            {'m', &CreateSimpleFormatItem<MessageFormatItem>},      // m represent MessageFormatItem,
            {'p', &CreateSimpleFormatItem<LevelFormatItem>},         // p represent LevelFormatItem,
            {'r', &CreateSimpleFormatItem<ElapsedFormatItem>},      // r represent ElapsedFormatItem,
            {'c', &CreateSimpleFormatItem<LoggerNameFormatItem>},   // c represent LoggerNameFormatItem,
            {'t', &CreateSimpleFormatItem<ThreadIdFormatItem>},      // t represent ThreadIdFormatItem,
            {'n', &CreateSimpleFormatItem<NewLineFormatItem>},       // n represent NewLineFormatItem,
            {'d', &CreateConfiguredFormatItem<DateTimeFormatItem>},   // d represent DateTimeFormatItem,
            {'u', &CreateSimpleFormatItem<MicrosecondsFormatItem>},   // u represent MicrosecondsFormatItem,
            {'f', &CreateSimpleFormatItem<FileNameFormatItem>},       // f represent FileNameFormatItem,
            {'l', &CreateSimpleFormatItem<LineNumberFormatItem>},     // l represent LineNumberFormatItem,
            {'T', &CreateSimpleFormatItem<TabFormatItem>},            // T represent TabFormatItem,
            {'F', &CreateSimpleFormatItem<FiberIdFormatItem>},        // F represent FiberIdFormatItem,
            {'N', &CreateSimpleFormatItem<ThreadNameFormatItem>},     // N represent ThreadNameFormatItem,
        };
        return FORMAT_ITEM_FACTORIES;
    }

    [[nodiscard]] std::unique_ptr<Formatter::FormatItem> CreateFormatItem(char directive, std::string_view format)
    {
        const auto& factories = GetFormatItemFactories();
        const auto it = factories.find(directive);
        ASSERT_RETVAL_INFO(it != factories.end(), nullptr,"unknown logger format directive: " << directive);
        return it->second(format);
    }

    void Formatter::addLiteral(std::string& literal)
    {
        if(literal.empty())
        {
            return;
        }
        m_items.push_back(std::make_unique<LiteralFormatItem>(std::move(literal)));
        literal.clear();
    }

    // 普通文本合并为 LiteralFormatItem，%% 转义为单个百分号。
    // 其余指令通过工厂创建格式项；不完整指令或非法括号返回错误码。
    // 示例：%d{%Y-%m-%d %H:%M:%S}.%u%Tthread=%t%T[%p]%T%m%n
    int Formatter::parse()
    {
        std::string literal;
        for(std::size_t index = 0; index < m_pattern.size(); ++index)
        {
            if(m_pattern[index] != '%')
            {
                literal.push_back(m_pattern[index]);
                continue;
            }
            ASSERT_RETVAL_INFO(index + 1 <m_pattern.size(), -1, "logger format pattern ends with an incomplete directive.");
            const char directive = m_pattern[++index];
            if(directive == '%')
            {
                literal.push_back('%');
                continue;
            }
            addLiteral(literal);
            std::string_view item_format;
            if(index + 1 < m_pattern.size() && m_pattern[index + 1] == '{')
            {
                const std::size_t closing_brace = m_pattern.find('}', index + 2);
                ASSERT_RETVAL_INFO(closing_brace != std::string::npos && closing_brace != (index + 2), -2, "missing a closing brace or empty");
                // TODO: 尚未将花括号内容赋给 item_format，自定义格式会被忽略。
                index = closing_brace;
            }
            // %Y-%m-%d %H:%M:%S
            m_items.push_back(CreateFormatItem(directive, item_format));
        }
        addLiteral(literal);
        for(const auto& item : m_items)
        {
            ASSERT_RETVAL_INFO(item != nullptr, -3, "logger formatter contains a null format item.");
        }
        return 0;
    }
}
