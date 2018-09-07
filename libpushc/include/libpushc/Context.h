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
#include "libpushc/Settings.h"

// Stores the current state and the settings for a compilation pass (or a incremental build)
class Context {
    Mutex mtx; // used for all async manipulations

    std::map<SettingType, std::unique_ptr<SettingValue>> settings; // store all the settings

public:
    Context() { update_global_settings(); }

    // Returns a previously saved setting. If it was not saved, returns the default value for the Setting type.
    template <typename ValT>
    auto get_setting( const SettingType &setting_type ) -> decltype( auto ) {
        Lock lock( mtx );
        if ( settings.find( setting_type ) == settings.end() )
            settings[setting_type] = std::make_unique<ValT>();
        return settings[setting_type]->get<ValT>().value;
    }
    // Returns a setting value or if it doesn't exist, saves \param default_value for the setting and returns it.
    template <typename ValT>
    auto get_setting_or_set( const SettingType &setting_type,
                             const decltype( ValT::value ) &default_value ) -> const decltype( ValT::value )& {
        Lock lock( mtx );
        if ( settings.find( setting_type ) == settings.end() ) {
            settings[setting_type] = std::make_unique<ValT>( default_value );
            return default_value;
        }
        return settings[setting_type]->get<ValT>().value;
    }
    // Stores a new setting or overwrites an existing one.
    template <typename SettingT, typename ValT>
    void set_setting( const SettingType &setting_type, const ValT &value ) {
        Lock lock( mtx );
        settings[setting_type] = std::make_unique<SettingT>( value );
    }

    // This method must be called to update some specific settings like tab length
    void update_global_settings();

    friend class QueryMgr;
};
