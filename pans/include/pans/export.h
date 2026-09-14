#ifndef PANS_INCLUDE_PANS_EXPORT_H_
#define PANS_INCLUDE_PANS_EXPORT_H_

// ---------------------------------------------------------------------------
// PANS_API：跨平台符号导出宏
//
// 静态库 / 非共享构建：宏为空，无额外属性。
// Windows 动态库：dllexport（编库时）/ dllimport（用库时）。
// GCC/Clang 动态库：visibility("default")，配合 -fvisibility=hidden 使用。
//
// 约定：
// - 构建 pans 共享库时定义 PANS_SHARED_LIBRARY + PANS_BUILDING_LIBRARY
// - 下游链接 pans 共享库时只定义 PANS_SHARED_LIBRARY
// ---------------------------------------------------------------------------

#if defined(_WIN32) && defined(PANS_SHARED_LIBRARY)
#if defined(PANS_BUILDING_LIBRARY)
#define PANS_API __declspec(dllexport)
#else
#define PANS_API __declspec(dllimport)
#endif

#elif defined(__GNUC__) && defined(PANS_SHARED_LIBRARY)
#define PANS_API __attribute__((visibility("default")))
#else
#define PANS_API
#endif

#endif
