# 锁与原子操作性能对比笔记（test_lock）

本文整理 `tests/test_lock.cpp` 的设计、实现要点，以及在 macOS 上的实验结果。

相关文件：

- `tests/test_lock.cpp` — 多线程锁 / 原子操作 benchmark
- `tests/CMakeLists.txt` — 自动为 `tests/*.cpp` 生成可执行文件并注册 CTest

---

## 1. 目的

在多线程环境下对比以下同步原语的开销：

| 原语 | 说明 |
|------|------|
| `std::mutex` | 独占锁 |
| `std::shared_mutex` | 读写锁（读共享、写独占） |
| `std::atomic` + `fetch_add` | 无锁自增 |
| `std::atomic` + CAS 自旋 | 模拟无法用 `fetch_add` 时的竞争路径 |

覆盖两类场景：

1. **write-only**：每次操作都是写（自增），考察纯竞争下的锁开销
2. **read-heavy**：95% 读、5% 写，考察 `shared_mutex` 在读多写少时是否优于 `mutex`

---

## 2. 运行方式

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target test_lock
./bin/tests/test_lock <总操作次数>
```

示例：

```bash
./bin/tests/test_lock 1000000
```

参数说明：

- `<总操作次数>`：所有线程操作次数之和（均分到各线程，余数分给前几个线程）
- 每种配置默认跑 **7 次取平均**（`RUNS = 7`）
- 读多写少比例由 `READ_PERCENT = 95` 控制（95% 读、5% 写）

---

## 3. 实现要点

### 3.1 通用计时框架 `BenchmarkThreads`

三阶段 `std::latch` 同步：

```
ready（线程就绪）→ start_gate（统一起跑）→ finished（全部完成）
```

只统计 `start_gate` 打开到 `finished` 归零的墙钟时间，排除线程创建开销。

操作次数通过 `OperationForThread` 均分：

```cpp
base = g_target / thread_count
remainder = g_target % thread_count
每个线程操作数 = base + (thread_index < remainder ? 1 : 0)
```

保证所有线程操作次数之和精确等于 `g_target`。

### 3.2 write-only 场景

四种方式对同一计数器做 `++`：

- `mutex` / `shared_mutex`：每次 `lock_guard` 后自增
- `atomic`：`fetch_add(1, relaxed)`
- `atomic_cas`：CAS 自旋自增

注意：纯写场景下 `shared_mutex` 也只能用独占锁（`lock_guard<shared_mutex>`），**并非其优势场景**，主要作为对照基线。

### 3.3 read-heavy 场景

共享数据 `g_shared_value`，按 `op_index % 100` 确定性划分读写：

```cpp
// op_index % 100 < read_percent → 读；否则 → 写
bool IsWriteOp(uint64_t op_index, int read_percent);
```

| 锁类型 | 读 | 写 |
|--------|----|----|
| `mutex` | `lock_guard<mutex>`（独占） | `lock_guard<mutex>`（独占） |
| `shared_mutex` | `shared_lock<shared_mutex>`（共享） | `unique_lock<shared_mutex>`（独占） |

读路径将值累加到线程局部变量 `local_sum`，并用不可达分支防止编译器消除读操作。

### 3.4 线程数配置

`main` 中测试线程数：`{1, 2, 4, 8, 10}`。

---

## 4. 实验环境

| 项 | 值 |
|----|-----|
| 平台 | macOS Darwin 25.5.0 (arm64) |
| 编译器 | Apple Clang 17.0.0 |
| 构建类型 | Release |
| 总操作次数 | 1,000,000 |
| 重复次数 | 7 次平均 |
| CPU 逻辑核心 | 14 |

---

## 5. 实验结果

### 5.1 write-only（纯自增）

| 线程数 | mutex (ns/op) | shared_mutex (ns/op) | atomic (ns/op) | atomic_cas (ns/op) |
|--------|---------------|----------------------|----------------|---------------------|
| 1 | 7.01 | 17.85 | 2.96 | 2.96 |
| 2 | 11.70 | 39.27 | 6.64 | 5.10 |
| 4 | 21.69 | 90.79 | 13.24 | 17.53 |
| 8 | 16.50 | 129.79 | 32.68 | 93.86 |
| 10 | 15.41 | 123.20 | 30.76 | 91.47 |

### 5.2 read-heavy（95% 读，5% 写）

| 线程数 | mutex (ns/op) | shared_mutex (ns/op) |
|--------|---------------|----------------------|
| 1 | 7.04 | 16.08 |
| 2 | 11.90 | 34.73 |
| 4 | 22.78 | 96.91 |
| 8 | 16.95 | 163.21 |
| 10 | 15.30 | 153.77 |

原始输出：

```
operations: 1000000, average of 7 runs

