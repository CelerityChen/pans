file(GLOB_RECURSE PANS_SOURCES CONFIGURE_DEPENDS *.cpp *.cc *.c)
file(GLOB_RECURSE PANS_HEADERS CONFIGURE_DEPENDS *.h *.hpp)

add_library(pans STATIC ${PANS_SOURCES} ${PANS_HEADERS})

target_include_directories(pans
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src   
)

target_link_libraries(pans PUBLIC pans_options) # 继承所有编译/链接选项

target_compile_options(pans PRIVATE
    # 主库将警告视为错误；含 AppleClang，以便在 macOS 上同样生效
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
        -Werror
    >
)

# pans PUBLIC 依赖 pans_options，二者须在同一 export set，否则 install/export 会失败
install(TARGETS pans pans_options
    EXPORT pansTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${PANS_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pans
)
