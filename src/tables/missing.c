/****************************************************************
 * File:    tables/missing.c
 * Purpose: Translator table manager for missing declarations.
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

/****************************************************************
 * This file manages missing declarations.  The tables are      *
 * stored in structures declared here.  There are four kinds	*
 * of missing declaration.					*
 *								*
 *   Missing x:T						*
 *								*
 *   Missing{strong} x:T					*
 *								*
 *   Missing{imported} x:T					*
 *								*
 *   Missing{hide} x:T						*
 ****************************************************************/

#include <string.h>
#include <memory.h>
#include "../misc/misc.h"
#include "../tables/tables.h"
#include "../dcls/dcls.h"
#include "../exprs/expr.h"
#include "../exprs/prtexpr.h"
#include "../classes/classes.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../unify/unify.h"
#include "../error/error.h"
#include "../lexer/modes.h"
#include "../parser/parser.h"
#include "../patmatch/patmatch.h"
#include "../generate/prim.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE void check_all_definitions_for_strong_missing(char *name, TYPE *ty);

/****************************************************************
 * 			PRIVATE VARIABLES			*
 ****************************************************************/

/************************************************************************
 *			missing_tables					*
 ************************************************************************
 * missing_tables is an array of tables of missing types for ids.	*
 *									*
 * missing_tables[BASIC_MISSING_TABLE] is the default table.		*
 *									*
 * missing_tables[STRONG_MISSING_TABLE] is for missing{strong} dcls.	*
 *									*
 * missing_tables[IMPORTED_MISSING_TABLE] is for missing{imported} dcls.*
 *									*
 * The BASIC_MISSING_TABLE and STRONG_MISSING_TABLE table each		*
 * associate a  list of types with each id.  The IMPORTED_MISSING_TABLE *
 * table associates a list of NAME_TABLE cells with each id, giving	*
 * both the type and the package where the missing declaration was 	*
 * made.								*
 ************************************************************************/

PRIVATE HASH2_TABLE* missing_tables[] = {NULL, NULL, NULL};


/****************************************************************
 *			    MISSING_INDEX			*
 ****************************************************************
 * Returns the index in missing_tables of the table appropriate *
 * for mode.						*
 ****************************************************************/

PRIVATE int missing_index(MODE_TYPE *mode)
{
  return 
    has_mode(mode, STRONG_MODE)    ? STRONG_MISSING_TABLE :
    has_mode(mode, IMPORTED_MODE)  ? IMPORTED_MISSING_TABLE
                                   : BASIC_MISSING_TABLE;
}


/****************************************************************
 *			TYPES_DECLARED_MISSING_TM		*
 ****************************************************************
 * Return a list of all of the types declared missing for id.	*
 * List l tells which tables to include.  It should consist	*
 * of one or more of BASIC_MISSING_TABLE, STRONG_MISSING_TABLE,	*
 * IMPORTED_MISSING_TABLE.					*
 *								*
 * hash is strhash(id).						*
 *								*
 * IMPORTANT NOTE: The returned list can have two kinds of	*
 * cells in it.  Those with tag TYPE_L are types.  Those with	*
 * tag NAME_TYPE_L are pointers to NAME_TYPE nodes, giving	*
 * a type and a package name.					*
 ****************************************************************/

TYPE_LIST* types_declared_missing_tm(char *id, INT_LIST *l, long hash)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  int i;
  TYPE_LIST *rest;

  if(l == NIL) return NIL;

  rest  = types_declared_missing_tm(id, l->tail, hash);
  u.str = id;
  i = toint(l->head.i);
  if(i < BASIC_MISSING_TABLE || i > IMPORTED_MISSING_TABLE) {
    die(140);
  }

  h = locate_hash2(missing_tables[i], u, hash, eq);
  if(h->key.num == 0) return rest;
  else return append(h->val.list, rest);
}


/****************************************************************
 *			    CHECK_MISSING_TM			*
 ****************************************************************
 * check_missing_tm(e) checks all global ids in e to see if any	*
 * overlap into the missing region of their types.  When such	*
 * an id is found, it is reported if reporting is not		*
 * suppressed.							*
 ****************************************************************/

PRIVATE INT_LIST* check_missing_select_list = NIL;

/*-----------------------------------------------------------*
 * check_missing_help(es) checks expression *es.  It is used *
 * in a call to scan_expr, so it will be called at each node *
 * of some larger expression.  It is only concerned with     *
 * identifiers, however, and should return 0 to scan_expr at *
 * most nodes to indicate that its subnodes should be	     *
 *-----------------------------------------------------------*/

