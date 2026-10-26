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

        /**
         * @brief 宽字符串转换为 UTF-8 字符串
         * @param wstr 待转换的宽字符串（Windows 下按 UTF-16 解释，Linux / macOS 下按 UTF-32 解释）
         * @return UTF-8 编码的字符串；不成对的代理项、超出 Unicode 范围的值以 U+FFFD 替代
         * @note 标记为 noexcept，内存分配失败时会调用 std::terminate
         */
        [[nodiscard]] static std::string WStringToString(std::wstring_view wstr) noexcept;

        /**
         * @brief UTF-8 字符串转换为宽字符串
         * @param str 待转换的 UTF-8 字符串
         * @return 宽字符串（Windows 下为 UTF-16，Linux / macOS 下为 UTF-32）；
         *         非法的 UTF-8 序列（截断、过长编码、代理区码点等）每个出错字节以一个 U+FFFD 替代
         * @note 标记为 noexcept，内存分配失败时会调用 std::terminate
         */
        [[nodiscard]] static std::wstring StringToWString(std::string_view str) noexcept;

        /// Trim 系列默认去除的 ASCII 空白字符：空格、制表符、换行、回车、换页、垂直制表符
        inline static constexpr std::string_view ASCII_WHITESPACE{" \t\n\r\f\v"};

        // string_view 不拥有 string text 这个字符串对象，也不持有，只是拿到原本的视图；其生命周期不应该超过 string 对象

        /**
         * @brief 去除字符串首尾的指定字符
         * @param text 待处理的字符串
         * @param delimiters 要去除的字符集合，其中任一字符出现在首尾都会被去除；默认为 ASCII 空白字符
         * @return 去除首尾字符后的新字符串；text 全部由 delimiters 中的字符组成时返回空串
         */
        [[nodiscard]] static std::string Trim(const std::string& text, std::string_view delimiters = ASCII_WHITESPACE);

        /**
         * @brief 去除字符串首尾的指定字符，不复制数据
         * @param text 待处理的字符串视图
         * @param delimiters 要去除的字符集合，其中任一字符出现在首尾都会被去除；默认为 ASCII 空白字符
         * @return 指向 text 原数据的子视图；text 全部由 delimiters 中的字符组成时返回空视图
         * @warning 返回值引用 text 指向的原始数据，原始数据销毁后不可再使用
         */
        [[nodiscard]] static std::string_view Trim(std::string_view text, std::string_view delimiters = ASCII_WHITESPACE) noexcept;

        /**
         * @brief 去除字符串开头的指定字符
         * @param text 待处理的字符串
         * @param delimiters 要去除的字符集合，其中任一字符出现在开头都会被去除；默认为 ASCII 空白字符
         * @return 去除开头字符后的新字符串；text 全部由 delimiters 中的字符组成时返回空串
         */
        [[nodiscard]] static std::string TrimLeft(const std::string& text, std::string_view delimiters = ASCII_WHITESPACE);

        /**
         * @brief 去除字符串开头的指定字符，不复制数据
         * @param text 待处理的字符串视图
         * @param delimiters 要去除的字符集合，其中任一字符出现在开头都会被去除；默认为 ASCII 空白字符
         * @return 指向 text 原数据的子视图；text 全部由 delimiters 中的字符组成时返回空视图
         * @warning 返回值引用 text 指向的原始数据，原始数据销毁后不可再使用
         */
        [[nodiscard]] static std::string_view TrimLeft(std::string_view text, std::string_view delimiters = ASCII_WHITESPACE) noexcept;

        /**
         * @brief 去除字符串末尾的指定字符。
         * @param text 待处理的字符串。
         * @param delimiters 要去除的字符集合，其中任一字符出现在末尾都会被去除；默认为 ASCII 空白字符。
         * @return 去除末尾字符后的新字符串；text 全部由 delimiters 中的字符组成时返回空串。
         */
        [[nodiscard]] static std::string TrimRight(const std::string& text, std::string_view delimiters = ASCII_WHITESPACE);

        /**
         * @brief 去除字符串末尾的指定字符，不复制数据。
         * @param text 待处理的字符串视图。
         * @param delimiters 要去除的字符集合，其中任一字符出现在末尾都会被去除；默认为 ASCII 空白字符。
         * @return 指向 text 原数据的子视图；text 全部由 delimiters 中的字符组成时返回空视图。
         * @warning 返回值引用 text 指向的原始数据，原始数据销毁后不可再使用。
         */
        [[nodiscard]] static std::string_view TrimRight(std::string_view text, std::string_view delimiters = ASCII_WHITESPACE) noexcept;
    };
}
#endif
