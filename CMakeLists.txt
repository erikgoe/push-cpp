cmake_minimum_required(VERSION 3.6)

# prepare catch and cotire
if(VCPKG_SOURCE_DIR)
	set(CMAKE_TOOLCHAIN_FILE "${VCPKG_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake")
	if(NOT DEFINED TEST_WITH_CATCH)
		set(TEST_WITH_CATCH true)
	endif()
endif()
if(COTIRE_PATH)
	set(BUILD_COTIRE_PROJ true)
endif()

# define the project name
project(push)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)

set(CMAKE_CXX_STANDARD 17)

# static builds for windows
if (MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MTd")
endif()

add_definitions(-DCMAKE_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

# scripts
if(BUILD_COTIRE_PROJ)
	include(${COTIRE_PATH})
endif()

# packages
find_package(Threads)
if(TEST_WITH_CATCH)
	find_package(Catch2 REQUIRED)
	#find_path(CATCH_INCLUDE_DIR NAMES catch.hpp PATH_SUFFIXES Catch2)
endif()

# subdirectories

set(LIB_NAME lib${PROJECT_NAME})
set(TEST_NAME ${LIB_NAME}_test)
add_subdirectory(libpush/src)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${TEST_NAME}) # set startup project

set(LIB_NAME lib${PROJECT_NAME}c)
set(TEST_NAME ${LIB_NAME}_test)
add_subdirectory(libpushc/src)

set(EXE_NAME ${PROJECT_NAME}c)
add_subdirectory(pushc/src)
