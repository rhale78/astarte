/*****************************************************************
 * File:    exprs/context.c
 * Purpose: Functions for handling context expressions
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
 * The functions in this file handle context declarations and    *
 * expressions.  The table of contexts that have been defined    *
 * is stored in tables/tableman.c.  The major part of this file  *
 * is replacing context expressions with more basic expressions. *
 *****************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../tables/tables.h"
#include "../exprs/expr.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../ids/ids.h"
#include "../generate/prim.h"
#include "../patmatch/patmatch.h"
#include "../parser/parser.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			local_context_table			*
 ****************************************************************
 * When a context expression is entered, local_context_table is *
 * set up.  local_context_table holds all potential translations.*
 * The value associated with id x is the expression that x	*
 * abbreviates. 						*
 ****************************************************************/

PRIVATE HASH2_TABLE* local_context_table = NULL;   


/****************************************************************
 *			actual_context_table			*
 ****************************************************************
 * When a context expression is entered, local_context_table is *
 * set up.  Suppose that a contexting identifier r is used, 	*
 * where r represents an expression E.  If E is just a constant *
 * or identifier, then nothing is done -- that constant or      *
 * identifier is substituted directly where r occurs.  If E is  *
 * more complicated than that, however,				*
 *								*
 *  (1) A new identifier r' is created, and an entry		*
 *      associating r' with key r is added to table 		*
 *      actual_context_table.					*
 *								*
 *  (2) A let expression					*
 *								*
 *       Let r' = E.						*
 *								*
 *      is added to context_expr_preamble.			*
 *								*
 * Each occurrence of r is replaced by r'.  When the context	*
 * expr has been completely read,  context_expr_preamble is	*
 * to the front of it.						*
 ****************************************************************/

PRIVATE HASH2_TABLE* actual_context_table = NULL; 


/****************************************************************
 *		       BUILD_LOCAL_CONTEXT_TABLE		*
 ****************************************************************
 * Build the local context table from the entries in list	*
 * context_list.  The context is for a value val_expr.  That is,*
 * we are processing a context expression of the form		*
 *								*
 *   Context op(val_expr) =>					*
 *     ...							*
 *   %Context							*
 ****************************************************************/

PRIVATE void
build_local_context_table(LIST *context_list, EXPR *val_expr, int line)
{
  LIST *p;
  EXPR *this_pair, *trans;
  HASH_KEY u;
  HASH2_CELLPTR h;
  LONG hash;
  char *name;

  local_context_table = NULL;
  for(p = context_list; p != NIL; p = p->tail) {
    this_pair = p->head.expr;
    name      = this_pair->E1->STR;
    u.str     = name;
    hash      = strhash(name);
    bump_expr(trans = copy_expr(this_pair->E2));
    set_expr_line(trans, line);

#   ifdef DEBUG
      if(trace_context_expr) {
	trace_t(504, name);
	print_expr(trans, 0);
      }
      if(trace_context_expr > 1) {
	trace_t(505, name);
	show_local_ids_tm();
	print_str_list_nl(local_id_names);
      }
#   endif

    /*---------------------------------------*
     * Check if this name is already in use. *
     *---------------------------------------*/

    if(is_visible_global_tm(name, current_package_name, TRUE)
       || is_local_id_tm(name)) {
      semantic_error1(ID_DEFINED_ERR, display_name(name), line);
    }
    h = insert_loc_hash2(&local_context_table, u, hash, eq);
    if(h->key.num != 0) {
      semantic_error1(ID_DEFINED_ERR, display_name(name), line);
    }
    else {

      /*---------------------------------------------*
       * Peform the substitution and put into table. *
       *---------------------------------------------*/

      h->key.str = name;
      bump_expr(h->val.expr = 
	        substitute_pm(NULL_E, NULL_E, trans, val_expr,0,0,0,0,line));
    }
    drop_expr(trans);
  }
}


/****************************************************************
 *			MAKE_CONTEXT_EXPR_HELP			*
 ****************************************************************
 * If *es is an identifier that is something that must be       *
 * replaced by a contexting expression, then rebind *es to the  *
 * appropriate expression.  Otherwise, do nothing to *es.	*
 * Always return 0.  						*
 ****************************************************************/

