#include <pans/macros.h>
#include <iostream>
#include <limits>
#include <type_traits>

// ---------------------------------------------------------------------------
// 编译期检查：类型宽度/符号性，以及 INVALID* 与 MAX_U* 常量取值
// ---------------------------------------------------------------------------
static_assert(sizeof(u8) == 1 && std::is_unsigned_v<u8>);
static_assert(sizeof(s8) == 1 && std::is_signed_v<s8>);
static_assert(sizeof(u16) == 2 && std::is_unsigned_v<u16>);
static_assert(sizeof(s16) == 2 && std::is_signed_v<s16>);
static_assert(sizeof(u32) == 4 && std::is_unsigned_v<u32>);
static_assert(sizeof(s32) == 4 && std::is_signed_v<s32>);
static_assert(sizeof(u64) == 8 && std::is_unsigned_v<u64>);
static_assert(sizeof(s64) == 8 && std::is_signed_v<s64>);
static_assert(INVALID8 == MAX_U8);
static_assert(INVALID16 == MAX_U16);
static_assert(INVALID32 == MAX_U32);
static_assert(INVALID64 == MAX_U64);
static_assert(INVALID8 == 0xFF);
static_assert(INVALID16 == 0xFFFF);
static_assert(INVALID32 == 0xFFFFFFFF);
static_assert(INVALID64 == ~0ULL);
static_assert(MAX_U8 == std::numeric_limits<u8>::max());
static_assert(MAX_U16 == std::numeric_limits<u16>::max());
static_assert(MAX_U32 == std::numeric_limits<u32>::max());
static_assert(MAX_U64 == std::numeric_limits<u64>::max());

/// 运行时复查类型与常量；顺带覆盖 ASSERT_NO_EFFECT / ASSERT_RET_NONE(_INFO)。
void test_types_and_constants()
{
    // 类型宽度与符号性
    ASSERT_NO_EFFECT(sizeof(u8) == 1 && std::is_unsigned_v<u8>);
    ASSERT_NO_EFFECT(sizeof(s8) == 1 && std::is_signed_v<s8>);
    ASSERT_NO_EFFECT(sizeof(u16) == 2 && std::is_unsigned_v<u16>);
    ASSERT_NO_EFFECT(sizeof(s16) == 2 && std::is_signed_v<s16>);
    ASSERT_NO_EFFECT(sizeof(u32) == 4 && std::is_unsigned_v<u32>);
    ASSERT_NO_EFFECT(sizeof(s32) == 4 && std::is_signed_v<s32>);
    ASSERT_NO_EFFECT(sizeof(u64) == 8 && std::is_unsigned_v<u64>);
    ASSERT_NO_EFFECT(sizeof(s64) == 8 && std::is_signed_v<s64>);

    // INVALID* 与 MAX_U* 应对齐
    ASSERT_NO_EFFECT(INVALID8 == MAX_U8);
    ASSERT_NO_EFFECT(INVALID16 == MAX_U16);
    ASSERT_NO_EFFECT(INVALID32 == MAX_U32);
    ASSERT_NO_EFFECT(INVALID64 == MAX_U64);

    // MAX_U* 应等于对应类型的 numeric_limits::max()
    ASSERT_RET_NONE(MAX_U8 == std::numeric_limits<u8>::max());
    ASSERT_RET_NONE(MAX_U16 == std::numeric_limits<u16>::max());
    ASSERT_RET_NONE_INFO(MAX_U32 == std::numeric_limits<u32>::max(), "This is a test for ASSERT_RET_NONE_INFO");
    ASSERT_RET_NONE_INFO(MAX_U64 == std::numeric_limits<u64>::max(), "This is a test for ASSERT_RET_NONE_INFO");
}

/// 故意让 ASSERT_RETVAL 失败，验证会返回 -1（而非落到后面的 return 0）。
[[nodiscard]] int test_assert_retval()
{
    ASSERT_RETVAL(false, -1);
    return 0;
}

/// 同上，带自定义信息的 ASSERT_RETVAL_INFO。
[[nodiscard]] int test_assert_retval_info()
{
    ASSERT_RETVAL_INFO(false, -1, "This is a test for ASSERT_RETVAL_INFO");
    return 0;
}

/// 在循环中演练 ASSERT_CONTINUE / ASSERT_BREAK / ASSERT_NO_EFFECT 及其 _INFO 变体。
void test_assert_macros()
{
    // 条件为真：不应触发断言
    ASSERT_NO_EFFECT(true);
    ASSERT_NO_EFFECT_INFO(true, "This should not trigger an assertion.");

    // 循环索引为 5 时视为“非法”，用于触发各断言宏的失败分支
    int never_reached_condition = 5;
    int i = 0;

    // ASSERT_CONTINUE：i == 5 时跳过本次迭代（Release 下仍 continue）
    for (int i = 0; i < 10; ++i)
    {
        ASSERT_CONTINUE(i != never_reached_condition);
        std::cout << "AAA-----:" << i << std::endl;
    }
    std::cout << "A----------------------------------------: " << i << std::endl;

    // ASSERT_CONTINUE_INFO：同上，失败时额外打印信息
    for (int i = 0; i < 10; ++i)
    {
        ASSERT_CONTINUE_INFO(i != never_reached_condition, "Loop index should not be" << i);
    }
    std::cout << "B----------------------------------------: " << i << std::endl;

    // ASSERT_BREAK：i == 5 时断言并 break（打印 0..4 后退出）
    for (int i = 0; i < 10; ++i)
    {
        ASSERT_BREAK(i != never_reached_condition);
        std::cout << "BBB-----:" << i << std::endl;
    }
    std::cout << "C----------------------------------------: " << i << std::endl;

    // ASSERT_BREAK_INFO：同上，失败时额外打印信息
    for (int i = 0; i < 10; ++i)
    {
        ASSERT_BREAK_INFO(i != never_reached_condition, "Loop index should be" << i);
    }
    std::cout << "D----------------------------------------: " << i << std::endl;

    // ASSERT_NO_EFFECT：i == 5 时仅断言，循环照常继续
    for (int i = 0; i < 10; ++i)
    {
        ASSERT_NO_EFFECT(i != never_reached_condition);
        std::cout << "CCC----:" << i << std::endl;
    }
    std::cout << "E----------------------------------------: " << i << std::endl;

    // ASSERT_NO_EFFECT_INFO：同上，失败时额外打印信息
    for (int i = 0; i < 10; ++i)
    {
        ASSERT_NO_EFFECT_INFO(i != never_reached_condition, "Loop index should be" << i);
    }
    std::cout << "F----------------------------------------: " << i << std::endl;
}

int main()
{
#ifdef PANS_DEBUG
    std::cout << "This program is compiled in debug moode. Assert will terminate the program on failure." << std::endl;
#endif

#ifdef NDEBUG
    std::cout << "This program is compiled in release mode. Assert will do nothing on failure." << std::endl;
#endif

    test_types_and_constants();
    test_assert_macros();

    // 预期均返回 -1（断言失败后的返回值）
    auto retval1 = test_assert_retval();
    std::cout << "test_assert_retval() returned: " << retval1 << std::endl;
    auto retval2 = test_assert_retval_info();
    std::cout << "test_assert_retval_info() returned: " << retval2 << std::endl;

    return EXIT_SUCCESS;
}
