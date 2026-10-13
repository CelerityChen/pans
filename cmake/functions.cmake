# 把源文件中的 __FILE__ 重定义为相对项目根目录的路径，
#   使日志、断言中不出现构建机器上的绝对路径（如 /Users/xxx/...），并让不同机器的产物一致。
# 用法：redefine_file_macro(${SOURCES}) 或 redefine_file_macro("${SOURCES}")。
# 重定义内置宏会触发 -Wbuiltin-macro-redefined 警告，已在 pansOptions.cmake 中通过
#   -Wno-builtin-macro-redefined 关闭（GCC、Clang、AppleClang 均支持），否则 -Werror 下会编译失败。
function(redefine_file_macro targets)
    # ARGV：函数收到的全部实参列表。
    #   不加引号调用时列表会被拆成多个实参，形参 targets 只接住第一个，其余在 ARGN 中，
    #   因此遍历 ARGV 才能同时兼容加引号和不加引号两种调用方式。
    foreach(source_file IN LISTS ARGV)
        # continue()：跳过本次循环，进入下一次。
        if("${source_file}" STREQUAL "")
            continue()
        endif()
        # get_filename_component(<输出变量> <路径> ABSOLUTE)：转成绝对路径，相对路径以当前源码目录为基准。
        get_filename_component(absolute_path "${source_file}" ABSOLUTE)
        # file(RELATIVE_PATH <输出变量> <基准目录> <路径>)：计算相对路径；
        #   路径在基准目录之外时结果以 ../ 开头。
        file(RELATIVE_PATH relative_path "${PROJECT_SOURCE_DIR}" "${absolute_path}")
        # 项目目录外的生成文件不保留其绝对路径
        # MATCHES：正则匹配；CMake 字符串中 \\ 表示一个反斜杠，因此 "^\\.\\./" 即正则 ^\.\./。
        # get_filename_component(... NAME)：只取文件名部分。
        if(IS_ABSOLUTE "${relative_path}" OR relative_path STREQUAL ".." OR relative_path MATCHES "^\\.\\./")
            get_filename_component(relative_path "${source_file}" NAME)
        endif()
        # set_property(SOURCE ... APPEND PROPERTY COMPILE_DEFINITIONS)：给单个源文件追加编译宏，
        #   APPEND 保留该文件已有的其他宏（set_source_files_properties 会整体覆盖）。
        # 源文件属性按目录作用域生效：只对与本次调用处于同一 CMakeLists.txt 目录中创建的目标有效，
        #   因此应在定义目标的那个 CMakeLists.txt 中调用。
        # \"：在 CMake 字符串中转义双引号，宏的值最终是带引号的字符串字面量。
        set_property(SOURCE "${source_file}" APPEND PROPERTY COMPILE_DEFINITIONS "__FILE__=\"${relative_path}\"")
    endforeach()
endfunction()
