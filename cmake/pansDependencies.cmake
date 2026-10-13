# set(<变量> <值> CACHE <类型> <说明>)：定义缓存变量，保存在构建目录的 CMakeCache.txt 中。
#   - 仅当缓存中尚无该变量时才写入默认值；之后修改这里的默认值不会生效，需删除缓存重新配置。
#   - 可在命令行覆盖：cmake -B build -DPANS_THIRD_PARTY_DIR=/path/to/deps
#   - 加 FORCE 会强制覆盖缓存值，但会让 -D 覆盖失效。
# PROJECT_SOURCE_DIR：最近一次 project() 所在目录，即 pans 根目录；
#   作为子项目被 add_subdirectory 引入时仍指向 pans 自身（区别于 CMAKE_SOURCE_DIR 指向最顶层项目）。
# PATH：缓存变量类型，仅影响 cmake-gui/ccmake 中显示为目录选择框，实际仍存为字符串。
set(PANS_THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party" CACHE PATH "third-party source")
# ${<变量>}：变量引用，在双引号内同样会展开；漏写 $ 则只是普通文本。
#   默认值在首次配置时求值后写入缓存：之后再用 -D 修改 PANS_THIRD_PARTY_DIR，此变量不会随之更新。
set(PANS_THIRD_PARTY_OUTPUT_DIR "${PANS_THIRD_PARTY_DIR}/output" CACHE PATH "compiled third-party output")
# STRING：普通字符串类型的缓存变量（GUI 中显示为文本框）。
#   不加引号的单个单词（如 Release）也是字符串，无需引号。
set(PANS_THIRD_PARTY_BUILD_TYPE Release CACHE STRING "third-party build-type")
# set_property(CACHE <变量> PROPERTY STRINGS ...)：为缓存变量设置可选值列表，
#   cmake-gui/ccmake 中显示为下拉框；仅是提示，命令行 -D 传其他值不会报错。
set_property(CACHE PANS_THIRD_PARTY_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

# option(<变量> <说明> <ON|OFF>)：定义布尔类型（BOOL）的缓存变量，等价于 set(... CACHE BOOL ...)。
#   同样只在缓存中不存在时写入默认值，可用 -DPANS_UPDATE_SUBMODULES=OFF 关闭。
option(PANS_UPDATE_SUBMODULES "Initialize missing third-party git submodules" ON)
option(PANS_REUSE_INSTALLED_DEPENDENCIES "Reuse compatible dependencies from third_party/output" ON)

# 不带 CACHE 的 set 定义普通变量；传多个值时变量是一个列表（内部以分号 ; 分隔的字符串）。
# 每项格式为 "子模块路径|哨兵文件"：哨兵文件存在即认为该子模块已检出。
#   用 | 而非 ; 分隔，是为了避免和列表分隔符冲突。
# 以 _ 开头是约定俗成的"内部变量"命名，表示不供外部使用。
set(_pans_required_submodules
    "third_party/yaml-cpp|CMakeLists.txt"
    "third_party/abseil-cpp|CMakeLists.txt"
    "third_party/protobuf|CMakeLists.txt"
    "third_party/rapidjson|include/rapidjson/document.h"
    "third_party/tinyxml2|CMakeLists.txt"
    "third_party/kcp|ikcp.c"
    "third_party/zlib|CMakeLists.txt"
    )

# find_package(Git)：调用 CMake 自带的 FindGit 模块查找 git 可执行文件，
#   找到后设置 Git_FOUND 和 GIT_EXECUTABLE。QUIET 表示找不到时不打印提示；
#   未加 REQUIRED，因此找不到也不会中断配置，由下方函数自行判断。
find_package(Git QUIET)

# function(<名称>) ... endfunction()：定义函数，需显式调用 pans_init_third_party_submodules() 才会执行。
#   函数有独立作用域：内部 set 的变量在函数返回后消失（除非用 PARENT_SCOPE），
#   但可以读取调用方的变量，如 _pans_required_submodules。
function(pans_init_third_party_submodules)
    # set(<变量>) 不给值：将变量置为空，这里用作空列表。
    set(missing_submodules)
    # foreach(<循环变量> IN LISTS <列表变量>) ... endforeach()：遍历列表；注意 IN LISTS 后写变量名，不加 ${}。
    foreach(entry IN LISTS _pans_required_submodules)
        # string(REPLACE <查找> <替换> <输出变量> <输入>)：把 | 换成 ;，使 parts 变成两元素的列表。
        string(REPLACE "|" ";" parts "${entry}")
        # list(GET <列表> <下标> <输出变量>)：按下标（从 0 开始）取元素。
        list(GET parts 0 submodule_path)
        list(GET parts 1 sentinel)
        # if(...) ... endif()：条件判断；NOT 取反，EXISTS 判断文件或目录是否存在（需传绝对路径）。
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${submodule_path}/${sentinel}")
            # list(APPEND <列表> <元素>...)：向列表末尾追加元素。
            list(APPEND missing_submodules "${submodule_path}")
        endif()
    endforeach()

    # if(<变量名>)：变量为空、0、OFF、FALSE、NO、NOTFOUND 等时为假；空列表即为假。
    if(NOT missing_submodules)
        # return()：提前退出当前函数。
        return()
    endif()

    if(NOT PANS_UPDATE_SUBMODULES)
        # message(FATAL_ERROR ...)：打印错误并立即终止配置。
        #   相邻的多个字符串参数会被直接拼接输出，\n 为换行。
        message(FATAL_ERROR
            "Missing third-party source trees: ${missing_submodules}\n"
            "Enable PANS_UPDATE_SUBMODULES or run:\n"
            " git submodule update --init --recursive")
    endif()

    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/.git")
        message(FATAL_ERROR
            "Missing third-party source trees: ${missing_submodules}\n"
            "The source tree is not a git checkout, so CMake cannot initialize submodules.")
    endif()

    # Git_FOUND 由上方 find_package(Git) 设置。
    if(NOT Git_FOUND)
        message(FATAL_ERROR "Git is required to initialize missing submodules.")
    endif()

    # message(STATUS ...)：普通状态信息，配置时以 "-- " 前缀输出。
    message(STATUS "Initializing missing git submodules: ${missing_submodules}")
    # execute_process：在配置阶段（运行 cmake 时）同步执行外部命令，而非构建阶段。
    #   COMMAND 后跟命令及参数；未加引号的 ${missing_submodules} 会按 ; 展开成多个参数。
    #   WORKING_DIRECTORY 指定工作目录；RESULT_VARIABLE 接收退出码，
    #   OUTPUT_VARIABLE / ERROR_VARIABLE 分别捕获 stdout / stderr（捕获后不会显示在终端）。
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive -- ${missing_submodules}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        RESULT_VARIABLE submodule_result
        OUTPUT_VARIABLE submodule_output
        ERROR_VARIABLE submodule_error
    )

    # EQUAL：数值比较（字符串比较用 STREQUAL）。
    if(NOT submodule_result EQUAL 0)
        message(FATAL_ERROR
            "git submodule update failed (exit=${submodule_result}):\n"
            "${submodule_output}${submodule_error}")
    endif()

    # 再次检查哨兵文件，确认 git 命令成功后子模块确实已检出。
    foreach(entry IN LISTS _pans_required_submodules)
        string(REPLACE "|" ";" parts "${entry}")
        list(GET parts 0 submodule_path)
        list(GET parts 1 sentinel)
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${submodule_path}/${sentinel}")
            message(FATAL_ERROR
                "Required third-party source is still missing: "
                "${submodule_path}/${sentinel}")
        endif()
    endforeach()
endfunction()

# 调用上面定义的函数：在配置阶段检查并按需初始化子模块。
pans_init_third_party_submodules()

# 判断子模块是否更新过
# function(<名称> <参数1> <参数2>)：带命名参数的函数，调用时按位置传入，函数内用 ${source_dir} 读取。
#   output_variable 接收的是"调用方变量名"，而不是值，用于把结果写回调用方。
function(pans_get_source_revision source_dir output_variable)
    # AND：逻辑与（另有 OR）。子模块中的 .git 通常是文件而非目录，EXISTS 对两者都成立。
    if(Git_FOUND AND EXISTS "${source_dir}/.git")
        # git -C <目录>：在指定目录中执行 git；rev-parse HEAD 输出当前提交哈希。
        # OUTPUT_STRIP_TRAILING_WHITESPACE：去掉输出末尾的换行等空白，否则哈希会带 \n。
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" rev-parse HEAD
            RESULT_VARIABLE revision_result
            OUTPUT_VARIABLE revision
            ERROR_VARIABLE revision_error
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT revision_result EQUAL 0)
            message(FATAL_ERROR "Failed to read third-party revision in ${source_dir}: ${revision_error}")
        endif()
    # else()：if 条件不成立时的分支（还可用 elseif()）。
    else()
        # file(SHA256 <文件> <输出变量>)：计算文件内容的 SHA256。
        #   非 git 检出时退而用 CMakeLists.txt 的哈希近似表示源码版本。
        file(SHA256 "${source_dir}/CMakeLists.txt" revision)
    endif()
    # set(${output_variable} ... PARENT_SCOPE)：${output_variable} 先展开成调用方传入的变量名，
    #   PARENT_SCOPE 把值写到调用方作用域；函数内的普通 set 在返回后会丢失。
    set(${output_variable} "${revision}" PARENT_SCOPE)

