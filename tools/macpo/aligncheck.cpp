/*
 * Copyright (c) 2011-2013  University of Texas at Austin. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * This file is part of PerfExpert.
 *
 * PerfExpert is free software: you can redistribute it and/or modify it under
 * the terms of the The University of Texas at Austin Research License
 * 
 * PerfExpert is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.
 * 
 * Authors: Leonardo Fialho and Ashay Rane
 *
 * $HEADER$
 */

#include <algorithm>
#include <rose.h>

#include "aligncheck.h"
#include "analysis_profile.h"
#include "generic_vars.h"
#include "inst_defs.h"
#include "ir_methods.h"
#include "loop_traversal.h"

using namespace SageBuilder;
using namespace SageInterface;

aligncheck_t::aligncheck_t(VariableRenaming*& _var_renaming) {
    var_renaming = _var_renaming;
    loop_traversal = new loop_traversal_t(var_renaming);
}

aligncheck_t::~aligncheck_t() {
    delete loop_traversal;
    loop_traversal = NULL;
}

name_list_t& aligncheck_t::get_stream_list() {
    return var_name_list;
}

void aligncheck_t::atTraversalStart() {
    var_name_list.clear();
    statement_list.clear();
}

void aligncheck_t::atTraversalEnd() {
}

bool aligncheck_t::contains_non_linear_reference(
        const reference_list_t& reference_list) {
    for (reference_list_t::const_iterator it2 = reference_list.begin();
            it2 != reference_list.end(); it2++) {
        const reference_info_t& reference_info = *it2;
        const SgNode* ref_node = reference_info.node;

        const SgPntrArrRefExp* pntr = isSgPntrArrRefExp(ref_node);

        // Check if pntr is a linear array reference.
        if (pntr && !ir_methods::is_linear_reference(pntr, false)) {
            return true;
        }
    }

    return false;
}

void aligncheck_t::instrument_loop_trip_count(Sg_File_Info* fileInfo,
        loop_info_t& loop_info) {
    SgScopeStatement* scope_stmt = loop_info.loop_stmt;
    SgExpression* idxv = loop_info.idxv_expr;
    SgExpression* init = loop_info.init_expr;
    SgExpression* test = loop_info.test_expr;

    if (SgBasicBlock* bb = getEnclosingNode<SgBasicBlock>(scope_stmt)) {
        int line_number = scope_stmt->get_file_info()->get_raw_line();

        // Create new integer variable calledi
        // "indigo__trip_count_<line_number>". Funky, eh?
        char var_name[32];
        snprintf (var_name, 32, "indigo__trip_count_%d", line_number);
        SgType* long_type = buildLongType();

        SgVariableDeclaration* trip_count = NULL;
        trip_count = ir_methods::create_long_variable(fileInfo, var_name, 0);
        trip_count->set_parent(bb);

        SgVarRefExp* trip_count_expr = buildVarRefExp(var_name);
        SgPlusPlusOp* incr_op = new SgPlusPlusOp(fileInfo,
                trip_count_expr, long_type);
        SgExprStatement* incr_statement = new SgExprStatement(fileInfo,
                incr_op);

        std::string function_name = SageInterface::is_Fortran_language()
            ? "indigo__tripcount_check_f" : "indigo__tripcount_check_c";

        std::vector<SgExpression*> params;
        params.push_back(buildIntVal(line_number));
        params.push_back(trip_count_expr);
        SgExprStatement* expr_statement = NULL;
        expr_statement = ir_methods::prepare_call_statement(bb, function_name,
                params, scope_stmt);

        statement_info_t tripcount_call;
        tripcount_call.statement = expr_statement;
        tripcount_call.reference_statement = scope_stmt;
        tripcount_call.before = false;
        statement_list.push_back(tripcount_call);

        statement_info_t tripcount_decl;
        tripcount_decl.statement = trip_count;
        tripcount_decl.reference_statement = scope_stmt;
        tripcount_decl.before = true;
        statement_list.push_back(tripcount_decl);

        statement_info_t tripcount_incr;
        tripcount_incr.statement = incr_statement;
        tripcount_incr.reference_statement = getFirstStatement(scope_stmt);
        tripcount_incr.before = true;
        statement_list.push_back(tripcount_incr);
    }
}

