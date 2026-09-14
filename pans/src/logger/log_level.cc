#include <logger/log_level.h>

#include <algorithm>
#include <array>
#include <cctype>

namespace pans {

std::string_view LogLevel::ToString(Level level) noexcept {
    switch (level) {
#define XX(name)              \
    case Level::LOG_LV_##name: \
        return #name;
        XX(DEBUG)
        XX(INFO)
        XX(WARN)
        XX(ERROR)
        XX(FATAL)
        XX(OFF)
#undef XX
        default:
            return "UNKNOWN";
    }
}

LogLevel::Level LogLevel::FromString(std::string_view level) noexcept {
    // 最长级别名为 DEBUG/ERROR/FATAL（5 字符）；超长输入直接视为无效。
    constexpr std::size_t kMaxNameLen = 5;
    if (level.empty() || level.size() > kMaxNameLen) {
        return Level::LOG_LV_OFF;
    }

    std::array<char, kMaxNameLen> normalized{};
    std::transform(level.begin(), level.end(), normalized.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::toupper(c));
                   });

    // 只用实际输入长度构造 view，才能与 "DEBUG" 等字面量正确比较。
    const std::string_view upper_value(normalized.data(), level.size());

#define XX(name) \
    if (upper_value == #name) return Level::LOG_LV_##name;
    XX(DEBUG)
    XX(INFO)
    XX(WARN)
    XX(ERROR)
    XX(FATAL)
    XX(OFF)
#undef XX

    return Level::LOG_LV_OFF;
}

}  // namespace pans
