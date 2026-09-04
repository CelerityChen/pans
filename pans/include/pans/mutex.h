#ifndef PANS_INCLUDE_PANS_MUTEX_H
#define PANS_INCLUDE_PANS_MUTEX_H

#include <atomic>
#include <mutex>

// ---------------------------------------------------------------------------
// 自旋等待时的 CPU 提示（pause / yield）
// - x86: immintrin.h → _mm_pause()（PAUSE）
// - ARM/Apple Silicon: arm_acle.h → __yield()（YIELD）
// ---------------------------------------------------------------------------
#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(__arm__)
#include <arm_acle.h>
#endif

// Linux（含 glibc）与 macOS 均提供 pthread；仅在需要时包含
#if (defined(__linux__) && defined(__GLIBC__)) || defined(__APPLE__)
#include <pthread.h>
#endif

namespace pans {

/// 自旋忙等中插入的 CPU 松弛提示，避免空转占满流水线。
/// 在不支持 pause/yield 的架构上退化为空操作。
inline void cpu_relax()
{
#if defined(__x86_64__) || defined(__i386__)
    _mm_pause();
#elif defined(__aarch64__) || defined(__arm__)
    // arm_acle.h：一般为 __builtin_arm_yield()，即一条 yield 指令
    __yield();
#endif
}

// ---------------------------------------------------------------------------
// Spinlock
//
// 满足 BasicLockable / Lockable，可与 std::lock_guard、std::unique_lock 配合。
//
// 平台策略：
// - Linux + glibc：pthread_spinlock_t（POSIX 自旋锁）
// - 其它（macOS、musl 等）：用户态 atomic_flag + TTAS
//   （Apple 不提供 pthread_spinlock_t；勿使用已废弃的 OSSpinLock）
//
// 仅适合极短临界区；长时间持锁会导致其它核空转。
// ---------------------------------------------------------------------------

#if defined(__linux__) && defined(__GLIBC__)

/// glibc：封装 pthread 自旋锁。
class Spinlock
{
private:
    pthread_spinlock_t m_mutex{};

public:
    using Lock = std::lock_guard<Spinlock>;

    Spinlock() noexcept
    {
        // PTHREAD_PROCESS_PRIVATE：仅进程内共享；init 对这种用法几乎总是成功
        pthread_spin_init(&m_mutex, PTHREAD_PROCESS_PRIVATE);
    }

    ~Spinlock() noexcept
    {
        pthread_spin_destroy(&m_mutex);
    }

    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    void lock() noexcept
    {
        pthread_spin_lock(&m_mutex);
    }

    /// 尝试获取锁；成功返回 true，已被占用返回 false。
    [[nodiscard]] bool try_lock() noexcept
    {
        return pthread_spin_trylock(&m_mutex) == 0;
    }

    void unlock() noexcept
    {
        pthread_spin_unlock(&m_mutex);
    }
};

#else

/// 非 glibc（含 macOS）：基于 atomic_flag 的 TTAS 自旋锁。
class Spinlock
{
private:
    std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;

public:
    using Lock = std::lock_guard<Spinlock>;

    Spinlock() noexcept = default;
    ~Spinlock() noexcept = default;

    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    void lock() noexcept
    {
        // TTAS：外层 test_and_set 争用；失败后内层只读 test + cpu_relax，
        // 减轻对同一缓存行的写无效流量。
        while (m_mutex.test_and_set(std::memory_order_acquire))
        {
            while (m_mutex.test(std::memory_order_relaxed))
            {
                cpu_relax();
            }
        }
    }

    /// 尝试获取锁；成功返回 true，已被占用返回 false。
    [[nodiscard]] bool try_lock() noexcept
    {
        return !m_mutex.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept
    {
        m_mutex.clear(std::memory_order_release);
    }
};

#endif

} // namespace pans

#endif
