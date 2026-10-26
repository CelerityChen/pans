#ifndef PANS_INCLUDE_CONFIG_CONVERT_H
#define PANS_INCLUDE_CONFIG_CONVERT_H

#include <charconv>
#include <concepts>
#include <cstddef>
#include <list>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <pans/export.h>
#include <pans/type_name.h>
#include <utils/string_utils.h>

namespace pans
{

    namespace detail
    {

        /**
         * @brief 抛出统一格式的配置转换异常。
         * @param type_name 目标类型名称。
         * @param reason 转换失败原因。
         * @param value 导致失败的原始值，可为空。
         * @note 函数实现放在源文件中，避免模板代码重复生成。
         */
        [[noreturn]] PANS_API void ThrowConvertError(std::string_view type_name, std::string_view reason, std::string_view value = {});

        /**
         * @brief 解析布尔值文本。
         * @param text 待解析文本，支持 true/false、yes/no、on/off、1/0、y/n、t/f，且不区分大小写。
         * @param out 解析成功时写入布尔结果。
         * @return 解析成功返回 true，否则返回 false。
         */
        [[nodiscard]] PANS_API bool ParseBool(std::string_view text, bool &out) noexcept;

        /**
         * @brief 获取 YAML 标量节点中的文本。
         * @param node 待读取的 YAML 节点。
         * @param type_name 目标类型名称，用于生成错误信息。
         * @return 节点内部字符串的常量引用。
         * @throws std::runtime_error 当节点未定义或不是标量时抛出。
         */
        [[nodiscard]] PANS_API const std::string &ScalarText(const YAML::Node &node, std::string_view type_name);

        /**
         * @brief 将字符串解析为算术类型。
         * @tparam T 目标算术类型，但不包括 bool。
         * @param text 待解析文本。
         * @param type_name 目标类型名称，用于生成错误信息。
         * @return 解析后的数值。
         * @throws std::runtime_error 当文本为空或格式不正确时抛出。
         */
        template <typename T>
            requires std::is_arithmetic_v<T> && (!std::is_same_v<std::remove_cv_t<T>, bool>)
        [[nodiscard]] T ParseArithmetic(std::string_view text, std::string_view type_name)
        {
            std::string_view body = StringUtils::Trim(text);
            if (body.empty())
            {
                ThrowConvertError(type_name, "empty scalar can not be converted");
            }

            if (body.front() == '+')
            {
                body.remove_prefix(1);
            }

            T value{};
            std::from_chars_result result{};
            if constexpr (std::is_integral_v<T>)
            {
                int base = 10;
                if (body.size() > 2 && body[0] == '0' && (body[1] == 'x' || body[1] == 'X'))
                {
                    base = 16;
                    body.remove_prefix(2);
                }
                result = std::from_chars(body.data(), body.data() + body.size(), value, base);
            }
            else
            {
                result = std::from_chars(body.data(), body.data() + body.size(), value);
            }
            if (result.ec != std::errc{} || result.ptr != body.data() + body.size())
            {
                ThrowConvertError(type_name, "Failed to parse number.", text);
            }
            return value;
        }

        template <typename T>
            requires std::is_arithmetic_v<T> && (!std::is_same_v<std::remove_cv_t<T>, bool>)
        /**
         * @brief 将算术类型转换为 YAML 标量节点。
         * @tparam T 算术类型，但不包括 bool。
         * @param value 待转换的数值。
         * @return 保存十进制文本的 YAML 标量节点。
         */
        [[nodiscard]] YAML::Node ArithmeticToNode(const T &value)
        {
            char buffer[64]{};
            const std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
            if (result.ec != std::errc{})
            {
                ThrowConvertError(TypeName<T>(), "Failed to convert number to string.");
            }
            return YAML::Node(std::string(buffer, static_cast<std::size_t>(result.ptr - buffer)));
        }

        template <typename T>
        concept YamlDecodable = requires(const YAML::Node &node, T &out) {
            { YAML::convert<T>::decode(node, out) } -> std::convertible_to<bool>;
        };

        template <typename T>
        concept YamlEncodable = requires(const T &value) {
            { YAML::convert<T>::encode(value) } -> std::convertible_to<YAML::Node>;
        };
    }

    template <typename T>
    class ConfigConvert
    {
    public:
        /**
         * @brief 将 YAML 节点转换为目标类型。
         * @param node 待读取的 YAML 节点。
         * @return 转换后的目标类型对象。
         * @throws std::runtime_error 当节点内容无法转换时抛出。
         */
        [[nodiscard]] static T FromNode(const YAML::Node &node)
        {
            constexpr std::string_view type_name = TypeName<T>();
            if constexpr (std::is_same_v<T, bool>)
            {
                bool value = false;
                const std::string &text = detail::ScalarText(node, type_name);
                if (!detail::ParseBool(text, value))
                {
                    detail::ThrowConvertError(type_name, "Failed to parse boolean.", text);
                }
                return value;
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                return detail::ScalarText(node, type_name);
            }
            else if constexpr (std::is_arithmetic_v<T>)
            {
                return detail::ParseArithmetic<T>(detail::ScalarText(node, type_name), type_name);
            }
            else if constexpr (std::is_enum_v<T>)
            {
                using Underlying = std::underlying_type_t<T>;
                return static_cast<T>(detail::ParseArithmetic<Underlying>(detail::ScalarText(node, type_name), type_name));
            }
            else
            {
                static_assert(detail::YamlDecodable<T>, "This type should be ");
                T value{};
                if (!node.IsDefined() || !YAML::convert<T>::decode(node, value))
                {
                    detail::ThrowConvertError(type_name, "YAML::convert failed");
                }
                return value;
            }
        }

