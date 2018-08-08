cmake_minimum_required(VERSION 3.5)

# add files
set(TEST_NAME ${LIB_NAME}_test)
add_executable(${TEST_NAME}
    test.cpp
)

# incudes
target_include_directories(${TEST_NAME}
    PUBLIC
        ${Catch_INCLUDE_DIRS}

    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# linking
target_link_libraries(${TEST_NAME}
    ${Catch_LIBRARIES}
    
	${LIB_NAME}
	${CMAKE_THREAD_LIBS_INIT}
)

# cotire speedup
if(BUILD_COTIRE_PROJ)
    set_target_properties(${TEST_NAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "../../include/${LIB_NAME}/tests/stdafx.h")
    cotire(${TEST_NAME})
endif()
