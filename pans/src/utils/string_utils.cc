#include <utils/string_utils.h>

#include <pans/macros.h>

namespace pans
{
    // 仅本翻译单元可见的编解码辅助
    namespace
    {
        constexpr u32 MAX_UNICODE_CODE_POINT = 0x10FFFF;
        constexpr u32 REPLACEMENT_CODE_POINT = 0xFFFD;
        constexpr u32 HIGH_SURROGATE_FIRST = 0xD800;
        constexpr u32 HIGH_SURROGATE_LAST = 0xDBFF;
        constexpr u32 LOW_SURROGATE_FIRST = 0xDC00;
        constexpr u32 LOW_SURROGATE_LAST = 0xDFFF;

        static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4,
                      "StringUtils requires a UTF-16 or UTF-32 wchar_t");

        /**
         * @brief 判断是否为 UTF-16 高位代理项（U+D800 ~ U+DBFF）
         * @param value 待判断的值
         * @return 是高位代理项返回 true
         */
        [[nodiscard]] constexpr bool IsHighSurrogate(u32 value) noexcept
        {
            return value >= HIGH_SURROGATE_FIRST && value <= HIGH_SURROGATE_LAST;
        }

        /**
         * @brief 判断是否为 UTF-16 低位代理项（U+DC00 ~ U+DFFF）
         * @param value 待判断的值
         * @return 是低位代理项返回 true
         */
        [[nodiscard]] constexpr bool IsLowSurrogate(u32 value) noexcept
        {
            return value >= LOW_SURROGATE_FIRST && value <= LOW_SURROGATE_LAST;
        }

        /**
         * @brief 判断是否为合法的 Unicode 码点
         * @param value 待判断的值
         * @return 不超过 U+10FFFF 且不在代理区时返回 true
         */
        [[nodiscard]] constexpr bool IsValidCodePoint(u32 value) noexcept
        {
            return value <= MAX_UNICODE_CODE_POINT && !IsHighSurrogate(value) &&
                   !IsLowSurrogate(value);
        }

        /**
         * @brief 将一个码点按 UTF-8 编码追加到字符串末尾（1 ~ 4 字节）
         * @param output 输出字符串
         * @param code_point 待编码的码点；非法码点以 U+FFFD 替代
         */
        void AppendUtf8(std::string &output, u32 code_point)
        {
            if (!IsValidCodePoint(code_point))
            {
                code_point = REPLACEMENT_CODE_POINT;
            }

            if (code_point <= 0x7F)
            {
                output.push_back(static_cast<char>(code_point));
            }
            else if (code_point <= 0x7FF)
            {
                output.push_back(static_cast<char>((code_point >> 6U) | 0xC0U));
                output.push_back(static_cast<char>((code_point & 0x3FU) | 0x80U));
            }
            else if (code_point <= 0xFFFF)
            {
                output.push_back(static_cast<char>((code_point >> 12U) | 0xE0U));
                output.push_back(static_cast<char>(((code_point >> 6U) & 0x3FU) | 0x80U));
                output.push_back(static_cast<char>((code_point & 0x3FU) | 0x80U));
            }
            else
            {
                output.push_back(static_cast<char>((code_point >> 18U) | 0xF0U));
                output.push_back(static_cast<char>(((code_point >> 12U) & 0x3FU) | 0x80U));
                output.push_back(static_cast<char>(((code_point >> 6U) & 0x3FU) | 0x80U));
                output.push_back(static_cast<char>((code_point & 0x3FU) | 0x80U));
            }
        }

        /**
         * @brief 将一个码点追加到宽字符串末尾
         * @param output 输出宽字符串
         * @param code_point 待追加的码点；非法码点以 U+FFFD 替代。
         *        wchar_t 为 2 字节（UTF-16）时，超过 U+FFFF 的码点拆成一对代理项
         */
        void AppendWChar(std::wstring &output, u32 code_point)
        {
            if (!IsValidCodePoint(code_point))
            {
                code_point = REPLACEMENT_CODE_POINT;
            }

            if constexpr (sizeof(wchar_t) == 2)
            {
                // UTF-16
                if (code_point <= 0xFFFF)
                {
                    output.push_back(static_cast<wchar_t>(code_point));
                }
                else
                {
                    const u32 adjusted = code_point - 0x10000U;
                    output.push_back(static_cast<wchar_t>(HIGH_SURROGATE_FIRST + (adjusted >> 10U)));
                    output.push_back(static_cast<wchar_t>(LOW_SURROGATE_FIRST + (adjusted & 0x3FFU)));
                }
            }
            else
            {
                // UTF-32
                output.push_back(static_cast<wchar_t>(code_point));
            }
        }

