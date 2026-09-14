#ifndef PANS_SRC_PANS_LOGGER_BUFFER_CONFIG_H
#define PANS_SRC_PANS_LOGGER_BUFFER_CONFIG_H

#include <cstddef>

namespace pans::detail {

// ---------------------------------------------------------------------------
// 日志缓冲区内联容量（字节）
//
// 多数日志较短，优先落在栈上的 InlineBuffer，避免堆分配；
// 超长消息再溢出到 heap。容量可按场景调大/调小。
// ---------------------------------------------------------------------------

/// 单条日志消息正文的内联容量。
inline constexpr std::size_t LOG_MESSAGE_INLINE_CAPACITY = 128;

/// printf 风格格式化临时缓冲的内联容量。
inline constexpr std::size_t PRINTF_FORMAT_INLINE_CAPACITY = 128;

/// 已格式化完整记录（时间/级别/正文等）的内联容量。
inline constexpr std::size_t FORMATTED_RECORD_INLINE_CAPACITY = 256;

}  // namespace pans::detail

#endif
