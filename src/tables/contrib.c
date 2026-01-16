/****************************************************************
 * File:    tables/contrib.c
 * Purpose: Translator table manager computing contributions of
 *          definitions to expectations.
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
 * This file helps in determining how to store items in the global	*
 * id table (globtbl.c).  It determines how each definition contributes *
 * to expectations.  The problem being solved here is as follows.       *
 *									*
 * If a definition contributes to any local expectations, then only     *
 * those forms of the definition that contribute to expectations should *
 * be taken -- other forms should be ignored.  For example, suppose a 	*
 * given definition of f has two forms, one of type Natural -> Natural  *
 * and the other of type Real -> Real.  Suppose that there is a local	*
 * expectation for f of type Natural -> Natural.  Then the definition   *
 * of type Real -> Real should be ignored.				*
 *									*
 * If a definition does not contribute to any local expectations, then  *
 * all of its forms should be taken.  					*
 *									*
 * In order to do this, we need to do two passes of type inference.     *
 * The first pass determines what forms are available, and the second	*
 * pass installs those that should be installed.  First pass 		*
 * contribution management is handled here.  Functions should_take,	*
 * below, can be called during the second pass to see if a form		*
 * should be used or skipped over.  should_take uses information	*
 * obtained during the first pass, so it should only be called in	*
 * the second pass.
 ************************************************************************/

#include <string.h>
#include <memory.h>
#include "../misc/misc.h"
#include "../tables/tables.h"
#include "../dcls/dcls.h"
#include "../exprs/expr.h"
#include "../exprs/prtexpr.h"
#include "../infer/infer.h"
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


/****************************************************************
 * 			PUBLIC VARIABLES			*
 ****************************************************************/

/************************************************************************
 *			self_overlap_table				*
 ************************************************************************
 * self_overlap_table is a table associating a list of types with 	*
 * an identifier, or several identifiers.  It is used during the first	*
 * pass of type inference.  As types for identifiers are found during	*
 * the first pass, they are entered into self_overlap_table.  At	*
 * the end of the first pass, that table is checked for self 		*
 * overlapping types, and an error is reported if appropriate.		*
 ************************************************************************/

HASH2_TABLE* self_overlap_table = NULL;


/*************************************************************************
 *			expectation_contributions			 *
 *************************************************************************
 * expectation_contributions is used to determine which of the solutions *
 * to type inference to take.  It has just one member for a define or    *
 * let declaration, and a member for each definition in a team.		 *
 * expectation_contributions is set during the first pass of type	 *
 * inference to indicate how the maximal solution contributes to	 *
 * expecatations, and holds a 1 for each definition that contributes	 *
 * to an expectation, and a 0 for each definition that does not		 *
 * contribute to an expectation.  During the second pass, this list	 *
 * is used to select only those solutions to type inference that	 *
 * have the maximal contribution.  For define and let declarations, that *
 * says that only definitions that contribute to an expectation are	 *
 * taken, if any contribute to expectations.				 *
 * contribute to expectations and which do not.				 *
 *************************************************************************/  

INT_LIST* expectation_contributions = NIL;
                                              

/************************************************************************
 *			CONTRIBUTION_LIST				*
 ************************************************************************
 * Return a list holding a 0 in each position of a definition in defs 	*
 * that does not contribute to an expectation, and a 1 in each position	*
 * that does contribute to an expectation.  If defs is not a team, there*
 * will be only one position.  If defs is a team, then there will be a	*
 * position for each definition in defs.  				*
 *									*
 * Parameter pass tells the pass.  For pass = 1, try all global id	*
 * cells.  For pass = 2, only look at old gic cells.			*
 ************************************************************************/

