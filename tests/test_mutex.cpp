// ---------------------------------------------------------------------------
// pans::Spinlock 与系统轻量锁对比
//
// - 各平台：pans::Spinlock（glibc 为 pthread_spinlock；其它为 atomic TTAS）
// - macOS 额外对比：os_unfair_lock 封装类（短自旋后可休眠，非纯自旋）
//
// 用法：./test_mutex <总操作次数>
// ---------------------------------------------------------------------------

#include <pans/mutex.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <latch>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <os/lock.h>
#endif

constexpr int RUNS = 7;

uint64_t g_target = 0;
uint64_t g_counter = 0;

#if defined(__APPLE__)

/// macOS：将 os_unfair_lock 封装成与 pans::Spinlock 相同的 BasicLockable 接口，
/// 便于用同一套 BenchmarkMutex<Mutex> 模板对比。
/// 注意：os_unfair_lock 不是纯自旋——忙等一段时间后会阻塞休眠。
class UnfairLock
{
private:
    os_unfair_lock m_mutex = OS_UNFAIR_LOCK_INIT;

public:
    using Lock = std::lock_guard<UnfairLock>;

    UnfairLock() noexcept = default;
    ~UnfairLock() noexcept = default;

    UnfairLock(const UnfairLock&) = delete;
    UnfairLock& operator=(const UnfairLock&) = delete;

    void lock() noexcept
    {
        os_unfair_lock_lock(&m_mutex);
    }

    [[nodiscard]] bool try_lock() noexcept
    {
        return os_unfair_lock_trylock(&m_mutex);
    }

    void unlock() noexcept
    {
        os_unfair_lock_unlock(&m_mutex);
    }
};

#endif // __APPLE__

uint64_t OperationForThread(std::size_t thread_index, std::size_t thread_count)
{
    const uint64_t base = g_target / thread_count;
    const uint64_t remainder = g_target % thread_count;
    return base + (thread_index < remainder ? 1 : 0);
}

template <typename Operation>
std::chrono::nanoseconds BenchmarkThreads(std::size_t thread_count, Operation&& operation)
{
    std::latch ready(static_cast<std::ptrdiff_t>(thread_count));
    std::latch start_gate(1);
    std::latch finished(static_cast<std::ptrdiff_t>(thread_count));

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index)
    {
        threads.emplace_back([&, thread_index] {
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

    for (auto& thread : threads)
    {
        thread.join();
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
}

template <typename Mutex>
std::chrono::nanoseconds BenchmarkMutex(std::size_t thread_count)
{
    Mutex mutex;
    g_counter = 0;
    return BenchmarkThreads(thread_count, [&](std::size_t, uint64_t operations) {
        for (uint64_t i = 0; i < operations; ++i)
        {
            // 统一用 lock_guard，兼容 pans::Spinlock / UnfairLock / std::mutex
            std::lock_guard<Mutex> lock(mutex);
            ++g_counter;
        }
    });
}

template <typename Benchmark>
double Run(Benchmark&& benchmark)
{
    long double total_nanoseconds = 0;
    for (int run = 0; run < RUNS; ++run)
    {
        total_nanoseconds += static_cast<long double>(benchmark().count());
    }
    return static_cast<double>(total_nanoseconds / RUNS);
}

void PrintResult(std::string_view name, double total_nanoseconds)
{
    const double nanoseconds_per_operation = total_nanoseconds / static_cast<double>(g_target);
    std::cout << std::left << std::setw(28) << name << std::right
              << total_nanoseconds << " ns \ttotal, "
              << nanoseconds_per_operation << " ns/op\n";
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./test_mutex count\n";
        return EXIT_FAILURE;
    }

    g_target = std::stoull(argv[1]);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "operations: " << g_target << ", average of " << RUNS << " runs\n\n";

    const std::vector<std::size_t> thread_counts = {1, 2, 4, 8, 16};
    for (const std::size_t thread_count : thread_counts)
    {
        std::cout << "threads: " << thread_count << "\n";

#if defined(__linux__) && defined(__GLIBC__)
        PrintResult("pans::Spinlock(pthread)", Run([&] {
            return BenchmarkMutex<pans::Spinlock>(thread_count);
        }));
#else
        PrintResult("pans::Spinlock(atomic)", Run([&] {
            return BenchmarkMutex<pans::Spinlock>(thread_count);
        }));
#endif

#if defined(__APPLE__)
        PrintResult("UnfairLock(os_unfair_lock)", Run([&] {
            return BenchmarkMutex<UnfairLock>(thread_count);
        }));
#endif

        PrintResult("std::mutex", Run([&] {
            return BenchmarkMutex<std::mutex>(thread_count);
        }));

        std::cout << "\n";
    }
    return EXIT_SUCCESS;
}
