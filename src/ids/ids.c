/*****************************************************************
 * File:    ids/ids.c
 * Purpose: Functions for handling identifiers.
 * Author:  Karl Abrahamson
 *****************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 1997 Karl Abrahamson					*
 * All rights reserved.							*
 *									*
 * Redistribution and use in source and binary forms, with or without	*
 * modification, are permitted provided that the following conditions	*
 * are met:								*
 *									*
 * 1. Redistributions of source code must retain the above copyright	*
 *    notice, this list of conditions and the following disclaimer.	*
 *									*
 * 2. Redistributions in binary form must reproduce the above copyright	*
 *    notice in the documentation and/or other materials provided with 	*
 *    the distribution.							*
 *									*
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY		*
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE	*
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 	*
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE	*
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 	*
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 	*
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 	*
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,*
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE *
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,    * 
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.			*
 *									*
 ************************************************************************/
#endif

/*****************************************************************
 * This file contains functions that manage ids in the early     *
 * stages of compilation.  The main task is to replace different *
 * occurrences of a given identifier by the same EXPR node at    *
 * each occurrence, so that type inference will see all		 *
 * occurrences as the same thing.				 *
 *								 *
 * Additionally:						 *
 *								 *
 *   We mark pattern functions here, so that they		 *
 *   will be recognized as pattern functions later.		 *
 *								 *
 *   Global identifiers that are local to the current package    *
 *   get their names replaced with package-local names.		 *
 *****************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../classes/classes.h"
#include "../tables/tables.h"
#include "../exprs/expr.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../parser/parser.h"
#include "../ids/open.h"
#include "../ids/ids.h"
#include "../dcls/dcls.h"
#include "../patmatch/patmatch.h"
#include "../generate/prim.h"
#include "../standard/stdids.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE EXPR* process_ids_help(EXPR *e, int context, HASH2_TABLE **ex_b);
PRIVATE void process_pat_list_ids(HASH2_TABLE **ex_b);

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			outer_scope_num				*
 ****************************************************************
 * Each declaration has a special number that represents the    *
 * scope of the variables that are actually at an outer level,  *
 * not local to the declaration.  For let and define dcls,	*
 * the outer level is 0.  For execute declarations, the outer	*
 * scope is 1.  That number is held in outer_scope_num.		*
 *								*
 * This variable is used in lateids.c to set up before doing	*
 * process_scopes.						*
 ****************************************************************/

int outer_scope_num;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			this_pat_fun				*
 ****************************************************************
 * this_pat_fun is used to hold a pattern function for function	*
 * declare_all.							*
 * (Please improve this one day.)				*
 ****************************************************************/

PRIVATE EXPR* this_pat_fun;


/*******************************************************************
 *                     DO_PROCESS_IDS                              *
 *******************************************************************
 * See process_ids for what is done.  This procedure sets up the   *
 * context for process_ids on expression e, of kind 'kind', and	   *
 * cleans up the scopes afterwards.  It also prepares the OPEN_E   *
 * expressions in e so that they are available to process_ids.     *
 *******************************************************************/

void do_process_ids(EXPR *e, int kind)
{
  /*---------------------------------------------------------------*
   * do_process_ids is called on a declaration, so set the context *
   * for a declaration rather than an expression.		   *
   *---------------------------------------------------------------*/

  int context = DECLARATION_B;
  if(kind == LET_E || kind == TEAM_E) context |= LET_B;
  outer_scope_num = 0;
  if(kind == EXECUTE_E) {context = 0; outer_scope_num = 1;}

  /*--------------------------------------------------------------------*
   * If there is an open if, etc. expr in e, then set the EL1 and EL2 	*
   * fields of each OPEN_E node before starting process_ids.		*
   *--------------------------------------------------------------------*/

  if(seen_open_expr) set_open_lists(e);
  process_ids(e, context);

  /*------------------------------------------------------------------*
   * Throw away anything that remains in the local id table and exit. *
   *------------------------------------------------------------------*/

  pop_scope_to_tm(NIL);
}


/************************************************************************
 *                     PROCESS_PATTERN_IDS                             	*
 ************************************************************************
 * Mark the pattern functions in expression *pattern as pattern functions*
 * so that they will be recognized later.  Perform the action of	*
 * process_ids_help (see below) on *pattern, replacing *pattern with the*
 * result.  Then perform the action of process_ids_help on expression 	*
 * *after, replacing *after with the result, if after is not null.  	*
 * Drop the bindings done by after if drop_bindings is true.  Then 	*
 * declare the ids associated with pattern variables in pattern.	*
 ************************************************************************/

PRIVATE void process_pattern_ids(EXPR **pattern, EXPR **after, 
				Boolean drop_bindings, int context, 
				HASH2_TABLE **ex_b)
{
  LIST *pat_vars;

  /*---------------------*
   * Remember the scope. *
   *---------------------*/

  LOCAL_FINGER finger = get_local_finger_tm();

  /*------------------------------------------------------------*
   * First identify pattern functions in the pattern, and mark 	*
   * them so that they will be visible as pattern functions    	*
   * later.  Do this with pat_vars_ok set TRUE so that error    *
   * reports of unknown ids can realize that the unknown id     *
   * is possibly a pattern variable with ? missing.		*
   *------------------------------------------------------------*/

  pat_vars_ok = TRUE;
  mark_pat_funs(*pattern, FALSE, context);

  /*------------------------------------------------------------*
   * Push a new empty list onto pat_var_table.  Each time a	*
   * pattern variable is encountered while processing the	*
   * pattern, that pattern variable will be added to the top	*
   * list of pat_var_table.  Process the pattern with 		*
   * pat_vars_ok set to TRUE, since it can contain pattern	*
   * variables.						    	*
   *------------------------------------------------------------*/

  push_list(pat_var_table, NIL);
  set_expr(pattern, process_ids_help(*pattern, context, ex_b));
  pat_vars_ok = FALSE;
  
  /*------------------------------------------------------------*
   * Get the list of pattern variables that were found in the  	*
   * pattern, and pop pat_var_table.			    	*
   *------------------------------------------------------------*/

  bump_list(pat_vars = pat_var_table->head.list);
  pop(&pat_var_table);

  /*-------------------------------------------------------*
   * Process ids in the after expression, if there is one. *
   *-------------------------------------------------------*/

  if(after != NULL) {
    set_expr(after, process_ids_help(*after, context, ex_b));
  }

  /*--------------------------------------------------------------------*
   * If called for, pop the scope. Note that it is generally important  *
   * to pop the scope after processing after when the identifiers       *
   * defined in after are to be hidden.					*
   *--------------------------------------------------------------------*/

  if(drop_bindings) pop_scope_to_tm(finger);
  dcl_pat_vars_tm(pat_vars);
  drop_list(pat_vars);
}


