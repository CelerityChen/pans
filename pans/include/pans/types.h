#ifndef PANS_INCLUDE_PANS_TYPES_H
#define PANS_INCLUDE_PANS_TYPES_H

#include <cstdint>

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