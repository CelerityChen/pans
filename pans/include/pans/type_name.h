#ifndef PANS_INCLUDE_PANS_TYPE_NAME_H
#define PANS_INCLUDE_PANS_TYPE_NAME_H

#include <cstddef>
#include <source_location>
#include <string_view>

namespace pans::detail
{

    /**
     * @brief 获取编译器生成的模板函数签名。
     * @tparam T 要获取名称的类型。
     * @return 包含类型信息的编译器函数签名。
     */
    template <typename T>
    [[nodiscard]] consteval std::string_view TypeNameProbe() noexcept
    {
#if defined(_MSC_VER)
        return __FUNCSIG__;
#else
        return std::source_location::current().function_name();
#endif
    }

    // GCC: consteval std::string_view pans::detail::TypeNameProbe() [with T = int;...]
    // Clang: std::string_view pans::detail::TypeNameProbe() [T = int]
    // MSVC: pans::detail::TypeNameProbe<int>(void) noexcept

    /**
     * @brief 从 MSVC 函数签名中提取模板类型名称。
     * @param signature 编译器生成的函数签名。
     * @return 提取出的类型名称；无法识别时返回原始签名。
     */
    [[nodiscard]] constexpr std::string_view ExtractMsvcTypeName(std::string_view signature) noexcept
    {
        constexpr std::string_view marker = "TypeNameProbe<";
        constexpr std::string_view suffix = ">(void)";

        // Use the outer function marker. A nested user type may itself contain
        // a component named TypeNameProbe<...>; rfind() would select that one.
        const std::size_t marker_pos = signature.find(marker);
        if (marker_pos == std::string_view::npos)
        {
            return signature;
        }

        const std::size_t begin = marker_pos + marker.size();
        const std::size_t end = signature.rfind(suffix);
        if (end == std::string_view::npos || end <= begin)
        {
            return signature;
        }
        return signature.substr(begin, end - begin);
    }

    /**
     * @brief 从当前编译器生成的函数签名中提取类型名称。
     * @tparam T 要获取名称的类型。
     * @return T 的编译期类型名称。
     */
    template <typename T>
    [[nodiscard]] consteval std::string_view ExtractTypeName() noexcept
    {
        constexpr std::string_view signature = TypeNameProbe<T>();
#if defined(_MSC_VER) && !defined(__clang__)
        constexpr std::string_view type_name = ExtractMsvcTypeName(signature);
        static_assert(type_name != signature, "Failed to recognize this MSVC function signature");
        return type_name;
#else
        constexpr std::string_view marker = "T = ";
        constexpr std::size_t marker_pos = signature.find(marker);

        if constexpr (marker_pos == std::string_view::npos)
        {
            static_assert(marker_pos != std::string_view::npos,
                          "Failed to recognize the compiler type-name signature");
            return signature;
        }
        else
        {
            constexpr std::size_t begin = marker_pos + marker.size();
            // GCC 在 T 之后还会追加 "; std::string_view = ..."，以第一个 ';' 结束；
            // Clang 以签名末尾的 ']' 结束，必须用 rfind：类型自身可能含 ']'，如 int[3]
            constexpr std::size_t semicolon = signature.find(';', begin);
            constexpr std::size_t bracket = signature.rfind(']');
            constexpr std::size_t end = semicolon != std::string_view::npos ? semicolon : bracket;
            if constexpr (end == std::string_view::npos || end <= begin)
            {
                static_assert(end != std::string_view::npos && end > begin,
                              "Failed to find the end of the compiler type name");
                return signature.substr(begin);
            }
            else
            {
                return signature.substr(begin, end - begin);
            }
        }
#endif
    }

    template <typename T>
    inline constexpr std::string_view TYPE_NAME = ExtractTypeName<T>();
}

namespace pans
{
    /**
     * @brief 在编译期获取类型名称。
     * @tparam T 要获取名称的类型。
     * @return T 的编译期类型名称。
     */
    template <typename T>
    [[nodiscard]] consteval std::string_view TypeName() noexcept
    {
        return detail::TYPE_NAME<T>;
    }
}
#endif
