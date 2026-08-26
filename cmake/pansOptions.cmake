# 公共编译/链接选项（INTERFACE），供 pans 及下游目标链接继承。
# 目标平台：Linux + macOS（Darwin）；编译器：GNU / Clang / AppleClang。
add_library(pans_options INTERFACE)

# ----- 通用警告与语义 -----
# Apple Clang 的 CMAKE_CXX_COMPILER_ID 为 AppleClang，需与 Clang 一并匹配。
target_compile_options(pans_options INTERFACE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
        -Wall
        -Wextra              # 比 -Wall 更严格的额外警告
        -Wpedantic           # 严格遵循语言标准
        -fno-strict-aliasing # 禁用严格别名优化，避免未定义行为相关陷阱
    >
)

# ----- 位置无关代码 -----
# Linux 上共享库/部分静态链接场景需要显式 -fPIC；
# macOS 默认即为 PIC，无需再加。
target_compile_options(pans_options INTERFACE
    $<$<AND:$<PLATFORM_ID:Linux>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:
        -fPIC
    >
)

# ----- 动态符号导出（便于栈回溯 / dladdr 等）-----
# Linux: -rdynamic
# macOS: 等价选项为 -Wl,-export_dynamic
target_link_options(pans_options INTERFACE
    $<$<AND:$<PLATFORM_ID:Linux>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:
        -rdynamic
    >
    $<$<AND:$<PLATFORM_ID:Darwin>,$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>>:
        -Wl,-export_dynamic
    >
)

target_compile_definitions(pans_options INTERFACE
    $<$<CONFIG:Debug>:PANS_DEBUG>
)

# ----- Debug -----
# -ggdb 为 GCC 专用；Clang / AppleClang 使用 -g3 即可。
target_compile_options(pans_options INTERFACE
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-g3>
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:GNU>>:-ggdb>
)

# ----- Release / RelWithDebInfo -----
target_compile_options(pans_options INTERFACE
    $<$<CONFIG:Release>:-DNDEBUG>
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Release>:-fno-omit-frame-pointer>

    $<$<CONFIG:RelWithDebInfo>:-DNDEBUG>
    $<$<CONFIG:RelWithDebInfo>:-O2>
    $<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer>
    $<$<CONFIG:RelWithDebInfo>:-g3>
)

# ----- 覆盖率（可选，主要用于 Linux GCC/Clang）-----
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
if(ENABLE_COVERAGE)
    target_compile_options(pans_options INTERFACE
        $<$<CONFIG:Debug>:--coverage>
    )
    target_link_options(pans_options INTERFACE
        $<$<CONFIG:Debug>:--coverage>
    )
endif()
