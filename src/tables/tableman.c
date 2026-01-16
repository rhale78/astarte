/***************************************************************
 * File:    tables/tableman.c
 * Purpose: Table manager for miscellaneous tables.
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
 * This file manages tables of						*
 *									*
 *	operators and precedences					*
 *									*
 *	directory name bindings						*
 *									*
 *	advisory tables							*
 *									*
 *	role completions and other information about how to handle	*
 *      roles								*
 *									*
 *	context declarations						*
 *									*
 * 	information about which exceptions are normally trapped		*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../unify/unify.h"
#include "../classes/classes.h"
#include "../standard/stdfuns.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../exprs/expr.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../dcls/dcls.h"
#include "../lexer/modes.h"
#include "../parser/parser.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../generate/prim.h"
#ifdef SMALL_STACK
#  ifdef USE_MALLOC_H
#    include <malloc.h>
#  else
#    include <stdlib.h>
#  endif
# include "../alloc/allocate.h"
#endif
#ifdef DEBUG
# include "../debug/debug.h"
#endif

extern HASH2_TABLE *global_id_table;


/*==============================================================*
 *			    OP_TABLE				*
 *==============================================================*/

/****************************************************************
 * Standard binary operators are stored in standard_op_table.	*
 * Binary operators declared in packages are stored in the    	*
 * local operator tables of the packages, kept by fileinfo.c.	*
 *								*
 * Each binary operator table is a hash table that associates 	*
 * either a positive or negative number with each operator name.*
 * The possible values stored for a binary operator op with	*
 * token TOK are as follows.					*
 *								*
 *	TOK		op is a closed binary operator that was *
 *			not declared to override.		*
 *								*
 *	-TOK		op is an open binary operator that was  *
 *			not declared to override.		*
 *								*
 *	TOK+1000	op is a closed binary operator that was *
 *			declared to override.			*
 *								*
 *	-(TOK+1000)	op is an open binary operator that was  *
 *			declared to override.			*
 *								*
 * Standard unary operators are stored in 			*
 * standard_unary_op_table.  Unary operators declared in	*
 * packages are stored in the unary operator tables of the	*
 * packages, kept by fileinfo.c.				*
 ****************************************************************/

/****************************************************************
 *			    OPERATOR_CODE_TM			*
 ****************************************************************
 * Return the binary operator code of identifier s, or 0 if s 	*
 * is not a binary operator.  Set *open to TRUE if s is open,   *
 * and to FALSE if s is not open, when s is a binary operator.  *
 ****************************************************************/

int operator_code_tm(char *s, Boolean *l_open)
{
  HASH_KEY u;
  HASH2_CELLPTR h;
  LONG hash;

  /*------------------------------------------------------------*
   * First try the current table.  If not found there, try the	*
   * standard table.						*
   *------------------------------------------------------------*/

  hash  = strhash(s);
  u.str = id_tb10(s, hash);
  h     = locate_hash2(current_op_table, u, hash, eq);
  if(h->key.num == 0) {
    h = locate_hash2(standard_op_table, u, hash, eq);
  }

# ifdef DEBUG
    if(trace_lexical > 1) {
      trace_t(403, s, (h->key.num == 0) ? 0L : h->val.num);
    }
# endif

  /*--------------------------------------------------------------*
   * If there is no entry, return 0.  If there is an entry, then  *
   * correct for its extra information (open or overrides), and   *
   * get the token.						  *
   *--------------------------------------------------------------*/

  if(h->key.num == 0) return 0;

  else {
   int tok = toint(h->val.num);
   if(tok > 0) {
     *l_open = FALSE;
     if(tok > 1000) return tok - 1000;
     else return tok;
   }
   else {
     *l_open = TRUE;
     if(tok < -1000) return -tok-1000;
     else return -tok;
   }
  }
}


/****************************************************************
 *			    IS_UNARY_OP_TM			*
 ****************************************************************
 * Return TRUE if s is a unary operator.			*
 ****************************************************************/

Boolean is_unary_op_tm(char *s)
{
  return str_mem_hash1(standard_unary_op_table, s) ||
         str_mem_hash1(current_unary_op_table, s);
}


