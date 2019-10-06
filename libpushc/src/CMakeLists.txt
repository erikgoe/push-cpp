cmake_minimum_required(VERSION 3.6)

# add files
add_library(${LIB_NAME}
    Ast.cpp
    AstParser.cpp
    Backend.cpp
    Compiler.cpp
    Linker.cpp
    MirTranslation.cpp
    Prelude.cpp
    Util.cpp
)

# includes
target_include_directories(${LIB_NAME}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../libpush/include
)

# linking
target_link_libraries(${LIB_NAME}
    libpush
)

# cotire speedup
if(BUILD_COTIRE_PROJ)
    set_target_properties(${LIB_NAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "../include/${LIB_NAME}/stdafx.h")
    cotire(${LIB_NAME})
endif()