INT_LIST* contribution_list(EXPR *defs, int pass)
{
  INT_LIST *result;
  EXPR_TAG_TYPE kind;

  defs = skip_sames(defs);
  if(defs == NULL || is_hermit_expr(defs)) return NIL;

  /*-----------------*
   * Case of a team. *
   *-----------------*/

  kind = EKIND(defs);
  if(kind == APPLY_E) {
    LIST *l1, *l2;
    bump_list(l1 = contribution_list(defs->E1, pass));
    bump_list(l2 = contribution_list(defs->E2, pass));
    bump_list(result = append(l1, l2));
    drop_list(l1);
    drop_list(l2);
    if(result != NIL) result->ref_cnt--;
    return result;
  }

  /*-----------------------*
   * Case of a definition. *
   *-----------------------*/

  if(kind == LET_E || kind == DEFINE_E || kind == PAT_DCL_E
     || kind == EXPAND_E) {
    char *name;
    TYPE *ty, *t;
    GLOBAL_ID_CELL *gic;
    EXPECTATION *exp;
    LIST *mark;
    EXPR *defined_id;

    ty         = defs->E1->ty;
    defined_id = get_defined_id(defs);
    name       = new_name(defined_id->STR, TRUE);
    gic        = get_gic_tm(name, FALSE);
    if(gic == NULL) return int_cons(0, NIL);

#   ifdef DEBUG
      if(trace_defs) {
        trace_t(371);
        trace_ty( ty);
        trace_t(372);
        print_gic(gic, 0);
      }
#   endif

    result = NIL;
    for(exp = gic->expectations; exp != NULL; exp = exp->next) {

      /*---------------------------------------------*
       * Ignore new expectations on the second pass. *
       *---------------------------------------------*/

      if(pass == 2 && !(exp->old)) continue;

      if(exp->type != NULL) {

	/*----------------------------------------------------------*
         * Mark this expectation old, so that it will be seen as    *
	 * old during second pass of type inference (in function    *
	 * should_take)  					    *
         *----------------------------------------------------------*/

	exp->old = 1;

	/*------------------------------------------------------*
	 * Check if this declaration would contribute to 	*
	 * expectation exp. 					*
	 *------------------------------------------------------*/

	bump_type(t = copy_type(exp->type, 0));
	bump_list(mark = finger_new_binding_list());
	if(unify_u(&t, &ty, TRUE)) {
#         ifdef DEBUG
            if(trace_defs) trace_t(373);
#         endif
	  result = int_cons(1, NIL);
        }
        undo_bindings_u(mark);
        drop_list(mark);
        drop_type(t);
        if(result != NIL) return result;
      } /* end if(exp->type != NULL) */
    } /* end for(exp...) */

    /*--------------------------------*
     * When search fails, return [0]. *
     *--------------------------------*/

    return int_cons(0, NIL);
  } /* end if(kind == LET_E...) */

  /*----------------------------------------------------*
   * For a non-let, non-lazy-let, non-team, return NIL. *
   *----------------------------------------------------*/

  return NIL;
}


/************************************************************************
 *			ONLY_ZERO					*
 ************************************************************************
 * Return TRUE if list l contains only 0's.				*
 ************************************************************************/

PRIVATE Boolean only_zero(INT_LIST *l)
{
  INT_LIST *p;
  for(p = l; p != NIL; p = p->tail) {
    if(p->head.i != 0) return FALSE;
  } 
  return TRUE;
}


/************************************************************************
 *			COMPARE_CONTRIBUTION_LISTS			*
 ************************************************************************
 * Compare lists a and b.  Each is a list of 0's and 1's, except that	*
 * the end of the list is thought of as being followed by infinitely	*
 * many 0's.  Compare them in the partial order where 0 < 1, and a	*
 * list is larger than another if it is larger in at least one		*
 * component, and is no smaller in every component.  For example,	*
 * [0,1,0] < [1,1,0], but [0,1,0] is incomparable to [1,0,0].  Return	*
 *									*
 *   0 if a = b								*
 *   1 if a < b								*
 *   2 if a > b								*
 *   3 if a and b are incomparable.					*
 *									*
 * Also set result to the union of a and b, defined to be the list that	*
 * has a 1 in each location where either a or b have a 1.  List result	*
 * is ref-counted.							*
 ************************************************************************/

int compare_contribution_lists(INT_LIST *a, INT_LIST *b, INT_LIST **result)
{
  int ha, hb, cmp, this_cmp;

  if(a == NIL) {
    bump_list(*result = b);
    if(only_zero(b)) return 0;
    return 1;
  }

  if(b == NIL) {
    bump_list(*result = a);
    if(only_zero(a)) return 0;
    return 2;
  }

  cmp = compare_contribution_lists(a->tail, b->tail, result);
  ha  = toint(a->head.i);
  hb  = toint(b->head.i);
  if(ha == hb) {
    SET_LIST(*result, int_cons(ha, *result));
    return cmp;
  }

  SET_LIST(*result, int_cons(1, *result));
  this_cmp = (ha < hb) ? 1 : 2;
  return this_cmp | cmp;
}


/************************************************************************
 *			SHOULD_TAKE					*
 ************************************************************************
 * Return TRUE if declaration dcl is compatible with			*
 * expectation_contributions, for second pass of type inference.	*
 * Only call this during the second pass, since it uses 		*
 * expectation_contributions, which is built during the first pass.	*
 ************************************************************************/

Boolean should_take(EXPR *dcl)
{ 
  INT_LIST *cl,*l;
  int cmp;

  bump_list(cl = contribution_list(dcl, 2));
  cmp = compare_contribution_lists(cl, expectation_contributions, &l);
  drop_list(l);
  drop_list(cl);
  return cmp == 0;
}


/************************************************************************
 *			ADD_ALL_TYPES					*
 ************************************************************************
 * For each definition of id:t in e, add t to the type list associated	*
 * with id in table tbl.						*
 ************************************************************************/

PRIVATE void add_all_types(HASH2_TABLE **tbl, EXPR *e)
{
  EXPR_TAG_TYPE kind;

  e = skip_sames(e);
  kind = EKIND(e);

  /*-----------------*
   * Case of a team. *
   *-----------------*/

  if(kind == APPLY_E) {
    add_all_types(tbl, e->E1);
    add_all_types(tbl, e->E2);
  }

  /*-----------------------*
   * Case of a definition. *
   *-----------------------*/

  else if(is_definition_kind(kind)) {
    EXPR *defined_id;
    TYPE *defined_id_ty;
    HASH_KEY u;
    HASH2_CELLPTR h;

    defined_id = get_defined_id(e);
    defined_id_ty = copy_type(defined_id->ty, 0);
    u.str = new_name(defined_id->STR, TRUE);
    h = insert_loc_hash2(tbl, u, strhash(u.str), eq);
    if(h->key.num == 0) {
      h->key = u;
      h->val.list = NIL;
    }
    SET_LIST(h->val.list, type_cons(defined_id_ty, h->val.list));
  }
}


