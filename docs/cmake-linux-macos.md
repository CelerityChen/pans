# CMake 跨平台构建笔记（Linux + macOS）

本文整理 pans 工程在支持 Linux / macOS 时对 CMake 的改动要点，以及配置阶段踩过的坑。

相关文件：

- `CMakeLists.txt` — 工程入口，不硬编码编译器
- `cmake/pansOptions.cmake` — 公共编译 / 链接选项（`pans_options` INTERFACE）
- `pans/CMakeLists.txt` — 主库目标与 install / export

---

## 1. 不要硬编码编译器路径

**反例：**

```cmake
set(CMAKE_C_COMPILER "/usr/bin/gcc" CACHE PATH "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "/usr/bin/g++" CACHE PATH "C++ compiler" FORCE)
```

**问题：**

| 点 | 说明 |
|----|------|
| 路径不通用 | Linux 上可能是真 GCC；macOS 上 `/usr/bin/g++` 多半是 Apple Clang 壳 |
| `FORCE` 过强 | 覆盖用户 / CI / toolchain 指定的编译器 |
| 违反惯例 | 编译器应在命令行或 toolchain 文件中指定 |

**做法：** 从 `CMakeLists.txt` 删除硬编码，交给 CMake 探测；需要指定时在配置时传入：

```bash
# macOS（默认 Apple Clang）
cmake -S . -B build

# Linux 指定 GCC
cmake -S . -B build -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
```

若只想给「未指定时的默认值」，可用 `cc` / `c++`，且**不要** `FORCE`。

---

## 2. 编译器 ID：务必包含 `AppleClang`

macOS 上 Xcode 工具链的 `CMAKE_CXX_COMPILER_ID` 是 **`AppleClang`**，不是 `Clang`。

凡用生成器表达式匹配编译器时，应写成：

```cmake
$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
    ...
>
```

本工程中需匹配的位置：

- `cmake/pansOptions.cmake`：通用警告等
- `pans/CMakeLists.txt`：主库 `-Werror`

漏写 `AppleClang` 时，在 macOS 上这些选项**静默不生效**。

---

## 3. 平台相关选项对照

| 选项 | Linux | macOS (Darwin) |
|------|-------|----------------|
| `-fPIC` | 共享库等场景常需显式开启 | 默认已是 PIC，一般不必加 |
| 动态符号导出（栈回溯） | `-rdynamic` | `-Wl,-export_dynamic` |
| Debug 调试信息 | GCC 可用 `-ggdb`；Clang 用 `-g` / `-g3` | AppleClang 用 `-g3`，不要用 `-ggdb` |

对应生成器表达式示例：

```cmake
# 仅 Linux
$<$<AND:$<PLATFORM_ID:Linux>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:
    -fPIC
>

# 仅 macOS
$<$<AND:$<PLATFORM_ID:Darwin>,$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>>:
    -Wl,-export_dynamic
>

# 仅 GNU + Debug
$<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU>>:-ggdb>
```

---

## 4. 生成器表达式易错点

### 4.1 `AND` / `OR` 嵌套不要多写 `$<`

**错误：**

```cmake
$<$<AND:$<PLATFORM_ID:Linux>,$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:
```

**正确：**

```cmake
$<$<AND:$<PLATFORM_ID:Linux>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:
```

`OR(...)` 本身已是完整生成器表达式，前面不要再套一层 `$<`。

### 4.2 闭合括号要配对

每个 `$<CXX_COMPILER_ID:...>` 都要有对应的 `>`，列表末尾还要有 `>:` 结束 `OR` / `AND`。

### 4.3 编译选项要带前导 `-`

写成 `ggdb` 无效，应为 `-ggdb`。

---

## 5. `CMAKE_BUILD_TYPE` 的可选值列表

**错误：**

```cmake
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRING Debug Release RelWithDebInfo MinSizeRel)
```

**正确：** 属性名是 **`STRINGS`**（复数），不是 `STRING`：

```cmake
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
```

`STRING` 是 cache 变量的 **TYPE**；`STRINGS` 才是下拉可选值列表。

报错形如：

```text
set_property given invalid CACHE property STRING.
Settable CACHE properties are: ADVANCED, HELPSTRING, STRINGS, TYPE, and VALUE.
```

---

## 6. install / export 与 `pans_options`

`pans` 若：

```cmake
target_link_libraries(pans PUBLIC pans_options)
```

则 `install(EXPORT ...)` / `export(TARGETS ...)` 时，**必须把 `pans_options` 放进同一 export set**，否则：

```text
install(EXPORT "pansTargets" ...) includes target "pans" which requires target
"pans_options" that is not in any export set.
```

**做法：**

```cmake
install(TARGETS pans pans_options
    EXPORT pansTargets
    ...
)

export(TARGETS pans pans_options
    FILE ${CMAKE_CURRENT_BINARY_DIR}/pansTargets.cmake
)
```

**备选：** 若编译选项只作用于本库、不希望传给依赖 `pans` 的下游，可改为：

```cmake
target_link_libraries(pans PRIVATE pans_options)
```

此时不必导出 `pans_options`（警告、`-rdynamic` 等也不会泄漏给消费者）。

---

## 7. 本地验证

```bash
# 需已安装 Xcode Command Line Tools / Xcode
xcode-select -p

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

配置成功时可见类似输出：

```text
-- The CXX compiler identification is AppleClang ...
-- Build type: Debug
-- Configuring done
-- Generating done
```

---

## 8. 改动清单速查

1. 删除 `CMakeLists.txt` 中硬编码的 `gcc` / `g++`
2. 所有 `CXX_COMPILER_ID` 判断加上 `AppleClang`（含主库 `-Werror`）
3. `-fPIC` 仅 Linux；macOS 用 `-Wl,-export_dynamic` 替代 `-rdynamic`
4. `-ggdb` 限制为仅 `GNU` + Debug
5. 修好生成器表达式嵌套与 `-ggdb` 前缀
6. `set_property(... PROPERTY STRINGS ...)`
7. `PUBLIC` 链接 `pans_options` 时，将其一并 `install` / `export`
