# macros 与单元测试搭建笔记

本文整理 pans 工程中断言宏头文件、测试用例与测试 CMake 的改动要点，以及配置 / 编译阶段踩过的坑。

相关文件：

- `pans/include/pans/macros.h` — 断言宏、无效值 / 最大值常量、固定宽度类型别名
- `tests/test_macros.cpp` — 对上述宏与常量的编译期 + 运行时检查
- `tests/CMakeLists.txt` — 为每个测试源文件生成可执行文件并注册 CTest

---

## 1. `macros.h` 内容概览

### 断言宏

| 宏 | 行为 |
|----|------|
| `PANS_ASSERT` / `PANS_ASSERT_INFO` | 失败时打印文件名、行号、表达式（及附加信息），再 `assert` |
| `ASSERT_RETVAL` / `_INFO` | 失败时断言并 `return val` |
| `ASSERT_RET_NONE` / `_INFO` | 失败时断言并 `return`（void 函数） |
| `ASSERT_NO_EFFECT` / `_INFO` | 失败时仅断言，不改变控制流 |
| `ASSERT_CONTINUE` / `_INFO` | 失败时断言并 `continue` |
| `ASSERT_BREAK` / `_INFO` | 失败时断言并 `break` |

要点：

- 使用 `[[likely]]` / `[[unlikely]]` 标注分支预期。
- Release（定义 `NDEBUG`）下 `assert` 为空操作，但 **`return` / `continue` / `break` 仍会执行**。
- `ASSERT_CONTINUE` / `ASSERT_BREAK` 末尾带空 `else {}`，避免后续 `if/else` 绑定错误。

### 常量与类型别名

- `INVALID8/16/32/64`：各宽度无符号全 1，作哨兵 / 未初始化标记。
- `MAX_U8/16/32/64`：对应无符号最大值（与 `INVALID*` 取值相同）。
- `u8`/`s8` … `u64`/`s64`：`stdint.h` 固定宽度别名。

---

## 2. `test_macros.cpp` 测什么

1. **编译期** `static_assert`：类型宽度 / 符号性，`INVALID*` 与 `MAX_U*` 对齐，`MAX_U*` 等于 `numeric_limits::max()`。
2. **运行时** `test_types_and_constants`：用 `ASSERT_NO_EFFECT` / `ASSERT_RET_NONE(_INFO)` 复查同上内容。
3. **控制流宏** `test_assert_macros`：在循环中演练 `CONTINUE` / `BREAK` / `NO_EFFECT`（及 `_INFO`）。
4. **返回值宏** `test_assert_retval(_info)`：故意让条件为假，预期返回 `-1`。

---

## 3. `tests/CMakeLists.txt` 机制

- `file(GLOB … CONFIGURE_DEPENDS)` 收集 `tests/` 下 `*.cpp` / `*.cc` / `*.cxx`。
- 每个源文件一个可执行目标；目标名 = 文件名去扩展名（`NAME_WE`）。
- 链接 `pans` 与 `Threads::Threads`。
- 运行时输出目录固定为 `${PROJECT_SOURCE_DIR}/bin/tests`（单配置 + 多配置生成器都设置）。
- `add_test` 注册到 CTest。

---

## 4. 踩坑：`bin/tests` 里没有可执行文件

**现象：** 配置成功后 `bin/tests/` 为空。

**原因：** 只跑了 `cmake ..`（生成构建文件），**没有编译**。

`RUNTIME_OUTPUT_DIRECTORY` 只规定「编译成功后产物放哪」；目录可事先存在，但二进制要等构建才会写出。

**正确流程：**

```bash
cmake -S . -B build          # 或 cd build && cmake ..
cmake --build build          # 或 cd build && make -j
# 产物：bin/tests/test_macros
ctest --test-dir build       # 可选：跑 CTest
```

### 配置阶段曾遇到的错误

| 错误 | 原因 | 处理 |
|------|------|------|
| `get_filename_component` 参数个数不对 | 缺少 `NAME_WE` | 写成 `get_filename_component(TEST_NAME "${TEST_SOURCE}" NAME_WE)` |
| `Target "test_macros" links to: PANS::pans` 找不到 | 工程内目标名是 `pans`，尚未导出 `PANS::pans` 别名 | `target_link_libraries(... pans ...)` |

---

## 5. 本次改动清单（相对此前工程骨架）

- 新增 `pans/include/pans/macros.h`（含分区与用途注释）。
- 新增 `tests/test_macros.cpp`（含测试意图注释）。
- 充实 `tests/CMakeLists.txt`（GLOB → 可执行文件 → CTest，含注释）。
- `.gitignore` 忽略 `bin/`（测试产物输出目录，勿入库）。