/****************************************************************
 *			    INSTALL_BINARY_OP			*
 ****************************************************************
 * Install u.str as a binary operator of kind tok in table 	*
 * *tbl.  u.str is open if l_open is true.  MODE tells the      *
 * mode of this declaration.  It is a safe pointer: it does not *
 * live longer than this function call.				*
 ****************************************************************/

PRIVATE void install_binary_op(HASH2_TABLE **tbl, HASH_KEY u, LONG hash, 
			      int tok, Boolean l_open, MODE_TYPE *mode)
{
  HASH2_CELLPTR h = insert_loc_hash2(tbl, u, hash, eq);

  if(has_mode(mode, OVERRIDES_MODE)) tok += 1000;

  if(l_open) tok = -tok;

  h->key.str = u.str;
  h->val.num = tok;
}


/****************************************************************
 *			    OPERATOR_TM				*
 ****************************************************************
 * Add s as an operator of kind tok.  s is open if l_open is	*
 * true.							*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

void operator_tm(char *s, int tok, Boolean l_open, MODE_TYPE *mode)
{
  HASH_KEY u;
  LONG hash;

  hash  = strhash(s);
  u.str = id_tb10(s, hash);

  /*-------------------------------------------------------------*
   * Insert a unary operator into either all local tables or the *
   * standard table, depending on whether we are in the preamble *
   * or not.		 					 *
   *-------------------------------------------------------------*/

  if(tok == UNARY_OP) {
    IMPORT_STACK* p = current_or_standard_frame();
    do {
      HASH1_CELLPTR h = insert_loc_hash1(&(p->unary_op_table), u, hash, eq);
      h->key.str = u.str;
      p = p->next;
    } while(p != NULL);
  }

  /*--------------------------------------------------------------*
   * Insert a binary operator into either all local tables or the *
   * standard table, depending on where whether we are in the	  *
   * preamble or not.						  *
   *								  *
   * If s was previously declared to be a binary operator with	  *
   * a different token, and this declaration does not override	  *
   * that one, then store BAD_OP instead of tok in the table.	  *
   *--------------------------------------------------------------*/

  else {
    Boolean       ll_open;
    IMPORT_STACK* p      = current_or_standard_frame();
    int           oldtok = operator_code_tm(u.str, &ll_open);

    if(oldtok != 0 && oldtok != tok && !has_mode(mode, OVERRIDES_MODE)) {
      tok = BAD_OP;
    }
    do {
      install_binary_op(&(p->op_table), u, hash, tok, l_open, mode);
      p = p->next;
    } while(p != NULL);
  }
}


/****************************************************************
 *			    SYM_OP_TM				*
 ****************************************************************
 * Same as operator_tm(std_id[sym], tok) where the operator is  *
 * closed and does not override.				*
 ****************************************************************/

void sym_op_tm(int sym, int tok)
{
  operator_tm(std_id[sym], tok, FALSE, 0);
}


/****************************************************************
 *			PRIM_UNOP_TM				*
 ****************************************************************
 * Declare name to be a unary operator.  This should only be    *
 * used when the memory pointed to by name is constant and	*
 * statically allocated.					*
 ****************************************************************/

void prim_unop_tm(char *name)
{
  operator_tm(stat_id_tb(name), UNARY_OP, FALSE, 0);
}


/****************************************************************
 *			SYM_PRIM_UNOP_TM			*
 ****************************************************************
 * Declare std_id[sym] to be a unary operator.			*
 ****************************************************************/

void sym_prim_unop_tm(int sym)
{
  operator_tm(std_id[sym], UNARY_OP, FALSE, 0);
}


/*==============================================================*
 *			    DIRECTORY NAME TABLE       		*
 *==============================================================*/

/****************************************************************
 *			DEF_IMPORT_DIR      			*
 ****************************************************************
 * Define id name to have value dir.				*
 ****************************************************************/