/************************************************************************
 *			DO_FIRST_PASS_TM				*
 ************************************************************************
 * Update solution_count, expectation_contribution and 			*
 * viable_expr for declaration dcl.					*
 ************************************************************************/

void do_first_pass_tm(EXPR *dcl)
{
  int cmp;
  INT_LIST *new_cl, *this_cl;

# ifdef DEBUG
    if(trace_infer) {
      tracenl();
      trace_t(475);
      trace_t(476);
      trace_t(477, solution_count);
      print_str_list_nl(expectation_contributions);
    }
# endif

  /*--------------------------------------------------------------------*
   * Compute the contribution list for dcl.  The contribution list	*
   * contains a 1 for each definition that contributes to an		*
   * expectation, and a 0 for each that does not. 			*
   *--------------------------------------------------------------------*/

  bump_list(this_cl = contribution_list(dcl, 1));

  /*-----------------------------------------------------------------------*
   * Compare this_cl with list expectation_contributions, the contribution *
   * list from prior solutions to type inference.  The following sets	   *
   * new_cl to the new contribution list, and sets cmp to		   *
   *     0 if this_cl = expectation_contributions			   *
   *     1 if this_cl < expectation_contributions			   *
   *	 2 if this_cl > expectation_contributions			   *
   *	 3 if this_cl and expectation_contributions are incomparable.	   *
   *-----------------------------------------------------------------------*/

  cmp = compare_contribution_lists(this_cl, expectation_contributions,&new_cl);

# ifdef DEBUG
    if(trace_infer) {
      trace_t(475);
      trace_t(478, this_cl);
      print_str_list_nl(this_cl);
      trace_t(480, cmp);
    }
# endif

  switch(cmp) {

    case 0:

      /*------------------------------------------------------------*
       * This is one more solution with the same contribution list. *
       *------------------------------------------------------------*/

      solution_count++;
      if(solution_count == 1) {
        SET_EXPR(viable_expr, dcl);
      }
      add_all_types(&self_overlap_table, dcl);
      break;

    case 1:

      /*----------------------------------*
       * This solution is of no interest. *
       *----------------------------------*/

      break;

    case 2:

      /*-------------------------------------------*
       * This solution replaces earlier solutions. *
       *-------------------------------------------*/

      solution_count = 1;
      SET_LIST(expectation_contributions, this_cl);
      SET_EXPR(viable_expr, dcl);
      scan_and_clear_hash2(&self_overlap_table, drop_hash_list);
      break;

    case 3:

      /*----------------------------------------------------------------*
       * This solution and prior solutions are incompatible.  Ignore	*
       * them all.  The new contribution list is the union of all	*
       * seen contribution lists. 					*
       *----------------------------------------------------------------*/

      solution_count = 0;
      SET_LIST(expectation_contributions, new_cl);
      scan_and_clear_hash2(&self_overlap_table, drop_hash_list);
  }

# ifdef DEBUG
    if(trace_infer) {
      trace_t(475);
      trace_t(479);
      trace_t(477, solution_count);
      print_str_list_nl(expectation_contributions);
    }
# endif

  drop_list(this_cl);
  drop_list(new_cl);
}


/************************************************************************
 *			CHECK_FOR_SELF_OVERLAPS				*
 ************************************************************************
 * Check whether any self-overlaps are apparent from table		*
 * self_overlap_table.							*
 *									*
 * MODE is the mode of the definition that is being checked.  It is a   *
 * safe pointer: it does not live longer than this function call.	*
 ************************************************************************/

PRIVATE Boolean did_self_overlap_warn;

PRIVATE void check_self_overlap_cell(HASH2_CELLPTR h)
{
  TYPE_LIST *p, *q;

  for(p = h->val.list; p != NIL; p = p->tail) {
    for(q = p->tail; q != NIL; q = q->tail) {
      if(!disjoint(p->head.type, q->head.type)) {
	warn0(SELF_OVERLAP_ERR, 0);
	did_self_overlap_warn = TRUE;
	if(do_show_at_type_err) force_show = TRUE;
        return;
      }
    }
  }
}	

/*-------------------------------------------------------------*/

void check_for_self_overlaps(EXPR *e, MODE_TYPE *mode)
{
  did_self_overlap_warn = FALSE;
  if(!has_mode(mode, DEFAULT_MODE) ||
     (!has_mode(mode, UNDERRIDES_MODE) && !has_mode(mode, OVERRIDES_MODE))) {
    scan_and_clear_hash2(&self_overlap_table, check_self_overlap_cell);
  }
  if(did_self_overlap_warn) {
    print_overload_info(e);
  }
}
