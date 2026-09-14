#ifndef PANS_SRC_PANS_LOGGER_BUFFER_H
#define PANS_SRC_PANS_LOGGER_BUFFER_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <streambuf>
#include <string_view>
#include <vector>

namespace pans::detail {

// ---------------------------------------------------------------------------
// InlineBuffer
//
// 小对象优化（SSO 风格）的可追加字符缓冲：
// - 数据未超过 INLINE_CAPACITY 时写在栈上 array，无堆分配
// - 一旦超出，把已有内容迁到 vector，之后只走堆路径
//
// 用于组装日志正文，降低高频短日志的分配开销。
// ---------------------------------------------------------------------------
template <std::size_t INLINE_CAPACITY>
class InlineBuffer {
public:
    void append(const char* data, std::size_t size) {
        if (size == 0) {
            return;
        }

        // 仍在内联区，且本次追加后仍放得下：直接 memcpy。
        if (m_overflow.empty() && m_size + size <= INLINE_CAPACITY) {
            std::memcpy(m_inline.data() + m_size, data, size);
            m_size += size;
            return;
        }

        // 首次溢出：为 vector 预留空间，并把内联区已有数据迁过去。
        if (m_overflow.empty() && m_size + size > INLINE_CAPACITY) {
            const std::size_t required_capacity = m_size + size;
            m_overflow.reserve(std::max(required_capacity, INLINE_CAPACITY * 2));
            m_overflow.insert(m_overflow.end(), m_inline.data(),
                              m_inline.data() + m_size);
        }

        // 已在（或刚切到）溢出区：追加新数据，并以 vector 大小为准。
        m_overflow.insert(m_overflow.end(), data, data + size);
        m_size = m_overflow.size();
    }

    void append(std::string_view value) {
        append(value.data(), value.size());
    }

    void append(char value) {
        append(&value, 1);
    }

    /// 当前有效数据的起始指针（内联或堆，取决于是否溢出）。
    [[nodiscard]] const char* data() const noexcept {
        return m_overflow.empty() ? m_inline.data() : m_overflow.data();
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return m_size;
    }

    [[nodiscard]] std::string_view view() const noexcept {
        return {data(), size()};
    }

private:
    std::array<char, INLINE_CAPACITY> m_inline{};  // 短日志：栈上存储
    std::vector<char> m_overflow;                  // 长日志：堆上存储
    std::size_t m_size = 0;                        // 当前有效字节数
};

// ---------------------------------------------------------------------------
// SmallStreamBuffer
//
// 把 InlineBuffer 接到 std::streambuf，从而继续用 ostream 与
// iostream 操纵符（hex / setw / setprecision 等）做格式化输出。
// ---------------------------------------------------------------------------
template <std::size_t INLINE_CAPACITY>
class SmallStreamBuffer final : public std::streambuf {
public:
    explicit SmallStreamBuffer(InlineBuffer<INLINE_CAPACITY>& buffer) noexcept
        : m_buffer(buffer) {}

protected:
    // 批量写入（<< "text"、write 等走此路径）。
    std::streamsize xsputn(const char* data, std::streamsize size) override {
        if (size <= 0) {
            return 0;
        }
        m_buffer.append(data, static_cast<std::size_t>(size));
        return size;
    }

    // 单字符溢出写入（未设 put 区时，逐字符输出会走此路径）。
    int_type overflow(int_type character) override {
        if (traits_type::eq_int_type(character, traits_type::eof())) {
            return traits_type::not_eof(character);
        }
        m_buffer.append(traits_type::to_char_type(character));
        return character;
    }

private:
    InlineBuffer<INLINE_CAPACITY>& m_buffer;
};

}  // namespace pans::detail

#endif
