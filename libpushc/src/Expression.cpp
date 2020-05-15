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
#include "libpushc/Expression.h"
#include "libpushc/SymbolUtil.h"
#include "libpushc/MirTranslation.h"

// Defined here, because libpush does not define the Expr symbol
MessageInfo::MessageInfo( const AstNode &expr, u32 message_idx, FmtStr::Color color )
        : MessageInfo( expr.pos_info, message_idx, color ) {}



// Used with alias statements. Returns the list of the subsitutions rules from the alias expr
std::vector<SymbolSubstitution> get_substitutions( CrateCtx &c_ctx, Worker &w_ctx, AstNode &expr ) {
    std::vector<SymbolSubstitution> result;
    if ( expr.children[0].has_prop( ExprProperty::assignment ) ) {
        result.push_back( { expr.children[0].named[AstChild::left_expr].get_symbol_chain( c_ctx, w_ctx ),
                            expr.children[0].named[AstChild::right_expr].get_symbol_chain( c_ctx, w_ctx ) } );
    } else {
        auto chain = expr.children[0].get_symbol_chain( c_ctx, w_ctx );
        result.push_back( { make_shared<std::vector<SymbolIdentifier>>( 1, chain->back() ), chain } );
    }
    return result;
}

void AstNode::generate_new_props() {
    switch ( type ) {
    case ExprType::token:
        props.insert( ExprProperty::temporary );
        break;

    case ExprType::decl_scope:
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::braces );
        props.insert( ExprProperty::decl_parent );
        break;
    case ExprType::imp_scope:
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::braces );
        props.insert( ExprProperty::anonymous_scope );
        break;
    case ExprType::single_completed:
        props.insert( ExprProperty::shallow );
        props.insert( ExprProperty::completed );
        break;
    case ExprType::block:
        props.insert( ExprProperty::shallow );
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::braces );
        break;
    case ExprType::set:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::braces );
        break;
    case ExprType::unit:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::parenthesis );
        props.insert( ExprProperty::symbol_like );
        break;
    case ExprType::term:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::parenthesis );
        break;
    case ExprType::tuple:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::parenthesis );
        props.insert( ExprProperty::symbol_like );
        break;
    case ExprType::array_specifier:
        props.insert( ExprProperty::shallow );
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::brackets );
        break;
    case ExprType::array_list:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::brackets );
        break;
    case ExprType::comma_list:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::literal );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::numeric_literal:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::literal );
        break;
    case ExprType::string_literal:
        props.insert( ExprProperty::shallow );
        props.insert( ExprProperty::operand );
        break;

    case ExprType::atomic_symbol:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::symbol );
        props.insert( ExprProperty::symbol_like );
        break;
    case ExprType::func_head:
        props.insert( ExprProperty::shallow );
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::func:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::named_scope );
        break;
    case ExprType::func_decl:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::named_scope );
        break;
    case ExprType::func_call:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        break;

    case ExprType::op:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::simple_bind:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::alias_bind:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        break;

    case ExprType::if_cond:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::anonymous_scope );
        break;
    case ExprType::if_else:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::anonymous_scope );
        break;
    case ExprType::pre_loop:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::anonymous_scope );
        break;
    case ExprType::post_loop:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::anonymous_scope );
        break;
    case ExprType::inf_loop:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::anonymous_scope );
        break;
    case ExprType::itr_loop:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::anonymous_scope );
        break;
    case ExprType::match:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::anonymous_scope );
        break;

    case ExprType::self:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::symbol_like );
        break;
    case ExprType::self_type:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::symbol_like );
        break;
    case ExprType::struct_initializer:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::anonymous_scope );
        break;

    case ExprType::structure:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::decl_parent );
        props.insert( ExprProperty::named_scope );
        break;
    case ExprType::trait:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::decl_parent );
        props.insert( ExprProperty::named_scope );
        break;
    case ExprType::implementation:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::decl_parent );
        props.insert( ExprProperty::named_scope );
        break;

    case ExprType::member_access:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::scope_access:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::symbol );
        props.insert( ExprProperty::symbol_like );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::array_access:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::brackets );
        props.insert( ExprProperty::separable );
        break;

    case ExprType::range:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::reference:
        props.insert( ExprProperty::shallow );
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::symbol_like );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::mutable_attr:
        props.insert( ExprProperty::shallow );
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::symbol_like );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::typeof_op:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::typed_op:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::symbol_like );
        props.insert( ExprProperty::separable );
        break;

    case ExprType::module:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::completed );
        props.insert( ExprProperty::separable );
        props.insert( ExprProperty::decl_parent );
        props.insert( ExprProperty::named_scope );
        break;
    case ExprType::declaration:
        props.insert( ExprProperty::shallow );
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::public_attr:
        props.insert( ExprProperty::shallow );
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::static_statement:
        props.insert( ExprProperty::shallow );
        props.insert( ExprProperty::anonymous_scope );
        break;
    case ExprType::compiler_annotation:
        props.insert( ExprProperty::shallow );
        props.insert( ExprProperty::completed );
        break;
    case ExprType::macro_call:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::unsafe:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::separable );
        break;
    case ExprType::template_postfix:
        props.insert( ExprProperty::operand );
        props.insert( ExprProperty::symbol );
        props.insert( ExprProperty::symbol_like );
        props.insert( ExprProperty::separable );
        break;

    default:
        break;
    }
}

void AstNode::split_prepend_recursively( std::vector<AstNode> &rev_list, std::vector<AstNode> &stst_set, u32 prec,
                                         bool ltr, u8 rule_length ) {
    stst_set.insert( stst_set.end(), static_statements.begin(), static_statements.end() );
    for ( auto expr_itr = original_list.rbegin(); expr_itr != original_list.rend(); expr_itr++ ) {
        auto s_expr = *expr_itr;
        if ( rev_list.size() < rule_length && s_expr.has_prop( ExprProperty::separable ) &&
             ( prec < s_expr.precedence || ( !ltr && prec == s_expr.precedence ) ) ) {
            s_expr.split_prepend_recursively( rev_list, stst_set, prec, ltr, rule_length );
        } else {
            rev_list.push_back( *expr_itr );
        }
    }
}

bool AstNode::visit( CrateCtx &c_ctx, Worker &w_ctx, VisitorPassType vpt, AstNode &parent ) {
    // Visit this
    if ( vpt == VisitorPassType::BASIC_SEMANTIC_CHECK ) {
        if ( !basic_semantic_check( c_ctx, w_ctx ) )
            return false;
    } else if ( vpt == VisitorPassType::FIRST_TRANSFORMATION ) {
        if ( !first_transformation( c_ctx, w_ctx, parent ) )
            return false;
    } else if ( vpt == VisitorPassType::SYMBOL_DISCOVERY ) {
        if ( !symbol_discovery( c_ctx, w_ctx ) )
            return false;
    }

    bool result = true;
    for ( auto &ss : static_statements ) {
        if ( !ss.visit( c_ctx, w_ctx, vpt, *this ) )
            result = false;
    }
    for ( auto &a : annotations ) {
        if ( !a.visit( c_ctx, w_ctx, vpt, *this ) )
            result = false;
    }

    // Visit sub-elements
    for ( auto &node : named ) {
        if ( !node.second.visit( c_ctx, w_ctx, vpt, *this ) )
            result = false;
    }
    for ( auto &node : children ) {
        if ( !node.visit( c_ctx, w_ctx, vpt, *this ) )
            result = false;
    }

    // Post-visit
    if ( result ) {
        if ( vpt == VisitorPassType::SYMBOL_DISCOVERY ) {
            if ( !post_symbol_discovery( c_ctx, w_ctx ) )
                return false;
        }
    }

    // Reset context
    c_ctx.expect_operand = true;

    return result;
}