/************************************************************************
 *                     PROCESS_IDS                                 	*
 ************************************************************************
 * Does the following processing on identifiers in e.		   	*
 *									*
 *  (1) Replaces each IDENTIFIER_E node by a node of kind		*
 *									*
 *    LOCAL_ID_E	for a local identifier				*
 *									*
 *    GLOBAL_ID_E	for a non-overloaded global identifier		*
 *									*
 *    OVERLOAD_E	for an overloaded global identifier		*
 *									*
 *    PAT_FUN_E		for a non-overloaded pattern function or 	*
 *			pattern constant				*
 *									*
 *  Overloaded pattern functions have kind OVERLOAD_E, but have their	*
 *  PRIMITIVE field set to PAT_FUN.  They are made into PAT_FUN_E nodes *
 *  while handling overloads during type inference.			*
 *									*
 *  (2) Makes all occurrences of a given local identifier in e point to *
 *  the same node.  This is done because all must have the same specific*
 *  type.  We want to retain line number information, so actually each  *
 *  occurrence of id that is not at the same line number where id was   *
 *  defined is replaced by a SAME_E node, where the SAME_E node has the *
 *  line number of this occurrence of id, and refers to the same node   *
 *  as the definition of e.						*
 *									*
 *  (3) When the parser builds a choose-matching expression, it also    *
 *  adds an entry to list 'choose_matching_lists', holding information  *
 *  about the target and patterns of the choose-matching expression.    *
 *  List choose_matching_lists is used to perform tests to see whether  *
 *  the patterns are exhaustive.  process_ids(e,-) replaces identifier  *
 *  nodes in the expressions in choose_matching_lists so that they are  *
 *  the same as corresponding nodes in e.  That way, type bindings done *
 *  during type inference will be available to the pattern matching     *
 *  exhaustiveness tester.						*
 *									*
 *  (4) Renames global identifiers that are local to the body of this   *
 *  package.  Identifier x in package p is renamed to p:x, when x is    *
 *  not exported.  This is done to local identifiers as well, but only  *
 *  because it is easy.  There is no real need to do this renaming for  *
 *  local identifiers in declarations.					*
 *									*
 *  (5) Reports an error if expression 'exception' occurs in a bad 	*
 *  context.  A bad context is one that is not in the else-part of a    *
 *  try-expression.							*
 *									*
 *  (6) Reports an error if a pattern variable is used in a bad context.*
 *									*
 * Parameter context is as follows.					*
 *									*
 *   DECLARATION_B (bit 0): 0 on expression				*
 *                          1 on declaration				*
 *									*
 *   TRY_ELSE_B    (bit 1): 0 if not in the else part of a try,		*
 *			    1 if in the else part of a try.		*
 *									*
 *   LET_B         (bit 2): 0 if not in a let dcl			*
 *                          1 if in a let dcl (so force ids outer)	*
 *									*
 * Returns the resulting expression. Changes are made in place, so e 	*
 * should be a tree on entry to process_ids.  On exit, e is generally	*
 * not a tree, since identifier nodes are shared.			*
 ************************************************************************/