PRIVATE Boolean make_context_expr_help(EXPR **es) 
{
  EXPR_TAG_TYPE e_kind, trans_kind;
  char *name;
  LONG hash;
  HASH2_CELLPTR h;
  HASH_KEY u;
  EXPR *e, *id, *trans;

  e      = *es;
  e_kind = EKIND(e);
  if(e_kind != IDENTIFIER_E && e_kind != PAT_VAR_E) return 0;

  /*------------------------------------------------------------*
   * Look up this identifier in local_context_table.  If it is  *
   * not there, then do nothing to this id.			*
   *------------------------------------------------------------*/

  name = display_name(e->STR);

# ifdef DEBUG
    if(trace_context_expr > 1) {
      trace_t(508, name);
    }
# endif

  hash  = strhash(name);
  u.str = name;
  h     = locate_hash2(local_context_table, u, hash, eq);
  if(h->key.num == 0) return 0;

# ifdef DEBUG
    if(trace_context_expr > 1) trace_t(509, name);
# endif

  /*-------------------------------------------------------------*
   * This is an abbreviation.  Get the expression that is being  *
   * abbreviated.						 *
   *-------------------------------------------------------------*/

  trans = h->val.expr;

  /*------------------------------------------------------------------*
   * If the abbreviation is an identifier or a constant, just use it. *
   *------------------------------------------------------------------*/

  trans_kind = EKIND(trans);
  if(trans_kind == IDENTIFIER_E || trans_kind == CONST_E || 
     trans_kind == GLOBAL_ID_E) {
    if(e_kind != PAT_VAR_E) {
      set_expr(es,trans);
    }
    else {
      set_expr(es, new_expr1(PAT_VAR_E, trans, trans->LINE_NUM));
      (*es)->STR = trans->STR;
    }
  }

  /*------------------------------------------------------------*
   * If the abbreviation is not an identifier or constant, then *
   * check if it has been used before. If so, then just use the *
   * value in the table.  					*
   *								*
   * This form can only be used when e is an identifier.	*
   *------------------------------------------------------------*/

  else {
    if(e_kind == PAT_VAR_E) return 0;
    h = insert_loc_hash2(&actual_context_table, u, hash, eq);
    if(h->key.num != 0) {

#     ifdef DEBUG
        if(trace_context_expr) {
          trace_t(501, name);
	  if(trace_context_expr > 1) {
	    print_expr(h->val.expr, 0);
	  }
        }
#     endif

      set_expr(es, h->val.expr);
    }

    /*----------------------------------------------------------------*
     * This is the first use, so make a new identifier, and put it in *
     * the table. 						      *
     *----------------------------------------------------------------*/
 
    else { 
      id = fresh_id_expr(attach_prefix_to_id("context_", name, 1), 
			 e->LINE_NUM);
      h->key.str = name;
      bump_expr(h->val.expr = id);

#     ifdef DEBUG
        if(trace_context_expr) trace_t(502, name);
#     endif

      set_expr(es, id);
    }
  }

  return 0;
}


/****************************************************************
 *			MAKE_CONTEXT_EXPR			*
 ****************************************************************
 * Return the result of substituting for the context ids from	*
 * context cntxt in expression bdy.  The value being contexted  *
 * is val.  That is, return the representation of expression	*
 *								*
 *    Context cntxt val =>					*
 *      bdy							*
 *    %Context							*
 *								*
 * make_context_expr should be used in the id handler,		*
 * where the local id table is maintained, since it looks in	*
 * the local id table.  (Note that the local id table is only   *
 * meaningful while processing a declaration, since its meaning *
 * varies depending on what part of a declaration is being	*
 * processed.)							*
 *								*
 * The result of this function has the form			*
 *								*
 *    prepreamble						*
 *    preamble							*
 *    bdy							*
 *								*
 * where prepreamble gives a name to the result of expresssion  *
 * val, if necessary, and preamble binds identifiers that 	*
 * correspond to contexted ids.					*
 *								*
 * If the context does not exist and complain is true, then an  *
 * error message is printed.  But if complain is false, then a  *
 * nonexistent context is treated as an empty context.		*
 ****************************************************************/

