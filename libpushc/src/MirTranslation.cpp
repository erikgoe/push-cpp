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
#include "libpushc/MirTranslation.h"
#include "libpushc/AstParser.h"
#include "libpushc/Expression.h"
#include "libpushc/SymbolUtil.h"


MirEntry &create_operation( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, sptr<Expr> original_expr,
                            MirEntry::Type type, std::vector<MirVarId> parameters ) {
    auto return_var = create_variable( c_ctx, w_ctx, function, "" );

    // Check if parameters are valid
    for ( auto &param : parameters ) {
        bool valid = false;
        for ( auto itr = c_ctx.curr_living_vars.rbegin(); itr != c_ctx.curr_living_vars.rend(); itr++ ) {
            if ( std::find( itr->begin(), itr->end(), param ) != itr->end() ) {
                // Variable is accessible
                valid = true;
                break;
            }
        }
        if ( !valid ) {
            // TODO detect which variable in the operation and add a note where the variable ended
            w_ctx.print_msg<MessageType::err_var_not_living>( MessageInfo( original_expr, 0, FmtStr::Color::Red ) );
        }
    }

    auto operation = MirEntry{ original_expr, type, return_var, parameters };
    c_ctx.functions[function].ops.push_back( operation );
    return c_ctx.functions[function].ops.back();
}

MirVarId create_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, const String &name ) {
    MirVarId id = c_ctx.functions[function].vars.size();
    c_ctx.functions[function].vars.emplace_back();
    c_ctx.functions[function].vars[id].name = name;
    c_ctx.curr_living_vars.back().push_back( id );
    if ( !name.empty() ) {
        c_ctx.curr_name_mapping.back()[name] = id;
    }
    return id;
}

void drop_variable( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId function, sptr<Expr> original_expr,
                    MirVarId variable ) {
    auto &var = c_ctx.functions[function].vars[variable];

    // Create drop operation
    if ( var.type == MirVariable::Type::value || var.type == MirVariable::Type::rvalue ) {
        auto op = create_operation( c_ctx, w_ctx, function, original_expr, MirEntry::Type::call, { variable } );
        op.symbol = c_ctx.type_table[c_ctx.drop_fn].symbol;
    }

    // Remove from living variables
    for ( auto itr = c_ctx.curr_living_vars.rbegin(); itr != c_ctx.curr_living_vars.rend(); itr++ ) {
        if ( auto var_itr = std::find( itr->begin(), itr->end(), variable ); var_itr != itr->end() ) {
            itr->erase( var_itr );
            break;
        }
    }
}

// Creates a function from a FuncExpr specified by @param symbolId
void generate_mir_function_impl( CrateCtx &c_ctx, Worker &w_ctx, SymbolId symbolId ) {
    auto &symbol = c_ctx.symbol_graph[symbolId];
    auto &type = c_ctx.type_table[symbol.value];
    auto expr = std::dynamic_pointer_cast<FuncExpr>( symbol.original_expr.front() );

    // Check if only one definition exists (must be at least one)
    if ( symbol.original_expr.size() > 1 ) {
        std::vector<MessageInfo> notes;
        for ( auto &original : symbol.original_expr ) {
            notes.push_back( MessageInfo( original, 1 ) );
        }
        notes.erase( notes.begin() );
        w_ctx.print_msg<MessageType::err_multiple_fn_definitions>(
            MessageInfo( symbol.original_expr.front(), 0, FmtStr::Color::Red ), notes );
        return;
    }

    c_ctx.curr_living_vars.clear();
    c_ctx.curr_living_vars.emplace_back();
    c_ctx.curr_name_mapping.clear();
    c_ctx.curr_name_mapping.emplace_back();

    // Create the function
    FunctionImplId func_id = c_ctx.functions.size();
    c_ctx.functions.emplace_back();
    FunctionImpl &function = c_ctx.functions.back();
    create_variable( c_ctx, w_ctx, func_id, "" ); // unit return value

    // Parse parameters
    auto paren_expr = std::dynamic_pointer_cast<ParenthesisExpr>( expr->parameters );
    for ( auto &entry : paren_expr->get_list() ) {
        auto symbol = std::dynamic_pointer_cast<SymbolExpr>( entry );
        sptr<Expr> type = nullptr;
        if ( symbol == nullptr ) {
            auto typed = std::dynamic_pointer_cast<TypedExpr>( entry );
            symbol = std::dynamic_pointer_cast<SymbolExpr>( typed->symbol );
            type = typed->type;
        }

        MirVarId id = create_variable( c_ctx, w_ctx, func_id );
        String name = get_full_symbol_name( c_ctx, symbol->get_symbol_id() );
        function.vars[id].name = name;
        c_ctx.curr_name_mapping.back()[name] = id;
        c_ctx.curr_living_vars.back().push_back( id );
        if ( type != nullptr ) {
            if ( auto type_symbol = std::dynamic_pointer_cast<SymbolExpr>( type ); type_symbol != nullptr ) {
                function.vars[id].value_type = c_ctx.symbol_graph[type_symbol->get_symbol_id()].value;
            }
            // TODO handle references
        }
    }

    // Parse body
    MirVarId ret = expr->parse_mir( c_ctx, w_ctx, func_id );
    if ( ret != 0 ) {
        create_operation( c_ctx, w_ctx, func_id, expr, MirEntry::Type::ret, { ret } );
    }
}

void get_mir( JobsBuilder &jb, UnitCtx &parent_ctx ) {
    jb.add_job<void>( []( Worker &w_ctx ) {
        auto c_ctx = w_ctx.do_query( get_ast )->jobs.back()->to<sptr<CrateCtx>>();

        // Generate the Mir function bodies
        for ( size_t i = 0; i < c_ctx->symbol_graph.size(); i++ ) {
            if ( c_ctx->symbol_graph[i].type == c_ctx->fn_type ) {
                generate_mir_function_impl( *c_ctx, w_ctx, i );
            }
        }
    } );
}