void aligncheck_t::instrument_streaming_stores(Sg_File_Info* fileInfo,
        loop_info_t& loop_info) {
    // Extract streaming stores from array references in the loop.
    sstore_map_t sstore_map;
    reference_list_t& reference_list = loop_info.reference_list;

    for (reference_list_t::iterator it2 = reference_list.begin();
            it2 != reference_list.end(); it2++) {
        reference_info_t& reference_info = *it2;
        std::string& ref_stream = reference_info.name;

        switch (reference_info.access_type) {
            case TYPE_WRITE:
                sstore_map[ref_stream].push_back(reference_info.node);
                break;

            case TYPE_READ:
            case TYPE_READ_AND_WRITE:
            case TYPE_UNKNOWN:  // Handle unknown type the same way as a read.
                sstore_map_t::iterator it = sstore_map.find(ref_stream);
                if (it != sstore_map.end()) {
                    sstore_map.erase(it);
                }

                break;
        }
    }

    for (sstore_map_t::iterator it = sstore_map.begin(); it != sstore_map.end();
            it++) {
        const std::string& key = it->first;
        node_list_t& value = it->second;

        // Replace index variable with init expression.
        expr_list_t expr_list;
        for (node_list_t::iterator it2 = value.begin(); it2 != value.end();
                it2++) {
            SgExpression* node = isSgExpression(*it2);
            if (node) {
                SgBinaryOp* copy_node = isSgBinaryOp(copyExpression(node));
                if (ir_methods::contains_expr(copy_node, loop_info.idxv_expr) &&
                        loop_info.init_expr != NULL) {
                    ir_methods::replace_expr(copy_node, loop_info.idxv_expr,
                            loop_info.init_expr);
                }

                expr_list.push_back(copy_node);
            }
        }

        // Remove duplicates.
        std::map<std::string, SgExpression*> string_expr_map;
        for (expr_list_t::iterator it2 = expr_list.begin();
                it2 != expr_list.end(); it2++) {
            SgExpression* expr = *it2;
            std::string expr_string = expr->unparseToString();
            if (string_expr_map.find(expr_string) == string_expr_map.end()) {
                string_expr_map[expr_string] = expr;
            } else {
                // An expression with the same string representation has
                // already been seen, so we skip this expression.
            }
        }

        // Reuse the same expression list from earlier.
        expr_list.clear();

        // Finally, loop over the map to get the unique expressions.
        for (std::map<std::string, SgExpression*>::iterator it2 =
                string_expr_map.begin(); it2 != string_expr_map.end(); it2++) {
            SgExpression* expr = it2->second;

            SgExpression *addr_expr =
                SageInterface::is_Fortran_language() ?
                (SgExpression*) expr : buildCastExp (
                        buildAddressOfOp((SgExpression*) expr),
                        buildPointerType(buildVoidType()));

            expr_list.push_back(addr_expr);
        }

        // If we have any expressions, add the instrumentation call.
        if (expr_list.size()) {
            int line_number = 0;
            line_number = loop_info.loop_stmt->get_file_info()->get_raw_line();
            SgIntVal* param_line_number = new SgIntVal(fileInfo, line_number);
            SgIntVal* param_count = new SgIntVal(fileInfo, expr_list.size());

            std::string function_name = SageInterface::is_Fortran_language()
                ? "indigo__sstore_aligncheck_f" : "indigo__sstore_aligncheck_c";

            SgBasicBlock* bb = NULL;
            bb = getEnclosingNode<SgBasicBlock>(loop_info.loop_stmt);
            std::vector<SgExpression*> params;
            params.push_back(param_line_number);
            params.push_back(param_count);
            params.insert(params.end(), expr_list.begin(), expr_list.end());

            SgExprStatement* expr_stmt = NULL;
            expr_stmt = ir_methods::prepare_call_statement(bb, function_name,
                    params, loop_info.loop_stmt);

            statement_info_t statement_info;
            statement_info.statement = expr_stmt;
            statement_info.reference_statement = loop_info.loop_stmt;
            statement_info.before = true;
            statement_list.push_back(statement_info);
        }
    }
}