PRIVATE Boolean check_missing_help(EXPR **es)
{
  LIST *l, *parts;
  GLOBAL_ID_CELL *gic;
  TYPE *this_type, *miss_type, *e_type;
  LIST *binding_list_mark;
  TYPE_LIST *declared_missing;
  LONG hash;
  char *id;
  EXPR *e, *sse;

  e = *es;

  /*------------------------------------------------*
   * If this is a node that indicates an expression *
   * (missing id), then ignore this node and its    *
   * descendant (id).  This is done by returning 1  *
   * to scan_expr.				    *
   *------------------------------------------------*/

  if(EKIND(e) == SAME_E && e->SAME_MODE == 4) return 1;

  /*---------------------------------------------------*
   * Only examine this node if it is a global id node. *
   *---------------------------------------------------*/

  sse = skip_sames(e);
  if(EKIND(sse) == GLOBAL_ID_E) {
    gic = sse->GIC;
    if(gic == NULL) return 1;

    id   = sse->STR;
    hash = strhash(id);
    id   = id_tb10(id, hash);

    /*--------------------------------------------------*
     * Find out the types declared missing for this id. *
     *--------------------------------------------------*/

    bump_list(declared_missing = 
	      types_declared_missing_tm(id, check_missing_select_list, hash));
    if(declared_missing == NIL) return 1;

    /*---------------------------------------------------------*
     * Check for overlaps between e->ty and the types declared *
     * missing.  We copy e->ty to ensure no side effects.      *
     *---------------------------------------------------------*/

    bump_type(e_type = copy_type(e->ty, 0));
    for(l = declared_missing; l != NIL; l = l->tail) {
      this_type = (LKIND(l) == TYPE_L) 
		    ? l->head.type 
		    : l->head.name_type->type;
      bump_list(binding_list_mark = finger_new_binding_list());

      /*------------------------------------------------------*
       * If this missing type does not unify with e->ty, then *
       * ignore it and try the next one. 		      *
       *------------------------------------------------------*/

      if(!unify_u(&this_type, &e_type, TRUE)) {
	drop_list(binding_list_mark);
	continue;
      }

#     ifdef DEBUG
        if(trace_missing) {
	  trace_t(368, id);
	  trace_ty(sse->ty);
	  tracenl();
	  trace_t(369);
	  trace_ty(this_type);
	  tracenl();
	}
#     endif      

      /*--------------------------------------------------------*
       * We have found a missing type that unifies with e->ty,  *
       * and this_type is now the intersection of that missing  *
       * type and e->ty.  We need to see whether there is any   *
       * type covered by this_type that is not also covered by  *
       * a definition.  So get the definition types.  Include   *
       * the local expectations, because those definitions 	*
       * presumable might come later.				*
       *--------------------------------------------------------*/

      bump_list(parts = entpart_type_list(gic->entparts, FALSE));
      SET_LIST(parts, append(local_expectation_list_tm(e->STR,0), parts));
      bump_type(miss_type = missing_type(parts, this_type));
      drop_list(parts);

      /*----------------------------------------*
       * If a missing type was found, complain. *
       *----------------------------------------*/

      if(miss_type != NULL) {
	char* specname = display_name(id);
	warn1(MISSING_TYPE_ERR, specname, e->LINE_NUM);
	err_print(TYPE_OF_STR_ERR, specname);
	err_print_ty(sse->ty);
	err_nl();
	err_print(MISSING_TYPE_IS_ERR);
	err_print_ty(miss_type);
	err_nl();
	drop_type(miss_type);
        undo_bindings_u(binding_list_mark);
        drop_list(binding_list_mark);
	break;
      }

      drop_type(miss_type);
      undo_bindings_u(binding_list_mark);
      drop_list(binding_list_mark);
    } /* end for(l = declared_missing...) */

    drop_type(e_type);
    drop_list(declared_missing);
  } /* end if(EKIND(e) == GLOBAL_ID_E) */

  return 0;
}

/*-------------------------------------------------------------*/

