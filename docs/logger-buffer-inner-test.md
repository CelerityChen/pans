# Logger 缓冲与内部测试搭建笔记

本文整理 pans 日志模块起步改动：级别 API、SSO 风格缓冲区、符号导出宏，以及 `inner_test` 的 CMake 接线与踩坑。

相关文件：

- `pans/include/pans/export.h` — 跨平台 `PANS_API` 符号导出宏
- `pans/include/logger/log_level.h` — 日志级别枚举与字符串互转声明
- `pans/src/logger/log_level.cc` — `ToString` / `FromString` 实现
- `pans/src/logger/buffer_config.h` — 各类缓冲区内联容量常量
- `pans/src/logger/buffer.h` — `InlineBuffer` + `SmallStreamBuffer`
- `tests/inner_test/` — 依赖 `pans/src` 的内部实现测试
- `tests/CMakeLists.txt` — 通过 `add_subdirectory(inner_test)` 接入

---

## 1. 分层约定

| 位置 | 可见性 | 内容 |
|------|--------|------|
| `pans/include/` | 公开 API | `LogLevel`、`PANS_API` |
| `pans/src/` | 库内部 | `InlineBuffer`、容量常量等（`pans::detail`） |

与 `target_include_directories` 一致：`include` 为 PUBLIC，`src` 为 PRIVATE。内部测试若要访问 `src` 头文件，需在测试目标上额外加 PRIVATE include。

---

## 2. `PANS_API`（`export.h`）

| 场景 | 宏展开 |
|------|--------|
| 静态库 / 未定义 `PANS_SHARED_LIBRARY` | 空 |
| Windows + 编共享库（`PANS_BUILDING_LIBRARY`） | `__declspec(dllexport)` |
| Windows + 用共享库 | `__declspec(dllimport)` |
| GCC/Clang + 共享库 | `__attribute__((visibility("default")))` |

约定：编 pans 共享库时定义 `PANS_SHARED_LIBRARY` + `PANS_BUILDING_LIBRARY`；下游只定义 `PANS_SHARED_LIBRARY`。

---

## 3. `LogLevel`

级别按严重程度递增：

`DEBUG < INFO < WARN < ERROR < FATAL < OFF`

- `OFF` 一般作过滤阈值（关闭输出），不作为真实日志内容级别写入。
- `ToString`：返回大写短名（如 `"DEBUG"`）；未知返回 `"UNKNOWN"`。
- `FromString`：不区分大小写；无法识别返回 `OFF`。

### `FromString` 易错点

必须按**输入实际长度**构造 `string_view` 再与 `"DEBUG"` 等比较。若固定写成长度 6 的 view，会与长度 5 的字面量永远不相等，结果恒为 `OFF`。超长或空串应直接视为无效。

---

## 4. `InlineBuffer` / `SmallStreamBuffer`

### `InlineBuffer`（SSO 风格）

1. 未超 `INLINE_CAPACITY`：写栈上 `std::array`，无堆分配。
2. 首次超限：把已有内容迁到 `std::vector`，之后只走堆。
3. `data()` / `view()` 按是否溢出选择后端。

适合高频短日志，降低 `malloc`。

### `SmallStreamBuffer`

继承 `std::streambuf`，把 `xsputn` / `overflow` 转发到 `InlineBuffer`，从而继续用 `ostream` 与 `hex` / `setw` / `setprecision` 等操纵符。

### 容量常量（`buffer_config.h`）

| 常量 | 默认 | 用途 |
|------|------|------|
| `LOG_MESSAGE_INLINE_CAPACITY` | 128 | 单条消息正文 |
| `PRINTF_FORMAT_INLINE_CAPACITY` | 128 | printf 风格临时区 |
| `FORMATTED_RECORD_INLINE_CAPACITY` | 256 | 完整格式化记录 |

---

## 5. `inner_test` 与公开测试的区别

`tests/` 根目录的 GLOB 只收集本层 `*.cpp`，适合测公开头（`include/`）。

`tests/inner_test/` 专门测**库内部实现**（如 `logger/buffer.h`）：

- 单独 `CMakeLists.txt`，由 `tests/CMakeLists.txt` 末尾 `add_subdirectory(inner_test)` 接入。
- `target_include_directories(... PRIVATE pans/src)`。
- 输出目录仍用父级的 `PANS_TEST_OUTPUT_DIRECTORY`（`bin/tests/`）。

当前 `test_log_buffer`：对比 `SmallStreamBuffer` 与 `std::stringstream` 在多种消息长度下的写入耗时。

运行示例：

```bash
./bin/tests/test_log_buffer 100000
```

---

## 6. 踩坑记录

### 6.1 构建目录里没有 `test_log_buffer`

**原因：** `tests/CMakeLists.txt` 未 `add_subdirectory(inner_test)`，子目录目标不进入构建图。

**处理：** 在公开测试 GLOB 循环之后增加：

```cmake
add_subdirectory(inner_test)
```

### 6.2 链接报 `Undefined symbols: _main`

**原因：** `INNER_TEST_SOURCE` 写成了目录路径：

```cmake
set(INNER_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/")  # 错误
```

此时 `add_executable` 没有有效 `.cpp`，`link.txt` 里只有 `libpans.a`，没有 `.o`，自然找不到 `main`。

**处理：**

```cmake
set(INNER_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/test_log_buffer.cpp")
```

改完后需重新 `cmake` 再编译。

### 6.3 观察缓冲内容不要用裸 `data()`

`InlineBuffer::data()` 返回的指针**不保证**以 `\0` 结尾。传给按 C 字符串构造的 `string_view` 可能越界。应使用 `buffer.view()`（带长度）。

---

## 7. 改动清单速查

1. 新增 `export.h`、`LogLevel`、`InlineBuffer` / `SmallStreamBuffer` 与容量常量
2. 公开 API 与内部实现分层（`include` vs `src`）
3. 增加 `tests/inner_test`，并在 `tests/CMakeLists.txt` 中 `add_subdirectory`
4. 内部测试源路径必须指向具体 `.cpp`；include `pans/src`
5. `FromString` 按输入长度比较；观察缓冲区用 `view()`
