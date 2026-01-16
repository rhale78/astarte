/****************************************************************
 * File:    tables/loctbl.c
 * Purpose: Table manager for local ids.
 * Author:  Karl Abrahamson
 ****************************************************************/

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

/************************************************************************
 * This file is part of the management of entity ids.  It manages a     *
 * table of locally declared identifiers.  Other files are globtbl.c,   *
 * which manages globally declared ids, and idtbl.c, which is a top     *
 * level to both of these files.					*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../classes/classes.h"
#include "../exprs/expr.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../parser/parser.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 * 			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 * Each scope of a declaration has an index.			*
 * Structure scopes[i] provides information about scope number	*
 * i in the current declaration.				*
 *								*
 * Only true scopes are included. A true scope is the scope of	*
 * a function, lazy expression or lazy list expression.  Such	*
 * expressions need their own environment nodes.  The body of	*
 * an if expression, for example, is a scope, but is not a true	*
 * scope since it shares the environment node of its embedding	*
 * context.							*
 *								*
 * scopes[i].pred   is the index of the scope in which scope	*
 * 		    i is embedded.				*
 *								*
 * scopes[i].nonempty is true if there are any identifiers to	*
 *		    store in scope i.				*
 *								*
 * scopes[i].offset is only used during code generation.  At 	*
 *		    run time, the environment nodes are chained *
 *		    by links to embedding environments, so that *
 * 		    nonlocal ids can be found by following the	*
 *		    chain.  The last environment node in the	*
 *		    chain is the global environment node, where *
 *		    globals that are required by this 		*
 *		    declaration are stored.  scopes[i].offset	*
 * 		    is the number of environment nodes from	*
 *		    scope i to the global environment node at	*
 *		    run time, counting node i itself.   So if a *
 *		    local environment points to the global	*
 *		    environment, its offset is 1.  The global	*
 *		    environment node has offset 0.		*
 *								*
 * Array scopes is reallocated when necessary.  scopes_size is  *
 * the current physical size of the array.			*
 ****************************************************************/

struct scopes_struct* scopes;
int scopes_size;

/****************************************************************
 *			current_scope				*
 *			next_scope				*
 ****************************************************************
 * When inside a function that manages scopes, current_scope is *
 * the index in scopes of the current scope.  next_scope is	*
 * the next available scope index.				*
 ****************************************************************/

int current_scope;
int next_scope;

/****************************************************************
 *		suppress_id_defined_but_not_used		*
 ****************************************************************
 * suppress_id_defined_but_not_used is positive in contexts	*
 * where we should not issue "id defined but not used" warnings.*
 ****************************************************************/

Boolean suppress_id_defined_but_not_used = 0;

/****************************************************************
 *			pat_var_table				*
 *			pat_vars_ok				*
 ****************************************************************
 * pat_var_table is a stack of lists.  The top list is a list   *
 * of pattern variables that have been seen while processing	*
 * a pattern, but have not been entered into the local		*
 * id table.  Only after the match is completed will they be	*
 * entered into the local id table.  Each time a pattern is 	*
 * started, a new list is pushed onto this table.		*
 *								*
 * pat_vars_ok should be set TRUE in any context where pattern	*
 * variables can be added to pat_var_table, and FALSE when 	*
 * pattern variables are not allowed.				*
 ****************************************************************/

LIST_LIST* pat_var_table;
Boolean pat_vars_ok;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			local_id_names				*
 *			local_id_exprs				*
 ****************************************************************
 * local_id_names and local_id_exprs are parallel lists that	*
 * are thought of as forming a list of pairs (s,e), where s 	*
 * is the name of an identifier that is in scope, and e points	*
 * to the expr node of that identifier.	 These lists are used	*
 * both to know what identifiers are defined and so that, each	*
 * time a local identifier is seen, the same node can be used	*
 * to represent it in an EXPR tree (really acyclic graph).	*
 *								*
 * These lists are only used here and in idtbl.c.		*
 *								*
 * Parallel lists are used, rather than a single list of pairs, *
 * just because this makes memory management easier.		*
 ****************************************************************/

STR_LIST*  local_id_names = NIL;
EXPR_LIST* local_id_exprs = NIL;