sptr<std::vector<SymbolIdentifier>> AstNode::get_symbol_chain( CrateCtx &c_ctx, Worker &w_ctx ) {
    if ( !has_prop( ExprProperty::symbol_like ) ) {
        LOG_ERR( "Tried to get symbol chain from non-symbol" );
        return make_shared<std::vector<SymbolIdentifier>>();
    }

    if ( type == ExprType::atomic_symbol ) {
        return make_shared<std::vector<SymbolIdentifier>>( 1, SymbolIdentifier{ symbol_name } );
    } else if ( type == ExprType::scope_access ) {
        auto base = named[AstChild::base].get_symbol_chain( c_ctx, w_ctx );
        auto member = named[AstChild::member].get_symbol_chain( c_ctx, w_ctx );
        base->insert( base->end(), member->begin(), member->end() );
        return base;
    } else if ( type == ExprType::template_postfix ) {
        std::vector<std::pair<TypeId, ConstValue>> template_values;
        for ( auto &c : children ) {
            TypeId type = c_ctx.type_type;
            if ( c.type == ExprType::typed_op ) {
                auto types = find_local_symbol_by_identifier_chain(
                    c_ctx, w_ctx, c.named[AstChild::right_expr].get_symbol_chain( c_ctx, w_ctx ) );
                if ( !expect_exactly_one_symbol( c_ctx, w_ctx, types, c ) )
                    return nullptr;
                type = types.front();
            }

            template_values.push_back( std::make_pair( type, ConstValue() ) ); // TODO insert default values here
        }

        auto chain = named[AstChild::symbol].get_symbol_chain( c_ctx, w_ctx );
        chain->back().template_values = template_values;
        return chain;
    } else if ( type == ExprType::unit ) {
        return make_shared<std::vector<SymbolIdentifier>>( 1, SymbolIdentifier{ "()" } );
    } else if ( type == ExprType::tuple ) {
        std::vector<std::pair<TypeId, ConstValue>> template_values;
        for ( auto &c : children ) {
            auto types = find_local_symbol_by_identifier_chain( c_ctx, w_ctx, c.get_symbol_chain( c_ctx, w_ctx ) );
            if ( !expect_exactly_one_symbol( c_ctx, w_ctx, types, c ) )
                return nullptr;
            template_values.push_back(
                std::make_pair( c_ctx.type_type, ConstValue( c_ctx.symbol_graph[types.front()].value ) ) );
        }

        SymbolId new_symbol =
            instantiate_template( c_ctx, w_ctx, c_ctx.type_table[c_ctx.tuple_type].symbol, template_values );
        return get_symbol_chain_from_symbol( c_ctx, w_ctx, new_symbol );
    }

    LOG_ERR( "Could not parse symbol chain from expr" );
    return nullptr;
}