EXPR* process_ids(EXPR *e, int context)
{
  /*-------------------*
   * Do the main work. *
   *-------------------*/

  HASH2_TABLE* ex_b = NULL;  /* Table of identifier bindings */

  EXPR* result = process_ids_help(e, context, &ex_b);
  bump_expr(result);

  /*------------------------------------------------------------------*
   * Process the ids in choose_matching_lists.  The expressions there *
   * can contain pattern variables, so permit them.		      *
   *------------------------------------------------------------------*/

  pat_vars_ok = TRUE;
  process_pat_list_ids(&ex_b);
  pat_vars_ok = FALSE;

  /*---------------------------------------------------*
   * Drop the reference counts of ex_b and free ex_b.  *
   *---------------------------------------------------*/

  scan_and_clear_hash2(&ex_b, drop_hash_expr);
  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			PROCESS_IDS_HELP			*
 ****************************************************************
 * Perform the actions of process_ids, above.  The extra 	*
 * parameter ex_b is a table that holds pairs (key,val) where   *
 * key is the address of an identifier node, and val is the     *
 * replacement for that node.  					*
 *								*
 * Normally, the new node for an identifier is found by 	*
 * cp_old_id_tm, which looks in the local and global 		*
 * environments, and uses the name of the identifier,		*
 * not the address of its node, to make a selection.  ex_b is   *
 * used in cases where an identifier is being processed for the *
 * second time (which can happen -- see below) and so that      *
 * replacements are available to process_pat_list_ids.	 	*
 ****************************************************************/

PRIVATE EXPR* process_ids_help(EXPR *e, int context, HASH2_TABLE **ex_b)
{
  EXPR_TAG_TYPE kind;
  EXPR *result;
  LOCAL_FINGER finger1;

  /*------------------------------*
   * A null e is an unused field. *
   *------------------------------*/

  if(e == NULL_E) return NULL_E;

  /*---------------------------------------------------------------*
   * Hold onto e.  Set the default result to e.  At several places *
   * we need to pop the scope back to where it was when e was      *
   * started, so we get a finger (finger1) to the current local    *
   * environment here, to avoid doing so many times.		   *
   *---------------------------------------------------------------*/

  bump_expr(e);
  kind    = EKIND(e);
  finger1 = get_local_finger_tm();
  bump_expr(result = e);

# ifdef DEBUG
    if(trace_ids) {
      trace_t(250, e, context, expr_kind_name[kind]);
      if(kind == IDENTIFIER_E) {
	trace_t(251, e->STR, e->STR);
      }
      tracenl();
      trace_t(402);
      print_str_list_nl(local_id_names);
    }
# endif

  /*---------------------------------*
   * Handle each kind of expression. *
   *---------------------------------*/

  switch(kind) {

    case SPECIAL_E:

      /*----------------------------------------------------------------*
       * A SPECIAL_E node might be 'exception'.  If so, then check that *
       * we are in the else-part of a try-expression.  If e is not      *
       * 'exception', then there are no ids here to worry about.        *
       *----------------------------------------------------------------*/

      if(e->PRIMITIVE == PRIM_EXCEPTION) {
        if((context & TRY_ELSE_B) == 0) {
          syntax_error(EXCEPTION_CONTEXT_ERR, e->LINE_NUM);
        }
      }
      break;

    case GLOBAL_ID_E:
    case OVERLOAD_E:
    case PAT_FUN_E:
    case CONST_E:

      /*-------------------------------------------------------------------*
       * An identifier that is a GLOBAL_ID_E, OVERLOAD_E or PAT_FUN_E node *
       * has already been handled. CONST_E nodes have no identifiers.	   *
       *-------------------------------------------------------------------*/

      break;

    case IDENTIFIER_E:
    case LOCAL_ID_E:

      /*----------------------------------------------------------*
       * IDENTIFIER_E nodes need to be converted to other kinds.  *
       * LOCAL_ID_E nodes should also be checked, to see whether  *
       * they need to be replaced.				  *
       *----------------------------------------------------------*/

      {HASH2_CELLPTR h;
       HASH_KEY u;
       int new_context = (kind == LOCAL_ID_E) ? 0 :
	                 (e->OUTER_ID_FORM)   ? 1 :
			 (context & LET_B)    ? 2 : 0;

       /*----------------------------------------------------------*
        * First check whether this identifier has been encountered *
	* before.						   *
        *----------------------------------------------------------*/

       u.num = (LONG) e;
       h     = insert_loc_hash2(ex_b, u, inthash(u.num), eq);

       /*----------------------------------------------------------*
        * If this identifier has not been dealt with before, then  *
	* use cp_old_id_tm to get its replacement.  Mark this      *
	* identifier as used, since it is now seen in a context    *
	* where it is being used, not defined.  Add this identifier*
	* and its new value into ex_b.	Also add the new value     *
        * as a key to ex_b, because sometimes when an id is seen   *
        * for the second time we see the new value.  This is       *
        * expecially true when doing process_pat_list_ids.	   *
        *----------------------------------------------------------*/

       if(h->key.num == 0) {
	 EXPR *ssresult;
         SET_EXPR(result, cp_old_id_tm(e, new_context));
	 ssresult          = skip_sames(result);
         ssresult->used_id = 1;
	 h->key.num        = u.num;
	 bump_expr(h->val.expr = result);
	 u.num      = (LONG) ssresult;
	 h          = insert_loc_hash2(ex_b, u, inthash(u.num), eq);
	 h->key.num = u.num;
	 bump_expr(h->val.expr = result);
       }

       /*----------------------------------------------------------*
        * If this identifier has been seen before (possibly during *
	* mark_pat_funs), then get the binding from table ex_b.    *
	* We must be careful not to create a cycle in SAME_E nodes.*
	* Keep in mind that *e will be replaced by result, and if  *
	* result refers to e itself via a SAME_E node, then such a *
	* replacement will create a cycle.			   *
        *----------------------------------------------------------*/

       else {
	 EXPR* this_id    = h->val.expr;
	 EXPR* ss_this_id = skip_sames(this_id);
	 if(ss_this_id == e) this_id = ss_this_id;
	 SET_EXPR(result, this_id);
       }
       break;
      }

    case PAT_VAR_E:
     {EXPR *p;
      HASH2_CELLPTR h;
      HASH_KEY u;
      char *name;

      /*-------------------------------------------------------------------*
       * If a pattern variable occurs in a context where pattern variables *
       * are not allowed, complain.  					   *
       *-------------------------------------------------------------------*/

      if(!pat_vars_ok) {
        semantic_error(NO_PAT_VARS_ERR, e->LINE_NUM);
        break;
      }

      /*-----------------------------------------------------------------*
       * If this is an anonymous pattern variable, then we don't need to *
       * do any replacement. 						 *
       *-----------------------------------------------------------------*/

      name = e->STR;
      if(name == NULL || name[0] == '#') break;

      /*------------------------------------------------------------------*
       * Check whether this pattern variable has already been dealt with, *
       * and has a binding in ex_b.  					  *
       *------------------------------------------------------------------*/

      u.num = (LONG) e;
      h     = insert_loc_hash2(ex_b, u, inthash(u.num), eq);

      /*----------------------------------------------------------------*
       * If there is no binding in ex_b, then add this pattern variable *
       * to the list of seen pattern variables for this pattern. (It    *
       * will be put into the local environment when the pattern is     *
       * finished.)  Add a binding for it to ex_b.  For reasons noted   *
       * under IDENTIFIER_E, add the new node as a key as well.		*
       *----------------------------------------------------------------*/

      if(h->key.num == 0) {
	p = pat_var_tm(e);
	SET_EXPR(result, wsame_e(p, e->LINE_NUM));
	h->key.num = u.num;
	bump_expr(h->val.expr = result);
	u.num      = (LONG) skip_sames(result);
	h          = insert_loc_hash2(ex_b, u, inthash(u.num), eq);
	h->key.num = u.num;
	bump_expr(h->val.expr = result);
      }

      /*----------------------------------------------------------*
       * If this identifier has been seen before (possibly during *
       * mark_pat_funs), then get the binding from table ex_b.    *
       * We must be careful not to create a cycle in SAME_E nodes.*
       * Keep in mind that *e will be replaced by result, and if  *
       * result refers to e itself via a SAME_E node, then such a *
       * replacement will create a cycle.			   *
       *----------------------------------------------------------*/

      else {
	EXPR* this_id    = h->val.expr;
	EXPR* ss_this_id = skip_sames(this_id);
	if(ss_this_id == e) this_id = ss_this_id;
	SET_EXPR(result, this_id);
      }
      break;
     }

    case SAME_E:

      /*-------------------------------------------------------*
       * A SAME_E node might indicate the beginning of a team. *
       *-------------------------------------------------------*/

       if(e->TEAM_MODE != 0) {

	 /*---------------------------------------------------------------*
	  * This is a team expression.  Get the list of ids being defined *
	  * in this team.						  *
	  *---------------------------------------------------------------*/

         EXPR_LIST *ids, *p;

	 bump_list(ids = get_id_list(e->E1, FALSE));

#        ifdef DEBUG
	   if(trace_ids) {
	     trace_t(252);
	     print_str_list_nl(ids);
	   }
#        endif

	 /*--------------------------------------------------------*
	  * Declare the ids being defined now, so that they can be *
	  * referred to while processing the team.		   *
	  *--------------------------------------------------------*/

         for(p = ids; p != NIL; p = p->tail) {
	   EXPR* hd = p->head.expr;
           EXPR* id_as_expr = new_id_tm(hd, NULL, LOCAL_ID_E, 
					context & DECLARATION_B);
	   if(context & DECLARATION_B) {
	     id_as_expr->TEAM_FORM = TEAM_DCL_NOTE;
	     id_as_expr->used_id = 1;
	   }
	   else {
             id_as_expr->TEAM_FORM = TEAM_NOTE;
	   }
         }
         drop_list(ids);
       }

       /*---------------------------------------------------------------*
        * Process the ids in the body of the SAME_E, and close off	*
	* the scope if called for.					*
	*---------------------------------------------------------------*/

       SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));
       if(e->SAME_CLOSE) pop_scope_to_tm(finger1);
       break;

    case IF_E:
    case TRY_E:
      {LOCAL_FINGER finger2, finger3;
       int else_context;

       /*-------------------------------------------------------*
        * Process the three subexpressions.  			*
	*							*
	* For an expression If a then b else c %If, 		*
	* b and c are processed in the scope that is just after *
	* executing a.						*
	*							*
 	* For expression Try a then b else c %Try, or for 	*
	* expression If a then b else c %If that represents a	*
	* case in a choose expression, bindings done by a are   *
	* visible to b, but not to c.				*
	*							*
	* In both cases, hide bindings done inside the if- or	*
	* try- expresssion from the outside.  			*
	* (See OPEN_E for open if and try exprs.) 		*
        *-------------------------------------------------------*/

       SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));
       finger2 = get_local_finger_tm();
       SET_EXPR(e->E2, process_ids_help(e->E2, context, ex_b));
       finger3 = (kind == IF_E && e->mark == 0) ? finger2 : finger1;
       pop_scope_to_tm(finger3);
       else_context = (kind == TRY_E && e->IF_MODE == 0) 
	                 ? context | TRY_ELSE_B : context;
       SET_EXPR(e->E3, process_ids_help(e->E3, else_context, ex_b));
       pop_scope_to_tm(finger1);
       break;
      }

    case OPEN_E:

       /*------------------------------------------------------------*
        * Form open If ... %If or open Try ... %Try 		     *
        * Bump up the temp fields to prevent warnings about ids that *
	* have been defined but not used.  Then process the ids      *
	* of the if- or try-expression.  Then restore the temp 	     *
	* fields.						     *
        *------------------------------------------------------------*/

       add_to_temps(e->EL1, 1);
       add_to_temps(e->EL2, 1);
       SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));
       add_to_temps(e->EL1, -1);
       add_to_temps(e->EL2, -1);
       local_declare_all(e->EL1);

       /*--------------------------------------------------------------*
        * Mark alternate ids used, so that code generator will not use *
	* DEAD_LET_I for them. 	That is, if identifier x is defined    *
	* in both the then and else branches, there will be two nodes  *
	* for x, one in the then branch and one in the else branch.    *
	* When x is used later, the node in the then branch is used.   *
	* So it would appear to the code generator that the node in    *
	* the else branch is defined but not used.		       *
        *--------------------------------------------------------------*/

       mark_alts_used(e);
       break;

    case PAIR_E:
    case LAZY_BOOL_E:
    case STREAM_E:

      /*--------------------------------------------------------*
       * Handle PAIR_E, LAZY_BOOL_E and STREAM_E expressions by *
       * processing the two children in separate scopes, and    *
       * popping the scope.  The only exception is an open      *
       * PAIR_E node, where the scope should not be popped. 	*
       *--------------------------------------------------------*/

      {Boolean should_pop = (kind != PAIR_E || !e->SCOPE);
       SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));
       if(should_pop) pop_scope_to_tm(finger1);
       SET_EXPR(e->E2, process_ids_help(e->E2, context, ex_b));
       if(should_pop) pop_scope_to_tm(finger1);
       break;
      }
           
    case WHERE_E:

      /*-----------------------------------------------------*
       * Expression a where b, presumably part of a pattern. *
       *-----------------------------------------------------*/

      {LOCAL_FINGER finger2;

       /*--------------------*
        * Process pattern a. *
        *--------------------*/

       SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));

       /*------------------------------------------------------------*
        * Process the where clause, b.  For this, we need to declare *
	* the pattern variables that are defined in a.  However, we  *
	* don't want to prevent their use later in this pattern, so  *
	* we remove them from the table after making them available  *
	* to the where clause.  It is important to suppress "id defined *
	* but not used" warnings caused by popping the pattern       *
	* variables, since otherwise we get spurious warnings.       *
        *------------------------------------------------------------*/

       finger2 = get_local_finger_tm();
       declare_patvars(e->E1);
       SET_EXPR(e->E2, process_ids_help(e->E2, context, ex_b));
       suppress_id_defined_but_not_used++;
       pop_scope_to_tm(finger2);
       suppress_id_defined_but_not_used--;
       break;
      }
           
    case TEST_E:

      /*------------------------------------------*
       * Handle {a else b} by processing a and b. *
       *------------------------------------------*/

      SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));
      SET_EXPR(e->E2, process_ids_help(e->E2, context, ex_b));
      break;

    case MANUAL_E:
      SET_EXPR(e->E1, 
	       process_ids_help(e->E1, 
				context & ~DECLARATION_B & ~LET_B, ex_b));
      break;

    case SEQUENCE_E:

      /*-----------------------------------------------------------------*
       * Handle a SEQUENCE_E node by doing its left subexpression, then  *
       * popping the scope if the SEQUENCE_E node is closed, then doing  *
       * the right subexpression.  Note that a closed SEQUENCE_E node    *
       * is only closed on its left subexpression.			 *
       *-----------------------------------------------------------------*/

      {SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));
       if(e->SCOPE == 0) pop_scope_to_tm(finger1);
       SET_EXPR(e->E2, process_ids_help(e->E2, context, ex_b));
       break;
      }

    case APPLY_E:

      /*-------------------------------------------------------------------*
       * Handle an application by doing the left and right subexpressions. *
       * If the expression is closed, pop the scope afterwards.            *
       *-------------------------------------------------------------------*/

      SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));
      SET_EXPR(e->E2, process_ids_help(e->E2, context, ex_b));
      if(e->SCOPE) pop_scope_to_tm(finger1);
      break;

    case TRAP_E:

      /*--------------------------------------------------------------------*
       * Handle a trap or untrap expression by handling each subexpression. *
       * If the expression is closed, pop the scope afterwards.             *
       *--------------------------------------------------------------------*/

      SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));
      SET_EXPR(e->E2, process_ids_help(e->E2, context, ex_b));
      if(e->SCOPE == 0) pop_scope_to_tm(finger1);
      break;

    case SINGLE_E:

      /*-----------------------------------------------------------*
       * All forms of SINGLE_E expression are handled by doing the *
       * subexpression and then popping the scope if closed.  An   *
       * isolate expression requires isolation to be done.         *
       *-----------------------------------------------------------*/

      SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));
      if(e->SCOPE == 0) pop_scope_to_tm(finger1);
      break;

    case LAZY_LIST_E:

      /*------------------------------------*
       * A lazy list always pops the scope. *
       *------------------------------------*/

      SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));
      pop_scope_to_tm(finger1);
      break;

    case AWAIT_E:

      /*---------------------------------------------------------*
       * Normally, doing an await expression involves doing each *
       * part and popping the scope after each part.  If this is *
       * an await that is only present to for pattern matching,  *
       * however, don't pop the scope. 				 *
       *---------------------------------------------------------*/

      {Boolean normal_await = e->extra == 0 && e->E2->pat == 0;

       SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));
       if(normal_await) pop_scope_to_tm(finger1);
       SET_EXPR(e->E2, process_ids_help(e->E2, context, ex_b));
       if(normal_await) pop_scope_to_tm(finger1);
       break;
      }

    case FUNCTION_E:

      /*--------------------------------------------------------------------*
       * Handle a function.  Identify the pattern functions in the pattern. *
       * Declare the ids associated with pattern variables in the pattern.  *
       *--------------------------------------------------------------------*/

      process_pattern_ids(&(e->E1), NULL, FALSE, context, ex_b);

      /*--------------*
       * Do the body. *
       *--------------*/

      SET_EXPR(e->E2, process_ids_help(e->E2, context, ex_b));

      /*------------------------------------------------*
       * Drop identifier bindings done in the function. *
       *------------------------------------------------*/

      pop_scope_to_tm(finger1);
      
      /*----------------------------------------------------------------*
       * Mark a subordinate functions that should share this function's *
       * scope.								*
       *----------------------------------------------------------------*/

      {EXPR* e2 = skip_sames(e->E2);
       if(EKIND(e2) == FUNCTION_E) e2->OFFSET = 1;
      }
      break;

    case LET_E:
      {int ldcl_context = context & DECLARATION_B;

       /*-----------------------------------------------------------*
        * Mark globally defined ids as used to suppress complaints. *
        *-----------------------------------------------------------*/

       if(ldcl_context) skip_sames(e->E1)->used_id = 1;

       /*------------------------------*
        * Process the right hand side. *
        *------------------------------*/

       SET_EXPR(e->E2, 
		process_ids_help(e->E2, context & ~DECLARATION_B, ex_b));

       /*---------------------------------------------------------*
        * Pop the scope back if a declaration or if a closed let. *
        *---------------------------------------------------------*/

       if(ldcl_context || e->SCOPE == 0) pop_scope_to_tm(finger1);

       /*--------------------------------------------------------*
        * Process the left hand side by making an entry into the *
        * local id table if this is a let expression.  For a     *
        * relet expression, the left-hand side is an old id, so  *
        * fetch from the local id table rather than making an    *
        * entry. 						 *
        *							 *
	* For a let declaration, make the id being defined either*
        * GLOBAL_ID_E or LOCAL_ID_E.  				 *
        *--------------------------------------------------------*/

       if(!ldcl_context) {
	 if(e->LET_FORM == 0) { /* A let */
	   EXPR* a = e;
	   while(EKIND(a->E1) == SAME_E) a = a->E1;
	   SET_EXPR(a->E1, new_id_tm(a->E1, NULL, LOCAL_ID_E, 0));
	 }
	 else { /* A relet */
	   SET_EXPR(e->E1, cp_old_id_tm(e->E1, 0));
	 }
       }
       else {
	 EXPR* e1 = skip_sames(e->E1);
	 if(e1->kind != GLOBAL_ID_E) {
           e1->kind = LOCAL_ID_E;
         }
       }
       break;
     }
      
    case DEFINE_E:
      {EXPR *id, *above_id;
       LOCAL_FINGER finger2;
       int ldcl_context, down_context, id_kind, team;

       /*-----------------------------------*
        * Get the identifier being defined. *
        *-----------------------------------*/

       above_id = e;
       while(EKIND(above_id->E1) == SAME_E) above_id = above_id->E1;
       id = above_id->E1;
       ldcl_context = context & DECLARATION_B;
       id->kind = LOCAL_ID_E;
       if(ldcl_context)  id->used_id = 1;

       /*-------------------------------------------------------------*
        * Declare the id up front, if not already declared as part of *
	* a team.  This is necessary since the definition might be    *
	* recursive. 						      *
        *-------------------------------------------------------------*/

       team = id->TEAM_FORM;
       if(team == 0 /* Not part of team and not a primitive */) {
	 EXPR *newid;
         id_kind = ldcl_context ? GLOBAL_ID_E : LOCAL_ID_E;
         SET_EXPR(above_id->E1, new_id_tm(id, NULL, id_kind, 0));
	 if(ldcl_context && id->kind == GLOBAL_ID_E) {
	   newid = above_id->E1;
	   newid->TEAM_FORM = id->TEAM_FORM;
	   newid->GIC       = id->GIC;
	   bump_type(newid->ty = id->ty);
	 }
       }
       finger2 = get_local_finger_tm();
       
       /*-------------------*
        * Process the body. *
        *-------------------*/

       down_context = ldcl_context ? context & ~DECLARATION_B & ~LET_B 
	                          : context;
       SET_EXPR(e->E2, process_ids_help(e->E2, down_context, ex_b));
       pop_scope_to_tm(finger2);
       break;
      }

    case MATCH_E:

      /*----------------------------------------------------------------*
       * Handle a MATCH_E expression.  Mark the pattern functions in 	*
       * the pattern, process the target, pop the bindings done in the 	*
       * target if this is a closed match, and put the bindings done   	*
       * in the pattern into the local environment.			*
       *----------------------------------------------------------------*/

      process_pattern_ids(&(e->E1), &(e->E2), !(e->SCOPE), context, ex_b);
      break;

    case EXPAND_E:

      /*-------------------------------*
       * Handle an expand declaration. *
       *-------------------------------*/

      {EXPR *id, *ssid;

       /*-------------------------------------------------------*
        * If the id being defined is local to the package body, *
	* give it its new name.					*
        *-------------------------------------------------------*/

       id = get_applied_fun(e->E1, TRUE);
       ssid = skip_sames(id);
       ssid->STR = new_name(ssid->STR, TRUE);
       ssid->used_id = 1;

       /*----------------------------------------------*
        * Handle expand f = g %expand by processing g. *
        *----------------------------------------------*/

       if(e->EXPAND_FORM) {
	 SET_EXPR(e->E2, process_ids_help(e->E2, context, ex_b));
	 break;
       }

       /*---------------------------------------------------------------*
        * For translation rules, change the id kind from IDENTIFIER_E 	*
        * to LOCAL_ID_E if necessary.  Record this_pat_fun for use	*
	* in declare_all.						*
        *---------------------------------------------------------------*/

       if(EKIND(id) == IDENTIFIER_E) {
	 id->kind = LOCAL_ID_E;
       }
       bump_expr(this_pat_fun = id);

       /*--------------------------------------------------------*
        * Handle an expansion with a translation by handling the *
	* left-hand side of the expansion as a pattern, then 	 *
	* declaring the patterns in it, and then handling the    *
	* right-hand side of the expansion with those pattern	 *
	* variables defined.					 *
        *--------------------------------------------------------*/

       pat_vars_ok = TRUE;
       push_list(pat_var_table, NIL);
       declare_all(e->E1);
       pat_vars_ok = FALSE;
       dcl_pat_vars_tm(pat_var_table->head.list);
       SET_EXPR(e->E2, process_ids_help(e->E2, context&~DECLARATION_B, ex_b));
       pop(&pat_var_table);
       break;
      }
       
    case LOOP_E:

      /*--------------------------------------------------------*
       * Handle a loop of the form 				*
       *    Loop p = i word cases %Loop 			*
       *--------------------------------------------------------*/

      {EXPR* e1 = e->E1;

       /*---------------------------------------------------*
        * If there is a p = i phrase, then process its ids. *
        *---------------------------------------------------*/

       if(e1 != NULL) {

	 /*-----------------------------------*
	  * Process i, and hide its bindings. *
	  *-----------------------------------*/

         SET_EXPR(e1->E2, process_ids_help(e1->E2, context, ex_b));
	 pop_scope_to_tm(finger1);

	 /*-------------------------------------------------------------*
	  * Process p, making its pattern variables visible. First mark *
	  * its pattern functions.  See the MATCH_E case.		*
	  *-------------------------------------------------------------*/

	 process_pattern_ids(&(e1->E1), NULL, FALSE, context, ex_b);
       }

       /*--------------------*
        * Process the cases. *
        *--------------------*/

       SET_EXPR(e->E2, process_ids_help(e->E2, context, ex_b));

       /*-------------------------------------------*
        * If the loop is closed, hide its bindings. *
        *-------------------------------------------*/

       if(e->OPEN_LOOP == 0) pop_scope_to_tm(finger1);
       break;
      }

    case RECUR_E:

      /*--------------------------------------------------------*
       * Handle a 'continue' part in a loop. Hide its bindings. *
       *--------------------------------------------------------*/

      if(e->E1 != NULL) {
	SET_EXPR(e->E1, process_ids_help(e->E1, context, ex_b));
      }
      pop_scope_to_tm(finger1);
      break;
      
    case FOR_E:

      /*------------------------------------------*
       * Handle a for loop 			  *
       *   For p from l do body %For 		  *
       *------------------------------------------*/

      /*-----------------------*
       * Handle the pattern p. *
       *-----------------------*/

      process_pattern_ids(&(e->E1), &(e->E2), FALSE, context, ex_b);

      /*---------------------------------------------------------*
       * Handle the body, and pop the scope, since a for loop is *
       * always closed.					         *
       *---------------------------------------------------------*/

      SET_EXPR(e->E3, process_ids_help(e->E3, context, ex_b));
      pop_scope_to_tm(finger1);
      break;

    case PAT_DCL_E:
      {EXPR *rule, *id, *ssid;
       LIST *pat_vars;
       LOCAL_FINGER finger2;
       Boolean is_pat_const;

       /*-------------------------------*
        * Get the name of the function. *
        *-------------------------------*/

       id   = e->E1;
       ssid = skip_sames(id);

       /*------------------------------------------------*
        * If the pattern function being defined is local *
        * to the package, give it its new name.		 *
        *------------------------------------------------*/

       ssid->STR     = new_name(ssid->STR, TRUE);
       ssid->used_id = 1;

       /*--------------------------------------------------------*
        * Handle pattern f = g %pattern by processing the r.h.s. *
        *--------------------------------------------------------*/

       if(e->PAT_DCL_FORM) {
	 SET_EXPR(e->E2, process_ids_help(e->E2, context, ex_b));
	 break;
       }

       /*---------------------------------------------------------------*
        * For translation rules, convert the function id to a LOCAL_ID_E*
	* if it is an IDENTIFIER_E -- type inference and table entry	*
	* will handle the local id correctly. Also record the local	*
	* environment position for popping. 				*
        *---------------------------------------------------------------*/

       finger2 = get_local_finger_tm();
       if(EKIND(id) == IDENTIFIER_E) {
	 id->kind = LOCAL_ID_E;
       }

       /*---------------------------------------------------------------*
        * Record the id so that it can be used in building the pattern  *
	* declaration. 							*
        *---------------------------------------------------------------*/

       bump_expr(this_pat_fun = id);

       /*--------------------------------------*
        * Handle definition with translations. *
        *--------------------------------------*/

       for(rule = skip_sames(e->E2); rule != NULL; 
	   rule = skip_sames(rule->E1)) {

         /*----------------------------------------------------*
          * Determine if pattern function or pattern constant. *
          *----------------------------------------------------*/

         is_pat_const = (EKIND(skip_sames(rule->E2)) == IDENTIFIER_E);

         /*-------------------------------------*
          * Declare the formals for a function. *
          *-------------------------------------*/

	 if(!is_pat_const) {
           pat_vars_ok = 1;
	   push_list(pat_var_table, NIL);
           declare_all(rule->E2);
           pat_vars_ok = 0;
	 }
	 else {
	   SET_EXPR(rule->E2, wsame_e(id, rule->E2->LINE_NUM));
	 }

         /*--------------------------*
          * Process the translation. *
          *--------------------------*/

         SET_EXPR(rule->E3, 
		  process_ids_help(rule->E3, context & ~DECLARATION_B, ex_b));

         /*--------------------------------------------------------------*
          * Check that the pattern variables were used correctly, for a  *
	  * function 							 *
          *--------------------------------------------------------------*/

	 if(!is_pat_const) {
           bump_list(pat_vars = pat_var_table->head.list);
	   pop(&pat_var_table);
	   if(!open_pat_dcl) check_pat_vars_declared_tm(pat_vars);
           check_for_bare_ids(rule->E3, pat_vars);
           drop_list(pat_vars);
	 }

         /*----------------------------*
          * Prepare for the next rule. *
          *----------------------------*/

         pop_scope_to_tm(finger2);
       }
       drop_expr(this_pat_fun);
       break;
      }
       
    case EXECUTE_E:

      /*-------------------------------------------------------*
       * Handle an execute declaration by processing its body. *
       *-------------------------------------------------------*/

      SET_EXPR(e->E1, process_ids_help(e->E1, 0, ex_b));
      break;

    case BAD_E:
    case UNKNOWN_ID_E:
       break; 

    default: 
      die(127, (char *) ((LONG) kind));
  }

