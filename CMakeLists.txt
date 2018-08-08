cmake_minimum_required(VERSION 3.5)

# include toolchain file
if(VCPKG_SOURCE_DIR)
	set(CMAKE_TOOLCHAIN_FILE "${VCPKG_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake")
	set(TEST_WITH_CATCH true)
endif()
if(DEV_SOURCE_DIR)
	set(BUILD_COTIRE_PROJ true)
endif()

# define the project name
project(pushc)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)

set(CMAKE_CXX_STANDARD 14)

# static builds for windows
if (MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MTd")
endif()

# scripts
if(BUILD_COTIRE_PROJ)
	include(${DEV_SOURCE_DIR}/.extlib/cotire/cotire.cmake)
endif()

# packages
find_package(Catch2 CONFIG QUIET)

# subdirectories
add_subdirectory(libpushc/src)
#add_subdirectory(tests)
