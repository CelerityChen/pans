# CMake 依赖构建与安装包导出记录

本次变更完善了 pans 的 CMake 工程化流程：统一管理第三方子模块的编译与缓存，补齐 KCP 的 CMake 包装和安装规则，支持安装后通过 `find_package(pans CONFIG)` 使用，并修正 macOS 与 Windows 的少量平台差异。

## 第三方依赖管理

- 新增 `cmake/pansDependencies.cmake`，集中管理 Abseil、zlib、yaml-cpp、TinyXML-2、KCP、Protobuf 和 RapidJSON。
- 配置阶段检查必需子模块的哨兵文件；源码缺失且 `PANS_UPDATE_SUBMODULES=ON` 时自动执行 `git submodule update --init --recursive`。
- Abseil、zlib、yaml-cpp、TinyXML-2、KCP 和 Protobuf 统一构建为静态库，RapidJSON 作为纯头文件导入目标使用。
- 第三方库输出到 `third_party/output/`，该目录已加入 `.gitignore`。
- 新增聚合目标 `pans_third_party_dependencies`，并让 `pans` 显式依赖它，确保第三方产物先于主库生成。

可配置项：

| 变量 | 默认值 | 作用 |
| --- | --- | --- |
| `PANS_THIRD_PARTY_DIR` | `<源码根>/third_party` | 第三方源码目录 |
| `PANS_THIRD_PARTY_OUTPUT_DIR` | `<第三方目录>/output` | 第三方构建与安装缓存目录 |
| `PANS_THIRD_PARTY_BUILD_TYPE` | `Release` | 第三方库构建类型 |
| `PANS_UPDATE_SUBMODULES` | `ON` | 缺失时自动初始化子模块 |
| `PANS_REUSE_INSTALLED_DEPENDENCIES` | `ON` | 复用兼容的第三方产物 |

## ABI 缓存与重建判定

- 每项编译型依赖根据源码提交、编译器及版本、平台、生成器、构建类型、C++ 标准、工具链和依赖参数计算 SHA-256 缓存键。
- 构建目录与安装目录均包含缓存键，不同 ABI 配置之间不会互相覆盖。
- 安装完成后由 `cmake/writeDependencyStamp.cmake` 写入完整缓存键；戳记和关键产物同时存在才视为缓存有效。
- Protobuf 的缓存键还包含 Abseil 与 zlib 的缓存键，底层依赖变化时会连带重建。
- `PANS_REUSE_INSTALLED_DEPENDENCIES=OFF` 可强制每次重新执行第三方库的配置、构建和安装流程。

## KCP CMake 包装

上游 KCP 没有适合本项目的安装规则，因此新增 `cmake/kcp/CMakeLists.txt`：

- 从子模块源码编译 `ikcp.c` 静态库。
- 安装 `ikcp.h` 和 KCP 静态库。
- 导出 `kcp::kcp` 目标及 `kcp-config.cmake`，供构建树和安装后的 pans 包统一引用。
- 将包装文件哈希加入缓存键，包装逻辑变化时自动触发 KCP 重建。

## pans 安装与使用方接入

- 主库公开要求 C++20，并通过导出的 CMake 目标把该要求传递给使用方。
- 公共头文件按 `logger/`、`utils/`、`pans/` 原目录结构安装。
- 第三方库的头文件、静态库和 CMake 包配置一并安装到 pans 的安装前缀。
- 新增 `pansConfig.cmake.in` 与版本配置，安装后导出 `pans::pans` 和 `pans::pans_options`。
- 包配置按顺序恢复 Threads、平台调用栈依赖及各第三方目标，避免混入系统中其他版本的同名库。

安装和使用示例：

```bash
cmake -S . -B build
cmake --build build
cmake --install build --prefix /path/to/pans-install
```

```cmake
find_package(pans CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE pans::pans)
```

配置使用方时可通过 `CMAKE_PREFIX_PATH` 指向安装前缀：

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/pans-install
```

## 构建可复现性与平台修正

- 新增 `redefine_file_macro()`，将主库和测试源文件中的 `__FILE__` 重定义为相对项目根目录的路径，避免日志和断言暴露构建机绝对路径。
- Linux 默认选择系统 GCC，同时保留命令行编译器和工具链文件覆盖能力。
- Linux 使用 libdw，Windows 使用 dbghelp，macOS 使用 libSystem 提供调用栈能力。
- 修正 Windows 下 `<io.h>` 头文件拼写。
- macOS 文件日志同步优先使用 `F_FULLFSYNC`，不支持时回退到 `fsync`。
- macOS 线程 ID 改用 `pthread_threadid_np` 获取系统范围的 64 位 ID。
- VS Code 增加 `*.cmake.in` 文件关联。

## 验证结果

在 macOS 上完成以下验证：

```bash
cmake -S . -B build
cmake --build build --parallel 8
cmake --install build --prefix /tmp/pans-install-check
```

- CMake 配置成功，并正确复用已有第三方 ABI 缓存。
- 主库与所有测试目标完整编译成功。
- 安装目录包含 pans、第三方静态库、公共头文件和 CMake 包配置。
- 独立消费项目通过 `find_package(pans CONFIG REQUIRED)` 找到安装包，成功链接 `pans::pans` 并运行日志示例。

执行 `ctest --test-dir build --output-on-failure -E test_thread_utils` 时，`test_datetime_consume`、`test_logger` 和 `test_singleton` 通过；现有 `test_lock`、`test_mutex`、`test_log_buffer` 因 CTest 未提供必需参数而失败，`test_macros` 在 Debug 模式下按测试代码设计触发断言终止。`test_thread_utils` 含较长人工观察等待，本次验证中排除。这些测试注册问题不影响本次完整编译、安装和外部消费验证。
