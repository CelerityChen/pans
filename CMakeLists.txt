# cmake_minimum_required：声明所需的最低 CMake 版本，同时启用该版本的策略（policy）行为。
cmake_minimum_required(VERSION 3.25)

# Linux 上默认使用系统 GCC；编译器必须在 project() 之前指定，project() 探测编译器后再改无效。
# CMAKE_HOST_SYSTEM_NAME：运行 cmake 的主机系统（区别于 CMAKE_SYSTEM_NAME 表示目标系统，后者在 project() 后才有值）。
# FILEPATH：缓存变量类型，GUI 中显示为文件选择框。
# 不加 FORCE：仅在未指定时写入默认值，仍可用 -DCMAKE_CXX_COMPILER=clang++ 覆盖；
#   使用工具链文件（交叉编译）时由工具链决定编译器，这里跳过。
# macOS 不设置，使用 Xcode 自带的 AppleClang。
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_C_COMPILER "/usr/bin/gcc" CACHE FILEPATH "C compiler")
    set(CMAKE_CXX_COMPILER "/usr/bin/g++" CACHE FILEPATH "C++ compiler")
endif()

# C++ 标准需在 project() 之前设置，才能对后续所有目标生效。
#   CMAKE_CXX_STANDARD_REQUIRED ON：编译器不支持 C++20 时直接报错，而不是悄悄降级。
#   CMAKE_CXX_EXTENSIONS ON：使用 gnu++20 而非 c++20，允许编译器扩展。
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# project()：声明项目名、版本和启用的语言（C 用于编译 kcp、zlib 等 C 库）。
#   执行后会设置 PROJECT_SOURCE_DIR、PROJECT_VERSION 等变量，并探测编译器。
project(pans VERSION 0.1.0 LANGUAGES CXX C)
# GNUInstallDirs：提供 CMAKE_INSTALL_LIBDIR、CMAKE_INSTALL_INCLUDEDIR 等标准安装目录变量。
include(GNUInstallDirs)
# CMakePackageConfigHelpers：提供 configure_package_config_file 等命令，用于生成 pansConfig.cmake。
include(CMakePackageConfigHelpers)

# 构建开关，可用 -DPANS_BUILD_TESTS=OFF 等方式关闭。
option(PANS_BUILD_EXAMPLES  "Build examples" ON)
option(PANS_BUILD_TESTS     "Build tests"    ON)
option(PANS_BUILD_TOOLS     "Build tools"    ON)

# 单配置生成器（Makefiles、Ninja）未指定构建类型时默认 Debug；
#   多配置生成器（Xcode、Visual Studio）有 CMAKE_CONFIGURATION_TYPES，构建类型在构建时用 --config 选择。
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# 并行编译任务数取逻辑核数的 1.5 倍。
# 注意：cmake --build 读取的是同名"环境变量"，这里的缓存变量不会影响顶层构建的并行度，
#   仅供 cmake/pansDependencies.cmake 构建第三方库时作为 --parallel 参数使用。
cmake_host_system_information(RESULT CPU_NUMS QUERY NUMBER_OF_LOGICAL_CORES)
message(STATUS "CPU NUMS on current server is: ${CPU_NUMS}")
math(EXPR JOBS "${CPU_NUMS} + ${CPU_NUMS} / 2")
set(CMAKE_BUILD_PARALLEL_LEVEL ${JOBS} CACHE STRING "Parallel compile jobs")
message(STATUS "Parallel compile jobs: ${CMAKE_BUILD_PARALLEL_LEVEL}")

# include(<文件>)：在当前作用域执行该文件，其中定义的变量、函数、目标在此之后均可使用（不同于 add_subdirectory 会创建新作用域）。
#   相对路径以当前 CMakeLists.txt 所在目录为基准。
# 顺序有要求：pansDependencies.cmake 依赖上面的 GNUInstallDirs 和 CMAKE_BUILD_PARALLEL_LEVEL，
#   且必须在 add_subdirectory(pans) 之前，pans 才能链接 yaml-cpp::yaml-cpp 等第三方目标。
include(cmake/pansOptions.cmake)
include(cmake/functions.cmake)
include(cmake/pansDependencies.cmake)

add_subdirectory(pans)
if(PANS_BUILD_TESTS)
    # enable_testing() 需在 add_subdirectory(tests) 之前调用，ctest 才能发现其中的 add_test。
    enable_testing()
    add_subdirectory(tests)
endif()
if(PANS_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# ---------- 安装 CMake 包，供使用方 find_package(pans CONFIG) ----------
# 包配置文件的安装目录：<前缀>/lib/cmake/pans，是 find_package 默认搜索的位置之一。
set(PANS_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/pans")

# install(EXPORT)：根据导出集 pansTargets（pans/CMakeLists.txt 中 install(TARGETS ... EXPORT pansTargets)）
#   生成 pansTargets.cmake，定义导入目标，由 pansConfig.cmake 最后 include。
# NAMESPACE pans::：导入目标加前缀，使用方链接 pans::pans、pans::pans_options。
install(EXPORT pansTargets
    FILE pansTargets.cmake
    NAMESPACE pans::
    DESTINATION "${PANS_INSTALL_CMAKEDIR}"
)

# configure_package_config_file(<模板> <输出> INSTALL_DESTINATION <安装目录>)：
#   类似 configure_file，替换 @变量@；并把 @PACKAGE_INIT@ 展开为
#   根据 INSTALL_DESTINATION 反推安装前缀的代码，使安装目录整体移动后仍能正确定位。
# CMAKE_CURRENT_BINARY_DIR：当前 CMakeLists.txt 对应的构建目录，生成的文件放在这里。
configure_package_config_file(
    cmake/pansConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/pansConfig.cmake"
    INSTALL_DESTINATION "${PANS_INSTALL_CMAKEDIR}"
)

# write_basic_package_version_file：生成版本文件，find_package(pans 0.1) 时据此判断版本是否兼容。
#   SameMajorVersion：主版本号相同且不低于请求版本即兼容（如 0.2.0 满足请求 0.1）。
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/pansConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

# install(FILES ...)：安装时把文件复制到 <前缀>/<DESTINATION>。
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/pansConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/pansConfigVersion.cmake"
    DESTINATION "${PANS_INSTALL_CMAKEDIR}"
)