EXPR* make_context_expr(char *cntxt, EXPR *val, EXPR *bdy, int line, 
			Boolean complain)
{
  LIST *context_list, *p;
  EXPR *this_pair, *result, *val_expr;
  EXPR  *pre_preamble, *context_expr_preamble;
  HASH2_CELLPTR h;
  HASH_KEY u;
  LONG hash;
  Boolean fnd;

# ifdef DEBUG
    if(trace_context_expr) {
      trace_t(503, cntxt);
      print_expr(bdy, 0);
    }
# endif

  /*------------------------------*
   * Get the list of definitions. *
   *------------------------------*/

  context_list = get_context_tm(cntxt, complain, &fnd, line);
  if(context_list == NIL) return bdy;

  /*------------------------------------------*
   * Put val into an identifier if necessary. *
   *------------------------------------------*/

  bump_expr(pre_preamble = force_eval_p(val, &val_expr));

  /*--------------------------------------------------------*
   * Build the local context table. This table tells what   *
   * will be substituted for each id, if that id is used.   *
   *--------------------------------------------------------*/

  build_local_context_table(context_list, val_expr, line);

  /*------------------------------------------------------------*
   * Build the actual context table, consisting of ids and      *
   * substitutions that are actually used.  It starts out	*
   * containing nothing, and gets entries when things are used  *
   * in the body.						*
   *------------------------------------------------------------*/

  actual_context_table = NULL;

  /*------------------------------------------------------------*
   * (1) Perform the abbreviation substitutions, possibly 	*
   * putting entries in actual_context_table.			*
   *------------------------------------------------------------*/

  bump_expr(bdy);
  scan_expr(&bdy, make_context_expr_help, 0);

  /*------------------------------------------------------------*
   * (2) The contexts themselves might refer to contexts that   *
   * were defined earlier.  Do replacements in them.		*
   *------------------------------------------------------------*/

  for(p = context_list; p != NIL; p = p->tail) {
    this_pair = p->head.expr;
    u.str     = this_pair->E1->STR;
    hash      = strhash(u.str);
    h         = locate_hash2(actual_context_table, u, hash, eq);
    if(h->key.num != 0) {
      h = locate_hash2(local_context_table, u, hash, eq);
      scan_expr(&(h->val.expr), make_context_expr_help, 0);
    }
  }

  /*---------------------*
   * Build the preamble. *
   *---------------------*/

  bump_expr(context_expr_preamble = same_e(hermit_expr, line));
  for(p = context_list; p != NIL; p = p->tail) {
    this_pair = p->head.expr;
    u.str     = this_pair->E1->STR;
    hash      = strhash(u.str);
    h         = locate_hash2(actual_context_table, u, hash, eq);
    if(h->key.num != 0) {
      HASH2_CELLPTR hh = locate_hash2(local_context_table, u, hash, eq);
      EXPR* let = new_expr2(LET_E, h->val.expr, hh->val.expr, line);
      SET_EXPR(context_expr_preamble, 
	       apply_expr(let, context_expr_preamble, line));
    }
  }

# ifdef DEBUG
    if(trace_context_expr) {
      trace_t(506);
      print_expr(context_expr_preamble, 0);
    }
# endif

  /*-------------------*
   * Build the result. *
   *-------------------*/

  bump_expr(result = 
	    possibly_apply(pre_preamble,
			   possibly_apply(context_expr_preamble, bdy, line),
			   line));

# ifdef DEBUG
    if(trace_context_expr) {
      trace_t(507);
      print_expr(result, 0);
    }
# endif

  scan_and_clear_hash2(&local_context_table, drop_hash_expr);
  scan_and_clear_hash2(&actual_context_table, drop_hash_expr);
  drop_expr(context_expr_preamble);
  drop_expr(pre_preamble);

  drop_expr(bdy);
  if(result != NULL) result->ref_cnt--;
  return result;
}




