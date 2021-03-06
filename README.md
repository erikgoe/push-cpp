# Push compiler (pushc)
PUSHlang (Performant, Universal, Safe and High level language) is a modern programming language which aims to be simple through abstract rules and to have high level features which are compiled to low level code with a fast (and incremental) compiler. 
The language is highly inspired by Rust but does not intend to be compatible in any way. I created this project to play around with programming language design. I wanted to build a language that works just the way I wish to and maybe other folks like this way too.
This repository contains the compiler implementation in c++ but for now also (indirectly) the language specification. Pushc is (will be) a cross-compiler, so this repo is only needed to bootstrap the language.

**WIP: The project is currently in a very early development stage. Everything may break frequently and the compiler is not ready to be used.**

## Current roadmap
- [x] Basic dataflow management
- [x] Input/Lexer
- [x] Prelude loading
- [x] Syntax parser
- [ ] Semantic checks
- [ ] Symbol management
- [ ] Type management
- [ ] Ownership & mutability management
- [ ] Static statement checks
- [ ] MIR generation
- [ ] LLVM backend
- [ ] Macros
- [ ] C-api/-abi
- [ ] Std-lib
- [ ] Project management

## Dependencies (all optional)
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
NOTE: Windows is not regularly tested any more, but may still work with some code modifications.

    mkdir build && cd build
    cmake .. -DCOTIRE_PATH=\path_to_cotire_repo\CMake\cotire.cmake -DVCPKG_SOURCE_DIR=C:\src\vcpkg -DVCPKG_TARGET_TRIPLET=x86-windows-static
Compile the .sln file.

## Usage
Currently, when you start *pushc* only the AST is generated and printed. 

After a successful build you can try it out by typing the following in the repo root directory:

    build/pushc/src/pushc Test/ast.push

Or get help with:

    build/pushc/src/pushc --help

## Tests
Most of the test cases only check the basic functionality, which is required for the functions/modules to be usable.

If you want to run the tests, build the project with Catch (so "-DTEST_WITH_CATCH" must be set to true) and run the test executable of the module from your shell(e. g. for "libpush" either build/libpush/src/tests/Debug/libpush_test.exe or build/libpush/src/tests/libpush_test). 

Its designated that the tests may print some warnings and errors. Just make sure the "All tests passed (x assertions in y test cases)" is printed at the end.