/****************************************************************
 *			local_assume_names			*
 *			local_assume_types			*
 *			local_assume_finger_st			*
 ****************************************************************
 * local_assume_names, local_assume_types and local_assume_roles*
 * are parallel lists holding the local assumptions.		*
 * (Local assumptions are assume-declarations that are made	*
 * inside an expression.)  The i-th entry in the three lists	*
 * taken together form a triple (name,t,r) where identifier	*
 * name has been assumed to have type t and role r.		*
 *								*
 * local_assume_finger_st is a stack that holds pointers into 	*
 * local_assume_names.  To pop the assumptions, you pop back    *
 * to where the top of local_assume_finger_st points, popping	*
 * the same number of entries from local_assume_types and	*
 * local_assume_roles.  Entries are popped from the front of	*
 * the lists.							*
 ****************************************************************/

PRIVATE STR_LIST*  local_assume_names = NULL;
PRIVATE TYPE_LIST* local_assume_types = NULL;
PRIVATE ROLE_LIST* local_assume_roles = NULL;
PRIVATE LIST_STACK local_assume_finger_st = NULL;


/****************************************************************
 *			NEW_ID_TM				*
 ****************************************************************
 * Find the id node e beneath possible SAME_E nodes of eorig.	*
 * Enter e into the local id table.  If where = NULL, enter at	*
 * the top of the table.  If where != NULL, enter just before 	*
 * where in the local id table.  (Pointer where must point	*
 * into list local_id_names.)  Also:				*
 *								*
 *   If kind == LOCAL_ID_E, then check whether this id is	*
 *   already defined.  Normally, this involves testing against	*
 *   both current local and global ids.  If no_check_global	*
 *   is nonzero, however, don't check the global table.		*
 *   If this is already defined as a global, allow its 		*
 *   definition, but warn about shadowing a global if such	*
 *   warnings are not suppressed.				*
 *								*
 *   Change the kind of e to LOCAL_ID_E if successful.		*
 *								*
 *   Return e.							*
 ****************************************************************/

EXPR* new_id_tm(EXPR *eorig, STR_LIST *where, int kind, int no_check_global)
{
  char *name;
  int teamflag;
  Boolean ignore;
  STR_LIST *p;
  EXPR_LIST *q;
  EXPR *this_id_as_expr;

  this_id_as_expr = skip_sames(eorig);
  if(EKIND(this_id_as_expr) == UNKNOWN_ID_E) {
    this_id_as_expr->kind = LOCAL_ID_E;
  }
  name = this_id_as_expr->STR;

  /*------------------------------------------------------------*
   * Check whether this identifier is available. Be careful	*
   * about identifiers in teams.  They are defined by the	*
   * team handler, so will already be defined when the		*
   * definition is processed.  Don't complain that they are	*
   * already defined.						*
   *------------------------------------------------------------*/

  ignore = FALSE;
  teamflag = 0;
  if(name != NULL && kind == LOCAL_ID_E) {
    Boolean name_is_global = FALSE;
    int local_index = str_member(name, local_id_names);
    if(!no_check_global) {
      name_is_global = is_visible_global_tm(name, current_package_name, TRUE);
    }
    if(local_index != 0) {
      teamflag = list_subscript(local_id_exprs, local_index)
	         ->head.expr->TEAM_FORM;
      if(teamflag == TEAM_NOTE || teamflag == TEAM_DCL_NOTE) {
        ignore = TRUE;
      }
    }
    if(!ignore) {
      if(local_index != 0) {
	semantic_error1(ID_DEFINED_ERR, display_name(name), 
			eorig->LINE_NUM);
	return this_id_as_expr;
      }
      if(name_is_global) {
	warn_global_shadow(name, eorig->LINE_NUM);
      }
    }
  }
  
  /*-------------------------*
   * Insert this identifier. *
   *-------------------------*/

  if((teamflag == TEAM_NOTE || teamflag == TEAM_DCL_NOTE)
     && this_id_as_expr->TEAM_FORM == 0) {
    semantic_error1(ID_DEFINED_ERR, 
		    display_name(this_id_as_expr->STR), 
		    eorig->LINE_NUM);
  }

  /*---------------------------------------------------------*
   * Note that this id is no longer undefined, if in a team. *
   *---------------------------------------------------------*/

  this_id_as_expr->TEAM_FORM = 0;  

  /*--------------------------------*
   * Insert this id, if called for. *
   *--------------------------------*/

  if(!ignore) {
    if(where == NULL) {
       push_str(local_id_names, name);
       push_expr(local_id_exprs, this_id_as_expr);
    }
    else {
      for(p = local_id_names, q = local_id_exprs; 
	  p->tail != where; 
	  p = p->tail, q = q->tail) {
      }
      SET_LIST(p->tail, str_cons(name, p->tail));
      SET_LIST(q->tail, expr_cons(this_id_as_expr, q->tail));
    }
    this_id_as_expr->kind  = LOCAL_ID_E;
  }

# ifdef DEBUG
  if(trace_locals) {
    trace_t(395, name);
    print_local_env();
  }
# endif

  return this_id_as_expr;
}
  