bool AstNode::basic_semantic_check( CrateCtx &c_ctx, Worker &w_ctx ) {
    // Checks based on properties
    if ( has_prop( ExprProperty::temporary ) ) {
        w_ctx.print_msg<MessageType::err_orphan_token>( MessageInfo( token, 0, FmtStr::Color::Red ) );
        return false;
    }

    // Checks based on type
    if ( type == ExprType::decl_scope ) {
        // All expr must be completed
        for ( auto expr = children.begin(); expr != children.end(); expr++ ) {
            if ( !expr->has_prop( ExprProperty::completed ) ) {
                w_ctx.print_msg<MessageType::err_unfinished_expr>( MessageInfo( *expr, 0, FmtStr::Color::Red ) );
                return false;
            }
        }
    } else if ( type == ExprType::block || type == ExprType::imp_scope || type == ExprType::array_specifier ) {
        // All expr but the last must be completed
        if ( !children.empty() ) {
            for ( auto expr = children.begin(); expr != children.end() - 1; expr++ ) {
                if ( !expr->has_prop( ExprProperty::completed ) ) {
                    w_ctx.print_msg<MessageType::err_unfinished_expr>( MessageInfo( *expr, 0, FmtStr::Color::Red ) );
                    return false;
                }
            }
        }
    } else if ( type == ExprType::single_completed ) {
        // double semicolon
        if ( children.empty() || children.front().has_prop( ExprProperty::completed ) ) {
            w_ctx.print_msg<MessageType::err_semicolon_without_meaning>( MessageInfo( *this, 0, FmtStr::Color::Red ) );
            return false;
        }
    } else if ( type == ExprType::simple_bind ) {
        auto &left = children.front().named[AstChild::left_expr];
        if ( left.type == ExprType::typed_op ) {
            if ( !left.named[AstChild::left_expr].has_prop( ExprProperty::symbol ) ) {
                w_ctx.print_msg<MessageType::err_expected_symbol>(
                    MessageInfo( left.named[AstChild::left_expr], 0, FmtStr::Color::Red ) );
                return false;
            }
            if ( !left.named[AstChild::right_expr].has_prop( ExprProperty::symbol_like ) ) {
                w_ctx.print_msg<MessageType::err_expected_symbol>(
                    MessageInfo( left.named[AstChild::right_expr], 0, FmtStr::Color::Red ) );
                return false;
            }
        } else if ( !left.has_prop( ExprProperty::symbol ) ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( left, 0, FmtStr::Color::Red ) );
            return false;
        }
    } else if ( type == ExprType::alias_bind ) {
        auto &left = children.front().named[AstChild::left_expr];
        auto &right = children.front().named[AstChild::right_expr];
        if ( !left.has_prop( ExprProperty::symbol ) ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( left, 0, FmtStr::Color::Red ) );
            return false;
        }
        if ( !right.has_prop( ExprProperty::symbol ) ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( right, 0, FmtStr::Color::Red ) );
            return false;
        }
    } else if ( type == ExprType::match ) {
        // check match branches
        if ( children.empty() || children.front().children.empty() ) {
            w_ctx.print_msg<MessageType::err_expected_comma_list>(
                MessageInfo( ( children.empty() ? *this : children.front() ), 0, FmtStr::Color::Red ) );
            return false;
        }
        auto *list = &children.front().children;
        if ( !list->empty() && list->front().type == ExprType::comma_list ) {
            list = &list->front().children;
        }
        for ( auto &b : *list ) {
            if ( !b.has_prop( ExprProperty::implication ) ) {
                w_ctx.print_msg<MessageType::err_expected_implication>( MessageInfo( b, 0, FmtStr::Color::Red ) );
                return false;
            }
        }
    } else if ( type == ExprType::structure || type == ExprType::trait || type == ExprType::implementation ) {
        // Only empty blocks are allowed (other empty children indicate an error)
        if ( children.empty() || ( children.front().children.empty() && children.front().type != ExprType::block ) ) {
            w_ctx.print_msg<MessageType::err_expected_comma_list>(
                MessageInfo( ( children.empty() ? *this : children.front() ), 0, FmtStr::Color::Red ) );
            return false;
        }

        auto &list = children.front().children;
        if ( list.size() == 1 && list.front().type == ExprType::comma_list )
            list = list.front().children; // if comma list
        for ( auto &entry : list ) {
            if ( entry.type == ExprType::public_attr ) {
                // public_attr checks already whether it is typed, symbol or func_head, but still this is needed
                if ( type == ExprType::structure && ( entry.children.front().type == ExprType::func ||
                                                      entry.children.front().type == ExprType::func_head ) ) {
                    w_ctx.print_msg<MessageType::err_method_not_allowed>( MessageInfo( entry, 0, FmtStr::Color::Red ) );
                    return false;
                } else if ( type == ExprType::trait && ( entry.children.front().type != ExprType::func &&
                                                         entry.children.front().type != ExprType::func_head ) ) {
                    w_ctx.print_msg<MessageType::err_expected_function_head>(
                        MessageInfo( entry, 0, FmtStr::Color::Red ) );
                    return false;
                } else if ( type == ExprType::implementation && entry.children.front().type != ExprType::func ) {
                    w_ctx.print_msg<MessageType::err_expected_function_definition>(
                        MessageInfo( entry, 0, FmtStr::Color::Red ) );
                    return false;
                }
            } else {
                if ( type == ExprType::structure ) {
                    if ( ( entry.type == ExprType::func || entry.type == ExprType::func_head ) ) {
                        w_ctx.print_msg<MessageType::err_method_not_allowed>(
                            MessageInfo( entry, 0, FmtStr::Color::Red ) );
                        return false;
                    } else if ( entry.type == ExprType::typed_op ) {
                        if ( !entry.named[AstChild::left_expr].has_prop( ExprProperty::symbol ) ) {
                            w_ctx.print_msg<MessageType::err_expected_symbol>(
                                MessageInfo( entry.named[AstChild::left_expr], 0, FmtStr::Color::Red ) );
                            return false;
                        }
                        if ( !entry.named[AstChild::right_expr].has_prop( ExprProperty::symbol_like ) ) {
                            w_ctx.print_msg<MessageType::err_expected_symbol>(
                                MessageInfo( entry.named[AstChild::right_expr], 0, FmtStr::Color::Red ) );
                            return false;
                        }
                    } else if ( !entry.has_prop( ExprProperty::symbol ) ) {
                        w_ctx.print_msg<MessageType::err_expected_symbol>(
                            MessageInfo( entry, 0, FmtStr::Color::Red ) );
                        return false;
                    }
                } else if ( type == ExprType::trait ) {
                    if ( entry.type != ExprType::func && entry.type != ExprType::func_head ) {
                        w_ctx.print_msg<MessageType::err_expected_function_head>(
                            MessageInfo( entry, 0, FmtStr::Color::Red ) );
                        return false;
                    }
                } else if ( type == ExprType::implementation ) {
                    if ( entry.type != ExprType::func ) {
                        w_ctx.print_msg<MessageType::err_expected_function_definition>(
                            MessageInfo( entry, 0, FmtStr::Color::Red ) );
                        return false;
                    }
                }
            }
        }
    } else if ( type == ExprType::reference ) {
        // may not contain another reference or mutable
        if ( named[AstChild::symbol_like].type == ExprType::reference ) {
            w_ctx.print_msg<MessageType::err_double_ref_op>(
                MessageInfo( named[AstChild::symbol_like], 0, FmtStr::Color::Red ) );
        }
        if ( named[AstChild::symbol_like].type == ExprType::mutable_attr ) {
            w_ctx.print_msg<MessageType::err_mut_ref_wrong_order>(
                MessageInfo( named[AstChild::symbol_like], 0, FmtStr::Color::Red ) );
        }
    } else if ( type == ExprType::mutable_attr ) {
        // may not contain another mutable
        if ( named[AstChild::symbol_like].type == ExprType::mutable_attr ) {
            w_ctx.print_msg<MessageType::err_double_mut_keyword>(
                MessageInfo( named[AstChild::symbol_like], 0, FmtStr::Color::Red ) );
        }
    } else if ( type == ExprType::public_attr ) {
        if ( children.front().type == ExprType::typed_op ) {
            // check only requirements in a struct (not trait or others)
            auto &left = children.front().named[AstChild::left_expr];
            auto &right = children.front().named[AstChild::right_expr];
            if ( !left.has_prop( ExprProperty::symbol ) ) {
                w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( left, 0, FmtStr::Color::Red ) );
                return false;
            }
            if ( !right.has_prop( ExprProperty::symbol ) ) {
                w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( right, 0, FmtStr::Color::Red ) );
                return false;
            }
        } else if ( !children.front().has_prop( ExprProperty::symbol ) && children.front().type != ExprType::func &&
                    children.front().type != ExprType::func_head && children.front().type != ExprType::structure &&
                    children.front().type != ExprType::trait && children.front().type != ExprType::implementation &&
                    children.front().type != ExprType::module ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( *this, 0, FmtStr::Color::Red ) );
            return false;
        }
    }

    // Checks based on common entries
    if ( named.find( AstChild::symbol ) != named.end() ) {
        // Must be a symbol, or for functions an array specifier (lambas)
        if ( !named[AstChild::symbol].has_prop( ExprProperty::symbol ) && type != ExprType::func_head &&
             ( type != ExprType::func || named[AstChild::symbol].type != ExprType::array_specifier ) ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>(
                MessageInfo( named[AstChild::symbol], 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    if ( named.find( AstChild::symbol_like ) != named.end() ) {
        // Must be a symbol_like
        if ( !named[AstChild::symbol_like].has_prop( ExprProperty::symbol_like ) ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>(
                MessageInfo( named[AstChild::symbol_like], 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    if ( named.find( AstChild::struct_symbol ) != named.end() ) {
        if ( !named[AstChild::struct_symbol].has_prop( ExprProperty::symbol ) ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>(
                MessageInfo( named[AstChild::struct_symbol], 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    if ( named.find( AstChild::trait_symbol ) != named.end() ) {
        if ( !named[AstChild::trait_symbol].has_prop( ExprProperty::symbol ) ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>(
                MessageInfo( named[AstChild::trait_symbol], 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    if ( named.find( AstChild::parameters ) != named.end() ) {
        if ( !named[AstChild::parameters].has_prop( ExprProperty::parenthesis ) ) {
            w_ctx.print_msg<MessageType::err_expected_parametes>(
                MessageInfo( named[AstChild::parameters], 0, FmtStr::Color::Red ) );
            return false;
        } else if ( type == ExprType::func || type == ExprType::func_decl ) {
            // Check parameter syntax (exclude func_call from check)
            for ( auto &p : named[AstChild::parameters].children ) {
                if ( p.type == ExprType::typed_op ) {
                    if ( !p.named[AstChild::left_expr].has_prop( ExprProperty::symbol ) &&
                         p.named[AstChild::left_expr].type != ExprType::self ) {
                        w_ctx.print_msg<MessageType::err_expected_symbol>(
                            MessageInfo( p.named[AstChild::left_expr], 0, FmtStr::Color::Red ) );
                        return false;
                    }
                    if ( !p.named[AstChild::right_expr].has_prop( ExprProperty::symbol_like ) ) {
                        w_ctx.print_msg<MessageType::err_expected_symbol>(
                            MessageInfo( p.named[AstChild::right_expr], 0, FmtStr::Color::Red ) );
                        return false;
                    }
                } else if ( !p.has_prop( ExprProperty::symbol ) && p.type != ExprType::self ) {
                    w_ctx.print_msg<MessageType::err_expected_symbol>( MessageInfo( p, 0, FmtStr::Color::Red ) );
                    return false;
                }
            }
        }
    }
    if ( named.find( AstChild::return_type ) != named.end() ) {
        if ( !named[AstChild::return_type].has_prop( ExprProperty::symbol_like ) ) {
            w_ctx.print_msg<MessageType::err_expected_symbol>(
                MessageInfo( named[AstChild::return_type], 0, FmtStr::Color::Red ) );
            return false;
        }
    }
    if ( named.find( AstChild::index ) != named.end() ) {
        // Check if only one expr
        if ( named[AstChild::index].children.empty() ) {
            w_ctx.print_msg<MessageType::err_expected_one_array_parameter>(
                MessageInfo( named[AstChild::index], 0, FmtStr::Color::Red ) );
            return false;
        } else if ( named[AstChild::index].children.size() > 1 ||
                    named[AstChild::index].children.front().type == ExprType::comma_list ) {
            w_ctx.print_msg<MessageType::err_expected_only_one_parameter>(
                MessageInfo( named[AstChild::index].children.front(), 0, FmtStr::Color::Red ) );
            return false;
        }
    }

    return true;
}

bool AstNode::first_transformation( CrateCtx &c_ctx, Worker &w_ctx, AstNode &parent ) {
    // Transformations based on properties
    if ( has_prop( ExprProperty::braces ) ) {
        std::vector<AstNode> annotation_list;
        for ( size_t i = 0; i < children.size(); i++ ) {
            // Resolve annotations
            if ( children[i].type == ExprType::compiler_annotation ) {
                annotation_list.push_back( children[i] );
                children.erase( children.begin() + i );
                i--;
                continue;
            } else if ( !annotation_list.empty() ) {
                children[i].annotations = std::move( annotation_list );
                annotation_list.clear();
            }

            if ( children[i].type == ExprType::single_completed ) {
                auto &expr = children[i].children[0];
                if ( expr.type == ExprType::comma_list ) {
                    // Resolve commas
                    children.insert( children.begin() + i + 1, expr.children.begin(), expr.children.end() );
                    children.erase( children.begin() + i );
                    i--;
                    continue;
                } else if ( expr.type == ExprType::alias_bind ) {
                    // Resolve alias statements
                    auto subs = get_substitutions( c_ctx, w_ctx, expr );
                    substitutions.insert( substitutions.end(), subs.begin(), subs.end() );
                    children.erase( children.begin() + i );
                    i--;
                    continue;
                }
            } else if ( children[i].type == ExprType::comma_list ) {
                // Resolve commas
                children.insert( children.begin() + i + 1, children[i].children.begin(), children[i].children.end() );
                children.erase( children.begin() + i );
                i--;
                continue;
            }
        }
    }

    // Transformations based on type
    if ( type == ExprType::decl_scope || type == ExprType::imp_scope ) {
        c_ctx.expect_operand = false; // allow
    } else if ( type == ExprType::single_completed ) {
        if ( parent.has_prop( ExprProperty::decl_parent ) && parent.type != ExprType::decl_scope ) {
            type = ExprType::decl_scope;
            props.clear();
            generate_new_props();
            return first_transformation( c_ctx, w_ctx, parent ); // repeat for new entry
        }

        auto tmp = children.front();
        *this = tmp;
        return first_transformation( c_ctx, w_ctx, parent ); // repeat for new entry
    } else if ( type == ExprType::block ) {
        if ( parent.has_prop( ExprProperty::decl_parent ) ) {
            type = ExprType::decl_scope;
            props.clear();
            generate_new_props();
            return first_transformation( c_ctx, w_ctx, parent ); // repeat for new entry
        } else {
            // Insert implicit return type
            if ( children.empty() || children.back().type == ExprType::single_completed ) {
                children.emplace_back();
                children.back().type = ExprType::unit;
                children.back().generate_new_props();
            }

            type = ExprType::imp_scope;
            props.clear();
            generate_new_props();
            return first_transformation( c_ctx, w_ctx, parent ); // repeat for new entry
        }
    } else if ( type == ExprType::set ) {
        if ( parent.has_prop( ExprProperty::decl_parent ) ) {
            type = ExprType::decl_scope;
            props.clear();
            generate_new_props();
            return first_transformation( c_ctx, w_ctx, parent ); // repeat for new entry
        }
    } else if ( type == ExprType::func_head ) {
        if ( !parent.has_prop( ExprProperty::decl_parent ) ) {
            type = ExprType::func_call;
            props.clear();
            generate_new_props();
            return first_transformation( c_ctx, w_ctx, parent ); // repeat for new entry
        } else {
            type = ExprType::func_decl;
            props.clear();
            generate_new_props();
            return basic_semantic_check( c_ctx, w_ctx ) && // repeat the parameter check
                   first_transformation( c_ctx, w_ctx, parent ); // repeat for new entry
        }
    } else if ( type == ExprType::func || type == ExprType::pre_loop || type == ExprType::post_loop ||
                type == ExprType::inf_loop || type == ExprType::itr_loop || type == ExprType::static_statement ||
                type == ExprType::unsafe ) {
        if ( type == ExprType::func && c_ctx.expect_operand && named.find( AstChild::parameters ) == named.end() &&
             ( children.front().type == ExprType::set || children.front().children.size() <= 1 ) ) {
            // Should be interpreted as struct initializer TODO this should be configurable using the prelude
            type = ExprType::struct_initializer;
            props.clear();
            generate_new_props();
            return first_transformation( c_ctx, w_ctx, parent ); // repeat for new entry
        }
        if ( children.front().type == ExprType::single_completed ) {
            // resolve single completed
            children.front().type = ExprType::imp_scope;
            children.front().props.clear();
            children.front().generate_new_props();
        }
    } else if ( type == ExprType::if_cond || type == ExprType::if_else ) {
        if ( children[0].type == ExprType::single_completed ) {
            // resolve single completed
            children[0].type = ExprType::imp_scope;
            children[0].props.clear();
            children[0].generate_new_props();
        }
        if ( type == ExprType::if_else && children[1].type == ExprType::single_completed ) {
            // resolve single completed
            children[1].type = ExprType::imp_scope;
            children[1].props.clear();
            children[1].generate_new_props();
        }
    } else if ( type == ExprType::match ) {
        if ( children.front().type == ExprType::single_completed || children.front().type == ExprType::block ) {
            // resolve single completed
            children.front().type = ExprType::set;
            children.front().props.clear();
            children.front().generate_new_props();
        }
    } else if ( type == ExprType::struct_initializer ) {
        if ( children.front().type != ExprType::set ) {
            // resolve single completed
            children.front().type = ExprType::set;
            children.front().props.clear();
            children.front().generate_new_props();
        }
    } else if ( type == ExprType::structure || type == ExprType::trait || type == ExprType::implementation ||
                type == ExprType::module ) {
        if ( children.front().type == ExprType::single_completed || children.front().type == ExprType::set ||
             children.front().type == ExprType::block ) {
            // resolve single completed
            children.front().type = ExprType::decl_scope;
            children.front().props.clear();
            children.front().generate_new_props();
        }
    } else if ( type == ExprType::array_access ) {
        // Replace the array specifier with its content
        auto tmp = named[AstChild::index].children.front();
        named[AstChild::index] = tmp;
    } else if ( type == ExprType::reference ) {
        auto tmp = named[AstChild::symbol_like];
        if ( props.find( ExprProperty::mut ) != props.end() )
            tmp.props.insert( ExprProperty::mut );
        tmp.props.insert( ExprProperty::ref );
        *this = tmp;
        return first_transformation( c_ctx, w_ctx, parent ); // repeat for new entry
    } else if ( type == ExprType::mutable_attr ) {
        auto tmp = named[AstChild::symbol_like];
        *this = tmp;
        props.insert( ExprProperty::mut );
        return first_transformation( c_ctx, w_ctx, parent ); // repeat for new entry
    } else if ( type == ExprType::public_attr ) {
        if ( parent.type != ExprType::decl_scope ) {
            // public symbols are only allowed in decl scopes
            w_ctx.print_msg<MessageType::err_public_not_allowed_in_context>(
                MessageInfo( *this, 0, FmtStr::Color::Red ) );
            return false;
        }

        auto tmp = children.front();
        *this = tmp;
        props.insert( ExprProperty::pub );
        return first_transformation( c_ctx, w_ctx, parent ); // repeat for new entry
    } else if ( type == ExprType::template_postfix ) {
        if ( children.front().type == ExprType::comma_list ) {
            children.insert( children.end(), children.front().children.begin(), children.front().children.end() );
            children.erase( children.begin() );
        }
    }
    return true;
}

bool AstNode::symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    c_ctx.current_substitutions.push_back( substitutions );

    if ( has_prop( ExprProperty::anonymous_scope ) ) {
        SymbolId new_id = create_new_local_symbol( c_ctx, w_ctx, SymbolIdentifier{} );
        scope_symbol = new_id;
        switch_scope_to_symbol( c_ctx, w_ctx, new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( this );
    } else if ( has_prop( ExprProperty::named_scope ) ) {
        auto &symbol = named[type == ExprType::implementation ? AstChild::struct_symbol : AstChild::symbol];
        SymbolId new_id =
            create_new_local_symbol_from_name_chain( c_ctx, w_ctx, symbol.get_symbol_chain( c_ctx, w_ctx ), symbol );
        scope_symbol = new_id;
        symbol.update_left_symbol_id( c_ctx.symbol_graph[c_ctx.current_scope].sub_nodes.back() );
        symbol.update_symbol_id( new_id );
        switch_scope_to_symbol( c_ctx, w_ctx, new_id );
        c_ctx.symbol_graph[new_id].original_expr.push_back( this );
        c_ctx.symbol_graph[new_id].pub = symbol.has_prop( ExprProperty::pub );
        if ( type == ExprType::structure ) {
            c_ctx.symbol_graph[new_id].type = c_ctx.struct_type;

            // Handle type
            if ( c_ctx.symbol_graph[new_id].value == 0 )
                create_new_type( c_ctx, w_ctx, new_id );

            // Handle Members
            for ( auto &expr : children.front().children ) {
                auto &symbol = expr;

                // Select symbol
                if ( expr.type == ExprType::typed_op ) {
                    symbol = symbol.named[AstChild::left_expr];
                } else if ( !expr.has_prop( ExprProperty::symbol ) ) {
                    LOG_ERR( "Struct member is not a symbol" );
                    return false;
                }

                // Check if scope is right
                auto identifier_list = symbol.get_symbol_chain( c_ctx, w_ctx );
                if ( identifier_list->size() != 1 ) {
                    w_ctx.print_msg<MessageType::err_member_in_invalid_scope>(
                        MessageInfo( symbol, 0, FmtStr::Color::Red ) );
                    return false;
                }

                // Check if symbol doesn't already exits
                auto indices = find_member_symbol_by_identifier( c_ctx, w_ctx, identifier_list->front(), new_id );
                auto &members = c_ctx.type_table[c_ctx.symbol_graph[new_id].type].members;
                if ( !indices.empty() ) {
                    std::vector<MessageInfo> notes;
                    for ( auto &idx : indices ) {
                        if ( !members[idx].original_expr.empty() )
                            notes.push_back( MessageInfo( *members[idx].original_expr.front(), 1 ) );
                    }
                    w_ctx.print_msg<MessageType::err_member_symbol_is_ambiguous>(
                        MessageInfo( symbol, 0, FmtStr::Color::Red ), notes );
                    return false;
                }

                // Create member
                auto &new_member = create_new_member_symbol( c_ctx, w_ctx, identifier_list->front(), new_id );
                new_member.original_expr.push_back( &expr );
                new_member.pub = symbol.has_prop( ExprProperty::pub );
            }
        } else if ( type == ExprType::trait ) {
            c_ctx.symbol_graph[new_id].type = c_ctx.trait_type;
            // Handle type
            if ( c_ctx.symbol_graph[new_id].value == 0 )
                create_new_type( c_ctx, w_ctx, new_id );
        } else if ( type == ExprType::implementation ) {
            c_ctx.symbol_graph[new_id].type = c_ctx.struct_type;
        } else if ( type == ExprType::module ) {
            c_ctx.symbol_graph[new_id].type = c_ctx.mod_type;
        } else { // function
            c_ctx.symbol_graph[new_id].type = c_ctx.fn_type;
            // Handle type
            if ( c_ctx.symbol_graph[new_id].value == 0 )
                create_new_type( c_ctx, w_ctx, new_id );
        }
    }
    return true;
}

bool AstNode::post_symbol_discovery( CrateCtx &c_ctx, Worker &w_ctx ) {
    c_ctx.current_substitutions.pop_back();

    if ( has_prop( ExprProperty::anonymous_scope ) ) {
        pop_scope( c_ctx, w_ctx );
    } else if ( has_prop( ExprProperty::named_scope ) ) {
        switch_scope_to_symbol(
            c_ctx, w_ctx,
            c_ctx
                .symbol_graph[named[type == ExprType::implementation ? AstChild::struct_symbol : AstChild::symbol]
                                  .get_left_symbol_id()]
                .parent );
    }
    return true;
}

void AstNode::update_symbol_id( SymbolId new_id ) {
    if ( type == ExprType::atomic_symbol ) {
        symbol = new_id;
    } else if ( type == ExprType::scope_access ) {
        named[AstChild::member].update_symbol_id( new_id );
    } else if ( type == ExprType::template_postfix ) {
        named[AstChild::symbol].update_symbol_id( new_id );
    } else {
        LOG_ERR( "Symbol is not a symbol" );
    }
}

SymbolId AstNode::get_symbol_id() {
    if ( type == ExprType::atomic_symbol ) {
        return symbol;
    } else if ( type == ExprType::scope_access ) {
        return named[AstChild::member].get_symbol_id();
    } else if ( type == ExprType::template_postfix ) {
        return named[AstChild::symbol].get_symbol_id();
    } else {
        LOG_ERR( "Symbol is not a symbol" );
        return 0;
    }
}

void AstNode::update_left_symbol_id( SymbolId new_id ) {
    if ( type == ExprType::scope_access ) {
        named[AstChild::base].update_left_symbol_id( new_id );
    } else if ( type == ExprType::atomic_symbol || type == ExprType::template_postfix ) {
        update_symbol_id( new_id );
    } else {
        LOG_ERR( "Symbol has no left sub-symbol" );
    }
}

SymbolId AstNode::get_left_symbol_id() {
    if ( type == ExprType::scope_access ) {
        return named[AstChild::base].get_left_symbol_id();
    } else if ( type == ExprType::atomic_symbol || type == ExprType::template_postfix ) {
        return get_symbol_id();
    } else {
        LOG_ERR( "Symbol has no left sub-symbol" );
        return 0;
    }
}

MirVarId AstNode::parse_mir( CrateCtx &c_ctx, Worker &w_ctx, FunctionImplId func ) {
    // Switch scope if needed
    auto parent_scope_symbol = c_ctx.current_scope;
    if ( scope_symbol != 0 )
        switch_scope_to_symbol( c_ctx, w_ctx, scope_symbol );

    MirVarId ret = 0; // initialize with invalid value

    // Create the mir instructions
    switch ( type ) {
    case ExprType::imp_scope: {
        c_ctx.curr_living_vars.emplace_back();
        c_ctx.curr_name_mapping.emplace_back();

        // Handle all expressions
        if ( children.size() > 1 ) {
            for ( auto expr = children.begin(); expr != children.end() - 1; expr++ ) {
                auto var = expr->parse_mir( c_ctx, w_ctx, func );
                if ( c_ctx.functions[func].vars[var].type == MirVariable::Type::rvalue ) {
                    drop_variable( c_ctx, w_ctx, func, *expr, var ); // drop dangling rvalue
                }
            }
        }
        if ( children.empty() ) {
            LOG_ERR( "No return value from block" );
        }
        ret = children.back().parse_mir( c_ctx, w_ctx, func );

        // Drop all created variables
        for ( auto var_itr = c_ctx.curr_living_vars.back().rbegin(); var_itr != c_ctx.curr_living_vars.back().rend();
              var_itr++ ) {
            if ( *var_itr != ret )
                drop_variable( c_ctx, w_ctx, func, *this, *var_itr );
        }

        c_ctx.curr_name_mapping.pop_back();
        c_ctx.curr_living_vars.pop_back();
        break;
    }
    case ExprType::unit: {
        ret = 0; // set the unit variable
        break;
    }
    case ExprType::numeric_literal: {
        auto &op = create_operation( c_ctx, w_ctx, func, *this, MirEntry::Type::literal, 0, {} );
        op.data = literal_number;
        c_ctx.functions[func].vars[op.ret].value_type = literal_type;
        c_ctx.functions[func].vars[op.ret].type = MirVariable::Type::rvalue;

        ret = op.ret;
        break;
    }
    case ExprType::atomic_symbol: {
        auto name_chain = get_symbol_chain( c_ctx, w_ctx );
        if ( !expect_unscoped_variable( c_ctx, w_ctx, *name_chain, *this ) )
            break;

        bool found = false;
        // Search for a local variable
        for ( auto itr = c_ctx.curr_name_mapping.rbegin(); itr != c_ctx.curr_name_mapping.rend(); itr++ ) {
            if ( auto var_itr = itr->find( name_chain->front().name ); var_itr != itr->end() ) {
                ret = var_itr->second.back();
                found = true;
                break;
            }
        }

        if ( !found ) {
            // Search for a symbol
            auto symbols = find_local_symbol_by_identifier_chain( c_ctx, w_ctx, name_chain );

            // TODO allow multiple symbols (e. g. for function overloading)

            if ( !symbols.empty() ) {
                if ( expect_exactly_one_symbol( c_ctx, w_ctx, symbols, *this ) ) {
                    ret = create_variable( c_ctx, w_ctx, func, this );
                    auto &result_var = c_ctx.functions[func].vars[ret];
                    result_var.type = MirVariable::Type::symbol;
                    result_var.value_type = c_ctx.symbol_graph[symbols.front()].value;
                    found = true;

                    // Create cosmetic operation
                    auto &op = create_operation( c_ctx, w_ctx, func, *this, MirEntry::Type::symbol, ret, {} );
                    op.symbol = symbols.front();
                }
            }
        }

        if ( !found ) {
            // Frist check if the symbol was just dropped earlier
            for ( auto itr = c_ctx.functions[func].drop_list.rbegin(); itr != c_ctx.functions[func].drop_list.rend();
                  itr++ ) {
                if ( itr->first == name_chain->front().name ) {
                    w_ctx.print_msg<MessageType::err_var_not_living>( MessageInfo( *this, 0, FmtStr::Color::Red ),
                                                                      { MessageInfo( *itr->second, 1 ) } );
                    found = true;
                    break;
                }
            }
        }
        if ( !found ) {
            // Symbol actually not found
            w_ctx.print_msg<MessageType::err_symbol_not_found>(
                MessageInfo( *this, 0, FmtStr::Color::Red ), std::vector<MessageInfo>(), symbol_name, token.content );
        }
        break;
    }
    case ExprType::func_call: {
        // Extract the symbol variable
        auto callee_var = named[AstChild::symbol].parse_mir( c_ctx, w_ctx, func );
        if ( callee_var != 0 ) {
            // Symbol found
            auto callee = c_ctx.type_table[c_ctx.functions[func].vars[callee_var].value_type].symbol;
            analyse_function_signature( c_ctx, w_ctx, callee );

            // TODO select the function based on its signature

            std::vector<MirVarId> params;
            if ( c_ctx.functions[func].vars[callee_var].base_ref != 0 )
                params.push_back( c_ctx.functions[func].vars[callee_var].base_ref ); // member access
            for ( auto &pe : named[AstChild::parameters].children ) {
                params.push_back( pe.parse_mir( c_ctx, w_ctx, func ) );
            }

            ret = create_call( c_ctx, w_ctx, func, *this, callee, 0, params ).ret;
        }
        break;
    }
    case ExprType::op: {
        auto calls = find_global_symbol_by_identifier_chain(
            c_ctx, w_ctx, split_symbol_chain( symbol_name, w_ctx.unit_ctx()->prelude_conf.scope_access_operator ) );

        for ( auto &candidate : calls ) {
            analyse_function_signature( c_ctx, w_ctx, candidate );
        }

        // TODO select the function based on its signature

        if ( calls.empty() ) {
            w_ctx.print_msg<MessageType::err_operator_symbol_not_found>(
                MessageInfo( *this, 0, FmtStr::Color::Red ), std::vector<MessageInfo>(), symbol_name, token.content );
            break;
        } else if ( calls.size() > 1 ) {
            std::vector<MessageInfo> notes;
            for ( auto &c : calls ) {
                if ( !c_ctx.symbol_graph[c].original_expr.empty() )
                    notes.push_back( MessageInfo( *c_ctx.symbol_graph[c].original_expr.front(), 1 ) );
            }
            w_ctx.print_msg<MessageType::err_operator_symbol_is_ambiguous>( MessageInfo( *this, 0, FmtStr::Color::Red ),
                                                                            notes, symbol_name, token.content );
            break;
        }


        auto left_result = named[AstChild::left_expr].parse_mir( c_ctx, w_ctx, func );
        auto right_result = named[AstChild::right_expr].parse_mir( c_ctx, w_ctx, func );

        auto &op = create_call( c_ctx, w_ctx, func, *this, calls.front(), 0, { left_result, right_result } );

        ret = op.ret;
        break;
    }
    case ExprType::simple_bind: {
        AstNode *symbol = &children.front().named[AstChild::left_expr];
        if ( symbol->type == ExprType::typed_op ) {
            symbol = &symbol->named[AstChild::left_expr];
        }

        auto name_chain = symbol->get_symbol_chain( c_ctx, w_ctx );
        if ( !expect_unscoped_variable( c_ctx, w_ctx, *name_chain, *symbol ) )
            break;

        // Create variable
        create_variable( c_ctx, w_ctx, func, symbol, name_chain->front().name );

        // Expr operation
        auto var = children.front().parse_mir( c_ctx, w_ctx, func );
        drop_variable( c_ctx, w_ctx, func, *this, var );

        break;
    }
    case ExprType::self: {
        if ( c_ctx.curr_self_var == 0 ) {
            w_ctx.print_msg<MessageType::err_self_in_free_function>( MessageInfo( *this, 0, FmtStr::Color::Red ) );
        }
        ret = c_ctx.curr_self_var;
        break;
    }
    case ExprType::self_type: {
        if ( c_ctx.curr_self_type == 0 ) {
            w_ctx.print_msg<MessageType::err_self_in_free_function>( MessageInfo( *this, 0, FmtStr::Color::Red ) );
        }

        ret = create_variable( c_ctx, w_ctx, func, this );
        auto &result_var = c_ctx.functions[func].vars[ret];
        result_var.type = MirVariable::Type::symbol;
        result_var.value_type = c_ctx.curr_self_type;

        // Create cosmetic operation
        auto &op = create_operation( c_ctx, w_ctx, func, *this, MirEntry::Type::symbol, ret, {} );
        op.symbol = c_ctx.type_table[c_ctx.curr_self_type].symbol;
        break;
    }
    case ExprType::struct_initializer: {
        auto struct_var = named[AstChild::symbol].parse_mir( c_ctx, w_ctx, func );
        if ( struct_var != 0 ) {
            // Symbol found
            auto &type = c_ctx.type_table[c_ctx.functions[func].vars[struct_var].value_type];
            if ( c_ctx.symbol_graph[type.symbol].type != c_ctx.struct_type ) {
                // Is not a struct
                w_ctx.print_msg<MessageType::err_instantiate_non_struct>(
                    MessageInfo( named[AstChild::member], 0, FmtStr::Color::Red ),
                    { MessageInfo( *c_ctx.symbol_graph[type.symbol].original_expr.front(), 1 ) } );
                break;
            }

            // Create variable TODO handle mutability
            ret = create_variable( c_ctx, w_ctx, func, this );
            auto &result_var = c_ctx.functions[func].vars[ret];
            result_var.type = MirVariable::Type::rvalue;
            result_var.value_type = c_ctx.functions[func].vars[struct_var].value_type;

            // Check member count
            if ( type.members.size() != children.front().children.size() ) {
                w_ctx.print_msg<MessageType::err_wrong_struct_initializer_member_count>(
                    MessageInfo( children.front(), 0, FmtStr::Color::Red ),
                    { MessageInfo( *c_ctx.symbol_graph[type.symbol].original_expr.front(), 1 ) }, type.members.size(),
                    children.front().children.size() );
                break;
            }

            // Crate member values
            std::vector<MirVarId> vars;
            vars.reserve( type.members.size() );
            for ( size_t i = 0; i < type.members.size(); i++ ) {
                auto &entry = children.front().children[i];
                auto var = entry.parse_mir( c_ctx, w_ctx, func );
                vars.push_back( var );

                // Add type check operation TODO handle if struct member is not already typed
                auto &op = create_operation( c_ctx, w_ctx, func, entry, MirEntry::Type::type, var, {} );
                op.symbol = type.members[i].type;
            }

            // Merge values into type
            create_operation( c_ctx, w_ctx, func, *this, MirEntry::Type::merge, ret, vars );

            // Set type operation (should be after the merge)
            auto &op = create_operation( c_ctx, w_ctx, func, *this, MirEntry::Type::type, ret, {} );
            op.symbol = c_ctx.type_table[result_var.value_type].symbol;

            // Drop vars if necessary
            for ( auto var_itr = vars.rbegin(); var_itr != vars.rend(); var_itr++ ) {
                drop_variable( c_ctx, w_ctx, func, children.front(), *var_itr );
            }
        }

        break;
    }
    case ExprType::member_access: {
        auto obj = named[AstChild::base].parse_mir( c_ctx, w_ctx, func );
        if ( obj == 0 ) {
            // Symbol not found (error message already generated)
            break;
        }

        auto base_symbol = c_ctx.type_table[c_ctx.functions[func].vars[obj].value_type].symbol;

        // Get member name
        auto member_chain = named[AstChild::member].get_symbol_chain( c_ctx, w_ctx );
        if ( member_chain->size() != 1 || !member_chain->front().template_values.empty() ) {
            w_ctx.print_msg<MessageType::err_member_in_invalid_scope>(
                MessageInfo( named[AstChild::member], 0, FmtStr::Color::Red ) );
        }

        // Find attributes
        auto attrs = find_member_symbol_by_identifier( c_ctx, w_ctx, member_chain->front(), base_symbol );

        // Find methods
        std::vector<SymbolId> methods;
        for ( auto &node : c_ctx.symbol_graph[base_symbol].sub_nodes ) {
            if ( symbol_identifier_matches( member_chain->front(), c_ctx.symbol_graph[node].identifier ) ) {
                methods.push_back( node );
            }
        }

        if ( attrs.empty() && methods.empty() ) {
            w_ctx.print_msg<MessageType::err_symbol_not_found>(
                MessageInfo( named[AstChild::member], 0, FmtStr::Color::Red ) );
            break;
        } else if ( attrs.size() + methods.size() != 1 ) {
            std::vector<MessageInfo> notes;
            for ( auto &attr : attrs ) {
                if ( !c_ctx.symbol_graph[attr].original_expr.empty() )
                    notes.push_back( MessageInfo( *c_ctx.symbol_graph[attr].original_expr.front(), 1 ) );
            }
            for ( auto &method : methods ) {
                if ( !c_ctx.symbol_graph[method].original_expr.empty() )
                    notes.push_back( MessageInfo( *c_ctx.symbol_graph[method].original_expr.front(), 1 ) );
            }
            w_ctx.print_msg<MessageType::err_member_symbol_is_ambiguous>(
                MessageInfo( named[AstChild::base], 0, FmtStr::Color::Red ), notes );
            break;
        }

        // Create operation
        if ( !attrs.empty() ) {
            // Access attribute
            auto &op = create_operation( c_ctx, w_ctx, func, *this, MirEntry::Type::member, 0, { obj } );
            auto &result_var = c_ctx.functions[func].vars[op.ret];
            result_var.member_idx = attrs.front();
            result_var.type = MirVariable::Type::l_ref;
            if ( c_ctx.functions[func].vars[obj].type == MirVariable::Type::l_ref ) {
                // Pass reference (never reference a l_ref)
                result_var.ref = c_ctx.functions[func].vars[obj].ref;
                result_var.member_idx += c_ctx.functions[func].vars[obj].member_idx;
            } else {
                result_var.ref = obj;
            }
            result_var.mut = c_ctx.functions[func].vars[obj].mut;
            result_var.value_type =
                c_ctx.type_table[c_ctx.symbol_graph[base_symbol].value].members[attrs.front()].value;
            ret = op.ret;
        } else {
            // Access method

            // First check if its a method and not a function
            auto &identifier = c_ctx.symbol_graph[methods.front()].identifier;
            if ( identifier.parameters.empty() || c_ctx.symbol_graph[c_ctx.symbol_graph[methods.front()].parent].type !=
                                                      c_ctx.struct_type ) { // TODO this check may be weak (rather check
                                                                            // if the parameter is actually "self")
                w_ctx.print_msg<MessageType::err_method_is_a_free_function>(
                    MessageInfo( *this, 0, FmtStr::Color::Red ),
                    { MessageInfo( *c_ctx.symbol_graph[methods.front()].original_expr.front(), 1 ) } );
                break;
            }

            ret = create_variable( c_ctx, w_ctx, func, this );
            auto &result_var = c_ctx.functions[func].vars[ret];
            result_var.type = MirVariable::Type::symbol;
            result_var.value_type = c_ctx.symbol_graph[methods.front()].value;
            result_var.base_ref = obj;

            // Create cosmetic operation
            auto &op = create_operation( c_ctx, w_ctx, func, *this, MirEntry::Type::symbol, ret, { obj } );
            op.symbol = methods.front();
        }
        break;
    }
    case ExprType::typed_op: {
        ret = named[AstChild::left_expr].parse_mir( c_ctx, w_ctx, func );
        if ( ret == 0 )
            break; // Error, don't do anything

        auto type_ids = find_local_symbol_by_identifier_chain(
            c_ctx, w_ctx, named[AstChild::right_expr].get_symbol_chain( c_ctx, w_ctx ) );

        if ( !expect_exactly_one_symbol( c_ctx, w_ctx, type_ids, named[AstChild::right_expr] ) )
            break;

        // Set type
        auto &op = create_operation( c_ctx, w_ctx, func, *this, MirEntry::Type::type, ret, {} );
        op.symbol = type_ids.front();
        if ( c_ctx.functions[func].vars[ret].value_type == 0 ) {
            c_ctx.functions[func].vars[ret].value_type = c_ctx.symbol_graph[type_ids.front()].value;
        }

        break;
    }
    default:
        LOG_ERR( "NOT IMPLEMENTED: parse_mir of type " + to_string( static_cast<size_t>( type ) ) );
        break;
    }

    // Switch back to parent scope if needed
    if ( scope_symbol != 0 )
        switch_scope_to_symbol( c_ctx, w_ctx, scope_symbol );

    return ret;
}

String AstNode::get_debug_repr() const {
    String add_debug_data;
    if ( !annotations.empty() ) {
        add_debug_data += "#(";
        for ( auto &a : annotations ) {
            add_debug_data += a.get_debug_repr() + ", ";
        }
        add_debug_data += ")";
    }
    if ( !static_statements.empty() ) {
        add_debug_data += "$(";
        for ( auto &stst : static_statements ) {
            add_debug_data += stst.get_debug_repr() + ", ";
        }
        add_debug_data += ")";
    }

    String str;
    switch ( type ) {
    case ExprType::token:
        return "TOKEN " + to_string( static_cast<int>( token.type ) ) + " \"" + token.content + "\" " + add_debug_data;

    case ExprType::decl_scope:
        str = "GLOBAL {\n ";
        for ( auto &child : children )
            str += child.get_debug_repr() + "\n ";
        return str + " }" + add_debug_data;
    case ExprType::imp_scope:
        str = "IMP {\n ";
        for ( auto &child : children )
            str += child.get_debug_repr() + "\n ";
        return str + " }" + add_debug_data;
    case ExprType::single_completed:
        return "SC " + children.front().get_debug_repr() + ";" + add_debug_data;
    case ExprType::block:
        str = "BLOCK {\n ";
        for ( auto &child : children )
            str += child.get_debug_repr() + "\n ";
        return str + " }" + add_debug_data;
    case ExprType::set:
        str = "SET { ";
        for ( auto &child : children )
            str += child.get_debug_repr() + ", ";
        return str + "}" + add_debug_data;
    case ExprType::unit:
        return "UNIT()";
    case ExprType::term:
        return "TERM( " + children.front().get_debug_repr() + " )" + add_debug_data;
    case ExprType::tuple:
        str = "TUPLE( ";
        for ( auto &child : children )
            str += child.get_debug_repr() + ", ";
        return str + ")" + add_debug_data;
    case ExprType::array_specifier:
        str = "ARRAY[ ";
        for ( auto &child : children )
            str += child.get_debug_repr();
        return str + " ]" + add_debug_data;
    case ExprType::array_list:
        str = "ARRAY_LIST[ ";
        for ( auto &child : children )
            str += child.get_debug_repr();
        return str + " ]" + add_debug_data;
    case ExprType::comma_list:
        str = "COMMA( ";
        for ( auto &child : children )
            str += child.get_debug_repr() + ", ";
        return str + ")" + add_debug_data;
    case ExprType::numeric_literal:
        return "BLOB_LITERAL(" + to_string( literal_number ) + ")" + add_debug_data;
    case ExprType::string_literal:
        return "STR \"" + literal_string + "\"" + add_debug_data;

    case ExprType::atomic_symbol:
        return "SYM(" + to_string( symbol ) + " " + symbol_name + ")" + add_debug_data;
    case ExprType::func_head:
        return "FUNC_HEAD(" +
               ( named.find( AstChild::parameters ) != named.end()
                     ? named.at( AstChild::parameters ).get_debug_repr() + " "
                     : "" ) +
               named.at( AstChild::symbol ).get_debug_repr() + ")" + add_debug_data;
    case ExprType::func:
        return "FUNC(" +
               ( named.find( AstChild::parameters ) != named.end()
                     ? named.at( AstChild::parameters ).get_debug_repr() + " "
                     : "" ) +
               ( named.find( AstChild::symbol ) != named.end() ? named.at( AstChild::symbol ).get_debug_repr()
                                                               : "<anonymous>" ) +
               ( named.find( AstChild::return_type ) != named.end()
                     ? " -> " + named.at( AstChild::return_type ).get_debug_repr()
                     : "" ) +
               " " + children.front().get_debug_repr() + ")" + add_debug_data;
    case ExprType::func_decl:
        return "FUNC_DECL(" +
               ( named.find( AstChild::parameters ) != named.end()
                     ? named.at( AstChild::parameters ).get_debug_repr() + " "
                     : "" ) +
               named.at( AstChild::symbol ).get_debug_repr() + ")" + add_debug_data;
    case ExprType::func_call:
        return "FN_CALL(" +
               ( named.find( AstChild::parameters ) != named.end()
                     ? named.at( AstChild::parameters ).get_debug_repr() + " "
                     : "" ) +
               named.at( AstChild::symbol ).get_debug_repr() + ")" + add_debug_data;

    case ExprType::op:
        return "OP(" +
               ( named.find( AstChild::left_expr ) != named.end()
                     ? named.at( AstChild::left_expr ).get_debug_repr() + " "
                     : "" ) +
               token.content +
               ( named.find( AstChild::right_expr ) != named.end()
                     ? " " + named.at( AstChild::right_expr ).get_debug_repr()
                     : "" ) +
               ")" + add_debug_data;
    case ExprType::simple_bind:
        return "BINDING(" + children.front().get_debug_repr() + ")" + add_debug_data;
    case ExprType::alias_bind:
        return "ALIAS(" + children.front().get_debug_repr() + ")" + add_debug_data;

    case ExprType::if_cond:
        return "IF(" + named.at( AstChild::cond ).get_debug_repr() + " THEN " + children.front().get_debug_repr() +
               " )" + add_debug_data;
    case ExprType::if_else:
        return "IF(" + named.at( AstChild::cond ).get_debug_repr() + " THEN " + children.front().get_debug_repr() +
               " ELSE " + children.at( 1 ).get_debug_repr() + " )" + add_debug_data;
    case ExprType::pre_loop:
        return "PRE_LOOP(" + String( continue_eval ? "TRUE: " : "FALSE: " ) +
               named.at( AstChild::cond ).get_debug_repr() + " DO " + children.front().get_debug_repr() + " )" +
               add_debug_data;
    case ExprType::post_loop:
        return "POST_LOOP(" + String( continue_eval ? "TRUE: " : "FALSE: " ) +
               named.at( AstChild::cond ).get_debug_repr() + " DO " + children.front().get_debug_repr() + " )" +
               add_debug_data;
    case ExprType::inf_loop:
        return "INF_LOOP(" + children.front().get_debug_repr() + " )" + add_debug_data;
    case ExprType::itr_loop:
        return "ITR_LOOP(" + named.at( AstChild::itr ).get_debug_repr() + " DO " + children.front().get_debug_repr() +
               " )" + add_debug_data;
    case ExprType::match:
        return "MATCH(" + named.at( AstChild::select ).get_debug_repr() + " WITH " + children.front().get_debug_repr() +
               ")" + add_debug_data;

    case ExprType::self:
        return "SELF" + add_debug_data;
    case ExprType::self_type:
        return "SELF_TYPE" + add_debug_data;
    case ExprType::struct_initializer:
        return "STRUCT_INIT(" + named.at( AstChild::symbol ).get_debug_repr() + " " +
               children.front().get_debug_repr() + ")" + add_debug_data;

    case ExprType::structure:
        return "STRUCT " +
               ( named.find( AstChild::symbol ) != named.end() ? named.at( AstChild::symbol ).get_debug_repr()
                                                               : "<anonymous>" ) +
               " " + ( !children.empty() ? children.front().get_debug_repr() : "<undefined>" ) + add_debug_data;
    case ExprType::trait:
        return "TRAIT " + named.at( AstChild::symbol ).get_debug_repr() + " " + children.front().get_debug_repr() +
               add_debug_data;
    case ExprType::implementation:
        if ( named.find( AstChild::trait_symbol ) != named.end() ) {
            return "IMPL " + named.at( AstChild::trait_symbol ).get_debug_repr() + " FOR " +
                   named.at( AstChild::struct_symbol ).get_debug_repr() + " " + children.front().get_debug_repr() +
                   add_debug_data;
        } else {
            return "IMPL " + named.at( AstChild::struct_symbol ).get_debug_repr() + " " +
                   children.front().get_debug_repr() + add_debug_data;
        }

    case ExprType::member_access:
        return "MEMBER(" + named.at( AstChild::base ).get_debug_repr() + "." +
               named.at( AstChild::member ).get_debug_repr() + ")" + add_debug_data;
    case ExprType::scope_access:
        return "SCOPE(" +
               ( named.find( AstChild::base ) != named.end() ? named.at( AstChild::base ).get_debug_repr()
                                                             : "<global>" ) +
               "::" + named.at( AstChild::member ).get_debug_repr() + ")" + add_debug_data;
    case ExprType::array_access:
        return "ARR_ACC " + named.at( AstChild::base ).get_debug_repr() + "[" +
               named.at( AstChild::index ).get_debug_repr() + "]" + add_debug_data;

    case ExprType::range:
        str = range_type == Operator::RangeOperatorType::exclude
                  ? "EXCLUDE"
                  : range_type == Operator::RangeOperatorType::exclude_from
                        ? "EXCLUDE_FROM"
                        : range_type == Operator::RangeOperatorType::exclude_to
                              ? "EXCLUDE_TO"
                              : range_type == Operator::RangeOperatorType::include
                                    ? "INCLUDE"
                                    : range_type == Operator::RangeOperatorType::include_to ? "INCLUDE_TO" : "INVALID";
        return "RANGE " + str + " " +
               ( named.find( AstChild::from ) != named.end() ? named.at( AstChild::from ).get_debug_repr() : "" ) +
               ( named.find( AstChild::from ) != named.end() && named.find( AstChild::to ) != named.end() ? ".."
                                                                                                          : "" ) +
               ( named.find( AstChild::to ) != named.end() ? named.at( AstChild::to ).get_debug_repr() : "" ) +
               add_debug_data;
    case ExprType::reference:
        return "REF(" + named.at( AstChild::symbol_like ).get_debug_repr() + ")" + add_debug_data;
    case ExprType::mutable_attr:
        return "MUT(" + named.at( AstChild::symbol_like ).get_debug_repr() + ")" + add_debug_data;
    case ExprType::typeof_op:
        return "TYPE_OF(" + children.front().get_debug_repr() + ")" + add_debug_data;
    case ExprType::typed_op:
        return "TYPED(" + named.at( AstChild::left_expr ).get_debug_repr() + ":" +
               named.at( AstChild::right_expr ).get_debug_repr() + ")" + add_debug_data;

    case ExprType::module:
        return "MODULE " + named.at( AstChild::symbol ).get_debug_repr() + " " + children.front().get_debug_repr() +
               add_debug_data;
    case ExprType::declaration:
        return "DECL(" + children.front().get_debug_repr() + ")" + add_debug_data;
    case ExprType::public_attr:
        return "PUBLIC(" + children.front().get_debug_repr() + ")" + add_debug_data;
    case ExprType::static_statement:
        return "STST " + children.front().get_debug_repr() + add_debug_data;
    case ExprType::compiler_annotation:
        return "ANNOTATE(" + named.at( AstChild::symbol ).get_debug_repr() + " " +
               named.at( AstChild::parameters ).get_debug_repr() + ")" + add_debug_data;
    case ExprType::macro_call:
        return "MACRO(" + named.at( AstChild::symbol ).get_debug_repr() + "! " + children.front().get_debug_repr() +
               ")" + add_debug_data;
    case ExprType::unsafe:
        return "UNSAFE " + children.front().get_debug_repr() + add_debug_data;
    case ExprType::template_postfix:
        str = "TEMPLATE " + named.at( AstChild::symbol ).get_debug_repr() + "<";
        for ( auto &child : children )
            str += child.get_debug_repr() + ", ";
        return str + " >" + add_debug_data;

    default:
        LOG_ERR( "NOT IMPLEMENTED: get_debug_repr for type " + to_string( static_cast<size_t>( type ) ) );
        return "NO" + add_debug_data;
    }
}