void def_import_dir(char *name, char *dir)
{
  HASH_KEY u;
  HASH2_CELLPTR h;
  LONG hash;

  IMPORT_STACK* p = current_or_standard_frame();
  hash = strhash(name);
  u.str = id_tb10(name, hash);
  do {
    h = insert_loc_hash2(&(p->import_dir_table), u, hash, eq);
    h->key.str = u.str;
    h->val.str = id_tb0(dir);
    p = p->next;
  } while(p != NULL);
}


/****************************************************************
 *			GET_IMPORT_DIR      			*
 ****************************************************************
 * Return the binding in import_dir_table of name, or null if	*
 * there is no such binding.					*
 ****************************************************************/

char* get_import_dir(char *name)
{
  HASH_KEY u;
  HASH2_CELLPTR h;
  LONG hash;

  hash  = strhash(name);
  u.str = id_tb10(name, hash);
  h     = locate_hash2(current_import_dir_table, u, hash, eq);

  if(h->key.num == 0) h = locate_hash2(standard_import_dir_table, u, hash, eq);
  if(h->key.num == 0) return NULL;
  return h->val.str;
}


/****************************************************************
 *			SET_IMPORT_DIR				*
 ****************************************************************
 * Set the import directory to name.  if is_id is true, then	*
 * name is an identifier name, and should be looked up in the	*
 * directory name table.  Otherwise, name is a constant.	*
 ****************************************************************/

void set_import_dir(char *name, Boolean is_id)
{
  char *d;

  if(is_id) {
    if(strcmp(name, STANDARD_AST_NAME) == 0) {
      d = STD_DIR;
    }
    else if(strcmp(name, "none") == 0) {
      d = NULL;
    }
    else d = get_import_dir(name);
  }
  else {
    d = name;
  }
  current_import_dir = d;
}


/*==============================================================*
 *		        ADVISORIES		       		*
 *==============================================================*/

/****************************************************************
 *			NO_TRO_TM				*
 ****************************************************************
 * Add id w to the no_tro_table, indicating that tail recursion	*
 * improvement should not be used on applications of w.		*
 *								*
 * all_status is false if this entry should be put into         *
 * no_tro_backout, assuming it is not already in the table.     *
 ****************************************************************/

void no_tro_tm(char *w, Boolean all_status)
{
  HASH_KEY u;
  HASH1_CELLPTR h;
  LONG hash;
  IMPORT_STACK* p = current_or_standard_frame();

  do {
    hash  = strhash(w);
    u.str = id_tb10(w, hash);
    h     = insert_loc_hash1(&(p->no_tro_table), u, hash, equalstr);
    if(h->key.str == NULL) {
      if(!all_status) {
        SET_LIST(p->no_tro_backout, str_cons(u.str, p->no_tro_backout));
      }
      h->key.str = u.str;
    }
    p = p->next;
  } while(p != NULL);
}


/****************************************************************
 *			NO_TRO_RESTORE_TM			*
 ****************************************************************
 * Remove everything in no_tro_backout from the no_tro table.   *
 ****************************************************************/

void no_tro_restore_tm(void)
{
  STR_LIST *strs;
  IMPORT_STACK* p = current_or_standard_frame();

  do {
    for(strs = p->no_tro_backout; strs != NIL; strs = strs->tail) {
      delete_str_hash1(p->no_tro_table, strs->head.str);
    }
    SET_LIST(p->no_tro_backout, NIL);
    p = p->next;
  } while(p != NULL);
}


/****************************************************************
 *			NO_TROQ_TM				*
 ****************************************************************
 * Return true if w is in no_tro_table.				*
 ****************************************************************/

Boolean no_troq_tm(char *w)
{
  return str_mem_hash1(current_no_tro_table, w) ||
	 str_mem_hash1(standard_no_tro_table, w);
}


/*==============================================================*
 *		        ROLES			       		*
 *==============================================================*/

PRIVATE HASH2_TABLE* role_completion_table          = NULL;
PRIVATE HASH1_TABLE* global_suppress_property_table = NULL;
HASH2_TABLE*         local_suppress_property_table  = NULL;


/****************************************************************
 *			ADD_ROLE_COMPLETION_TM			*
 ****************************************************************
 * Add completion rule s => t to role_completion_table.		*
 ****************************************************************/