/****************************************************************
 *			GET_LOCAL_ID_TM				*
 ****************************************************************
 * Return the expression node for identifier name, or NULL if	*
 * there is none.						*
 ****************************************************************/

EXPR* get_local_id_tm(char *name)
{
  STR_LIST *p;
  EXPR_LIST *q;

  for(p = local_id_names, q = local_id_exprs;
      p != NIL;
      p = p->tail, q = q->tail) {
    if(LKIND(p) == STR_L && p->head.str == name) {
      return q->head.expr;
    }
  }

  return NULL;
}


/****************************************************************
 *			IS_LOCAL_ID_TM				*
 ****************************************************************
 * Return TRUE if id is in the local id table, FALSE if not.	*
 ****************************************************************/

Boolean is_local_id_tm(char *id)
{
  return str_member(id, local_id_names);
}


/****************************************************************
 *			SHOW_LOCAL_IDS_TM			*
 ****************************************************************
 * Print the local ids, for debugging.				*
 ****************************************************************/

#ifdef DEBUG
void show_local_ids_tm(void)
{
  print_str_list_nl(local_id_names);
}
#endif


/****************************************************************
 *			PRINT_LOCAL_ENV				*
 ****************************************************************
 * Print the local environment, for debugging.			*
 ****************************************************************/

#ifdef DEBUG
void print_local_env(void)
{
  LIST *p, *q;

  fprintf(TRACE_FILE,"scope %d (", current_scope);
  for(p = local_id_names, q = local_id_exprs;
      p != NIL;
      p = p->tail, q = q->tail) {
    if(p->kind == INT_L) fprintf(TRACE_FILE,"%d,", top_i(p));
    else {
      fprintf(TRACE_FILE,
	      "%s(%p@%p(rc=%ld)), ", p->head.str, p->head.str, 
	      q->head.expr, q->head.expr->ref_cnt);
    }
  }
  fprintf(TRACE_FILE,")\n");
}
#endif


/****************************************************************
 *			GET_LOCAL_FINGER_TM			*
 ****************************************************************
 * Return a finger into the local id table, so that if we do	*
 *								*
 *    finger = get_local_finger_tm();				*
 *								*
 * then later do						*
 *								*
 *    pop_scope_to_tm(finger);					*
 *								*
 * the local id scope will be restored to what it was when we	*
 * got the finger.						*
 ****************************************************************/

LOCAL_FINGER get_local_finger_tm(void)
{
  return local_id_names;
}


/****************************************************************
 *			POP_SCOPE_TO_TM				*
 ****************************************************************
 * Pop the local scope to the point where local_id_names is	*
 * where.							*
 ****************************************************************/

void pop_scope_to_tm(STR_LIST *where)
{
  STR_LIST *p;
  EXPR_LIST *q;
  EXPR *id;

  for(p = local_id_names, q = local_id_exprs; 
    p != where; 
    p = p->tail, q = q->tail) {
    if(LKIND(q) == EXPR_L && EKIND(id = q->head.expr) == LOCAL_ID_E) {
      if(id->used_id == 0 && id->ETEMP == 0) {
	if(!suppress_id_defined_but_not_used &&
	   !should_suppress_warning(err_flags.suppress_unused_warnings)) {
	  char* spec_name = display_name(id->STR);
	  if(spec_name[0] != HIDE_CHAR && 
	     !prefix_ignore_case("unused", spec_name) &&
	     !suffix_ignore_case("unused", spec_name)) {
	    warn1(DEFINED_BUT_NOT_USED_ERR, spec_name, id->LINE_NUM);
	  }
	}
      }
    }
  }
  SET_LIST(local_id_names, p);
  SET_LIST(local_id_exprs, q);

# ifdef DEBUG
    if(trace_locals) {
      trace_t(396);
      print_local_env();
    }
# endif
}
 

/****************************************************************
 *			FIX_SCOPES_ARRAY_G			*
 ****************************************************************
 * Set scopes[i].offset to the offset of environment number i 	*
 * from the outer environment, not counting empty environments. *
 * This should be done after processing scopes (using		*
 * process_scopes) and before code generation is done.		*
 ****************************************************************/

