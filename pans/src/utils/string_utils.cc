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

        [[nodiscard]] constexpr bool IsHighSurrogate(u32 value) noexcept
        {
            return value >= HIGH_SURROGATE_FIRST && value <= HIGH_SURROGATE_LAST;
        }

        [[nodiscard]] constexpr bool IsLowSurrogate(u32 value) noexcept
        {
            return value >= LOW_SURROGATE_FIRST && value <= LOW_SURROGATE_LAST;
        }

        [[nodiscard]] constexpr bool IsValidCodePoint(u32 value) noexcept
        {
            return value <= MAX_UNICODE_CODE_POINT && !IsHighSurrogate(value) &&
                   !IsLowSurrogate(value);
        }

        void AppendUtf8(std::string& output, u32 code_point)
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

        void AppendWChar(std::wstring& output, u32 code_point)
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

        /// 从 text[offset] 起解码一个 UTF-8 码点；成功则推进 offset。
        [[nodiscard]] bool DecodeUtf8CodePoint(std::string_view text,
                                               std::size_t& offset,
                                               u32& code_point) noexcept
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
    }  // namespace

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
}  // namespace pans
