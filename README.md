# Push compiler (pushc)
PUSHlang (Performant, Universal, Safe and High level) is a modern programming language which aims to be simple through abstract rules and to have high level features which are compiled to low level code with a fast (and incremental) compiler. 
The language is highly inspired by Rust but does not intend to be compatible in any way. I created this project to play around with programming language design. I wanted to build a language that works just the way i which it to do and maybe other folks like this way too.
This project directory contains the c++ compiler implementation but for now also the language specification. Pushc is (will be) a cross-compiler, thus this repo is only needed to bootstrap the language.

Wip: The project is currently in a very early development stage and thus everything may change frequently.

## Push?
This is the current development name and thus the language may be renamed some day.

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
    cmake .. -DDEV_SOURCE_DIR=C:/dev -DVCPKG_SOURCE_DIR=C:/src/vcpkg -DVCPKG_TARGET_TRIPLET=x86-windows-static
Compile the .sln file.

### Linux
NOTE: linux is currently not tested but may work ^^

    mkdir build && cd build
    cmake .. -DDEV_SOURCE_DIR=~/dev -DVCPKG_SOURCE_DIR=~/src/vcpkg -DVCPKG_TARGET_TRIPLET=x64-linux
    make

## Tests
Most of the test cases only check the basic functionality, which is required for the functions/modules to be usable.

If you want to run the tests, build the project with Catch (so VCPKG_SOURCE_DIR must be set) and run the executable (either build/libpush/src/tests/Debug/libpush_test.exe or build/libpush/src/tests/libpush_test) from your shell. 

The output will print some of warnings and error messages by design. Just make sure catchs' "All tests passed (x assertions in y test cases)" gets printed at the end.