void fix_scopes_array_g()
{
  int i, n;

# ifdef DEBUG
    if(trace > 1) {
      trace_t(207);
      for(i = 0; i < next_scope; i++) {
        trace_t(208, i, scopes[i].pred, scopes[i].nonempty);
      }
    }
# endif

  for(i = 0; i < next_scope; i++) {
    n = scopes[i].pred;
    if(n >= 0) scopes[i].offset = scopes[n].offset + scopes[i].nonempty;
  }

# ifdef DEBUG
    if(trace > 1) {
      trace_t(209);
        for(i = 0; i < next_scope; i++) {
          trace_t(210, i, scopes[i].pred, scopes[i].offset);
        }
    }
# endif
}


/*===============================================================*
 * 			LOCAL ASSUME TABLE			 *
 *===============================================================*/

/****************************************************************
 * The local assume table is represented by parallel lists      *
 * local_assume_names, local_assume_types and local_assume_roles.*
 * Generally, the n-th entry in each of these lists represent   *
 * an assumption of a given name having a given type and given  *
 * role.  There are a few special cases.			*
 *								*
 *   name		type	meaning				*
 *   ----		----	-------				*
 *   ";#"		t	natural constants have type t.	*
 *   ";##"		t	real constants have type t.	*
 *   x			NULL	x is an assumed pattern		*
 *			 	function 			*
 ****************************************************************/


/****************************************************************
 * 			PUSH_LOCAL_ASSUME_TM			*
 ****************************************************************
 * Push a local assumption assume x:t onto the local assume     *
 * stack.  							*
 ****************************************************************/

void push_local_assume_tm(char *x, RTYPE t)
{
  SET_LIST(local_assume_names, str_cons(x, local_assume_names));
  SET_LIST(local_assume_types, 
	   type_cons(copy_type(t.type, 1), local_assume_types));
  SET_LIST(local_assume_roles, role_cons(t.role, local_assume_roles));
}


/****************************************************************
 * 			POP_LOCAL_ASSUMES_TM			*
 ****************************************************************
 * Pop local assumes back to the top of local_assume_finger_st. *
 ****************************************************************/

void pop_local_assumes_tm(void)
{
  STR_LIST*  finger = top_list(local_assume_finger_st);
  STR_LIST*  names = local_assume_names;
  TYPE_LIST* types = local_assume_types;  
  ROLE_LIST* roles = local_assume_roles;

  while(names != NULL && names != finger) {
    names = names->tail;
    types = types->tail;
    roles = roles->tail;
  }

  SET_LIST(local_assume_names, names);
  SET_LIST(local_assume_types, types); 
  SET_LIST(local_assume_roles, roles);
}


/****************************************************************
 * 			GET_LOCAL_ASSUME_TM			*
 ****************************************************************
 * Return the local assumption for x, or NULL if there is none. *
 * String x must be in the string table.			*
 ****************************************************************/

RTYPE get_local_assume_tm(char *x)
{
  STR_LIST*  names = local_assume_names;
  TYPE_LIST* types = local_assume_types;
  ROLE_LIST* roles = local_assume_roles;
  RTYPE result;

  /*---------------------------------------------------------------------*
   * Look for x in the names list.  Skip over any with a NULL associated *
   * type, since they are pattern assumes.				 *
   *---------------------------------------------------------------------*/

  while(names != NULL && (names->head.str != x || types->head.type == NULL)) {
    names = names->tail;
    types = types->tail;
    roles = roles->tail;
  }

  /*--------------------------------*
   * Get the assumed type and role. *
   *--------------------------------*/

  if(names != NULL) {
    result.type = copy_type(types->head.type, 1);
    result.role = roles->head.role;
  }
  else {
    result.type = NULL;
    result.role = NULL;
  }
  return result;
}


/****************************************************************
 * In the local assume table, we will use id ";#" to indicate	*
 * an assumption of the form 					*
 *								*
 *    Assume 0: T.						*
 * 								*
 * and id ";##" t indicate an assumption of the form		*
 *								*
 *    Assume 0.0: T.						*
 ****************************************************************/

PRIVATE char nat_const_id[]  = ";#";
PRIVATE char real_const_id[] = ";##";

/****************************************************************
 * 			PUSH_LOCAL_CONST_ASSUME_TM		*
 ****************************************************************
 * Push an assumption about natural constants if kind = 0, and  *
 * about real constants if kind = 1.				*
 ****************************************************************/

