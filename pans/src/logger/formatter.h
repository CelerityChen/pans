#ifndef PANS_SRC_PANS_LOGGER_FORMATTER_H
#define PANS_SRC_PANS_LOGGER_FORMATTER_H

#include <memory>
#include <string>
#include <vector>

#include "logger/buffer.h"
#include "logger/buffer_config.h"
#include "logger/log_record.h"

namespace pans::detail{
    using FormattedRecordBuffer = InlineBuffer<FORMATTED_RECORD_INLINE_CAPACITY>;

    // 构造时解析 pattern，格式化时复用格式项，避免逐条日志重新扫描模板。
    class Formatter final{
        public:
        // 作为FormatItem的基类，所有的FormatItem子类都必须实现format方法，定义不同item相同的接口
        class FormatItem{
            public:
                virtual ~FormatItem() = default;
                virtual void format(const LogRecordView& record, FormattedRecordBuffer& output) const = 0;
        };

        explicit Formatter(std::string_view pattern);

        // 追加到 output，不主动清空；复用缓冲区时由调用方决定清理时机。
        void format(const LogRecordView& record, FormattedRecordBuffer& output) const;
        [[nodiscard]] const std::string& getPattern() const noexcept { return m_pattern; }

        private:
            int parse();
            void addLiteral(std::string& literal);

        private:
        std::string m_pattern = "";
        std::vector<std::unique_ptr<FormatItem>> m_items;
    };
}

#endif
