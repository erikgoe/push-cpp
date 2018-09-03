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

// Contains all possible settings
enum class SettingType {
    release_optimization,
    backend,
    platform,

    count
};

// Contains any possible value type for a setting
class SettingValue {
public:
    virtual ~SettingValue() {}

    template <typename T>
    const T get() const {
        return static_cast<const T *>( this )->value;
    }
};

// Stores a arbitrary type setting
template<typename T>
class AnySV : public SettingValue {
public:
    AnySV( const T &value ) { this->value = value; }
    T value;
};

using BoolSV = AnySV<bool>;
using IntSV = AnySV<i32>;
using FloatSV = AnySV<f64>;
using StringSV = AnySV<String>;
