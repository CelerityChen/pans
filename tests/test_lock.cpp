// ---------------------------------------------------------------------------
// 锁与原子操作性能对比
//
// 场景一（write-only）：多线程纯自增，对比 mutex / shared_mutex / atomic / CAS
// 场景二（read-heavy）：读多写少，对比 mutex 与 shared_mutex 的读写锁语义
//
// 用法：./test_lock <总操作次数>
// ---------------------------------------------------------------------------

#include <atomic>
#include <mutex>
#include <thread>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <latch>
#include <numeric>
#include <shared_mutex>
#include <stdexcept>
#include <vector>

constexpr int RUNS = 7;
constexpr int READ_PERCENT = 95;  // 读多写少：95% 读，5% 写

// 各 benchmark 共用的全局状态（单进程串行跑，无需隔离）
uint64_t g_target = 0;              // 总操作次数（由命令行传入）
uint64_t g_counter = 0;               // write-only 场景计数器
uint64_t g_shared_value = 0;          // read-heavy 场景共享数据
std::atomic<uint64_t> g_atomic_counter = 0;
std::mutex g_mutex;
std::shared_mutex g_shared_mutex;

// 将 g_target 次操作均分到各线程，余数分配给前几个线程，保证总和精确等于 g_target
uint64_t OperationForThread(std::size_t thread_index, std::size_t thread_count)
{
    const uint64_t base = g_target / thread_count;
    const uint64_t remainder = g_target % thread_count;
    return base + (thread_index < remainder ? 1 : 0);
}

// 通用多线程计时框架
// 三阶段 latch：ready（线程就绪）→ start_gate（统一起跑）→ finished（全部完成）
// 只计 start_gate 打开到 finished 归零的墙钟时间，排除线程创建开销
template<typename Operation>
std::chrono::nanoseconds BenchmarkThreads(std::size_t thread_count, Operation&& operation)
{
    std::latch ready(static_cast<std::ptrdiff_t>(thread_count));
    std::latch start_gate(1);
    std::latch finished(static_cast<std::ptrdiff_t>(thread_count));

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for(std::size_t thread_index = 0; thread_index < thread_count; ++thread_index)
    {
        threads.emplace_back([&, thread_index]{
            ready.count_down();
            start_gate.wait();
            operation(thread_index, OperationForThread(thread_index, thread_count));
            finished.count_down();
        });
    }
    ready.wait();
    const auto begin = std::chrono::steady_clock::now();
    start_gate.count_down();
    finished.wait();
    const auto end = std::chrono::steady_clock::now();

    for(auto& thread : threads)
    {
        thread.join();
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
}

// ----- write-only：每次操作都是写，shared_mutex 也用独占锁（非其优势场景）-----

std::chrono::nanoseconds BenchmarkMutex(std::size_t thread_count){
    g_counter = 0;
    const auto elapsed = BenchmarkThreads(thread_count, [](std::size_t, uint64_t operations){
        for(uint64_t i = 0; i < operations; ++i)
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            ++g_counter;
        }
    });
    return elapsed;
}

std::chrono::nanoseconds BenchmarkSharedMutex(std::size_t thread_count){
    g_counter = 0;
    const auto elapsed = BenchmarkThreads(thread_count, [](std::size_t, uint64_t operations){
        for(uint64_t i = 0; i < operations; ++i)
        {
            // 纯写场景下 shared_mutex 只能独占，作为 mutex 的对照基线
            std::lock_guard<std::shared_mutex> lock(g_shared_mutex);
            ++g_counter;
        }
    });
    return elapsed;
}

std::chrono::nanoseconds BenchmarkAtomic(std::size_t thread_count){
    g_atomic_counter = 0;
    const auto elapsed = BenchmarkThreads(thread_count, [](std::size_t, uint64_t operations){
        for(uint64_t i = 0; i < operations; ++i)
        {
            g_atomic_counter.fetch_add(1, std::memory_order_relaxed);
        }
    });
    return elapsed;
}

std::chrono::nanoseconds BenchmarkAtomicCas(std::size_t thread_count){
    g_atomic_counter = 0;
    const auto elapsed = BenchmarkThreads(thread_count, [](std::size_t, uint64_t operations){
        for(uint64_t i = 0; i < operations; ++i)
        {
            // CAS 自旋自增，模拟无法用 fetch_add 时的竞争路径
            uint64_t expected = g_atomic_counter.load(std::memory_order_relaxed);
            while(!g_atomic_counter.compare_exchange_weak(expected, expected + 1, std::memory_order_relaxed, std::memory_order_relaxed)){}
        }
    });
    return elapsed;
}

