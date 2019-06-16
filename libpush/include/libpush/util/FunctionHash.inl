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

#include "libpush/UnitCtx.h"

template <typename FuncT, typename... Args>
FunctionSignature FunctionSignature::create( FuncT fn, UnitCtx &ctx, const Args &... args ) {
    FunctionSignature fs;
    std::stringstream ss;

    ss << typeid( fn ).hash_code() << '|' << ctx.id;
    create_helper( ss, args... );

    fs.data = ss.str();
    return fs;
}
