# Push compiler (pushc)
PUSHlang (Performant, Universal, Safe and High level) is a modern programming language which aims to be simple through abstract rules and to have high level features which are compiled to low level code with a fast (and incremental) compiler. The language is highly inspired by Rust but does not intend to be compatible in any way.
This project directory contains the c++ compiler implementation but for now also the language specification. Pushc is (will be) a cross-compiler, thus this repo is only needed to bootstrap the language.

## Dependencies
* cotire (optional)
    * used for precompiled headers
    * download the "cotire.cmake" file from the repository (https://github.com/sakra/cotire/blob/master/CMake/cotire.cmake)
    * copy the file into your "dev" directory under "<dev>/.extlib/cotire/cotire.cmake"
* catch2 (optional)
    * install vcpkg on your system (https://github.com/Microsoft/vcpkg) and then install "catch2"

## Building
This project uses cmake as build system. Vcpkg is required for windows (visual studio projects).
The "dev" directory is currently only needed for cotire builds (which enables platform independent precompiled headers). Leaving out the "-DDEV_SOURCE_DIR" parameter will only disable cotire.
The Catch Framework is used for testing and is included through vcpkg. Leaving out the "-DVCPKG_SOURCE_DIR" parameter will only disable Catch.
Make sure the development source(dev) and vcpkg(src/vcpkg) directories are right!

### Windows
    mkdir build && cd build
    cmake .. -DDEV_SOURCE_DIR=C:/dev -DVCPKG_SOURCE_DIR=C:/src/vcpkg    -DVCPKG_TARGET_TRIPLET=x86-windows-static
Compile the .sln file.

### Linux
NOTE: linux is currently not tested but may work ^^

    mkdir build && cd build
    cmake .. -DDEV_SOURCE_DIR=~/dev -DVCPKG_SOURCE_DIR=~/src/vcpkg -DVCPKG_TARGET_TRIPLET=x64-linux
    make

## Tests
TODO
