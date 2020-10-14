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

#include "libpushc/stdafx.h"
#include "libpushc/CrateCtx.h"
#include "libpushc/Expression.h"
#include "libpushc/SymbolUtil.h"

bool SyntaxRule::matches_reversed( std::vector<AstNode> &rev_list ) {
    if ( rev_list.size() < expr_list.size() )
        return false;

    for ( int i = 0; i < expr_list.size(); i++ ) {
        if ( !rev_list[i].matches( expr_list[expr_list.size() - i - 1] ) ) {
            return false;
        }
    }
    return true;
}

bool SymbolIdentifier::ParamSig::operator==( const ParamSig &other ) const {
    if ( name != other.name || ref != other.ref || mut != other.mut )
        return false;
    if ( tmp_type_symbol != nullptr || other.tmp_type_symbol != nullptr )
        return false;
    if ( template_type_index != 0 && other.template_type_index != 0 &&
         template_type_index != other.template_type_index )
        return false;
    return type == 0 || other.type == 0 ||
           type == other.type; // type == 0 means the type of this is not known, so it's a candidate
}

void TypeSelection::add_requirement( const TypeSelection &type ) {
    if ( type.is_final() ) {
        if ( type.final_type == final_type )
            return;
        assert( final_type == 0 );
        type_requirements.push_back( type.final_type );
    } else {
        if ( type.type_requirements.size() == 1 && type.type_requirements.front() == final_type )
            return;
        assert( final_type == 0 );
        add_requirements( type.type_requirements );
    }
}

const std::vector<TypeId> TypeSelection::get_all_requirements( CrateCtx *c_ctx, FunctionImplId fn ) const {
    std::vector<TypeId> ret;

    // Collect all types in the type group
    for ( auto &var : type_group ) {
        auto &ts = c_ctx->functions[fn].vars[var].value_type;
        if ( ts.final_type != 0 ) {
            ret.push_back( ts.final_type );
        } else {
            ret.insert( ret.end(), ts.type_requirements.begin(), ts.type_requirements.end() );
        }
    }

    // Add the own type requirements
    if ( final_type == 0 ) {
        ret.insert( ret.begin(), type_requirements.begin(), type_requirements.end() );
    } else {
        ret.push_back( final_type );
    }

    return ret;
}

void TypeSelection::bind_variable( CrateCtx *c_ctx, FunctionImplId fn, MirVarId var, MirVarId own_id ) {
    auto tmp_tg = type_group;
    auto &other_tg = c_ctx->functions[fn].vars[var].value_type.type_group;

    type_group.insert( type_group.end(), other_tg.begin(), other_tg.end() );
    type_group.push_back( var );

    // Update the corresponding variable
    other_tg.push_back( own_id );
    other_tg.insert( other_tg.end(), tmp_tg.begin(), tmp_tg.end() );
}


const MirVarId &ParamContainerIterator::operator*() const {
    return container->params[index].second;
}

const size_t ParamContainer::INVALID_POSITION_VAL = SIZE_MAX;

bool ParamContainer::get_param_permutation( const std::vector<String> &names, std::vector<size_t> &out_permutation,
                                            bool skip_first ) {
    out_permutation.resize( names.size() );
    size_t next_unnamed = 0;
    size_t used_parmeter_count = 0;

    for ( size_t i = ( skip_first ? 1 : 0 ); i < names.size(); i++ ) {
        size_t candidate = SIZE_MAX;

        // Search for named match
        for ( size_t j = 0; j < params.size(); j++ ) {
            if ( params[j].first == names[i] ) {
                if ( candidate != SIZE_MAX )
                    return false; // multiple matches found
                candidate = j;
                used_parmeter_count++;
            }
        }

        // Search for unnamed match
        if ( candidate == SIZE_MAX ) {
            for ( size_t j = next_unnamed; j < params.size(); j++ ) {
                if ( params[j].first.empty() ) {
                    candidate = j;
                    next_unnamed = j + 1;
                    used_parmeter_count++;
                    break;
                }
            }
        }

        out_permutation[i] = candidate;
    }

    if ( used_parmeter_count < params.size() )
        return false; // some parameter was not used in the permutation. This can be caused by using a not-existing
                      // parameter name

    return true;
}

void ParamContainer::apply_param_permutation( const std::vector<String> &names ) {
    std::vector<size_t> parameter_permutation;
    if ( !get_param_permutation( names, parameter_permutation ) )
        LOG_ERR( "Could not apply parameter permutation (invalid)" );

    decltype( params ) new_params;
    new_params.reserve( params.size() );
    for ( size_t i = 0; i < params.size(); i++ )
        new_params.push_back( params[parameter_permutation[i]] );
    params = new_params;
}

CrateCtx::CrateCtx() {
    ast = make_shared<AstNode>();
    symbol_graph.resize( 2 );
    type_table.resize( LAST_FIX_TYPE + 1 );
    functions.resize( 1 );
}
