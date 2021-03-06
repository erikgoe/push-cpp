cmake_minimum_required(VERSION 3.6)

# add files
add_library(${LIB_NAME}
    basic_queries/FileQueries.cpp
    Message.cpp
    GlobalCtx.cpp
    Worker.cpp
    input/StreamInput.cpp
    input/SourceInput.cpp
    UnitCtx.cpp
    util/String.cpp
)

# includes
target_include_directories(${LIB_NAME}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# linking
target_link_libraries(${LIB_NAME}
    ${CMAKE_THREAD_LIBS_INIT}
    stdc++fs
)

# cotire speedup
if(BUILD_COTIRE_PROJ)
    set_target_properties(${LIB_NAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "../include/${LIB_NAME}/stdafx.h")
    cotire(${LIB_NAME})
endif()

# add tests
if(TEST_WITH_CATCH)
    add_subdirectory(tests)
endif()
