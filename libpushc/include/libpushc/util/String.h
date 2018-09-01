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

class StringSlice;

// Better implementation of a string TODO add string conversions (utf-8-16-32, wstring) + to_lower/to_upper
class String : public std::string {
    StringSlice *last_slice = nullptr; // last returned slice

public:
	// Constants
    static const size_t TAB_WIDTH = 4;// used to translate tabs into spaces


    // Inherit base class constructors
    using basic_string::basic_string;

    String() {}
    String( const std::string &other ) { *static_cast<std::string *>( this ) = other; }

    // Replace all occurrences in the string
    static String replace_all( String &search_in, const String &search_for, const String &replace_with ) {
        size_t pos = 0;
        while ( pos != String::npos ) {
            pos = search_in.find( search_for, pos );
            if ( pos != String::npos ) {
                search_in.replace( pos, search_for.size(), replace_with );
                pos += replace_with.size();
            }
        }
        return search_in;
    }
    // Replace all occurrences in this string
    String replace_all( const String &search_for, const String &replace_with ) {
        return replace_all( *this, search_for, replace_with );
    }

    // Like substr() but returns a StringSlice instead of a substr. Earlier results of this method are invalidated.
    const StringSlice &slice( size_t pos, size_t size = String::npos );

    // Like substr() but returns a StringSlice instead of a substr. Earlier results of this method are still valid.
    StringSlice slice( size_t pos, size_t size = String::npos ) const;

    // Returns the length of the string in code points
    size_t length_cp() const;

    // Returns the lenght of the string in grapheme-blocks. This method takes only simple characters into account.
    size_t length_grapheme() const;
};

// Temporary slice of a string. Operations on the original string may INVALIDATE this slice.
class StringSlice {
    const char *m_ref;
    size_t m_size;

public:
    StringSlice( const String &str, size_t pos, size_t size ) { reslice( str, pos, size ); }

    // Returns the size of the slice in bytes
    size_t size() const { return m_size; }

    // Returns the length of the string in code points
    size_t length_cp() const;

    // Returns the lenght of the string in grapheme-blocks. This method takes only simple characters into account.
    size_t length_grapheme() const;

    // Return a not null-terminated pointer to the slice string.
    const char *c_str() const { return m_ref; }

    // This method does not check the size of the original string!
    StringSlice &resize( size_t size ) {
        m_size = size;
        return *this;
    }

    // Set a new source string, offset and size
    StringSlice &reslice( const String &str, size_t pos, size_t size ) {
        if ( str.size() <= pos ) {
            LOG_ERR( "String is to small for this slice [" + to_string( pos ) + ".." + to_string( pos + size ) +
                     "] in " + to_string( str.size() ) + " string." );
            m_ref = str.c_str();
            m_size = 0;
        } else if ( size == String::npos || str.size() < pos + size ) {
            m_ref = str.c_str() + pos;
            m_size = str.size() - pos;
        } else {
            m_ref = str.c_str() + pos;
            m_size = size;
        }
        return *this;
    }

    template <typename T>
    bool operator==( const T &other ) const {
        if ( other.size() != m_size )
            return false;

        auto o_ref = other.c_str();
        for ( size_t i = 0; i < m_size; i++ ) {
            if ( m_ref[i] != o_ref[i] )
                return false;
        }
        return true;
    }
};
