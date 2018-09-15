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
#include "libpush/util/String.h"

// Contains any possible value type for a pref
class PrefValue {
public:
    virtual ~PrefValue() {}

    template <typename T>
    const T get() const {
        return static_cast<const T *>( this )->value;
    }
};

// Stores a arbitrary type pref
template<typename T>
class AnySV : public PrefValue {
public:
    AnySV() { this->value = {}; }
    AnySV( const T &value ) { this->value = value; }
    T value;
};

using BoolSV = AnySV<bool>;
using IntSV = AnySV<i32>;
using SizeSV = AnySV<size_t>;
using FloatSV = AnySV<f64>;
using StringSV = AnySV<String>;


// Contains all possible preference
enum class PrefType {
    tab_size, // tab size in spaces
    max_errors,
    max_warnings,
    max_notifications,

    release_optimization,
    backend,
    platform,

    input_source,

    count
};

// Sets the default initial prefs
inline void set_default_preferences( std::map<PrefType, std::unique_ptr<PrefValue>> &prefs ) {
    prefs[PrefType::input_source] = std::make_unique<StringSV>( "file" );

}