// ----- read-heavy：按 op_index 确定性划分读写，各线程比例一致 -----

// op_index % 100 < read_percent 为读，否则为写
bool IsWriteOp(uint64_t op_index, int read_percent)
{
    return static_cast<int>(op_index % 100) >= read_percent;
}

std::chrono::nanoseconds BenchmarkMutexReadHeavy(std::size_t thread_count, int read_percent)
{
    g_shared_value = 0;
    const auto elapsed = BenchmarkThreads(thread_count, [read_percent](std::size_t, uint64_t operations){
        uint64_t local_sum = 0;
        for(uint64_t i = 0; i < operations; ++i)
        {
            // mutex 读写均独占，读之间无法并发
            std::lock_guard<std::mutex> lock(g_mutex);
            if(IsWriteOp(i, read_percent))
            {
                ++g_shared_value;
            }
            else
            {
                local_sum += g_shared_value;
            }
        }
        // 防止编译器消除读路径（local_sum 永不为 UINT64_MAX，分支不可达）
        if(local_sum == UINT64_MAX)
        {
            std::cerr << "impossible\n";
        }
    });
    return elapsed;
}

std::chrono::nanoseconds BenchmarkSharedMutexReadHeavy(std::size_t thread_count, int read_percent)
{
    g_shared_value = 0;
    const auto elapsed = BenchmarkThreads(thread_count, [read_percent](std::size_t, uint64_t operations){
        uint64_t local_sum = 0;
        for(uint64_t i = 0; i < operations; ++i)
        {
            if(IsWriteOp(i, read_percent))
            {
                std::unique_lock<std::shared_mutex> lock(g_shared_mutex);  // 写：独占
                ++g_shared_value;
            }
            else
            {
                std::shared_lock<std::shared_mutex> lock(g_shared_mutex);  // 读：共享，允许多读并发
                local_sum += g_shared_value;
            }
        }
        if(local_sum == UINT64_MAX)
        {
            std::cerr << "impossible\n";
        }
    });
    return elapsed;
}

// 多次运行取平均，降低调度抖动带来的误差
template<typename Benchmark>
double Run(Benchmark&& benchmark)
{
    long double total_nanoseconds = 0;
    for(int run = 0; run < RUNS; ++run)
    {
        total_nanoseconds += static_cast<long double>(benchmark().count());
    }
    return static_cast<double>(total_nanoseconds / RUNS);
}

void PrintResults(std::string_view name, double total_nanoseconds)
{
    const double nanoseconds_per_operation = total_nanoseconds / static_cast<double>(g_target);
    std::cout << std::left << std::setw(14) << name << std::right
        << total_nanoseconds << " ns \ttotal, "
        << nanoseconds_per_operation << " ns/op\n";
}

int main(int argc, char** argv)
{
    if(argc != 2)
    {
        std::cerr << "Usage: ./test_lock count\n";
        return -1;
    }

    g_target = std::atoi(argv[1]);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "operations: " << g_target << ", average of " << RUNS << " runs\n\n";

    std::cout << "=== write-only (all increments) ===\n\n";
    const std::vector<std::size_t> thread_counts = {1,2,4,8,10};
    for(const std::size_t thread_count : thread_counts)
    {
        std::cout << "threads: " << thread_count << "\n";
        PrintResults("mutex",   Run([&](){return BenchmarkMutex(thread_count);}));
        PrintResults("shared_mutex",   Run([&](){return BenchmarkSharedMutex(thread_count);}));
        PrintResults("atomic",   Run([&](){return BenchmarkAtomic(thread_count);}));
        PrintResults("atomic_cas",   Run([&](){return BenchmarkAtomicCas(thread_count);}));
        std::cout << "\n";
    }

    std::cout << "=== read-heavy (" << READ_PERCENT << "% read, "
              << (100 - READ_PERCENT) << "% write) ===\n\n";
    for(const std::size_t thread_count : thread_counts)
    {
        std::cout << "threads: " << thread_count << "\n";
        PrintResults("mutex", Run([&](){
            return BenchmarkMutexReadHeavy(thread_count, READ_PERCENT);
        }));
        PrintResults("shared_mutex", Run([&](){
            return BenchmarkSharedMutexReadHeavy(thread_count, READ_PERCENT);
        }));
        std::cout << "\n";
    }
    return 0;
}