# ifdef DEBUG
    if(trace_ids) {
      trace_t(253, e);
      trace_t(402);
      print_str_list_nl(local_id_names);
    }
# endif

  drop_expr(e);
  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			PROCESS_PAT_LIST_IDS			*
 ****************************************************************
 * Replace identifiers in choose_matching_lists by their	*
 * equivalents, found in table ex_b.  choose_matching_lists     *
 * is a list of lists of expressions.				*
 ****************************************************************/

PRIVATE void process_pat_list_ids(HASH2_TABLE **ex_b)
{
  LIST *p, *q;
 
  for(p = choose_matching_lists; p != NIL; p = p->tail) {
    for(q = p->head.list->tail; q != NIL; q = q->tail) {

#     ifdef DEBUG
        if(trace_pat_complete) {
	  trace_t(254);
	  print_expr(q->head.expr, 1);
	}
#     endif

      SET_EXPR(q->head.expr, process_ids_help(q->head.expr, 0, ex_b));
    }

#   ifdef DEBUG
      if(trace_pat_complete) trace_t(110);
#   endif

  }
}


/************************************************************************
 *			MARK_PAT_FUNS					*
 ************************************************************************
 * Mark global ids in e that are in a pattern function context		*
 * as pattern functions, by setting their kind to PAT_FUN_E.  Also	*
 * replace IDENTIFIER_E nodes that are global ids by appropriate	*
 * global id nodes.  Context is the declaration context, and influences	*
 * how IDENTIFIER_E nodes are translated.  If 'all' is true, 		*
 * then also mark local ids that are pattern functions			*
 * and that have GIC entries.  						*
 *									*
 * Return TRUE iff there are any pattern functions or pattern variables *
 * in e, and set the pat field of e if e contains any pattern functions *
 * or variables.							*
 *									*
 * Note: also complain if there is any kind of subexpression that	*
 * is not permitted in a pattern.					*
 ************************************************************************/

