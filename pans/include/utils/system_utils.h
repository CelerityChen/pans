#ifndef PANS_INCLUDE_UTILS_SYSTEM_UTILS_H_
#define PANS_INCLUDE_UTILS_SYSTEM_UTILS_H_

#include <chrono>
#include <string>

#include <pans/types.h>
#include <pans/export.h>

namespace pans
{
    /**
     * @brief 获取库初始化以来经过的单调时钟时长。
     * @return 从库初始化时刻开始计算的时长。
     */
    [[nodiscard]] PANS_API std::chrono::steady_clock::duration GetElapsedTime() noexcept;

    /**
     * @brief 获取当前协程或纤程 ID。
     * @return 当前纤程 ID；不支持纤程时返回 0。
     */
    [[nodiscard]] PANS_API u64 GetFiberId() noexcept;

    /**
     * @brief Get the backtrace of the current stack
     * @param size The number of frames to include in the backtrace
     * @param skip The number of frames to skip in the backtrace
     * @param prefix The prefix to add to the backtrace
     * @return The backtrace as a string
     */
    [[nodiscard]] PANS_API std::string GetBacktrace(int size = 20, int skip = 1, const std::string& prefix = "");
}
#endif
