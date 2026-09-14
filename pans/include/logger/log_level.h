#ifndef PANS_INCLUDE_PANS_LOGGER_LOG_LEVEL_H
#define PANS_INCLUDE_PANS_LOGGER_LOG_LEVEL_H

#include <cstdint>
#include <string_view>

#include <pans/export.h>

namespace pans {

// ---------------------------------------------------------------------------
// LogLevel
//
// 日志严重程度。数值越大表示越严重；过滤时通常输出「>= 当前阈值」的级别。
// OFF 表示关闭输出（不作为真实日志级别写入）。
// ---------------------------------------------------------------------------
class PANS_API LogLevel final {
public:
    enum class Level : std::uint8_t {
        LOG_LV_DEBUG = 1,  // 调试：变量值、调用路径等
        LOG_LV_INFO = 2,   // 信息：启动、配置加载等正常事件
        LOG_LV_WARN = 3,   // 警告：可恢复的异常或潜在风险
        LOG_LV_ERROR = 4,  // 错误：功能失败，但进程可继续
        LOG_LV_FATAL = 5,  // 致命：不可恢复，通常随后终止
        LOG_LV_OFF = 6,    // 关闭：用作阈值，表示不输出任何日志
    };

    /// 将级别转为大写短名（如 "DEBUG"）；未知级别返回 "UNKNOWN"。
    [[nodiscard]] static std::string_view ToString(Level level) noexcept;

    /// 不区分大小写解析级别名；无法识别时返回 LOG_LV_OFF。
    [[nodiscard]] static Level FromString(std::string_view level) noexcept;
};

}  // namespace pans

#endif
