file(GLOB_RECURSE PANS_SOURCES CONFIGURE_DEPENDS *.cpp *.cc *.c)
file(GLOB_RECURSE PANS_HEADERS CONFIGURE_DEPENDS *.h *.hpp)

add_library(pans STATIC ${PANS_SOURCES} ${PANS_HEADERS})
# 源文件属性只对同一目录中创建的目标生效，因此在 add_library(pans) 所在的这个文件中调用。
redefine_file_macro("${PANS_SOURCES}")

# target_compile_features(... PUBLIC cxx_std_20)：要求至少 C++20，并通过 PUBLIC 传递给使用方；
#   与顶层 CMAKE_CXX_STANDARD 不同，它会记录进导出的 pansTargets.cmake，安装后对外部使用方同样生效。
target_compile_features(pans PUBLIC cxx_std_20)

target_include_directories(pans
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src   
)

target_link_libraries(pans PUBLIC pans_options) # 继承所有编译/链接选项

# Threads::Threads：由 pansDependencies.cmake 中的 find_package(Threads) 提供。
target_link_libraries(pans PUBLIC Threads::Threads)

# 调用栈符号化所需的平台库，按平台的选择统一在 pansDependencies.cmake 中完成：
#   Linux 为 PkgConfig::PANS_LIBDW，Windows 为 dbghelp，macOS 为空（backtrace / dladdr 位于默认链接的 libSystem）。
# pans 是静态库，PRIVATE 依赖仍会以 $<LINK_ONLY:...> 写入导出文件，
#   因此 pansConfig.cmake.in 中同样要查找 libdw。
target_link_libraries(pans PRIVATE ${PANS_STACKTRACE_LIBRARIES})

target_link_libraries(pans PUBLIC yaml-cpp::yaml-cpp)

# 第三方库在构建阶段才编译安装，目标级依赖保证先构建完再编译、链接 pans。
add_dependencies(pans pans_third_party_dependencies)

target_compile_options(pans PRIVATE
    # 主库将警告视为错误；含 AppleClang，以便在 macOS 上同样生效
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
        -Werror
    >
)

# pans PUBLIC 依赖 pans_options，二者须在同一 export set，否则 install/export 会失败
# 同一目标在一个 export set 中只能出现一次，重复 install(TARGETS ... EXPORT) 会在生成阶段报错。
install(TARGETS pans pans_options
    EXPORT pansTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# 按 include/ 下的目录结构安装公共头文件（logger/、utils/、pans/），
#   与 #include <logger/log.h> 等写法及 INSTALL_INTERFACE 的 include 目录对应；
#   src/ 下的私有头文件不安装。
# 直接写成 install(DIRECTORY include/ DESTINATION ...) 等价于带 ${CMAKE_CURRENT_SOURCE_DIR}，但推荐加绝对路径更稳妥，避免 add_subdirectory 时路径混淆。
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
# 更简写的写法：
# install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# 作用等同，但依赖当前 CMakeLists.txt 目录为起点，通常也没问题。实际取舍看项目结构和可维护性。
# 推荐继续用绝对路径，避免因 add_subdirectory 或 CMake 工作目录变动造成潜在问题。