void add_role_completion_tm(char *s, char *t)
{
  HASH2_CELLPTR h;
  HASH_KEY u;

  u.str = s;
  h = insert_loc_hash2(&role_completion_table, u, strhash(u.str), equalstr);
  if(h->key.num == 0) h->val.list = NIL;
  h->key.str = s;
  SET_LIST(h->val.list, str_cons(t, h->val.list));
}


/******************************************************************
 *			GET_ROLE_COMPLETION_LIST_TM		  *
 ******************************************************************
 * Return the list associated with name in role_completion_table. *
 ******************************************************************/

STR_LIST* get_role_completion_list_tm(char *name)
{
  HASH2_CELLPTR h;
  HASH_KEY u;

  u.str = name;
  h = locate_hash2(role_completion_table, u, strhash(u.str), equalstr);
  if(h->key.num == 0) return NULL;
  return h->val.list;
}


/****************************************************************
 *			GLOBAL_SUPPRESS_PROPERTY	        *
 ****************************************************************
 * Add property w to the global property suppression table.	*
 ****************************************************************/

void global_suppress_property(char *w)
{
  insert_str_hash1(&global_suppress_property_table, w);
}


/****************************************************************
 *			GLOBAL_UNSUPPRESS_PROPERTY	        *
 ****************************************************************
 * Delete property w from the global property suppression	*
 * table.							*
 ****************************************************************/

void global_unsuppress_property(char *w)
{
  delete_str_hash1(global_suppress_property_table, w);
}


/****************************************************************
 *			LOCAL_SUPPRESS_PROPERTY			*
 ****************************************************************
 * Suppress property w if suppress is true, and unsuppress it	*
 * if suppress is false.					*
 ****************************************************************/

void local_suppress_property(char *w, Boolean suppress)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  LONG hash;

  hash       = strhash(w);
  u.str      = id_tb10(w, hash);
  h          = insert_loc_hash2(&local_suppress_property_table, u, hash, eq);
  h->key.str = u.str;
  h->val.num = suppress;
}


/****************************************************************
 *			IS_SUPPRESSED_PROPERTY			*
 ****************************************************************
 * Return true if property w is suppressed.			*
 ****************************************************************/

Boolean is_suppressed_property(char *w)
{
  HASH2_CELLPTR h;
  HASH_KEY u;

  if(should_suppress_warning(err_flags.suppress_property_warnings)) {
    return TRUE;
  }
  u.str = w;
  h = locate_hash2(local_suppress_property_table, u, strhash(w), eq);
  if(h->key.num != 0) return tobool(h->val.num);
  if(str_mem_hash1(global_suppress_property_table, w)) return TRUE;
  return FALSE;
}


/*==============================================================*
 *			CONTEXT DCLS				*
 *==============================================================*/

PRIVATE HASH2_TABLE* context_table = NULL;

/****************************************************************
 *		        ADD_CONTEXT_TM				*
 ****************************************************************
 * Add pair (id,val) to context cntx, creating cntx if it does 	*
 * not already exist.						*
 *								*
 * If id is NULL, then only create the context if it does not	*
 * exist.							*
 ****************************************************************/

void add_context_tm(char *cntx, char *id, EXPR *val)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  EXPR_LIST *p;
  LONG hash;
  char *myid;

  /*-----------------------------------------*
   * Look up context cntx in the hash table. *
   * If it does not exist, create it.	     *
   *-----------------------------------------*/

  cntx  = display_name(cntx);
  hash  = strhash(cntx);
  u.str = id_tb10(cntx, hash);
  h     = insert_loc_hash2(&context_table, u, hash, eq);

  if(h->key.num == 0) {
    h->key.str  = u.str;
    h->val.list = NIL;
  }

  if(id != NULL) {

    /*--------------------------------------------*
     * If there is already a context id, and the  *
     * context expression is different, complain. *
     * But don't complain if the two expressions  *
     * are identical.				  *
     *--------------------------------------------*/

    id   = display_name(id);
    myid = attach_prefix_to_id("my-", id, 0);
    for(p = h->val.list; p != NIL; p = p->tail) {
      EXPR* entry = p->head.expr;
      if(entry->E1->STR == myid) {
	if(!expr_equal(entry->E2, val)) {
	  semantic_error2(DUP_CONTEXT_ERR, id, cntx, 0);
	}
	return;
      }
    }

    /*--------------------*
     * Install the entry. *
     *--------------------*/

    {EXPR* e = new_expr2(PAIR_E, id_expr(myid, 0), val, 0);
     SET_LIST(h->val.list, expr_cons(e, h->val.list));
    }
  }
}