void check_missing_tm(EXPR *e)
{
  if(e == NULL || 
     should_suppress_warning(err_flags.suppress_missing_warnings)) return;

  if(check_missing_select_list == NULL) {
    bump_list(check_missing_select_list = 
      int_cons(IMPORTED_MISSING_TABLE, int_cons(BASIC_MISSING_TABLE, NIL)));
  }

# ifdef DEBUG
    if(trace_missing > 1) trace_missing_type += trace_missing - 1;
# endif

  scan_expr(&e, check_missing_help, TRUE);

# ifdef DEBUG
    if(trace_missing > 1) trace_missing_type -= trace_missing - 1;
# endif
}


/****************************************************************
 *			    ISSUE_MISSING_TM			*
 ****************************************************************
 * Add class t to the missing table for identifier name.	*
 * MODE is the mode, and is used to determine which table	*
 * to add to.							*
 *								*
 * Note: if MODE includes HIDE_MODE, then instead mark		*
 * each definition of name whose type overlaps type t as	*
 * hidden.  Missing tests will ignore those definitions.	*
 *								*
 * MODE is a safe pointer.  It only lives as long as this	*
 * function call.						*
 ****************************************************************/

void issue_missing_tm(char *name, TYPE *t, MODE_TYPE *mode)
{
  bump_type(t);

  name = new_name(name, TRUE);
  if(has_mode(mode, HIDE_MODE)) {
    mark_hidden(name, t);
  }

  else {
    HASH_KEY u;
    HASH2_CELLPTR h;

    int           m_index = missing_index(mode);
    HASH2_TABLE** tbl     = &missing_tables[m_index];

    u.str = name;
    h     = insert_loc_hash2(tbl, u, strhash(name), eq);

    /*---------------------------*
     * If not in table, install. *
     *---------------------------*/

    if(h->key.num == 0) {
      h->key.str  = name;
      h->val.list = NIL;
    }

    /*---------------------------------------*
     * Add type t to the list in this table. *
     *---------------------------------------*/

    if(m_index != IMPORTED_MISSING_TABLE) {
      SET_LIST(h->val.list, type_cons(copy_type(t,0), h->val.list));
    }
    else {
      HEAD_TYPE ht;
      NAME_TYPE* nt = (NAME_TYPE*) alloc_small(sizeof(NAME_TYPE));
      bump_type(nt->type = copy_type(t, 0));
      nt->name = current_package_name;
      ht.name_type = nt;
      SET_LIST(h->val.list, general_cons(ht, h->val.list, NAME_TYPE_L));
    }

    /*------------------------------------------------------------------*
     * For a strong missing declaration, check existing definitions. 	*
     *------------------------------------------------------------------*/

    if(has_mode(mode, STRONG_MODE)) {
      check_all_definitions_for_strong_missing(name, t);
    }

#   ifdef DEBUG
      if(trace_missing) {
	trace_t(370, name);
	print_type_list(h->val.list);
	tracenl();
      }
#   endif

  }

  drop_type(t);
}


/****************************************************************
 *		CHECK_ALL_DEFINITIONS_FOR_STRONG_MISSING	*
 ****************************************************************
 * Check whether any definitions of name:T exist, for some	*
 * species T in the set of species defined by ty.  Warn about   *
 * a strongly missing definition of 'name' if so.  Check all	*
 * definitions, even invisible and hidden ones.  Only report	*
 * the first error.						*
 ****************************************************************/

PRIVATE void check_all_definitions_for_strong_missing(char *name, TYPE *ty)
{
  GLOBAL_ID_CELL* gic= get_gic_tm(name, FALSE);
  ENTPART *p;
      
  if(gic != NULL) {
    for(p = gic->entparts; p != NULL; p = p->next) {
      if(!disjoint(p->ty, ty)) {
	warn_strong_missing(name, p->ty, ty);
	return;
      }
    }
  }
}


/****************************************************************
 *			    CHECK_STRONG_MISSING_TM		*
 ****************************************************************
 * Check if id:t overlaps a strongly missing class for id,	*
 * and issue a warning if it does.				*
 ****************************************************************/

void check_strong_missing_tm(char *id, TYPE *t)
{
  HASH_KEY u;
  HASH2_CELLPTR h;
  LIST *p;

  u.str = id;
  h     = locate_hash2(missing_tables[STRONG_MISSING_TABLE], 
		       u, strhash(u.str), eq);
  if(h->key.num == 0) return;
  for(p = h->val.list; p != NIL; p = p->tail) {
    if(!disjoint(p->head.type, t)) {
      warn_strong_missing(id, t, p->head.type);
      return;
    }
  }
}


