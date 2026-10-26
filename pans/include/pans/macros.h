#ifndef PANS_INCLUDE_PANS_MACROS_H
#define PANS_INCLUDE_PANS_MACROS_H

#include <cassert>

#include <logger/log.h>
#include <utils/system_utils.h>

// ---------------------------------------------------------------------------
// 断言宏
// 失败时打印文件名、行号与表达式，并调用 assert(x)。
// Release（定义 NDEBUG）下 assert 为空操作，但返回/continue/break 仍会执行。
// ---------------------------------------------------------------------------

/// 条件为假时打印错误并触发 assert。
#define PANS_ASSERT(x)                                                                                   \
    if (const bool pans_assert_ok = static_cast<bool>(x); !pans_assert_ok) [[unlikely]]                 \
    {                                                                                                    \
        PANS_LOG_FATAL(PANS_LOG_ROOT()) << __FILE__ << ":" << __LINE__ << " ASSERT FAILED: " << #x << "\nStacktrace: \n" << pans::GetBacktrace(); \
        assert(pans_assert_ok);                                                                          \
    }

/// 同上，额外打印自定义信息 w。
#define PANS_ASSERT_INFO(x, w)                                                                                           \
    if (const bool pans_assert_ok = static_cast<bool>(x); !pans_assert_ok) [[unlikely]]                                 \
    {                                                                                                                    \
        PANS_LOG_FATAL(PANS_LOG_ROOT()) << __FILE__ << ":" << __LINE__ << " ASSERT FAILED: " << #x << "\n[" << w << "]\nStack trace: \n" << pans::GetBacktrace(); \
        assert(pans_assert_ok);                                                                                          \
    }

/// 条件为假时断言并返回指定值 val（用于有返回值的函数）。
#define ASSERT_RETVAL(x, val) \
    do                        \
    {                         \
        if (x) [[likely]]     \
            break;            \
        PANS_ASSERT(x);       \
        return val;           \
    } while (0)

/// 同上，额外打印自定义信息 info。
#define ASSERT_RETVAL_INFO(x, val, info) \
    do                                   \
    {                                    \
        if (x) [[likely]]                \
            break;                       \
        PANS_ASSERT_INFO(x, info);       \
        return val;                      \
    } while (0)

/// 条件为假时断言并 return（用于 void 函数）。
#define ASSERT_RET_NONE(x) \
    do                     \
    {                      \
        if (x) [[likely]]  \
            break;         \
        PANS_ASSERT(x);    \
        return;            \
    } while (0)

/// 同上，额外打印自定义信息 info。
#define ASSERT_RET_NONE_INFO(x, info) \
    do                                \
    {                                 \
        if (x) [[likely]]             \
            break;                    \
        PANS_ASSERT_INFO(x, info);    \
        return;                       \
    } while (0)

/// 条件为假时仅断言，不改变控制流。
#define ASSERT_NO_EFFECT(x) \
    do                      \
    {                       \
        if (x) [[likely]]   \
            break;          \
        PANS_ASSERT(x);     \
    } while (0)

/// 同上，额外打印自定义信息 info。
#define ASSERT_NO_EFFECT_INFO(x, info) \
    do                                 \
    {                                  \
        if (x) [[likely]]              \
            break;                     \
        PANS_ASSERT_INFO(x, info);     \
    } while (0)

/// 条件为假时断言并 continue（用于循环体内）。
/// 末尾 else 空块用于保证后续 if/else 绑定正确。
#define ASSERT_CONTINUE(x) \
    if (!(x)) [[unlikely]] \
    {                      \
        PANS_ASSERT(x);    \
        continue;          \
    }                      \
    else                   \
    {                      \
    }

/// 同上，额外打印自定义信息 info。
#define ASSERT_CONTINUE_INFO(x, info) \
    if (!(x)) [[unlikely]]            \
    {                                 \
        PANS_ASSERT_INFO(x, info);    \
        continue;                     \
    }                                 \
    else                              \
    {                                 \
    }

/// 条件为假时断言并 break（用于循环体内）。
#define ASSERT_BREAK(x)    \
    if (!(x)) [[unlikely]] \
    {                      \
        PANS_ASSERT(x);    \
        break;             \
    }                      \
    else                   \
    {                      \
    }

/// 同上，额外打印自定义信息 info。
#define ASSERT_BREAK_INFO(x, info) \
    if (!(x)) [[unlikely]]         \
    {                              \
        PANS_ASSERT_INFO(x, info); \
        break;                     \
    }                              \
    else                           \
    {                              \
    }

#endif
