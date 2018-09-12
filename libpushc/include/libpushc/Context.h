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
#include "libpushc/Preferences.h"

// Stores the current state and the prefs for a compilation pass (or a incremental build)
class Context {
    Mutex mtx; // used for all async manipulations

    std::map<PrefType, std::unique_ptr<PrefValue>> prefs; // store all the prefs

public:
    std::atomic_size_t error_count;
    std::atomic_size_t warning_count;
    std::atomic_size_t notification_count;
    std::atomic_size_t max_allowed_errors;
    std::atomic_size_t max_allowed_warnings;
    std::atomic_size_t max_allowed_notifications;


    Context() {
        update_global_prefs();
        error_count = 0;
        warning_count = 0;
        notification_count = 0;
    }

    // Returns a previously saved pref. If it was not saved, returns the default value for the preference type.
    template <typename ValT>
    auto get_pref( const PrefType &pref_type ) -> const decltype( ValT::value ) {
        Lock lock( mtx );
        if ( prefs.find( pref_type ) == prefs.end() ) {
            prefs[pref_type] = std::make_unique<ValT>();
            LOG_WARN( "Using preference (" + to_string( static_cast<u32>( pref_type ) ) +
                      ") which was not set before." );
        }
        return prefs[pref_type]->get<ValT>().value;
    }
    // Returns a pref value or if it doesn't exist, saves \param default_value for the pref and returns it.
    template <typename ValT>
    auto get_pref_or_set( const PrefType &pref_type, const decltype( ValT::value ) &default_value ) -> const
        decltype( ValT::value ) {
        Lock lock( mtx );
        if ( prefs.find( pref_type ) == prefs.end() ) {
            prefs[pref_type] = std::make_unique<ValT>( default_value );
            return default_value;
        }
        return prefs[pref_type]->get<ValT>().value;
    }
    // Stores a new pref or overwrites an existing one.
    template <typename PrefT, typename ValT>
    void set_pref( const PrefType &pref_type, const ValT &value ) {
        Lock lock( mtx );
        prefs[pref_type] = std::make_unique<PrefT>( value );
    }

    // This method must be called to update some specific prefs like tab length
    void update_global_prefs();

    // Returns the name of the triplet element corresponding to the value. Returns and empty string if this value does
    // not exist
    static String get_triplet_elem_name( const String &value );
    // Returns the index-position of a triplet name
    static size_t get_triplet_pos( const String &name );

    friend class QueryMgr;
};