        /**
         * @brief 从 text[offset] 起解码一个 UTF-8 码点
         * @param text 待解码的 UTF-8 字符串
         * @param offset 起始位置；解码成功时推进到下一个码点的起始位置，失败时保持不变
         * @param code_point 输出解码得到的码点，仅在成功时写入
         * @return 成功返回 true；首字节非法、序列被截断、续字节非法、过长编码、
         *         超出 Unicode 范围或落在代理区时返回 false
         */
        [[nodiscard]] bool DecodeUtf8CodePoint(std::string_view text,
                                               std::size_t &offset,
                                               u32 &code_point) noexcept
        {
            if (offset >= text.size())
            {
                return false;
            }

            const auto lead = static_cast<u8>(text[offset]);
            std::size_t length = 0;
            u32 value = 0;

            if (lead <= 0x7F)
            {
                length = 1;
                value = lead;
            }
            else if ((lead & 0xE0U) == 0xC0U)
            {
                length = 2;
                value = lead & 0x1FU;
            }
            else if ((lead & 0xF0U) == 0xE0U)
            {
                length = 3;
                value = lead & 0x0FU;
            }
            else if ((lead & 0xF8U) == 0xF0U)
            {
                length = 4;
                value = lead & 0x07U;
            }
            else
            {
                return false;
            }

            if (offset + length > text.size())
            {
                return false;
            }

            for (std::size_t i = 1; i < length; ++i)
            {
                const auto cont = static_cast<u8>(text[offset + i]);
                if ((cont & 0xC0U) != 0x80U)
                {
                    return false;
                }
                value = (value << 6U) | (cont & 0x3FU);
            }

            // 拒绝过长编码、越界与代理区
            if ((length == 2 && value < 0x80U) || (length == 3 && value < 0x800U) ||
                (length == 4 && value < 0x10000U) || !IsValidCodePoint(value))
            {
                return false;
            }

            offset += length;
            code_point = value;
            return true;
        }
    } // namespace

    std::string StringUtils::WStringToString(std::wstring_view wstr) noexcept
    {
        std::string output;
        output.reserve(wstr.size());

        for (std::size_t i = 0; i < wstr.size();)
        {
            const auto unit = static_cast<u32>(wstr[i]);

            if constexpr (sizeof(wchar_t) == 2)
            {
                if (IsHighSurrogate(unit))
                {
                    if (i + 1 < wstr.size())
                    {
                        const auto low = static_cast<u32>(wstr[i + 1]);
                        if (IsLowSurrogate(low))
                        {
                            const u32 code_point =
                                0x10000U + (((unit - HIGH_SURROGATE_FIRST) << 10U) |
                                            (low - LOW_SURROGATE_FIRST));
                            AppendUtf8(output, code_point);
                            i += 2;
                            continue;
                        }
                    }
                    AppendUtf8(output, REPLACEMENT_CODE_POINT);
                    ++i;
                    continue;
                }

                if (IsLowSurrogate(unit))
                {
                    AppendUtf8(output, REPLACEMENT_CODE_POINT);
                    ++i;
                    continue;
                }
            }

            AppendUtf8(output, unit);
            ++i;
        }

        return output;
    }

    std::wstring StringUtils::StringToWString(std::string_view str) noexcept
    {
        std::wstring output;
        output.reserve(str.size());

        std::size_t offset = 0;
        while (offset < str.size())
        {
            u32 code_point = 0;
            if (DecodeUtf8CodePoint(str, offset, code_point))
            {
                AppendWChar(output, code_point);
            }
            else
            {
                AppendWChar(output, REPLACEMENT_CODE_POINT);
                ++offset;
            }
        }

        return output;
    }

    std::string StringUtils::Trim(const std::string &text, std::string_view delimiters)
    {
        return std::string(Trim(std::string_view(text), delimiters));
    }

    std::string_view StringUtils::Trim(std::string_view text, std::string_view delimiters) noexcept
    {
        const std::size_t begin = text.find_first_not_of(delimiters);
        if (begin == std::string_view::npos)
        {
            return {};
        }
        const std::size_t end = text.find_last_not_of(delimiters);
        return text.substr(begin, end - begin + 1);
    }

    std::string StringUtils::TrimLeft(const std::string &text, std::string_view delimiters)
    {
        return std::string(TrimLeft(std::string_view(text), delimiters));
    }

    std::string_view StringUtils::TrimLeft(std::string_view text, std::string_view delimiters) noexcept
    {
        const std::size_t begin = text.find_first_not_of(delimiters);
        if (begin == std::string_view::npos)
        {
            return {};
        }
        return text.substr(begin);
    }

    std::string StringUtils::TrimRight(const std::string &text, std::string_view delimiters)
    {
        return std::string(TrimRight(std::string_view(text), delimiters));
    }

    std::string_view StringUtils::TrimRight(std::string_view text, std::string_view delimiters) noexcept
    {
        const std::size_t end = text.find_last_not_of(delimiters);
        if (end == std::string_view::npos)
        {
            return {};
        }
        return text.substr(0, end + 1);
    }
} // namespace pans
