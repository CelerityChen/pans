# ---------------------------------------------------------------------------
# pans 单元测试：为 tests/ 下每个源文件生成独立可执行文件，并注册到 CTest
# ---------------------------------------------------------------------------

# 收集本目录下的测试源文件（CONFIGURE_DEPENDS：新增/删除文件时自动重配置）
file(GLOB PANS_TEST_SOURCES CONFIGURE_DEPENDS
"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
"${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
)

list(SORT PANS_TEST_SOURCES)

find_package(Threads REQUIRED)

# 测试可执行文件统一输出到项目根下的 bin/tests/
set(PANS_TEST_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/tests")

foreach(TEST_SOURCE IN LISTS PANS_TEST_SOURCES)
    # 取源文件名（不含路径与扩展名）作为目标名，例如 test_macros.cpp -> test_macros
    get_filename_component(TEST_NAME "${TEST_SOURCE}" NAME_WE)

    # 避免与已有目标重名
    if(TARGET "${TEST_NAME}")
        message(FATAL_ERROR "Cannot create test target '{TEST_NAME}': a target with that name already exists")
        endif()

    add_executable("${TEST_NAME}" "${TEST_SOURCE}")

    # 链接 pans 库与线程库
    target_link_libraries("${TEST_NAME}" PRIVATE pans Threads::Threads)

    # 单配置生成器（如 Make/Ninja）：设置运行时输出目录
    set_target_properties("${TEST_NAME}" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PANS_TEST_OUTPUT_DIRECTORY}")

    # 多配置生成器（如 Visual Studio/Xcode）：为每种配置单独设置输出目录，避免落到 Debug/Release 子目录
    foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${CONFIG}" CONFIG_UPPER)
        set_target_properties("${TEST_NAME}" PROPERTIES "RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER}" "${PANS_TEST_OUTPUT_DIRECTORY}")
    endforeach()

    # 注册到 CTest，可用 ctest 运行
    add_test(NAME "${TEST_NAME}" COMMAND "${TEST_NAME}")
endforeach()