        /**
         * @brief 将目标类型转换为 YAML 节点。
         * @param value 待转换的对象。
         * @return 转换后的 YAML 节点。
         */
        [[nodiscard]] static YAML::Node ToNode(const T &value)
        {
            if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool>)
            {
                return detail::ArithmeticToNode(value);
            }
            else if constexpr (std::is_enum_v<T>)
            {
                return detail::ArithmeticToNode(static_cast<std::underlying_type_t<T>>(value));
            }
            else
            {
                static_assert(detail::YamlEncodable<T>, "This type should be pans::ConfigConvert<T> or YAML::convert<T>::encode");
                return YAML::Node(value);
            }
        }
    };

    namespace detail
    {
        template <typename C>
        concept BackEmplacceable = requires(C &container, typename C::value_type &&value) {
            container.emplace_back(std::move(value));
        };

        template <typename C>
        concept Reservable = requires(C &container, typename C::size_type size) {
            container.reserve(size);
        };

        template <typename Container>
        class SequenceConvert
        {
        public:
            using value_type = typename Container::value_type;

            /**
             * @brief 将 YAML 序列节点转换为容器。
             * @param node 待读取的 YAML 序列节点。
             * @return 转换后的容器。
             * @throws std::runtime_error 当节点不是序列或元素转换失败时抛出。
             */
            [[nodiscard]] static Container FromNode(const YAML::Node &node)
            {
                if (!node.IsSequence())
                {
                    ThrowConvertError(TypeName<Container>(), "Expected a sequence node");
                }

                Container container;
                if constexpr (Reservable<Container>)
                {
                    container.reserve(node.size());
                }
                for (const YAML::Node &item : node)
                {
                    if constexpr (BackEmplacceable<Container>)
                    {
                        container.emplace_back(ConfigConvert<value_type>::FromNode(item));
                    }
                    else
                    {
                        container.emplace(ConfigConvert<value_type>::FromNode(item));
                    }
                }
                return container; // 命名返回值优化   Named Return Value Optimization   NRVO
            }

            /**
             * @brief 将容器转换为 YAML 序列节点。
             * @param container 待转换的容器。
             * @return 转换后的 YAML 序列节点。
             */
            [[nodiscard]] static YAML::Node ToNode(const Container &container)
            {
                YAML::Node node(YAML::NodeType::Sequence);
                for (const value_type &item : container)
                {
                    node.push_back(ConfigConvert<value_type>::ToNode(item));
                }
                return node;
            }
        };

        template <typename Map>
        class StringMapConvert
        {
        public:
            using mapped_type = typename Map::mapped_type;

            /**
             * @brief 将 YAML 映射节点转换为字符串键容器。
             * @param node 待读取的 YAML 映射节点。
             * @return 转换后的映射容器。
             * @throws std::runtime_error 当节点不是映射或值转换失败时抛出。
             */
            [[nodiscard]] static Map FromNode(const YAML::Node &node)
            {
                if (!node.IsMap())
                {
                    ThrowConvertError(TypeName<Map>(), "Expected a map node");
                }
                Map container;
                if constexpr (Reservable<Map>)
                {
                    container.reserve(node.size());
                }
                for (const auto &entry : node)
                {
                    if (!entry.first.IsScalar())
                    {
                        ThrowConvertError(TypeName<Map>(), "Expected a string key in map");
                    }
                    container.emplace(entry.first.Scalar(), ConfigConvert<mapped_type>::FromNode(entry.second));
                }
                return container;
            }

            /**
             * @brief 将字符串键容器转换为 YAML 映射节点。
             * @param container 待转换的映射容器。
             * @return 转换后的 YAML 映射节点。
             */
            [[nodiscard]] static YAML::Node ToNode(const Map &container)
            {
                YAML::Node node(YAML::NodeType::Map);
                for (const auto &[key, value] : container)
                {
                    node[key] = ConfigConvert<mapped_type>::ToNode(value);
                }
                return node;
            }
        };
    }

    template <typename T, typename A>
    class ConfigConvert<std::vector<T, A>> : public detail::SequenceConvert<std::vector<T, A>>
    {
    };

    template <typename T, typename A>
    class ConfigConvert<std::list<T, A>> : public detail::SequenceConvert<std::list<T, A>>
    {
    };

    template <typename T, typename A>
    class ConfigConvert<std::set<T, A>> : public detail::SequenceConvert<std::set<T, A>>
    {
    };

    template <typename T, typename A>
    class ConfigConvert<std::unordered_set<T, A>> : public detail::SequenceConvert<std::unordered_set<T, A>>
    {
    };

    template <typename T, typename C, typename A>
    class ConfigConvert<std::set<T, C, A>> : public detail::SequenceConvert<std::set<T, C, A>>
    {
    };

    template <typename T, typename H, typename E, typename A>
    class ConfigConvert<std::unordered_set<T, H, E, A>> : public detail::SequenceConvert<std::unordered_set<T, H, E, A>>
    {
    };

    template <typename T, typename C, typename A>
    class ConfigConvert<std::map<std::string, T, C, A>> : public detail::StringMapConvert<std::map<std::string, T, C, A>>
    {
    };

    template <typename T, typename H, typename E, typename A>
    class ConfigConvert<std::unordered_map<std::string, T, H, E, A>> : public detail::StringMapConvert<std::unordered_map<std::string, T, H, E, A>>
    {
    };
}
#endif