endfunction()

# 不声明形参的函数：参数全部交给 cmake_parse_arguments 按关键字解析。
function(pans_add_cmake_dependency)
    # cmake_parse_arguments(PARSE_ARGV <起始下标> <前缀> <开关关键字> <单值关键字> <多值关键字>)
    #   PARSE_ARGV 0：从第 0 个参数开始解析函数的全部实参（只能在 function 中使用）。
    #   前缀 dep：解析结果存入 dep_<关键字>，如 dep_NAME、dep_CMAKE_ARGS。
    #   ""：没有开关型（布尔）关键字；单值关键字后跟一个值，多值关键字后跟一个列表。
    #   未出现的关键字对应变量为未定义。调用示例：
    #   pans_add_cmake_dependency(NAME zlib SOURCE_DIR ... SOURCE_REVISION ... SENTINELS lib/libz.a)
    cmake_parse_arguments(PARSE_ARGV 0 dep ""
        "NAME;SOURCE_DIR;SOURCE_REVISION"
        "CMAKE_ARGS;SENTINELS;DEPENDS;KEY_MATERIAL")

    if(NOT dep_NAME OR NOT dep_SOURCE_DIR OR NOT dep_SOURCE_REVISION)
        message(FATAL_ERROR "pans_add_cmake_dependency requires NAME, SOURCE_DIR and SOURCE_REVISION.")
    endif()
    if(NOT dep_SENTINELS)
        message(FATAL_ERROR "pans_add_cmake_dependency(${dep_NAME}) requires SENTINELS.")
    endif()
    # string(CONCAT <输出变量> <字符串>...)：把多个字符串原样拼接（不加分隔符），这里用 ; 手动分隔。
    # 拼接所有影响二进制兼容性（ABI）的因素：源码版本、编译器、平台、生成器、构建类型、编译参数等，
    #   任一变化都会得到不同的缓存键，从而触发重新编译。
    # CMAKE_C_COMPILER 等是 CMake 内置变量，在 project() 启用语言后才有值。
    string(CONCAT cache_material
        "${dep_SOURCE_REVISION};${CMAKE_C_COMPILER};${CMAKE_C_COMPILER_ID};"
        "${CMAKE_C_COMPILER_VERSION};${CMAKE_CXX_COMPILER};${CMAKE_CXX_COMPILER_ID};"
        "${CMAKE_CXX_COMPILER_VERSION};${CMAKE_SYSTEM_NAME};${CMAKE_SYSTEM_PROCESSOR};"
        "${CMAKE_GENERATOR};${CMAKE_GENERATOR_PLATFORM};${CMAKE_GENERATOR_TOOLSET};"
        "${CMAKE_OSX_ARCHITECTURES};${CMAKE_OSX_DEPLOYMENT_TARGET};${CMAKE_OSX_SYSROOT};"
        "${CMAKE_MSVC_RUNTIME_LIBRARY};${PANS_THIRD_PARTY_BUILD_TYPE};static;"
        "${CMAKE_CXX_STANDARD};${CMAKE_CXX_EXTENSIONS};"
        "${dep_CMAKE_ARGS};${dep_KEY_MATERIAL}")
    # string(SHA256 <输出变量> <输入>)：计算字符串的 SHA256（64 位十六进制）。
    string(SHA256 cache_key "${cache_material}")
    # string(SUBSTRING <字符串> <起始> <长度> <输出变量>)：截取前 16 位，用作较短的目录名。
    string(SUBSTRING "${cache_key}" 0 16 cache_key_short)# Application binary interface 一致

    # 安装目录和构建目录都带缓存键：不同配置的产物互不覆盖，配置不变时可直接复用。
    set(install_dir "${PANS_THIRD_PARTY_OUTPUT_DIR}/${dep_NAME}/${cache_key_short}")
    set(build_dir "${PANS_THIRD_PARTY_OUTPUT_DIR}/build/${dep_NAME}-${cache_key_short}")
    # 戳记文件（stamp）：文件名含完整缓存键，存在即表示该配置已完整安装。
    set(cache_stamp "${install_dir}/.pans-cache-${cache_key}")

    set(installed_outputs "${cache_stamp}")
    # TRUE / FALSE：CMake 布尔常量（也可写 ON/OFF、YES/NO）。
    set(cache_complete TRUE)
    foreach(sentinel IN LISTS dep_SENTINELS)
        # IS_ABSOLUTE：判断路径是否为绝对路径；相对路径按安装目录解析。
        if(IS_ABSOLUTE "${sentinel}")
            set(sentinel_path "${sentinel}")
        else()
            set(sentinel_path "${install_dir}/${sentinel}")
        endif()
        list(APPEND installed_outputs "${sentinel_path}")
        if(NOT EXISTS "${sentinel_path}")
            set(cache_complete FALSE)
        endif()
    endforeach()

    # 组装配置第三方库的命令行，等价于在终端执行：
    #   cmake -S <源码目录> -B <构建目录> -G <生成器> -D...
    # CMAKE_COMMAND：当前正在运行的 cmake 可执行文件的完整路径，保证父子项目使用同一版本 cmake。
    # 命令保存为列表，每个元素是一个参数；之后以不加引号的 ${configure_command} 展开传给 execute_process。
    set(configure_command "${CMAKE_COMMAND}" -S "${dep_SOURCE_DIR}" -B "${build_dir}" -G "${CMAKE_GENERATOR}")
    # -A / -T：仅 Visual Studio 等生成器使用的目标平台和工具集，其他平台（包括 macOS）下为空、自动跳过。
    if(CMAKE_GENERATOR_PLATFORM)
        list(APPEND configure_command -A "${CMAKE_GENERATOR_PLATFORM}")
    endif()
    if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND configure_command -T "${CMAKE_GENERATOR_TOOLSET}")
    endif()

    # CMAKE_INSTALL_LIBDIR=lib：固定库目录为 lib，避免部分 Linux 发行版默认装到 lib64，使哨兵路径跨平台一致。
    # CMAKE_BUILD_TYPE：仅对单配置生成器（Makefiles、Ninja）有效；
    #   Xcode、Visual Studio 等多配置生成器会忽略它，需在构建/安装时传 --config。
    # CMAKE_POSITION_INDEPENDENT_CODE=ON：生成位置无关代码（-fPIC），静态库才能被链接进动态库。
    # BUILD_SHARED_LIBS=OFF：强制构建静态库，与缓存键中的 static 保持一致；
    #   在 macOS 上还能避免 .dylib 的 install_name/@rpath 在运行时找不到的问题。
    list(APPEND configure_command
        "-DCMAKE_INSTALL_PREFIX=${install_dir}"
        "-DCMAKE_INSTALL_LIBDIR=lib"
        "-DCMAKE_BUILD_TYPE=${PANS_THIRD_PARTY_BUILD_TYPE}"
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
        "-DBUILD_SHARED_LIBS=OFF"
        "-DBUILD_TESTING=OFF"
    )
    # 把父项目的编译器和工具链透传给子项目，保证 ABI 一致。
    if(CMAKE_C_COMPILER)
        list(APPEND configure_command "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
    endif()
    if(CMAKE_CXX_COMPILER)
        list(APPEND configure_command "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
    endif()
    if(CMAKE_TOOLCHAIN_FILE)
        list(APPEND configure_command "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    endif()
    # 透传 C++ 标准：AppleClang 默认标准低于 C++17（Linux 上 GCC 默认 gnu++17），
    #   abseil 会因此配置失败；且 abseil 要求自身与所有使用它的库（protobuf、pans）使用相同的 C++ 标准。
    if(CMAKE_CXX_STANDARD)
        list(APPEND configure_command
            "-DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}"
            "-DCMAKE_CXX_STANDARD_REQUIRED=ON"
            "-DCMAKE_CXX_EXTENSIONS=${CMAKE_CXX_EXTENSIONS}")
    endif()
    if(CMAKE_MSVC_RUNTIME_LIBRARY)
        list(APPEND configure_command "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}")
    endif()
    # CMAKE_OSX_ARCHITECTURES 可能是列表（如 arm64;x86_64 构建通用二进制）。
    #   直接拼进 "-D...=${...}" 后再追加到命令列表，分号会把它拆成两个参数；
    #   命令最终交给 add_custom_command 执行，需替换为生成器表达式 $<SEMICOLON>，
    #   在生成构建文件时才还原成 ;，从而作为一个完整参数传递（不能配合 COMMAND_EXPAND_LISTS）。
    if(CMAKE_OSX_ARCHITECTURES)
        string(REPLACE ";" "$<SEMICOLON>" osx_architectures "${CMAKE_OSX_ARCHITECTURES}")
        list(APPEND configure_command "-DCMAKE_OSX_ARCHITECTURES=${osx_architectures}")
    endif()
    # CMAKE_OSX_DEPLOYMENT_TARGET：最低支持的 macOS 版本；需在顶层 project() 之前设置才对父项目生效。
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        list(APPEND configure_command "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
    # CMAKE_OSX_SYSROOT：使用的 macOS SDK 路径；与父项目保持一致，避免子项目选中另一个 SDK。
    if(CMAKE_OSX_SYSROOT)
        list(APPEND configure_command "-DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}")
    endif()
    # 调用方的额外参数放在最后，同名 -D 后者覆盖前者，因此调用方可以覆盖上面的默认值。
    list(APPEND configure_command ${dep_CMAKE_ARGS})

    # 配置 → 编译 → 安装 → 写戳记，四条命令在构建阶段依次执行（而非配置阶段）。
    # build_commands 是包含 COMMAND 关键字的列表，展开后可直接作为 add_custom_command 的参数。
    # --config：多配置生成器（Xcode、Visual Studio）据此选择构建类型，单配置生成器会忽略。
    # --parallel <N>：并行编译任务数，取顶层 CMakeLists.txt 中设置的 CMAKE_BUILD_PARALLEL_LEVEL。
    # cmake -D... -P <脚本>：以脚本模式运行 .cmake 文件，-D 必须写在 -P 之前才会传入脚本。
    set(build_commands
        COMMAND ${configure_command}
        COMMAND "${CMAKE_COMMAND}" --build "${build_dir}" --config "${PANS_THIRD_PARTY_BUILD_TYPE}" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL}"
        COMMAND "${CMAKE_COMMAND}" --install "${build_dir}" --config "${PANS_THIRD_PARTY_BUILD_TYPE}"
        COMMAND "${CMAKE_COMMAND}" "-DPANS_DEPS_STAMP_FILE=${cache_stamp}" "-DPANS_DEPS_STAMP_VALUE=${cache_key}" -P "${PROJECT_SOURCE_DIR}/cmake/writeDependencyStamp.cmake"
    )

    # 目标名不能含 -（部分生成器不支持），因此 abseil-cpp 对应 pans_dep_abseil_cpp_ready。
    set(ready_target "pans_dep_${dep_NAME}_ready")
    string(REPLACE "-" "_" ready_target "${ready_target}")

    if(PANS_REUSE_INSTALLED_DEPENDENCIES)
        # add_custom_command(OUTPUT <文件>...)：声明"如何生成这些文件"。
        #   只有输出文件缺失（或比依赖旧）时才会执行命令，产物齐全即跳过，实现缓存复用。
        #   它本身不是目标，需要有目标 DEPENDS 这些输出才会被触发。
        # COMMENT：执行时打印的提示。
        # VERBATIM：参数按原样转义传给命令，避免不同平台 shell 对引号、空格的解释差异，建议总是加上。
        add_custom_command(
            OUTPUT ${installed_outputs}
            ${build_commands}
            COMMENT "Building and installing third-party dependency: ${dep_NAME}"
            VERBATIM
        )
        # add_custom_target(<名称> DEPENDS <文件>...)：创建一个目标，构建它时确保这些文件已生成。
        add_custom_target("${ready_target}" DEPENDS ${installed_outputs})
    else()
        # 不复用缓存：命令直接挂在目标上，每次构建该目标都会重新执行。
        add_custom_target("${ready_target}"
            ${build_commands}
            COMMENT "Rebuilding third-party dependency: ${dep_NAME}"
            VERBATIM
        )
    endif()

    # add_dependencies(<目标> <依赖目标>...)：目标级构建顺序，保证依赖库（如 protobuf 依赖的 abseil、zlib）先构建。
    if(dep_DEPENDS)
        add_dependencies("${ready_target}" ${dep_DEPENDS})
    endif()
    if(cache_complete AND PANS_REUSE_INSTALLED_DEPENDENCIES)
        message(STATUS "${dep_NAME}: reusing ${install_dir}")
    else()
        message(STATUS "${dep_NAME}: build scheduled; output ${install_dir}")
    endif()
    # string(TOUPPER ...)：转大写。把结果以 PANS_<名称>_* 的形式返回给调用方，如 PANS_ABSEIL_CPP_INSTALL_DIR。
    # set("PANS_${variable_prefix}_..." ...)：变量名本身也可以由变量拼接而成。
    string(TOUPPER "${dep_NAME}" variable_prefix)
    string(REPLACE "-" "_" variable_prefix "${variable_prefix}")
    set("PANS_${variable_prefix}_INSTALL_DIR" "${install_dir}" PARENT_SCOPE)
    set("PANS_${variable_prefix}_CACHE_KEY" "${cache_key}" PARENT_SCOPE)
    set("PANS_${variable_prefix}_CACHE_COMPLETE" "${cache_complete}" PARENT_SCOPE)
    set("PANS_${variable_prefix}_DEPENDENCY_TARGET" "${ready_target}" PARENT_SCOPE)
endfunction()

# 静态库文件名模板：CMAKE_STATIC_LIBRARY_PREFIX/SUFFIX 由平台决定，
#   macOS/Linux 为 lib<名称>.a，Windows(MSVC) 为 <名称>.lib；用 string(REPLACE "@NAME@" ...) 填入库名。
# 注意：Debug 构建时部分库（如 yaml-cpp、protobuf）会给文件名加 d 后缀，哨兵路径会对不上。
set(_pans_static_library "${CMAKE_STATIC_LIBRARY_PREFIX}@NAME@${CMAKE_STATIC_LIBRARY_SUFFIX}")

# --------- abseil-cpp -----------
set(_pans_absl_source "${PANS_THIRD_PARTY_DIR}/abseil-cpp")
pans_get_source_revision("${_pans_absl_source}" _pans_absl_revision)
string(REPLACE "@NAME@" "absl_base" _pans_absl_library "${_pans_static_library}")
pans_add_cmake_dependency(
    NAME abseil-cpp
    SOURCE_DIR "${_pans_absl_source}"
    SOURCE_REVISION "${_pans_absl_revision}"
    CMAKE_ARGS
        "-DBUILD_SHARED_LIBS=OFF"
        "-DABSL_ENABLE_INSTALL=ON"
        "-DABSL_PROPAGATE_CXX_STD=ON"
        "-DABSL_BUILD_TESTING=OFF"
    SENTINELS
        "lib/cmake/absl/abslConfig.cmake"
        "include/absl/base/config.h"
        "lib/${_pans_absl_library}"
)

#--------- zlib -----------
set(_pans_zlib_source "${PANS_THIRD_PARTY_DIR}/zlib")
pans_get_source_revision("${_pans_zlib_source}" _pans_zlib_revision)
string(REPLACE "@NAME@" "z" _pans_zlib_library "${_pans_static_library}")
# MSVC：使用 MSVC 或兼容编译器（如 clang-cl）时为真的内置变量；macOS 下为假。
if(MSVC)
    string(REPLACE "@NAME@" "zlibstatic" _pans_zlib_library "${_pans_static_library}")
endif()
pans_add_cmake_dependency(
    NAME zlib
    SOURCE_DIR "${_pans_zlib_source}"
    SOURCE_REVISION "${_pans_zlib_revision}"
    CMAKE_ARGS
        "-DZLIB_BUILD_TESTING=OFF"
        "-DZLIB_BUILD_SHARED=OFF"
        "-DZLIB_BUILD_STATIC=ON"
        "-DZLIB_INSTALL=ON"
    SENTINELS
        "lib/cmake/zlib/ZLIBConfig.cmake"
        "include/zlib.h"
        "lib/${_pans_zlib_library}"
)

# --------- yaml-cpp -----------
set(_pans_yaml_source "${PANS_THIRD_PARTY_DIR}/yaml-cpp")
pans_get_source_revision("${_pans_yaml_source}" _pans_yaml_revision)
string(REPLACE "@NAME@" "yaml-cpp" _pans_yaml_library "${_pans_static_library}")
pans_add_cmake_dependency(
    NAME yaml-cpp
    SOURCE_DIR "${_pans_yaml_source}"
    SOURCE_REVISION "${_pans_yaml_revision}"
    CMAKE_ARGS
        "-DBUILD_SHARED_LIBS=OFF"
        "-DYAML_BUILD_SHARED_LIBS=OFF"
        "-DYAML_CPP_BUILD_TESTS=OFF"
        "-DYAML_CPP_BUILD_TOOLS=OFF"
        "-DYAML_CPP_FORMAT_SOURCE=OFF"
        "-DYAML_CPP_INSTALL=ON"
        "-DYAML_CPP_DISABLE_UNINSTALL=ON"
    SENTINELS
        "lib/cmake/yaml-cpp/yaml-cpp-config.cmake"
        "include/yaml-cpp/yaml.h"
        "lib/${_pans_yaml_library}"
)

# ---------- tinyxml2----------
set(_pans_tinyxml2_source "${PANS_THIRD_PARTY_DIR}/tinyxml2")
pans_get_source_revision("${_pans_tinyxml2_source}" _pans_tinyxml2_revision)
string(REPLACE "@NAME@" "tinyxml2" _pans_tinyxml2_library "${_pans_static_library}")
pans_add_cmake_dependency(
    NAME tinyxml2
    SOURCE_DIR "${_pans_tinyxml2_source}"
    SOURCE_REVISION "${_pans_tinyxml2_revision}"
    CMAKE_ARGS
        "-DBUILD_SHARED_LIBS=OFF"
        "-Dtinyxml2_SHARED_LIBS=OFF"
        "-Dtinyxml2_BUILD_TESTING=OFF"
    SENTINELS
        "lib/cmake/tinyxml2/tinyxml2-config.cmake"
        "include/tinyxml2.h"
        "lib/${_pans_tinyxml2_library}"
)

# --------- kcp -----------
set(_pans_kcp_source "${PANS_THIRD_PARTY_DIR}/kcp")
pans_get_source_revision("${_pans_kcp_source}" _pans_kcp_revision)
# kcp 上游没有可用的安装规则，用 cmake/kcp/CMakeLists.txt 包装构建；
#   包装文件的哈希放进 KEY_MATERIAL，修改包装文件后缓存键随之变化、自动重新构建。
file(SHA256 "${PROJECT_SOURCE_DIR}/cmake/kcp/CMakeLists.txt" _pans_kcp_wrapper_hash)
string(REPLACE "@NAME@" "kcp" _pans_kcp_library "${_pans_static_library}")
pans_add_cmake_dependency(
    NAME kcp
    SOURCE_DIR "${PROJECT_SOURCE_DIR}/cmake/kcp"
    SOURCE_REVISION "${_pans_kcp_revision}"
    KEY_MATERIAL "wrapper=${_pans_kcp_wrapper_hash}"
    CMAKE_ARGS
        "-DPANS_KCP_SOURCE_DIR=${_pans_kcp_source}"
        "-DBUILD_SHARED_LIBS=OFF"
    SENTINELS
        "lib/cmake/kcp/kcp-config.cmake"
        "include/ikcp.h"
        "lib/${_pans_kcp_library}"
)

# ---------- protobuf -----------
set(_pans_protobuf_source "${PANS_THIRD_PARTY_DIR}/protobuf")
pans_get_source_revision("${_pans_protobuf_source}" _pans_protobuf_revision)
string(REPLACE "@NAME@" "protobuf" _pans_protobuf_library "${_pans_static_library}")
if(MSVC)
    string(REPLACE "@NAME@" "libprotobuf" _pans_protobuf_library "${_pans_static_library}")
endif()
# CMAKE_EXECUTABLE_SUFFIX：可执行文件后缀，Windows 为 .exe，macOS/Linux 为空。
set(_pans_protoc "bin/protoc${CMAKE_EXECUTABLE_SUFFIX}")
# protobuf 依赖 abseil 和 zlib：把它们的缓存键放进 KEY_MATERIAL，依赖重编后 protobuf 也会重编；
#   DEPENDS 保证构建顺序；-Dabsl_DIR / -DZLIB_* 让 protobuf 使用上面刚安装的版本而非系统版本。
pans_add_cmake_dependency(
    NAME protobuf
    SOURCE_DIR "${_pans_protobuf_source}"
    SOURCE_REVISION "${_pans_protobuf_revision}"
    KEY_MATERIAL
        "abseil=${PANS_ABSEIL_CPP_CACHE_KEY}"
        "zlib=${PANS_ZLIB_CACHE_KEY}"
    DEPENDS
        "${PANS_ABSEIL_CPP_DEPENDENCY_TARGET}"
        "${PANS_ZLIB_DEPENDENCY_TARGET}"
    CMAKE_ARGS
        "-DCMAKE_CXX_STANDARD=20"
        "-DBUILD_SHARED_LIBS=OFF"
        "-Dprotobuf_BUILD_SHARED_LIBS=OFF"
        "-Dprotobuf_BUILD_TESTS=OFF"
        "-Dprotobuf_BUILD_CONFORMANCE=OFF"
        "-Dprotobuf_BUILD_EXAMPLES=OFF"
        "-Dprotobuf_LOCAL_DEPENDENCIES_ONLY=ON"
        "-Dprotobuf_WITH_ZLIB=ON"
        "-Dabsl_DIR=${PANS_ABSEIL_CPP_INSTALL_DIR}/lib/cmake/absl"
        "-DZLIB_ROOT=${PANS_ZLIB_INSTALL_DIR}"
        "-DZLIB_INCLUDE_DIR=${PANS_ZLIB_INSTALL_DIR}/include"
        "-DZLIB_LIBRARY=${PANS_ZLIB_INSTALL_DIR}/lib/${_pans_zlib_library}"
    SENTINELS
        "lib/cmake/protobuf/protobuf-config.cmake"
        "include/google/protobuf/message.h"
        "lib/${_pans_protobuf_library}"
        "${_pans_protoc}"
)

# ---------- rapidjson -----------
# rapidjson 是纯头文件库，无需编译，直接创建导入目标。
# if(NOT TARGET <名称>)：目标不存在时才创建，避免与其他地方重复定义。
# add_library(<名称> INTERFACE IMPORTED GLOBAL)：
#   INTERFACE 表示没有编译产物，只携带头文件路径等使用要求；
#   IMPORTED 表示来自项目外部；GLOBAL 使其在所有目录中可见（导入目标默认仅当前目录可见）。
# set_target_properties(... INTERFACE_INCLUDE_DIRECTORIES ...)：链接该目标的代码自动获得此头文件路径。
if(NOT TARGET rapidjson::rapidjson)
    add_library(rapidjson::rapidjson INTERFACE IMPORTED GLOBAL)
    set_target_properties(rapidjson::rapidjson PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${PANS_THIRD_PARTY_DIR}/rapidjson/include"
    )
endif()
message(STATUS "rapidjson: header-only; using ${PANS_THIRD_PARTY_DIR}/rapidjson/include")

# ---------- all dependencies -----------
# 汇总目标：ALL 表示默认构建（cmake --build 不指定目标时）也会构建它，从而触发所有第三方库。
# 使用第三方库的目标应 add_dependencies(<目标> pans_third_party_dependencies)，保证先构建依赖。
add_custom_target(pans_third_party_dependencies ALL)
add_dependencies(pans_third_party_dependencies
    "${PANS_ABSEIL_CPP_DEPENDENCY_TARGET}"
    "${PANS_ZLIB_DEPENDENCY_TARGET}"
    "${PANS_YAML_CPP_DEPENDENCY_TARGET}"
    "${PANS_TINYXML2_DEPENDENCY_TARGET}"
    "${PANS_KCP_DEPENDENCY_TARGET}"
    "${PANS_PROTOBUF_DEPENDENCY_TARGET}"
)

# ---------- system dependencies -----------
# find_package(Threads)：查找平台线程库，提供 Threads::Threads 目标（Linux 为 pthread，macOS 由 libSystem 提供）。
#   REQUIRED：找不到时直接报错终止配置。
find_package(Threads REQUIRED)

# 调用栈符号化后端：
#   Linux：libdw（elfutils）读取 ELF/DWARF 调试信息，通过 pkg-config 查找。
#   macOS：没有 libdw（elfutils 不支持 Mach-O），backtrace()/dladdr()/abi::__cxa_demangle
#     都由系统库 libSystem 和 libc++ 提供，无需额外查找或链接。
# 使用方统一链接 ${PANS_STACKTRACE_LIBRARIES}，不必再按平台区分。
# STREQUAL：字符串相等比较。APPLE：目标平台为 Apple 系（macOS、iOS 等）时为真的内置变量。
set(PANS_STACKTRACE_LIBRARIES)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # find_package(PkgConfig)：加载 FindPkgConfig 模块，提供 pkg_check_modules 命令。
    # pkg_check_modules(<前缀> REQUIRED IMPORTED_TARGET <模块>)：通过 pkg-config 查找 libdw，
    #   IMPORTED_TARGET 会创建导入目标 PkgConfig::<前缀>，携带头文件路径和链接参数。
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PANS_LIBDW REQUIRED IMPORTED_TARGET libdw)
    list(APPEND PANS_STACKTRACE_LIBRARIES PkgConfig::PANS_LIBDW)
elseif(APPLE)
    message(STATUS "stacktrace: using libSystem backtrace/dladdr")
elseif(WIN32)
    # dbghelp 是 Windows 系统库（StackWalk64 / SymFromAddr），按库名直接链接即可，无需查找。
    list(APPEND PANS_STACKTRACE_LIBRARIES dbghelp)
else()
    message(FATAL_ERROR "Unsupported platform for stacktrace: ${CMAKE_SYSTEM_NAME}")
endif()

# ---------- yaml-cpp target -----------
# 缓存已完整：用 yaml-cpp 自带的 CMake 包配置导入目标。
#   CONFIG：只查找 <包名>-config.cmake / <包名>Config.cmake，不使用 Find<包名>.cmake 模块。
#   PATHS ... NO_DEFAULT_PATH：只在指定目录中查找，避免误用系统或 Homebrew 安装的版本。
if(PANS_YAML_CPP_CACHE_COMPLETE)
    find_package(yaml-cpp CONFIG REQUIRED PATHS "${PANS_YAML_CPP_INSTALL_DIR}" NO_DEFAULT_PATH)
else()
    # 首次构建时库尚未安装，config 文件不存在，只能先手动声明导入目标，指向将来的产物路径。
    # file(MAKE_DIRECTORY ...)：预先创建 include 目录，
    #   因为 CMake 在配置阶段会检查 INTERFACE_INCLUDE_DIRECTORIES 中的路径是否存在。
    # add_library(<名称> STATIC IMPORTED GLOBAL)：声明外部已编译好的静态库。
    # IMPORTED_LOCATION：库文件的完整路径。
    # INTERFACE_COMPILE_DEFINITIONS：链接方编译时自动加上的宏；YAML_CPP_STATIC_DEFINE 表示按静态库使用 yaml-cpp。
    file(MAKE_DIRECTORY "${PANS_YAML_CPP_INSTALL_DIR}/include")
    add_library(yaml-cpp::yaml-cpp STATIC IMPORTED GLOBAL)
    set_target_properties(yaml-cpp::yaml-cpp PROPERTIES
        IMPORTED_LOCATION "${PANS_YAML_CPP_INSTALL_DIR}/lib/${_pans_yaml_library}"
        INTERFACE_COMPILE_DEFINITIONS YAML_CPP_STATIC_DEFINE
        INTERFACE_INCLUDE_DIRECTORIES "${PANS_YAML_CPP_INSTALL_DIR}/include"
    )
endif()

# ---------- install -----------
# 把各依赖的安装目录整体复制到 pans 的安装前缀下，使安装后的 pans 可独立使用。
# foreach(... IN ITEMS ...)：直接遍历给出的值（区别于 IN LISTS 遍历列表变量）。
# install(DIRECTORY <目录>/ ...)：源目录末尾带 / 表示复制目录内容，不带 / 则会多出一层同名目录。
#   DESTINATION "."：相对于安装前缀 CMAKE_INSTALL_PREFIX 的根目录。
#   PATTERN ... EXCLUDE：排除匹配的文件，这里排除缓存戳记文件。
foreach(_pans_dependency_install_dir IN ITEMS
    "${PANS_YAML_CPP_INSTALL_DIR}"
    "${PANS_ABSEIL_CPP_INSTALL_DIR}"
    "${PANS_ZLIB_INSTALL_DIR}"
    "${PANS_PROTOBUF_INSTALL_DIR}"
    "${PANS_TINYXML2_INSTALL_DIR}"
    "${PANS_KCP_INSTALL_DIR}"
)
    install(
        DIRECTORY "${_pans_dependency_install_dir}/"
        DESTINATION "."
        PATTERN ".pans-cache-*" EXCLUDE
    )
endforeach()

# CMAKE_INSTALL_INCLUDEDIR：由 GNUInstallDirs 定义，默认为 include。
install(
    DIRECTORY "${PANS_THIRD_PARTY_DIR}/rapidjson/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# unset(<变量>)：删除变量。本文件被 include 到调用方作用域，清理内部变量以免污染。
unset(_pans_required_submodules)
unset(_pans_static_library)