SgExpression* aligncheck_t::instrument_alignment_checks(Sg_File_Info* fileInfo,
        SgScopeStatement* outer_scope_stmt, loop_info_t& loop_info,
        name_list_t& stream_list, expr_map_t& loop_map) {
    pntr_list_t pntr_list;
    std::set<std::string> stream_set;
    reference_list_t& reference_list = loop_info.reference_list;

    for (reference_list_t::iterator it2 = reference_list.begin();
            it2 != reference_list.end(); it2++) {
        reference_info_t& reference_info = *it2;
        SgNode* ref_node = reference_info.node;

        SgPntrArrRefExp* pntr = isSgPntrArrRefExp(ref_node);

        if (pntr) {
            std::string stream_name = pntr->unparseToString();
            // FIXME: Rather rudimentary check, using just the stream name
            // instead of using the index expression as well.
            if (std::find(stream_list.begin(), stream_list.end(), stream_name)
                    == stream_list.end()) {
                stream_list.push_back(stream_name);

                if (stream_set.find(stream_name) == stream_set.end()) {
                    stream_set.insert(stream_name);
                    SgPntrArrRefExp* copy_pntr = isSgPntrArrRefExp(
                            copyExpression(pntr));

                    if (copy_pntr)
                        pntr_list.push_back(copy_pntr);
                }
            }
        }
    }

    std::string function_name = SageInterface::is_Fortran_language()
        ? "indigo__aligncheck_f" : "indigo__aligncheck_c";
    SgBasicBlock* outer_bb = getEnclosingNode<SgBasicBlock>(loop_info.loop_stmt);

    std::set<std::string> expr_set;
    expr_list_t param_list;
    for (pntr_list_t::iterator it = pntr_list.begin(); it != pntr_list.end();
            it++) {
        SgPntrArrRefExp*& init_ref = *it;

        if (init_ref) {
            SgBinaryOp* bin_op = isSgBinaryOp(init_ref);
            SgBinaryOp* bin_copy = isSgBinaryOp(copyExpression(bin_op));

            if (bin_op && bin_copy) {
                // Replace all known index variables
                // with initial and final values.
                for (expr_map_t::iterator it2 = loop_map.begin();
                        it2 != loop_map.end(); it2++) {
                    SgExpression* idxv = it2->first;
                    loop_info_t* loop_info = it2->second;

                    SgExpression* init = loop_info->init_expr;
                    SgExpression* test = loop_info->test_expr;
                    SgExpression* incr = loop_info->incr_expr;
                    int incr_op = loop_info->incr_op;

                    if (ir_methods::contains_expr(bin_op, idxv) && init) {
                        if (!isSgValueExp(init))
                            expr_set.insert(init->unparseToString());

                        ir_methods::replace_expr(bin_op, idxv, init);
                    }

                    if (ir_methods::contains_expr(bin_copy, idxv)) {
                        if (!isSgValueExp(test))
                            expr_set.insert(test->unparseToString());

                        // Construct final value based on
                        // the expressions: test, incr_op and incr_expr.
                        SgExpression* final_value = NULL;
                        final_value = ir_methods::get_final_value(fileInfo,
                                test, incr, incr_op);
                        assert(final_value);

                        ir_methods::replace_expr(bin_copy, idxv, final_value);
                    }
                }

                SgExpression *param_addr_initial =
                    SageInterface::is_Fortran_language() ?
                    (SgExpression*) bin_op : buildCastExp (
                            buildAddressOfOp((SgExpression*) bin_op),
                            buildPointerType(buildVoidType()));

                SgExpression *param_addr_final =
                    SageInterface::is_Fortran_language() ?
                    (SgExpression*) bin_copy : buildCastExp (
                            buildAddressOfOp((SgExpression*) bin_copy),
                            buildPointerType(buildVoidType()));

                if (SgExpression* bin_expr = isSgExpression(bin_op)) {
                    SgSizeOfOp* sizeofop = new SgSizeOfOp(fileInfo,
                            NULL, bin_expr->get_type(), bin_expr->get_type());

                    ROSE_ASSERT(param_addr_initial);
                    ROSE_ASSERT(param_addr_final);
                    ROSE_ASSERT(sizeofop);

                    param_list.push_back(param_addr_initial);
                    param_list.push_back(param_addr_final);
                    param_list.push_back(sizeofop);
                }
            }
        }
    }

    if (pntr_list.size()) {
        statement_info_t statement_info;
        statement_info.statement = NULL;

        int line_number = outer_scope_stmt->get_file_info()->get_raw_line();
        SgIntVal* param_count = new SgIntVal(fileInfo, param_list.size() / 3);
        SgIntVal* param_line_no = new SgIntVal(fileInfo, line_number);

#if 0
        char var_name[32];
        snprintf (var_name, 32, "indigo__type_size_%d", line_number);

        SgBasicBlock* outer_bb = getEnclosingNode<SgBasicBlock>(outer_scope_stmt);
        SgVariableDeclaration* type_size = NULL;
        type_size = ir_methods::create_long_variable(fileInfo, var_name, 0);
        type_size->set_parent(outer_bb);

        SgExpression* type_size_addr = buildCastExp (
                buildAddressOfOp((SgExpression*) buildVarRefExp(var_name)),
                buildPointerType(buildIntType()));

        statement_info_t type_size_decl;
        type_size_decl.statement = type_size;
        type_size_decl.reference_statement = outer_scope_stmt;
        type_size_decl.before = true;
        statement_list.push_back(type_size_decl);
#endif

        if (expr_set.size() == 1) {
            def_map_t def_map;
            std::string expr = *(expr_set.begin());

            VariableRenaming::NumNodeRenameTable rename_table =
                var_renaming->getReachingDefsAtNode(loop_info.loop_stmt);

            // Expand the iterator list into a map for easier lookup.
            ir_methods::construct_def_map(rename_table, def_map);

            if (def_map.find(expr) != def_map.end()) {
                VariableRenaming::NumNodeRenameEntry entry_list = def_map[expr];
                for (entry_iterator entry_it = entry_list.begin();
                        entry_it != entry_list.end(); entry_it++) {
                    SgNode* def_node = (*entry_it).second;

                    SgBasicBlock* bb = NULL;
                    SgStatement* reference_statement = NULL;

                    if (isSgInitializedName(def_node)) {
                        // Instrument at start of loop.
                        statement_info.reference_statement = outer_scope_stmt;
                        bb = getEnclosingNode<SgBasicBlock>(outer_scope_stmt);
                    } else {
                        statement_info.reference_statement =
                            isSgStatement(def_node) ? isSgStatement(def_node) :
                            getEnclosingNode<SgStatement>(def_node);
                        bb = getEnclosingNode<SgBasicBlock>(def_node);
                    }

                    if (bb) {
                        std::vector<SgExpression*> params;
                        params.push_back(param_line_no);
                        // params.push_back(type_size_addr);
                        params.push_back(param_count);
                        params.insert(params.end(), param_list.begin(),
                                param_list.end());

                        statement_info.statement =
                            ir_methods::prepare_call_statement(bb, function_name,
                                    params, statement_info.reference_statement);
                        statement_info.before = true;
                    }
                }
            } else {
                // TODO: Place function call before the start of the outermost loop.
                std::vector<SgExpression*> params;
                params.push_back(param_line_no);
                // params.push_back(type_size_addr);
                params.push_back(param_count);
                params.insert(params.end(), param_list.begin(),
                        param_list.end());

                statement_info.statement = ir_methods::prepare_call_statement(
                        getEnclosingNode<SgBasicBlock>(outer_scope_stmt),
                        function_name, params, outer_scope_stmt);
                statement_info.reference_statement = outer_scope_stmt;
                statement_info.before = true;
            }
        } else {
            // Instrument just before the loop under consideration.
            std::vector<SgExpression*> params;
            params.push_back(param_line_no);
            // params.push_back(type_size_addr);
            params.push_back(param_count);
            params.insert(params.end(), param_list.begin(), param_list.end());

            statement_info.statement = ir_methods::prepare_call_statement(
                    getEnclosingNode<SgBasicBlock>(loop_info.loop_stmt),
                    function_name, params, loop_info.loop_stmt);
            statement_info.reference_statement = loop_info.loop_stmt;
            statement_info.before = true;
        }

        if (statement_info.statement) {
            statement_list.push_back(statement_info);

#if 0
            char var_name[32];
            snprintf (var_name, 32, "indigo__alignment_%d", line_number);

            SgVariableDeclaration* alignment = NULL;
            SgExprStatement* expr_stmt = isSgExprStatement(
                    statement_info.statement);
            ROSE_ASSERT(expr_stmt);

            SgBasicBlock* outer_bb = getEnclosingNode<SgBasicBlock>(
                    statement_info.reference_statement);
            alignment = ir_methods::create_long_variable_with_init(fileInfo, var_name, expr_stmt->get_expression());
            alignment->set_parent(outer_bb);

            statement_info_t decl_info;
            decl_info.statement = alignment;
            decl_info.reference_statement = statement_info.reference_statement;
            decl_info.before = true;
            statement_list.push_back(decl_info);

            return buildVarRefExp(var_name);
#else
            return NULL;
#endif
        }
    }
}