PRIVATE Boolean mark_pat_funs_help(EXPR *e, Boolean all, int context)
{
  EXPR_TAG_TYPE kind   = EKIND(e);
  Boolean       result = FALSE;     /* default */

# ifdef DEBUG
    if(trace_pm) {
      trace_t(440, e, toint(all), context, expr_kind_name[kind]);
    }
# endif

  switch(kind) {
    case APPLY_E:
      {EXPR *fun;
       EXPR_TAG_TYPE fun_kind;
       Boolean pat_arg, have_pat_fun;

       /*--------------------------------------------------------*
        * Get the function being applied.  (If e is f x y, then  *
        * fun is f.)  Also check the arguments to see if any of  *
        * them contain a pattern variable.  			 *
        *--------------------------------------------------------*/

       fun      = skip_sames(e->E1);
       fun_kind = EKIND(fun);
       pat_arg  = e->E2->pat;
       while(fun_kind == APPLY_E) {
         if(fun->E2->pat) pat_arg = TRUE;
         fun      = skip_sames(fun->E1);
         fun_kind = EKIND(fun);
       }
       have_pat_fun = pat_arg;

       /*---------------------------------------------------------------*
	* If the function contains any pattern variables, there is	*
	* trouble.  Complain that the pattern is bad.  An identifier    *
	* that has been marked as a pattern function will show up as a  *
	* pattern variable, so don't complain about it.			*
	*---------------------------------------------------------------*/

       if(fun->pat && 
	  fun_kind != PAT_FUN_E && 
	  fun_kind != GLOBAL_ID_E &&
	  fun_kind != OVERLOAD_E) {
	 expr_error(BAD_PAT_ERR, e);
       }

       /*-----------------------------------------------------------------*
	* If the function is an IDENTIFIER_E, then process it now.        *
        * Be careful: mark_pat_funs might change fun, so need to redo     *
        * skip_sames.							  *
        *-----------------------------------------------------------------*/

	if(fun_kind == IDENTIFIER_E) {
	  mark_pat_funs_help(fun, all, context);
          fun      = skip_sames(fun);
	  fun_kind = EKIND(fun);
	}

       /*-----------------------------------------------------------------*
        * If the function is an identifier, mark it as a pattern	  *
        * function if 							  *
	*								  *
 	*  (a) it is being applied to something that has pattern 	  *
	*      variables, or						  *
        *								  *
	*  (b) it is an assumed pattern function, or 			  *
        *								  *
	*  (c) it has a mark indicating that it appeared after modifier	  *
	*      'pattern', as in expression 				  *
        *								  *
        *          pattern f						  *
	*								  *
        * Mark it as a global id if it is a pattern function without	  *
	* reason to be a pattern function.  This is necessary to handle	  *
 	* pattern match substitution.	  				  *
        *-----------------------------------------------------------------*/

       if(   fun_kind == GLOBAL_ID_E 
          || fun_kind == OVERLOAD_E 
          || fun_kind == PAT_FUN_E
	  || (fun_kind == LOCAL_ID_E && all)) {

         if(!pat_arg) {
           have_pat_fun = fun_kind != LOCAL_ID_E && fun->MARKED_PF;
         }

         if(have_pat_fun) {

#          ifdef DEBUG
	     if(trace_infer || trace_ids || trace_pm) {
	       trace_t(441, fun->STR, fun->LINE_NUM);
	     }
#          endif

           /*-----------------------------------------------------------*
            * fun is a pattern function.  Mark it so by either by 	*
	    * setting its kind to PAT_FUN_E or by setting its 		*
	    * PRIMITIVE field to PAT_FUN.  The latter is done for 	*
	    * an OVERLOAD_E node, since we can't install the rules now  *
	    * for those.  Later (during type inference) this will cause *
	    * the OVERLOAD_E node to be changed to a PAT_FUN_E node.	*
            *-----------------------------------------------------------*/

	   result = TRUE;
	   fun->PRIMITIVE = PAT_FUN;
	   if(fun_kind == GLOBAL_ID_E) fun->kind = PAT_FUN_E;

	   /*--------------------------------------------------------*
	    * For local ids, install the main gic for the identifier *
	    * in the GIC field.  				     *
	    *--------------------------------------------------------*/

	   else if(fun_kind == LOCAL_ID_E || 
		   (fun_kind == PAT_FUN_E && fun->GIC == NULL)) {
	     fun->GIC = get_gic_tm(fun->STR, TRUE);
	   }
	 }

	 /*-------------------------------------------------------------*
          * It is possible to have a PAT_FUN_E node in an expansion	*
          * that now ends up in a context where it should become	*
	  * a global id because its actual argument has no pattern	*
	  * variables in it.  Fix it here.				*
	  *-------------------------------------------------------------*/

         else /* Not a pattern function */ {
           if(fun_kind == PAT_FUN_E) fun->kind = GLOBAL_ID_E;
         }
       }

       /*--------------------------------------------------------*
        * If the function being applied is a pattern function,   *
        * then do recursive calls on the arguments.  Don't do a  *
        * PAT_FUN_E node that is being applied.  It was handled  *
	* above.     						 *
        *--------------------------------------------------------*/

       if(have_pat_fun) {
         fun    = skip_sames(e);
	 result = TRUE;
	 while(EKIND(fun) == APPLY_E) {
	   mark_pat_funs_help(fun->E2, all, context);
	   fun = skip_sames(fun->E1);
	 }
       }
       break;
      }

    case GLOBAL_ID_E:
    case OVERLOAD_E:
      break;

    case PAT_FUN_E:
    case PAT_VAR_E:
      result = TRUE;
      break;

    case PAIR_E:
      if(mark_pat_funs_help(e->E2, all, context)) result = TRUE;
      /* No break - fall through to next case. */

    case WHERE_E:
    case SAME_E:
    case SINGLE_E:
      if(mark_pat_funs_help(e->E1, all, context)) result = TRUE;
      break;

    case AWAIT_E:
      if(e->E1 != NULL) {
	expr_error(BAD_PAT_ERR, e);
      }
      else result = mark_pat_funs_help(e->E2, all, context);
      break;

    default:
      if(e->pat) expr_error(BAD_PAT_ERR, e);
      break;
  }

# ifdef DEBUG
    if(trace_pm) {
      trace_t(442, e, result);
    }
# endif

  e->pat = result;
  return result;
}