void push_local_const_assume_tm(int kind, TYPE *t)
{
  char* name = kind ? real_const_id : nat_const_id;
  RTYPE rt;
  rt.type = t;
  rt.role = NULL;
  push_local_assume_tm(name, rt); 
}


/****************************************************************
 * 			GET_LOCAL_CONST_ASSUME_TM		*
 ****************************************************************
 * Return the local assumption for constants of given kind, 	*
 * or NULL if there is none.  kind is 0 for natural constants   *
 * and 1 for real constants.					*
 ****************************************************************/

TYPE* get_local_const_assume_tm(int kind)
{
  char* name = kind ? real_const_id : nat_const_id;
  RTYPE rt   = get_local_assume_tm(name);
  return rt.type;
}


/****************************************************************
 * 			PUSH_LOCAL_PATTERN_ASSUME_TM		*
 ****************************************************************
 * Push an assumption x: pattern.  				*
 ****************************************************************/

void push_local_pattern_assume_tm(char *x)
{
  RTYPE rt;
  rt.type = NULL;
  rt.role = NULL;
  push_local_assume_tm(x, rt); 
}


/****************************************************************
 * 			GET_LOCAL_PATTERN_ASSUME_TM		*
 ****************************************************************
 * Return TRUE if x is an assumed pattern function, and FALSE   *
 * otherwise.  String x must be in the string table.		*
 ****************************************************************/

Boolean get_local_pattern_assume_tm(char *x)
{
  STR_LIST*  names = local_assume_names;
  TYPE_LIST* types = local_assume_types;

  /*----------------------------------------------------------*
   * Look for x in the names list, associated with NULL type. *
   *----------------------------------------------------------*/

  while(names != NULL && (names->head.str != x || types->head.type != NULL)) {
    names = names->tail;
    types = types->tail;
  }

  return (names != NULL);
}


/****************************************************************
 * 			PUSH_ASSUME_FINGER_TM			*
 ****************************************************************
 * Push local_assume_names onto local_assume_finger_st.		*
 ****************************************************************/

void push_assume_finger_tm(void)
{
  push_list(local_assume_finger_st, local_assume_names);
}


/****************************************************************
 * 			POP_ASSUME_FINGER_TM			*
 ****************************************************************
 * Pop local_assume_finger_st.					*
 ****************************************************************/

void pop_assume_finger_tm(void)
{
  pop(&local_assume_finger_st);
}


/****************************************************************
 * 		POP_LOCAL_ASSUMES_AND_FINGER_TM			*
 ****************************************************************/

void pop_local_assumes_and_finger_tm(void)
{
  pop_local_assumes_tm();
  pop_assume_finger_tm();
}

/****************************************************************
 * 			CLEAR_LOCAL_ASSUMES_TM			*
 ****************************************************************
 * Clear all local assume information.				*
 ****************************************************************/

void clear_local_assumes_tm(void)
{
  SET_LIST(local_assume_finger_st, NULL);
  SET_LIST(local_assume_names, NULL);
  SET_LIST(local_assume_types, NULL);
  SET_LIST(local_assume_roles, NULL);
}


/****************************************************************
 * 			INIT_LOCTBL_TM				*
 ****************************************************************
 * Prepare the local id table at the start of the compiler.	*
 ****************************************************************/

void init_loctbl_tm(void)
{
  scopes = (struct scopes_struct *)
           alloc(MAX_NUM_SCOPES * sizeof(struct scopes_struct));
  scopes_size = MAX_NUM_SCOPES;
}


/****************************************************************
 * 			INIT_DCL_TM				*
 ****************************************************************
 * Prepare the local id table at the start of a declaration.	*
 ****************************************************************/

void init_dcl_tm(int outer_scope)
{
  SET_LIST(local_id_names, NIL);
  SET_LIST(local_id_exprs, NIL);
  current_scope      = outer_scope;
  next_scope         = outer_scope + 1;
  scopes[0].pred     = -1;
  scopes[0].nonempty = 0;
  scopes[0].offset   = 0;
}


/*===============================================================
 * 			PATTERN VARIABLE TABLE			*
 *===============================================================*/


/****************************************************************
 *			    PAT_VAR_TM				*
 ****************************************************************
 * Add pattern variable e to the pattern variable table.	*
 ****************************************************************/