/****************************************************************
 *		        COPY_CONTEXT_TM				*
 ****************************************************************
 * Add context pair 'pair' to the context list for context      *
 * to_context.							*
 *								*
 * String to_context should be a display name, and should be    *
 * in the id table, and hash must be strhash(to_context).	*
 ****************************************************************/

PRIVATE void copy_context_tm(char *to_context, EXPR *pair, LONG hash)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  EXPR_LIST *p;
  char *id;

  /*----------------------------------------------------*
   * Look up this to_context in the context table.	*
   * If it does not exist, create it.			*
   *----------------------------------------------------*/

  u.str = to_context;
  h     = insert_loc_hash2(&context_table, u, hash, eq);

  if(h->key.num == 0) {
    h->key.str  = u.str;
    h->val.list = NIL;
  }

  /*------------------------------------------------*
   * If there is already a context this id, and     *
   * the context expression is different, complain. *
   * But don't complain if the two expressions 	    *
   * are identical.			  	    *
   *------------------------------------------------*/

  id = pair->E1->STR;
  for(p = h->val.list; p != NIL; p = p->tail) {
    EXPR* entry = p->head.expr;
    if(entry->E1->STR == id) {
      if(!expr_equal(entry->E2, pair->E2)) {
        semantic_error2(DUP_CONTEXT_ERR, id, to_context, 0);
      }
      return;
    }
  }

  /*--------------------*
   * Install the entry. *
   *--------------------*/

  SET_LIST(h->val.list, expr_cons(pair, h->val.list));
}


/****************************************************************
 *			ADD_SIMPLE_CONTEXTS			*
 ****************************************************************
 * Add context							*
 *								*
 *    case f => f(target)					*
 *								*
 * to context context_id.  					*
 ****************************************************************/

void add_simple_context(char *context_id, char *f, int line)
{
  EXPR *body, *target, *f_as_expr;

  /*----------------------------------*
   * Add context 		      *
   *				      *
   *    case f => f(target)	      *
   *----------------------------------*/

  target            = typed_target(NULL_T, NULL_E, line);
  f_as_expr         = id_expr(f, line);
  bump_expr(body    = apply_expr(f_as_expr, target, line));
  add_context_tm(context_id, f, body);
  drop_expr(body);
}


/****************************************************************
 *			ADD_SIMPLE_CONTEXTS			*
 ****************************************************************
 * For each member f of list idlist, add context		*
 *								*
 *    case f => f(target)					*
 *								*
 * to context context_id.  					*
 ****************************************************************/

void add_simple_contexts(char *context_id, STR_LIST *idlist, int line)
{
  STR_LIST *p;

  for(p = idlist; p != NIL; p = p->tail) {
    add_simple_context(context_id, p->head.str, line);
  }
}


/****************************************************************
 *			DO_CONTEXT_INHERIT			*
 ****************************************************************
 * Copy each item from context from_context to context 		*
 * to_context.							*
 ****************************************************************/

void do_context_inherit(char *to_context, char *from_context)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  STR_LIST *p;
  LONG hash;

  /*-----------------------------------------*
   * Look up from_context in the hash table. *
   *-----------------------------------------*/

  from_context = display_name(from_context);
  hash         = strhash(from_context);
  u.str        = id_tb10(from_context, hash);
  h            = locate_hash2(context_table, u, hash, eq);

  if(h->key.num == 0) {
    semantic_error1(BAD_CONTEXT_ERR, from_context, 0);
    return;
  }

  /*--------------------------------------------------------*
   * Put to_context in the id table and get its hash value. *
   *--------------------------------------------------------*/

  to_context = id_tb0(display_name(to_context));
  hash       = strhash(to_context);

  /*--------------------*
   * Copy the entries.	*
   *--------------------*/

  for(p = h->val.list; p != NIL; p = p->tail) {
    copy_context_tm(to_context, p->head.expr, hash);
  }
}


