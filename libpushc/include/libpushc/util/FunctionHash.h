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
#include "libpushc/Base.h"
#include "libpushc/util/String.h"

template <typename T>
struct HashSerializer {
public:
    void operator()( std::stringstream &ss, const T &obj ) { ss << '|' << obj; }
};
template <typename Entry>
struct HashSerializer<std::list<Entry>> {
public:
    void operator()( std::stringstream &ss, const std::list<Entry> &obj ) {
        ss << '{';
        for ( auto &e : obj )
            HashSerializer<Entry>{}( ss, e );
        ss << '}';
    }
};
template <typename Entry>
struct HashSerializer<std::vector<Entry>> {
public:
    void operator()( std::stringstream &ss, const std::vector<Entry> &obj ) {
        ss << '{';
        for ( auto &e : obj )
            HashSerializer<Entry>{}( ss, e );
        ss << '}';
    }
};

// Declaration
class FunctionSignature;
namespace std {
template <>
struct hash<FunctionSignature>;
}

// Makes a function (including its parameter values) identifiable
class FunctionSignature {
    String data;

    // General serializing
    template <typename T>
    static void create_helper( std::stringstream &ss, const T &first ) {
        HashSerializer<T>{}( ss, first );
    }
    template <typename T, typename... Args>
    static void create_helper( std::stringstream &ss, const T &first, const Args &... other ) {
        HashSerializer<T>{}( ss, first );
        create_helper( ss, other... );
    }

public:
    template <typename FuncT, typename... Args>
    static FunctionSignature create( FuncT fn, UnitCtx &ctx, const Args &... args ) {
        FunctionSignature fs;
        std::stringstream ss;

        ss << typeid( fn ).hash_code() << '|' << ctx.id;
        create_helper( ss, args... );

        fs.data = ss.str();
        return fs;
    }

    bool operator==( const FunctionSignature &other ) const { return other.data == data; }

    friend std::hash<FunctionSignature>;
};

namespace std {
// Creates a hash from a function with all is parameters
template <>
struct hash<FunctionSignature> {
public:
    size_t operator()( const FunctionSignature &func ) const noexcept { return hash<std::string>{}( func.data ); }
};
} // namespace std
