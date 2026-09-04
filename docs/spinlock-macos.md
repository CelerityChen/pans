# pans::Spinlock 与 macOS 适配笔记

本文整理 `pans/include/pans/mutex.h`、`tests/test_mutex.cpp` 的设计，以及 Linux / macOS 上自旋锁相关 API 的差异。

相关文件：

- `pans/include/pans/mutex.h` — `cpu_relax`、跨平台 `pans::Spinlock`
- `tests/test_mutex.cpp` — Spinlock / `os_unfair_lock` / `std::mutex` 性能对比

---

## 1. 改动概要

1. 新增 `cpu_relax()`：x86 用 `_mm_pause()`，ARM/Apple Silicon 用 `arm_acle.h` 的 `__yield()`。
2. 新增 `pans::Spinlock`（BasicLockable / Lockable）：
   - **Linux + glibc**：封装 `pthread_spinlock_t`
   - **其它（含 macOS）**：`std::atomic_flag` + TTAS（test-and-test-and-set）
3. 两边 API 对齐：`lock` / `try_lock` / `unlock`，以及 `using Lock = std::lock_guard<Spinlock>`。
4. 测试侧在 macOS 条件编译下封装 `UnfairLock`（`os_unfair_lock`），与 `Spinlock` 接口一致，便于同一套 benchmark 模板对比。

---

## 2. 平台宏速查

| 宏 | 含义 |
|----|------|
| `__linux__` | 目标 OS 为 Linux |
| `__GLIBC__` | C 库为 GNU libc（Alpine/musl 等没有） |
| `__APPLE__` | Apple 平台（macOS / iOS 等） |
| `__MACH__` | Mach 内核；现代 Apple 上通常与 `__APPLE__` 同时为真，日常判断用 `__APPLE__` 即可 |

常用写法：

```cpp
// POSIX pthread（Linux 与 mac 都有）
#if (defined(__linux__) && defined(__GLIBC__)) || defined(__APPLE__)
#include <pthread.h>
#endif

// 仅 glibc 才有的 pthread_spinlock
#if defined(__linux__) && defined(__GLIBC__)
// pthread_spin_*
#endif
```

注意：`&&` 优先于 `||`，跨平台条件建议加括号，避免误读。

---

## 3. macOS 有没有自旋锁？

**macOS 有完整 pthread**（`pthread_mutex` / `rwlock` / `cond` 等），**没有** `pthread_spinlock_t`。

| API | 状态 | 说明 |
|-----|------|------|
| `pthread_spinlock_t` | 无 | Apple libpthread 未实现 |
| `OSSpinLock` | 已废弃 | 优先级反转问题，勿新用 |
| `os_unfair_lock` | 推荐轻量锁 | 短忙等后可休眠，**不是纯自旋** |
| 自写 `atomic_flag` + `cpu_relax` | 可用 | 真用户态自旋，与当前 `pans::Spinlock`（非 glibc）一致 |

对应关系：

```text
Linux                          macOS
────────────────────────────────────────────────
pthread_mutex_t           →    pthread_mutex / std::mutex / os_unfair_lock
pthread_rwlock_t          →    pthread_rwlock / std::shared_mutex
pthread_spinlock_t        →    ❌ → atomic TTAS，或近似用 os_unfair_lock
```

---

## 4. `pans::Spinlock` 实现要点

### glibc 路径

封装 `pthread_spin_init/lock/trylock/unlock/destroy`，`PTHREAD_PROCESS_PRIVATE`。

### 非 glibc 路径（macOS 等）

```cpp
while (flag.test_and_set(acquire)) {
    while (flag.test(relaxed)) {
        cpu_relax();  // TTAS：减少对缓存行的写无效
    }
}
flag.clear(release);
```

仅适合**极短临界区**；持锁线程被调度走时，其它核会空转。

---

## 5. 测试中的 `UnfairLock`

`tests/test_mutex.cpp` 在 `#if defined(__APPLE__)` 下将 `os_unfair_lock` 封成与 `Spinlock` 相同接口：

```cpp
class UnfairLock {
    os_unfair_lock m_mutex = OS_UNFAIR_LOCK_INIT;
public:
    void lock() noexcept { os_unfair_lock_lock(&m_mutex); }
    bool try_lock() noexcept { return os_unfair_lock_trylock(&m_mutex); }
    void unlock() noexcept { os_unfair_lock_unlock(&m_mutex); }
};
```

benchmark 用 `std::lock_guard<Mutex>`，可同时测 `pans::Spinlock`、`UnfairLock`、`std::mutex`。

### 运行

```bash
cmake --build build --target test_mutex
./bin/tests/test_mutex 100000
```

### macOS 上一次冒烟结果（100000 ops，7 次平均，Apple Silicon）

| 线程数 | Spinlock(atomic) ns/op | UnfairLock ns/op | std::mutex ns/op |
|--------|------------------------|------------------|------------------|
| 1 | 2.23 | 2.82 | 6.71 |
| 2 | 42.79 | 18.02 | 10.14 |
| 4 | 137.87 | 60.96 | 31.30 |
| 8 | 323.69 | 12.62 | 19.11 |
| 16 | 622.81 | 11.58 | 16.62 |

单线程时纯自旋最快；线程一多，atomic 自旋空转代价飙升，`os_unfair_lock` / `std::mutex` 因可休眠反而更快——说明「短临界区 + 低竞争」才适合真自旋。

---

## 6. 使用建议

| 场景 | 建议 |
|------|------|
| 极短临界区、低竞争 | `pans::Spinlock` |
| macOS 通用轻量锁 | `os_unfair_lock`（或直接 `std::mutex`） |
| 需要可移植、语义清晰 | `std::mutex` |
| 不要用 | `OSSpinLock` |

---

## 7. 改动文件清单

- `pans/include/pans/mutex.h`（新增）
- `tests/test_mutex.cpp`（新增）
- `docs/spinlock-macos.md`（本文）
