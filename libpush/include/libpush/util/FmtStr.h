// Copyright 2020 Erik Götzfried
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

// String which consists of multiple formatted parts
class FmtStr {
public:
    enum class Color {
        Black,
        Red,
        Green,
        Blue,
        Yellow,
        BoldBlack,
        BoldRed,
        BoldGreen,
        BoldBlue,
        BoldYellow,

        count
    };
    struct Piece {
        String text;
        Color color = Color::Black;

        Piece( const String &text, Color color = Color::Black ) {
            this->text = text;
            this->color = color;
        }

        FmtStr operator+( const Piece &other ) { return FmtStr( *this ) + FmtStr( other ); }
        bool operator==( const Piece &other ) const { return text == other.text && color == other.color; }
    };

private:
    std::list<Piece> pieces;

public:
    FmtStr() {}
    FmtStr( const Piece &piece ) { pieces.push_back( piece ); }

    FmtStr &operator+=( const FmtStr &other ) {
        pieces.insert( pieces.end(), other.pieces.begin(), other.pieces.end() );
        return *this;
    }
    FmtStr operator+( const FmtStr &other ) const {
        auto tmp = *this;
        return tmp += other;
    }
    FmtStr &operator+=( const Piece &other ) {
        pieces.insert( pieces.end(), other );
        return *this;
    }
    FmtStr operator+( const Piece &other ) const {
        auto tmp = *this;
        return tmp += other;
    }

    bool operator==( const FmtStr &other ) const { return pieces == other.pieces; }

    // Returns the next formatted piece from the string. First check if is empty()
    Piece consume() {
        auto tmp = pieces.front();
        pieces.pop_front();
        return tmp;
    }

    bool empty() { return pieces.empty(); }

    size_t size() { return pieces.size(); }

    std::list<Piece> get_raw() const { return pieces; }
};