void aligncheck_t::instrument_branches(Sg_File_Info* fileInfo,
        SgScopeStatement* scope_stmt, SgExpression* idxv_expr,
        SgExpression* common_alignment) {
    SgStatement* first_stmt = getFirstStatement(scope_stmt);
    SgStatement* stmt = first_stmt;
    while (stmt) {
        if (SgIfStmt* if_stmt = isSgIfStmt(stmt)) {
            int line_number = if_stmt->get_file_info()->get_raw_line();
            int for_line_number = scope_stmt->get_file_info()->get_raw_line();
            SgStatement* true_body = if_stmt->get_true_body();
            SgStatement* false_body = if_stmt->get_false_body();

            ROSE_ASSERT(true_body);

            // Create two variables for each branch.
            SgVariableDeclaration *var_true = NULL, *var_false = NULL;

            char var_true_name[32], var_false_name[32];
            snprintf (var_true_name, 32, "indigo__branch_true_%d", line_number);
            snprintf (var_false_name, 32, "indigo__branch_false_%d",
                    line_number);

            SgBasicBlock* outer_bb = getEnclosingNode<SgBasicBlock>(
                    scope_stmt);
            var_true = ir_methods::create_long_variable(fileInfo, var_true_name,
                    0);
            var_true->set_parent(outer_bb);
            var_false = ir_methods::create_long_variable(fileInfo,
                    var_false_name, 0);
            var_false->set_parent(outer_bb);

            statement_info_t true_decl;
            true_decl.before = true;
            true_decl.statement = var_true;
            true_decl.reference_statement = scope_stmt;
            statement_list.push_back(true_decl);

            statement_info_t false_decl;
            false_decl.before = true;
            false_decl.statement = var_false;
            false_decl.reference_statement = scope_stmt;
            statement_list.push_back(false_decl);

            SgExprStatement *true_incr = NULL, *false_incr = NULL;
            true_incr = ir_methods::create_long_incr_statement(fileInfo,
                    var_true_name);
            false_incr = ir_methods::create_long_incr_statement(fileInfo,
                    var_false_name);
            true_incr->set_parent(if_stmt);
            false_incr->set_parent(if_stmt);

            if (true_body == NULL) {
                if_stmt->set_true_body(true_incr);
            } else {
                statement_info_t true_stmt;
                true_stmt.statement = true_incr;
                true_stmt.reference_statement = true_body;
                true_stmt.before = true;
                statement_list.push_back(true_stmt);
            }

            if (false_body == NULL) {
                if_stmt->set_false_body(false_incr);
            } else {
                statement_info_t false_stmt;
                false_stmt.statement = false_incr;
                false_stmt.reference_statement = false_body;
                false_stmt.before = true;
                statement_list.push_back(false_stmt);
            }

            std::string function_name = SageInterface::is_Fortran_language()
                ? "indigo__record_branch_f" : "indigo__record_branch_c";

            std::vector<SgExpression*> params;
            params.push_back(new SgIntVal(fileInfo, line_number));
            params.push_back(new SgIntVal(fileInfo, for_line_number));
            params.push_back(buildVarRefExp(var_true_name));
            params.push_back(buildVarRefExp(var_false_name));

            statement_info_t branch_statement;
            branch_statement.statement = ir_methods::prepare_call_statement(
                    getEnclosingNode<SgBasicBlock>(scope_stmt),
                    function_name, params, scope_stmt);
            branch_statement.reference_statement = scope_stmt;
            branch_statement.before = false;
            statement_list.push_back(branch_statement);

    #if 0
            char var_dir_name[32];
            snprintf (var_dir_name, 32, "indigo__branch_dir_%d", line_number);

            SgVariableDeclaration* branch_dir = NULL;
            SgBasicBlock* bb = getEnclosingNode<SgBasicBlock>(if_stmt);
            branch_dir = ir_methods::create_long_variable(fileInfo,
                    var_dir_name, 0);
            branch_dir->set_parent(bb);

            statement_info_t branch_dir_decl;
            branch_dir_decl.statement = branch_dir;
            branch_dir_decl.reference_statement = if_stmt;
            branch_dir_decl.before = true;
            statement_list.push_back(branch_dir_decl);

            statement_info_t assign_stmt;
            assign_stmt.statement = ir_methods::create_long_assign_statement(
                    fileInfo, var_dir_name, new SgIntVal(fileInfo, 1));
            assign_stmt.statement->set_parent(outer_bb);
            assign_stmt.reference_statement = false_incr;
            assign_stmt.before = false;
            statement_list.push_back(assign_stmt);

            statement_info_t dir_call_stmt;
            function_name = SageInterface::is_Fortran_language()
                ? "indigo__simd_branch_f" : "indigo__simd_branch_c";

            char var_record_name[32];
            snprintf (var_record_name, 32, "indigo__record_dir_%d",
                    line_number);

            SgVariableDeclaration* record_branch = NULL;
            record_branch = ir_methods::create_long_variable(fileInfo,
                    var_record_name, -1);
            record_branch->set_parent(outer_bb);

            statement_info_t record_stmt;
            record_stmt.statement = record_branch;
            record_stmt.reference_statement = scope_stmt;
            record_stmt.before = true;
            statement_list.push_back(record_stmt);

            SgVarRefExp* recorded_branch = buildVarRefExp(var_record_name);
            SgExpression* recorded_addr = buildCastExp (
                    buildAddressOfOp((SgExpression*) recorded_branch),
                    buildPointerType(buildIntType()));

            char var_type_size[32];
            snprintf (var_type_size, 32, "indigo__type_size_%d",
                    for_line_number);

            params.clear();
            params.push_back(new SgIntVal(fileInfo, line_number));
            params.push_back(idxv_expr);
            params.push_back(buildVarRefExp(var_type_size));
            params.push_back(buildVarRefExp(var_dir_name));
            params.push_back(common_alignment);
            params.push_back(recorded_addr);

            dir_call_stmt.statement = ir_methods::prepare_call_statement(
                    getEnclosingNode<SgBasicBlock>(scope_stmt),
                    function_name, params, scope_stmt);
            dir_call_stmt.reference_statement = if_stmt;
            dir_call_stmt.before = false;
            statement_list.push_back(dir_call_stmt);
    #endif
        }

        stmt = getNextStatement(stmt);
    }
}