/****************************************************************
 *			DO_CONTEXT_INHERITS			*
 ****************************************************************
 * Let context to_context inherit all members of each context   *
 * whose name is in list from_contexts.				*
 ****************************************************************/

void do_context_inherits(char *to_context, STR_LIST *from_contexts)
{
  STR_LIST *p;
  for(p = from_contexts; p != NIL; p = p->tail) {
    do_context_inherit(to_context, p->head.str);
  }
}


/****************************************************************
 *		        GET_CONTEXT_TM				*
 ****************************************************************
 * Get the list of exprs associated with context cntx, or NIL	*
 * if there is none.						*
 *								*
 * If complain is true and the context does not exist, then	*
 * print an error message about the missing context.		*
 * Line is the line number for the complaint.			*
 *								*
 * Also, variable *found is set to true if the context exists   *
 * and to false if it does not exist.
 ****************************************************************/

LIST* get_context_tm(char *cntx, Boolean complain, Boolean *found, int line)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  LONG hash;

  cntx  = display_name(cntx);
  hash  = strhash(cntx);
  u.str = id_tb10(cntx, hash);
  h     = locate_hash2(context_table, u, hash, eq);

  if(h->key.num == 0) {
    if(complain) {
      semantic_error1(BAD_CONTEXT_ERR, cntx, line);
    }
    *found = FALSE;
    return NIL;
  }
  else {
    *found = TRUE;
    return h->val.list;
  }
}


/*================================================================*
 * 			EXCEPTION TRAPPING			  *
 *================================================================*/

/***********************************************************************
 * Table exc_trap_table tells which exceptions are trapped by default. *
 ***********************************************************************/

PRIVATE HASH2_TABLE* exc_trap_table = NULL;

/****************************************************************
 *			EXC_TRAP_TM				*
 *			SYM_EXC_TRAP_TM				*
 ****************************************************************
 * If val is 0, then indicate that exception name ex_name is	*
 * usually not trapped.  If val is 1, indicate that it is	*
 * usually trapped.						*
 ****************************************************************/

void exc_trap_tm(char *ex_name, int val)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  LONG hash;

  hash       = strhash(ex_name);
  u.str      = id_tb10(ex_name, hash);
  h          = insert_loc_hash2(&exc_trap_table, u, hash, eq);
  h->key.str = u.str;
  h->val.num = val;
}

/*--------------------------------------------------------*/

void sym_exc_trap_tm(int sym, int val)
{
  exc_trap_tm(std_id[sym], val);
}


/****************************************************************
 *			USUALLY_TRAPPED_TM 			*
 ****************************************************************
 * Return 0 if ex_name is usually not trapped, 1 if it is	*
 * usually trapped, and 2 if it is not in the table.		*
 ****************************************************************/

int usually_trapped_tm(char *ex_name)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  LONG hash;

  hash  = strhash(ex_name);
  u.str = id_tb10(ex_name, hash);
  h     = locate_hash2(exc_trap_table, u, hash, eq);

  if(h->key.num == 0) return 2;
  return toint(h->val.num);
}


/*==============================================================*
 *			    INITIALIZATION			*
 *==============================================================*/

/****************************************************************
 *			INIT_TABLES_TM				*
 ****************************************************************/

void init_tables_tm()
{
  standard_import_frame     = allocate_import_stack();
  standard_op_table	    = create_hash2(5);
  standard_unary_op_table   = create_hash1(5);
  global_id_table	    = create_hash2(9);
}


/****************************************************************
 *			    CLEAR_TABLE_MEMORY			*
 ****************************************************************
 * Clear out information that tables are keeping to speed up    *
 * lookup.  This should be called when starting or ending an    *
 * import, or any time a new table context is entered.          *
 ****************************************************************/

void clear_table_memory(void)
{
  clear_class_table_memory();
}
