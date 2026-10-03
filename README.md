# pans

`pans` is a C++20 study project for building a high-performance server
framework. The repository is currently focused on reusable concurrency,
logging, formatting, and platform utility components; the larger multi-Reactor
and coroutine-based server architecture is still under development.

The project is developed for learning and exploring high-performance server
design, including event-driven I/O, concurrency, coroutine scheduling, and
network programming.

It is suitable for building and experimenting with applications such as:

- Game servers
- HTTP servers
- RPC services
- Other high-concurrency network services

## Current Components

The project currently includes:

- A configurable logger with named loggers, severity filtering, formatting,
  and root logger fallback
- Stream-style and `printf`-style logging macros
- Thread-safe stdout and file appenders with explicit flush and disk sync
- Cross-platform thread ID and thread name utilities for Linux, macOS, and
  Windows
- UTF-8 and wide-string conversion utilities with invalid-input replacement
- Process elapsed-time and a fiber ID placeholder interface
- A thread-safe Meyers Singleton template
- Mutex, spinlock, logger, formatter, buffer, macro, and platform utility tests

See [Logger and runtime utilities](docs/logger-runtime-utils.md) for the
implementation notes and current platform behavior.

## Logging Example

```cpp
#include <logger/log.h>

int main()
{
    auto logger = pans::GetLogger("example");
    logger->addAppender(pans::MakeStdoutAppender());

    PANS_LOG_INFO(logger) << "server started";
    PANS_LOG_FMT_WARN(logger, "connection count: %d", 42);
    logger->flush();
}
```

Named loggers without their own appenders forward records to the root logger,
which writes to stdout by default.

## Repository Layout

```text
pans/include/   Public headers
pans/src/       Library implementation
tests/          Executable tests and internal implementation tests
docs/           Design notes and experiment records
cmake/          Shared CMake compile options
third_party/    Git submodules reserved for external dependencies
```

The current top-level build creates the `pans` static library. The
`PANS_BUILD_TESTS` option controls test generation and is enabled by default;
`PANS_BUILD_TOOLS` and `PANS_BUILD_EXAMPLES` are reserved for the project’s
growing tools and examples areas, which currently contain no executables.

## Clone and Submodules

The project tracks third-party source trees as Git submodules under
`third_party/`, including Abseil, KCP, Protobuf, RapidJSON, TinyXML-2,
yaml-cpp, and zlib. They are not all required by the current library targets,
but recursive initialization keeps a checkout complete for ongoing work.

Clone the repository and initialize all submodules in one command:

```bash
git clone --recurse-submodules https://github.com/CelerityChen/pans.git
cd pans
```

If the repository was cloned without `--recurse-submodules`, initialize the
dependencies afterward:

```bash
git submodule update --init --recursive
```

After pulling a newer project revision, synchronize the submodules to the
commits recorded by that revision:

```bash
git pull
git submodule update --init --recursive
```

You can also ask `git pull` to recurse into already initialized submodules;
the second command initializes any submodules newly added by the pull:

```bash
git pull --recurse-submodules
git submodule update --init --recursive
```

For maintainers who intentionally want to advance submodules to the latest
commits from their configured branches:

```bash
git submodule update --remote --recursive
git status
```

This last command changes the submodule commits recorded by the main project,
so review and commit those pointer changes together with any required
compatibility updates.

## Build and Test

The project requires CMake 3.25 or later and a C++20 compiler.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Test executables are generated under `bin/tests/`. The regular CTest suite
includes logger, mutex, macro, buffer, formatter, and concurrency checks.
`test_thread_utils` is an interactive thread-name inspection program with
intentional sleeps, so run it manually when platform-level observation is
needed. The lock and date-time programs also serve as focused experiments.

## Project Origin

This is a study project based on:

https://gitee.com/huajianke2026/pans
