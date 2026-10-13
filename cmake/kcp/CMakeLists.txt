# kcp 包装项目：上游 kcp 没有可用的安装规则，这里自行编译 ikcp.c 为静态库，
#   并安装头文件、库文件和 kcp-config.cmake，使其能被 find_package(kcp CONFIG) 找到。
# 由 cmake/pansDependencies.cmake 作为独立项目配置和构建，源码目录通过 -DPANS_KCP_SOURCE_DIR 传入。
cmake_minimum_required(VERSION 3.25)

project(pans-kcp LANGUAGES C)

include(GNUInstallDirs)

# 校验源码目录：未传入或缺少 ikcp.c / ikcp.h 时立即终止配置。
if(NOT PANS_KCP_SOURCE_DIR)
    message(FATAL_ERROR "PANS_KCP_SOURCE_DIR is required")
endif()
if(NOT EXISTS "${PANS_KCP_SOURCE_DIR}/ikcp.c" OR
    NOT EXISTS "${PANS_KCP_SOURCE_DIR}/ikcp.h")
    message(FATAL_ERROR "Invalid KCP source directory: ${PANS_KCP_SOURCE_DIR}")
endif()

# ALIAS：创建只读别名 kcp::kcp，与安装后导出的目标名一致，使用方写法统一。
add_library(kcp STATIC "${PANS_KCP_SOURCE_DIR}/ikcp.c")
add_library(kcp::kcp ALIAS kcp)

# 生成器表达式区分两种使用场景：
#   BUILD_INTERFACE：在本构建树中使用时，直接指向源码目录；
#   INSTALL_INTERFACE：安装后使用时，指向安装前缀下的 include（相对路径，导出时自动拼上前缀）。
target_include_directories(kcp PUBLIC
    $<BUILD_INTERFACE:${PANS_KCP_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(FILES "${PANS_KCP_SOURCE_DIR}/ikcp.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
# EXPORT kcp-targets：把目标记录到导出集，供下面的 install(EXPORT) 生成导入脚本。
# ARCHIVE：静态库的安装位置。
install(TARGETS kcp EXPORT kcp-targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
# 导出脚本直接命名为 kcp-config.cmake，find_package(kcp CONFIG) 即可加载并得到 kcp::kcp 目标。
#   kcp 无其他依赖，不需要单独的 Config 文件来 find_dependency。
install(EXPORT kcp-targets
    FILE kcp-config.cmake
    NAMESPACE kcp::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/kcp"
)
