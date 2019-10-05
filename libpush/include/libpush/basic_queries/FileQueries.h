// Copyright 2019 Erik Götzfried
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
#include "libpush/stdafx.h"
#include "libpush/input/FileInput.h"
#include "libpush/UnitCtx.h"

// NOT A QUERY! Returns a source input defined by the current prefs
sptr<SourceInput> get_source_input( const String file, Worker &w_ctx );

// NOT A QUERY! Returns the path to the installed std-library path
sptr<String> get_std_dir();

// Extracts source lines from any file
void get_source_lines( const String file, size_t line_begin, size_t line_end, JobsBuilder &jb, UnitCtx &ctx );
