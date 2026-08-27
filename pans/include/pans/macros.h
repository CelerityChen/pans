#ifndef PANS_INCLUDE_PANS_MACROS_H
#define PANS_INCLUDE_PANS_MACROS_H

#include <cassert>
#include <iostream>
#include <cstdint>

// ---------------------------------------------------------------------------
// 断言宏
// 失败时打印文件名、行号与表达式，并调用 assert(x)。
// Release（定义 NDEBUG）下 assert 为空操作，但返回/continue/break 仍会执行。
// ---------------------------------------------------------------------------

/// 条件为假时打印错误并触发 assert。
#define PANS_ASSERT(x)                                                                                   \
    if (!(x)) [[unlikely]]                                                                               \
    {                                                                                                    \
        std::cerr << __FILE__ << ":" << __LINE__ << " ASSERT FAILED: " << #x << "\nStacktrace: to do\n"; \
        assert(x);                                                                                       \
    }

/// 同上，额外打印自定义信息 w。
#define PANS_ASSERT_INFO(x, w)                                                                                           \
    if (!(x)) [[unlikely]]                                                                                               \
    {                                                                                                                    \
        std::cerr << __FILE__ << ":" << __LINE__ << " ASSERT FAILED: " << #x << "\n[" << w << "]\nStack trace: to do\n"; \
        assert(x);                                                                                                       \
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

// ---------------------------------------------------------------------------
// 无效值常量（各宽度无符号整数的全 1，常作哨兵/未初始化标记）
// ---------------------------------------------------------------------------
#define INVALID64 (~0ULL)
#define INVALID32 0xFFFFFFFF
#define INVALID16 0xFFFF
#define INVALID8 0xFF

// ---------------------------------------------------------------------------
// 各宽度无符号整数最大值
// ---------------------------------------------------------------------------
#define MAX_U8 0xFF
#define MAX_U16 0xFFFF
#define MAX_U32 0xFFFFFFFF
#define MAX_U64 (~0ULL)

// ---------------------------------------------------------------------------
// 固定宽度整数类型别名
// ---------------------------------------------------------------------------
using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;
using s64 = int64_t;

#endif
