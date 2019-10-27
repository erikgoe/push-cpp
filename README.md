# Push compiler (pushc)
PUSHlang (Performant, Universal, Safe and High level language) is a modern programming language which aims to be simple through abstract rules and to have high level features which are compiled to low level code with a fast (and incremental) compiler. 
The language is highly inspired by Rust but does not intend to be compatible in any way. I created this project to play around with programming language design. I wanted to build a language that works just the way I wish to and maybe other folks like this way too.
This repository contains the compiler implementation in c++ but for now also (indirectly) the language specification. Pushc is (will be) a cross-compiler, so this repo is only needed to bootstrap the language.

WIP: The project is currently in a very early development stage. Everything may break frequently.

## Push?
This is the current development name and thus the language may be renamed some day.

## Dependencies
* cotire: https://github.com/sakra/cotire (optional)
    * Used for precompiled headers
    * Clone the repo and refer to the "CMake/cotire.cmake" file with the "-DCOTIRE_PATH" cmake parameter
* catch2: https://github.com/catchorg/Catch2 (optional)
    * Install it with a package manager like "pacman" or follow the instructions on the website
    * For windows you can alternatively install vcpkg onto your system (https://github.com/Microsoft/vcpkg) and then install "catch2"

## Building
This project uses cmake as build system.
Cotire can be enabled to platform-independently get precompiled headers, by setting the "-DCOTIRE_PATH" parameter.
The Catch Framework is used for testing and is enabled through the "-DTEST_WITH_CATCH" flag.
Vcpkg is optional to easier get catch2 for windows (visual studio projects). Setting the "-DVCPKG_SOURCE_DIR" parameter will automatically enable catch for you.

### Linux
    mkdir build && cd build
    cmake .. -DCOTIRE_PATH=/path_to_cotire_repo/CMake/cotire.cmake -DTEST_WITH_CATCH=true
    make

### Windows
NOTE: Windows is not tested regularly any more, but should still work with minor code modifications.

    mkdir build && cd build
    cmake .. -DCOTIRE_PATH=\path_to_cotire_repo\CMake\cotire.cmake -DVCPKG_SOURCE_DIR=C:\src\vcpkg -DVCPKG_TARGET_TRIPLET=x86-windows-static
Compile the .sln file.

## Tests
Most of the test cases only check the basic functionality, which is required for the functions/modules to be usable.

If you want to run the tests, build the project with Catch (so "-DTEST_WITH_CATCH" must be set to true) and run the executable (either build/libpush/src/tests/Debug/libpush_test.exe or build/libpush/src/tests/libpush_test) from your shell. 

Its designated that the test will print some warnings and errors. Just make sure the "All tests passed (x assertions in y test cases)" gets printed at the end.