/*----------------------------------------------------------------*
 * mark_assumed_patfuns finds all functions in e that are assumed *
 * to be pattern functions, and marks them as if they had been    *
 * marked with modifier pattern.  It also updates the pat fields  *
 * of subexpressions that contain assumed pattern fuinctions.     *
 *----------------------------------------------------------------*/

PRIVATE void mark_assumed_pat_funs(EXPR *e, Boolean all, int context)
{
  for(;;) {            /* for tail recursion */

    switch(EKIND(e)) {
      case APPLY_E:
      case PAIR_E:
        mark_assumed_pat_funs(e->E1, all, context);
	mark_assumed_pat_funs(e->E2, all, context);
	if(e->E1->pat || e->E2->pat) e->pat = 1;
	return;

      case IDENTIFIER_E:
        {EXPR *newid;
	 LONG rc;

	 /*-----------------------------------------------------------*
	  * An identifier in a pattern should be replaced by its node *
	  * from the identifier table.				      *
	  *-----------------------------------------------------------*/

	 bump_expr(newid = 
		   cp_old_id_tm(e, 
				e->OUTER_ID_FORM 
				   ? 1 
				   : (context & LET_B) ? 2 : 0));

	 /*-----------------------------------------------------------------*
	  * Destructively replace this identifier node's contents with the  *
	  * new identifier node contents.  Be careful to maintain reference *
	  * counts.							    *
	  *-----------------------------------------------------------------*/

	 drop_type(e->ty);
	 rc = e->ref_cnt;
	 *e = *newid;
	 e->ref_cnt = rc;
	 bump_type(e->ty);
	 if(EKIND(e) == SAME_E) bump_expr(e->E1);
	 drop_expr(newid);

	 /*--------------------------------------------------------*
	  * The result from cp_old_id_tm might be a GLOBAL_ID_E or *
	  * OVERLOAD_E node.  If that node is an assumed pattern   *
	  * function, we need to mark it.  So tail recur.	   *
	  *--------------------------------------------------------*/

	 break;
        }

      case GLOBAL_ID_E:
      case OVERLOAD_E:
	if(is_assumed_patfun_tm(e->STR)) {
	  e->MARKED_PF = TRUE;
	  e->pat = 1;
	}
	return;

      case SAME_E:
      case WHERE_E:
      case SINGLE_E:
	mark_assumed_pat_funs(e->E1, all, context);
	if(e->E1->pat) e->pat = 1;
	return;

      case AWAIT_E:
	mark_assumed_pat_funs(e->E2, all, context);
	if(e->E2->pat) e->pat = 1;
	return;

      default:
	return;
    }
  }
}