EXPR* pat_var_tm(EXPR *e)
{
  char *name;
  EXPR *result;
  LIST_LIST *p;
  EXPR_LIST *q;

  if(!pat_vars_ok) {
    syntax_error(NO_PAT_VARS_ERR, e->LINE_NUM);
    return bad_expr;
  }

  bump_expr(e);
  name = e->STR;

  /*-------------------------------------*
   * Ignore anonymous pattern variables. *
   *-------------------------------------*/

  if(name == NULL || name[0] == '#') {
    drop_expr(e); 
    return e;
  }

# ifdef DEBUG
    if(trace_locals) trace_t(397, name);
# endif

  /*------------------------------------------------------*
   * See if this variable is already in the table. If so, *
   * and if in another scope, move to current scope. 	  *
   *------------------------------------------------------*/

  for(p = pat_var_table; p != NIL; p = p->tail) {
    for(q = p->head.list; q != NIL; q = q->tail) {
      if(name == q->head.expr->STR) {

	/*------------------------------------------------------*
	 * This identifier is already in the table.  That means *
	 * we are looking at a second or greater occurrence of  *
	 * this pattern variable.  Mark it used, since an	*
	 * equality check will be performed between the two	*
	 * occurrences.	 Return the pattern variable from the	*
	 * table.						*
	 *------------------------------------------------------*/

	result = q->head.expr;
	if(result->E1 != NULL) result->E1->used_id = 1;
	if(e->ty != NULL_T) {
	  result = new_expr1t(SAME_E, result, e->ty, result->LINE_NUM);
        }
        drop_expr(e);

	/*-------------------------------------------------------*
	 * If we are not reading the top level of pat_var_table, *
	 * then we need to copy this pattern variable to the 	 *
	 * top level.						 *
	 *-------------------------------------------------------*/

	if(p != pat_var_table) {
	  SET_LIST(pat_var_table->head.list, 
		   expr_cons(result, pat_var_table->head.list));
        }

#       ifdef DEBUG
          if(trace_locals) {
	    trace_t(398, name);
	    print_expr(result, 1);
          }
#       endif

        return result;
      }
    }
  }

  /*-----------------------------------------------------------*
   * We only get here if this pattern variable is not 	       *
   * in the table.  Insert this variable at the current scope. *
   *-----------------------------------------------------------*/

  e->STR     = name;
  e->E1->STR = name;
  SET_LIST(pat_var_table->head.list, expr_cons(e, pat_var_table->head.list));

# ifdef DEBUG
    if(trace_locals) {
      trace_t(399, name);
    }
# endif

  e->ref_cnt--;
  return e;
}


/****************************************************************
 *			    DCL_PAT_VARS_TM			*
 ****************************************************************
 * Declare all of the pattern variables in list l by adding	*
 * their ids to the local id table.				*
 ****************************************************************/

void dcl_pat_vars_tm(EXPR_LIST *l)
{
  EXPR_LIST *q;
  EXPR *e;

# ifdef DEBUG
    if(trace_locals) trace_t(400);
# endif

  for(q = l; q != NIL; q = q->tail) {
    e = skip_sames(q->head.expr);
    SET_EXPR(e->E1, new_id_tm(skip_sames(e->E1), NULL, LOCAL_ID_E, 0));
  }
}


/****************************************************************
 *		     CHECK_PAT_VARS_DECLARED_TM			*
 ****************************************************************
 * Check that all of the pattern variables in list l have been  *
 * added to the local id table.  For each that is not found,	*
 * issue a warning that the pattern variable was not bound.	*
 *								*
 * Ignore compiler-generated patterns, whose names start with	*
 * HIDE_CHAR.							*
 ****************************************************************/

void check_pat_vars_declared_tm(EXPR_LIST *l)
{
  EXPR_LIST *q, *exprs;
  STR_LIST *names;
  EXPR *this_expr;
  char *this_name;

# ifdef DEBUG
    if(trace_locals) {
      trace_t(401);
      print_expr_list(l);
      tracenl();
      trace_t(402);
      print_local_env();
      tracenl();
    }
# endif

  for(q = l; q != NIL; q = q->tail) {
    this_expr = q->head.expr;
    this_name = this_expr->STR;

    /*--------------------*
     * Ignore hidden ids. *
     *--------------------*/

    if(this_name != NULL && this_name[0] != HIDE_CHAR) {
      for(names = local_id_names, exprs = local_id_exprs; 
	  names != NIL; 
	  names = names->tail, exprs = exprs->tail) {
        if(names->head.str == this_name) goto next_in_l;
      }
      warn1(PATVAR_NOT_BOUND_ERR, display_name(this_name), 
		      this_expr->LINE_NUM);
    }
    next_in_l: {}
  }
}

