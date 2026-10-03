# Logger 与运行时工具实现记录

本次变更在已有日志级别、格式化器和缓冲区基础上补齐可用的日志入口，增加输出器、日志管理、线程信息、字符串编码转换与单例工具，并加入对应的示例测试。

## 日志接口

- 新增 `Logger` 公共接口，支持按名称获取日志器、获取根日志器、设置日志级别和格式模板，以及增删输出器。
- 新增流式日志宏 `PANS_LOG_DEBUG/INFO/WARN/ERROR/FATAL`，在析构 `LogLine` 时提交完整记录。
- 新增 printf 风格日志宏 `PANS_LOG_FMT_DEBUG/INFO/WARN/ERROR/FATAL`。
- 日志记录包含时间、线程 ID、协程 ID、级别、源文件、行号和正文；不再保留未使用的函数名字段。
- `LoggerManager` 统一缓存命名日志器，根日志器默认连接标准输出；未配置输出器的命名日志器会回退到根日志器。
- 日志级别和输出器列表支持并发读取与更新，并提供 `flush()` 和 `sync()` 接口。

## 输出器

- 新增标准输出和文件输出器工厂：`MakeStdoutAppender()` 与 `MakeFileAppender()`。
- 输出器可独立设置日志级别，并通过互斥锁保护写入、刷新和同步操作。
- 文件输出器采用追加模式，`sync()` 在 Linux、macOS 和 Windows 上调用对应的系统刷盘接口。
- `FATAL` 日志写入后立即刷新；输出失败时捕获异常，避免日志路径影响主业务流程。

## 运行时工具

- 新增 `GetThreadId()`、`SetThreadName()` 和 `GetThreadName()`，适配 Linux、macOS 与 Windows 的原生线程接口。
- 线程名使用线程局部存储保存，同时同步至操作系统；超出系统限制时按平台安全截断。
- 新增 UTF-8 与宽字符串双向转换，非法编码和无效 Unicode 码点统一替换为 `U+FFFD`。
- 新增基于 `steady_clock` 的进程内运行时长查询，以及当前返回 `0` 的协程 ID 占位接口。
- 新增 Meyers Singleton 模板，依赖函数内静态对象保证首次构造的线程安全。

## 测试与使用示例

- `test_logger` 覆盖流式日志、printf 风格日志、标准输出、文件输出和显式刷盘。
- `test_singleton` 通过 10 个线程验证单例仅构造和析构一次。
- `test_thread_utils` 用于人工观察线程 ID 与原生线程名变化；其中包含等待时间，便于配合系统线程查看工具验证。
- `.vscode/settings.json` 增加 C++ 头文件关联，改善编辑器对 `sstream` 的识别。

验证命令：

```bash
cmake --build build -j 8
./bin/tests/test_logger
./bin/tests/test_singleton
```

以上构建及两个快速测试已在 macOS 上通过。`test_thread_utils` 属于带等待的人工观察测试，本次未执行完整流程。
