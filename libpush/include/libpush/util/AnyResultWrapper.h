// Copyright 2018 Erik Götzfried
// Licensed under the Apache License, Version 2.0( the "License" );
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include "libpush/Base.h"

// Wraps any function call and its result type for later use (including void)
template <typename Rty>
struct AnyResultWrapper {
    Rty result;
    template <typename U, typename... Args>
    void wrap( U &fn, Args &... args ) {
        result = fn( args... );
    }
    const Rty &get() { return result; }
};
// Fake implementation for void
template <>
struct AnyResultWrapper<void> {
    template <typename U, typename... Args>
    void wrap( U &fn, Args &... args ) {
        fn( args... );
    }
    void get() {}
};