/*-------------------------------------------------------------*/

Boolean mark_pat_funs(EXPR *e, Boolean all, int context)
{
  mark_assumed_pat_funs(e, all, context);
  return mark_pat_funs_help(e, all, context);
}


/*****************************************************************
 *			DECLARE_PATVARS				 *
 *****************************************************************
 * Declare the identifiers associated with the pattern variables *
 * in e.							 *
 *****************************************************************/

void declare_patvars(EXPR *e)
{
  EXPR_TAG_TYPE kind;

  if(e == NULL_E) return;
  if(e->pat == 0) return;

  kind = EKIND(e);
  switch(kind) {
    case PAT_VAR_E:

      /*----------------------------------------------*
       * Don't declare an anonymous pattern variable. *
       *----------------------------------------------*/

      if(e->STR != NULL && e->STR[0] != '#') {
        new_id_tm(skip_sames(e->E1), NULL, LOCAL_ID_E, 0);
      }
      break;

    case APPLY_E:
    case PAIR_E:
      declare_patvars(e->E2);
      /* No break - fall through to next case. */

    case SAME_E:
    case WHERE_E:
      declare_patvars(e->E1);
      break;

    default: {}
  }
}


/****************************************************************
 * 			DECLARE_ALL				*
 ****************************************************************
 * Declare the identifiers in expression e, and set function	*
 * names and global constants to global ids.  Also set main 	*
 * function being applied in e to this_pat_fun.  Only suitable 	*
 * when e is a formal parameter to a pattern translation or 	*
 * expand rule.							*
 ****************************************************************/