void aligncheck_t::process_loop(SgScopeStatement* outer_scope_stmt,
        loop_info_t& loop_info, expr_map_t& loop_map,
        name_list_t& stream_list) {
    int line_number = loop_info.loop_stmt->get_file_info()->get_raw_line();

    loop_map[loop_info.idxv_expr] = &loop_info;
    if (contains_non_linear_reference(loop_info.reference_list)) {
        std::cerr << mprefix << "Found non-linear reference(s) in loop." <<
            std::endl;
        return;
    }

    // Instrument this loop only if
    // the loop header components have been identified.
    // Allow empty init expressions (which is always the case with while and
    // do-while loops).
    if (loop_info.idxv_expr && loop_info.test_expr && loop_info.test_expr) {
        loop_info.processed = true;

        Sg_File_Info *fileInfo =
            Sg_File_Info::generateFileInfoForTransformationNode(
                    ((SgLocatedNode*)
                     outer_scope_stmt)->get_file_info()->get_filenameString());

        instrument_loop_trip_count(fileInfo, loop_info);
        instrument_streaming_stores(fileInfo, loop_info);
        SgExpression* common_alignment = instrument_alignment_checks(fileInfo,
                outer_scope_stmt, loop_info, stream_list, loop_map);
        instrument_branches(fileInfo, loop_info.loop_stmt, loop_info.idxv_expr,
                common_alignment);
    }

    for(std::vector<loop_info_list_t>::iterator it =
                loop_info.child_loop_info.begin();
                it != loop_info.child_loop_info.end(); it++) {
        loop_info_list_t& loop_info_list = *it;
        for(loop_info_list_t::iterator it2 = loop_info_list.begin();
                it2 != loop_info_list.end(); it2++) {
            loop_info_t& loop_info = *it2;
            process_loop(outer_scope_stmt, loop_info, loop_map, stream_list);
        }
    }
}

void aligncheck_t::process_node(SgNode* node) {
    // Begin processing.
    analysis_profile.start_timer();

    // Since this is not really a traversal, manually invoke init function.
    atTraversalStart();

    loop_traversal->traverse(node, attrib());
    loop_info_list_t& loop_info_list = loop_traversal->get_loop_info_list();

    expr_map_t loop_map;
    name_list_t stream_list;

    for(loop_info_list_t::iterator it = loop_info_list.begin();
            it != loop_info_list.end(); it++) {
        loop_info_t& loop_info = *it;
        process_loop(loop_info.loop_stmt, loop_info, loop_map, stream_list);
    }

    // Since this is not really a traversal, manually invoke atTraversalEnd();
    atTraversalEnd();

    analysis_profile.end_timer();
    analysis_profile.set_loop_info_list(loop_info_list);
}

const analysis_profile_t& aligncheck_t::get_analysis_profile() {
    return analysis_profile;
}

const statement_list_t::iterator aligncheck_t::stmt_begin() {
    return statement_list.begin();
}

const statement_list_t::iterator aligncheck_t::stmt_end() {
    return statement_list.end();
}
