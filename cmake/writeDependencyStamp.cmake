# 以脚本模式运行（cmake -D... -P writeDependencyStamp.cmake），由 pansDependencies.cmake
#   在第三方库安装完成后调用，写入缓存戳记文件，标记该缓存键对应的产物已完整安装。
# 参数：
#   PANS_DEPS_STAMP_FILE：戳记文件路径（文件名中含完整缓存键）。
#   PANS_DEPS_STAMP_VALUE：写入文件的内容，即缓存键。
if(NOT DEFINED PANS_DEPS_STAMP_FILE OR NOT DEFINED PANS_DEPS_STAMP_VALUE)
    message(FATAL_ERROR "Dependency stamp file and value are required")
endif()

# 确保所在目录存在后再写入；file(WRITE) 会覆盖已有内容。
get_filename_component(stamp_directory "${PANS_DEPS_STAMP_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${stamp_directory}")
file(WRITE "${PANS_DEPS_STAMP_FILE}" "${PANS_DEPS_STAMP_VALUE}\n")
