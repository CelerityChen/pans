#ifndef PANS_INCLUDE_UTILS_STRING_UTILS_H
#define PANS_INCLUDE_UTILS_STRING_UTILS_H

// UTF-8 <-> 宽字符（wchar_t）转换
// Windows: wchar_t 为 UTF-16；Linux / macOS: wchar_t 为 UTF-32。

#include <string>
#include <string_view>

#include <pans/export.h>

namespace pans
{
    class PANS_API StringUtils final
    {
    public:
        StringUtils() = delete;

        /// 宽字符串 → UTF-8。非法代理项用 U+FFFD 替代。
        [[nodiscard]] static std::string WStringToString(std::wstring_view wstr) noexcept;

        /// UTF-8 → 宽字符串。非法 UTF-8 序列用 U+FFFD 替代。
        [[nodiscard]] static std::wstring StringToWString(std::string_view str) noexcept;
    };
}
#endif
