cmake_minimum_required(VERSION 3.6)

# add files
add_executable(${TEST_NAME}
    Lexer.cpp
    Message.cpp
    Preferences.cpp
    Test.cpp
    Workflow.cpp
)

# incudes
target_include_directories(${TEST_NAME}
    PUBLIC
        ${CATCH_INCLUDE_DIR}

    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# linking
target_link_libraries(${TEST_NAME}
    Catch2::Catch2
	${LIB_NAME}
	${CMAKE_THREAD_LIBS_INIT}
)

# cotire speedup
if(BUILD_COTIRE_PROJ)
    set_target_properties(${TEST_NAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "../../include/${LIB_NAME}/tests/stdafx.h")
    set_source_files_properties(Test.cpp PROPERTIES COTIRE_EXCLUDED TRUE)
    cotire(${TEST_NAME})
endif()