PRIVATE EXPR* declare_all1(EXPR *e)
{
  EXPR_TAG_TYPE kind;

  if(e == NULL_E) return NULL;

  kind = EKIND(e);
  switch(kind) {
    case PAT_VAR_E:

      /*----------------------------------------------------------*
       * Add a pattern variable to the top list in pat_var_table. *
       * We don't want complaints about unused ids in these 	  *
       * formal expressions, so mark them used.			  *
       *----------------------------------------------------------*/

      pat_var_tm(e);
      if(e->E1 != NULL) e->E1->used_id = 1;
      return e;

    case IDENTIFIER_E:

      /*-----------------------------------------------------------*
       * An global should become a GLOBAL_ID_E or OVERLOAD_E node, *
       * but a non-global id should be entered as a new id, since  *
       * it is really a formal parameter to the pattern or expand  *
       * rule.							   *
       *-----------------------------------------------------------*/

      if(is_visible_global_tm(e->STR, current_package_name, TRUE)) {
	return cp_old_id_tm(e, 1);
      }
      new_id_tm(e, NULL, LOCAL_ID_E, 0);
      return e;

    case APPLY_E:

      /*---------------------------------------------------------*
       * A function name is necessarily already known. So if	 *
       * the function being applied is an identifier or     	 *
       * pattern function, use cp_old_id_tm to get it as a  	 *
       * global node.  If the function is not an identifier, 	 *
       * then just do recursive calls on the two subexpressions, *
       * as is done for PAIR_E nodes.				 *
       *---------------------------------------------------------*/

      {EXPR*         e1     = skip_sames(e->E1);
       EXPR_TAG_TYPE e1kind = EKIND(e1);
       if(e1kind == IDENTIFIER_E || e1kind == PAT_FUN_E) {
	 SET_EXPR(e->E1, cp_old_id_tm(e->E1, 0));
         SET_EXPR(e->E2, declare_all1(e->E2));
         return e;
       }
      }
      /* No break - fall through to next case. */

    case PAIR_E:
      SET_EXPR(e->E2, declare_all1(e->E2));
      /* No break - fall through to next case. */

    case SAME_E:
    case WHERE_E:
      SET_EXPR(e->E1, declare_all1(e->E1));
      /* No break - fall through to next case. */

    default: return e;
  }
}

/*--------------------------------------------------------------*/

void declare_all(EXPR *e)
{
  EXPR *sse, **sse1;

  if(e == NULL_E) return;
  sse = skip_sames(e);

  /*------------------------------------------------------------*
   * Find the function being applied at the top level.  It must *
   * be replaced by this_pat_fun.				*
   *------------------------------------------------------------*/

  if(EKIND(sse) == APPLY_E) {
    sse1 = &(e->E1);
    while(EKIND(*sse1) == SAME_E) sse1 = &((*sse1)->E1);
    if(EKIND(*sse1) == IDENTIFIER_E) {
      set_expr(sse1, wsame_e(this_pat_fun, sse->E1->LINE_NUM));
    }
    else declare_all(*sse1);
    SET_EXPR(e->E2, declare_all1(e->E2));
  }
  else declare_all1(e);
}


/****************************************************************
 *			LOCAL_DECLARE_ALL			*
 ****************************************************************
 * Declare the identifiers in list l.				*
 ****************************************************************/

void local_declare_all(EXPR_LIST *l)
{
  EXPR_LIST *p;

# ifdef DEBUG
    if(trace_locals) {
      trace_t(560);
      print_str_list_nl(l);
    }
# endif

  for(p = l; p != NIL; p = p->tail) {
    new_id_tm(p->head.expr, NULL, LOCAL_ID_E, 0);
  }
}


/************************************************************************
 *				ADD_TO_TEMPS				*
 ************************************************************************
 * Add k to the temp field of each expression in list l.  Ignore	*
 * if first id in l has a name that starts with character code 1.	*
 ************************************************************************/

void add_to_temps(EXPR_LIST *l, int k)
{
  EXPR_LIST *p;

  if(l == NIL || l->head.expr->STR[0] == 1) return;
  for(p = l; p != NIL; p = p->tail) {
    p->head.expr->ETEMP += k;
  }
}


