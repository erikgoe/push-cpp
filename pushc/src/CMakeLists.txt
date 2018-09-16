cmake_minimum_required(VERSION 3.6)

# add files
add_executable(${EXE_NAME}
    CLI.cpp
    Help.cpp
    Pref.cpp
)

# includes
target_include_directories(${EXE_NAME}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../libpush/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../libpushc/include
)

# linking
target_link_libraries(${EXE_NAME}
    libpush
    libpushc
)

# cotire speedup
if(BUILD_COTIRE_PROJ)
    set_target_properties(${EXE_NAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "../include/${EXE_NAME}/stdafx.h")
    cotire(${EXE_NAME})
endif()
