#ifndef PANS_INCLUDE_UTILS_THREAD_UTILS_H
#define PANS_INCLUDE_UTILS_THREAD_UTILS_H

#include <string>
#include <string_view>

#include <pans/export.h>
#include <pans/types.h>

namespace pans{
    /**
     * @brief 获取当前线程 ID。
     * @return 当前线程的数值 ID。
     */
    [[nodiscard]] PANS_API u64 GetThreadId() noexcept;

    /**
     * @brief 设置当前线程名称。
     * @param name 新的线程名称。
     */
    PANS_API void SetThreadName(std::string name);

    /**
     * @brief 获取当前线程名称。
     * @return 当前线程名称的字符串视图。
     */
    [[nodiscard]] PANS_API std::string_view GetThreadName() noexcept;
}
#endif