=== write-only (all increments) ===

threads: 1
mutex         7007690.57 ns 	total, 7.01 ns/op
shared_mutex  17848726.29 ns 	total, 17.85 ns/op
atomic        2964648.86 ns 	total, 2.96 ns/op
atomic_cas    2960226.14 ns 	total, 2.96 ns/op

threads: 2
mutex         11699005.86 ns 	total, 11.70 ns/op
shared_mutex  39271905.14 ns 	total, 39.27 ns/op
atomic        6636547.43 ns 	total, 6.64 ns/op
atomic_cas    5098642.57 ns 	total, 5.10 ns/op

threads: 4
mutex         21691541.71 ns 	total, 21.69 ns/op
shared_mutex  90786839.71 ns 	total, 90.79 ns/op
atomic        13244470.29 ns 	total, 13.24 ns/op
atomic_cas    17525416.86 ns 	total, 17.53 ns/op

threads: 8
mutex         16502047.43 ns 	total, 16.50 ns/op
shared_mutex  129793363.29 ns 	total, 129.79 ns/op
atomic        32678660.86 ns 	total, 32.68 ns/op
atomic_cas    93855499.86 ns 	total, 93.86 ns/op

threads: 10
mutex         15405660.71 ns 	total, 15.41 ns/op
shared_mutex  123198059.86 ns 	total, 123.20 ns/op
atomic        30761166.57 ns 	total, 30.76 ns/op
atomic_cas    91469005.86 ns 	total, 91.47 ns/op

=== read-heavy (95% read, 5% write) ===

threads: 1
mutex         7035523.71 ns 	total, 7.04 ns/op
shared_mutex  16082642.71 ns 	total, 16.08 ns/op

threads: 2
mutex         11900595.14 ns 	total, 11.90 ns/op
shared_mutex  34732541.71 ns 	total, 34.73 ns/op

threads: 4
mutex         22778857.43 ns 	total, 22.78 ns/op
shared_mutex  96912381.00 ns 	total, 96.91 ns/op

threads: 8
mutex         16945196.57 ns 	total, 16.95 ns/op
shared_mutex  163210148.86 ns 	total, 163.21 ns/op

threads: 10
mutex         15300547.43 ns 	total, 15.30 ns/op
shared_mutex  153773744.14 ns 	total, 153.77 ns/op
```

---

## 6. 结果分析

### write-only

1. **atomic 最快**：单线程约 3 ns/op，无锁路径开销最低。
2. **mutex 居中**：多线程竞争下约 12–22 ns/op，随线程数增加先升后稳。
3. **shared_mutex 最慢**：纯写只能独占，且 `shared_mutex` 本身比 `mutex` 更重，约 18–130 ns/op。
4. **CAS 在高竞争下劣于 fetch_add**：8–10 线程时 CAS 自旋开销显著（约 92–94 ns/op）。

### read-heavy（95% 读）

在本机（macOS / Apple Clang）上，**`shared_mutex` 仍显著慢于 `mutex`**，读多写少并未带来优势。可能原因：

| 因素 | 说明 |
|------|------|
| 临界区极短 | 仅一次 `++` 或读取，锁本身的固定开销占主导 |
| Apple 实现 | macOS 上 `shared_mutex` 实现开销较大，短临界区场景常不如简单 `mutex` |
| 写比例 | 5% 写操作仍会产生独占锁，打断读者并发 |
| 线程数有限 | 14 核机器上 10 线程未必能充分体现多读并发收益 |

### 使用建议

| 场景 | 建议 |
|------|------|
| 简单计数、标志位 | 优先 `std::atomic` |
| 需要保护复杂数据结构 | `std::mutex` |
| 读多写少 + 临界区有一定工作量 | 可尝试 `shared_mutex`，**需在目标平台实测** |
| 跨平台 | Linux（glibc）与 macOS 上 `shared_mutex` 表现可能差异很大，不可仅凭单一平台结论选型 |

---

## 7. 后续可扩展方向

- 命令行传入读写比例：`./test_lock <ops> <read_percent>`
- 在读路径加入少量计算，模拟真实业务负载
- 增加 Linux 对照数据，观察 glibc `shared_mutex` 表现
- 将 `READ_PERCENT` 提高到 99%，观察读写比变化的影响
