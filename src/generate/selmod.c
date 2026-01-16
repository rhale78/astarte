/*****************************************************************
 * File:    generate/selmod.c
 * Purpose: Support for generating and translating role
 *	    selection and modification.
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

/************************************************************************
 * This file provides functions that help to generate primitives and	*
 * patterns that are associated with the '!!' and '??' functions that   *
 * are created with roles as selectors and modifiers.			*
 ************************************************************************/

#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../utils/hash.h"
#include "../evaluate/instruc.h"
#include "../error/error.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../exprs/expr.h"
#include "../tables/tables.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../generate/selmod.h"
#include "../patmatch/patmatch.h"
#include "../unify/unify.h"
#include "../machdata/except.h"
#include "../parser/parser.h"
#include "../dcls/dcls.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/************************************************************************
 *			GET_ROLE_SELECT_STRUCTURE			*
 ************************************************************************
 * Return a tree structure from the primitives in pattern e.		*
 * For example, if e is							*
 *									*
 *                   pair						*
 *                 /      \						*
 *            apply         pair					*
 *           /     \       /      \					*
 *        r1'??'   p1  apply        apply				*
 *                    /     \      /     \				*
 *                 r2'??'   p2  r3'??'    p3				*
 *									*
 * and r1 has selection list [R,L,L], r2 has selection list [R,R]	*
 * and r3 has selection list [R,R,L], then the structure produced	*
 * is									*
 *									*
 *           apply							*
 *          /      \							*
 *        conv     _ pair _						*
 *                /        \						*
 *            pair          pair					*
 *           /     \       /    \					*
 *        pair      pair  ?      p2					*
 *       /    \    /    \						*
 *      ?     p1  ?     p3						*
 *									*
 * where ? is an anonymous pattern var, and conv is function that	*
 * converts from an unknown type to type image_type.			*
 *									*
 * If any of the pattern functions has no primitive, return NULL.	*
 ************************************************************************/


/*----------------------------------------------------------------------*
 * reset_pats(e) sets the pat field of each node in e correctly.  The   *
 * x->pat is 1 if there are any pattern variables or pattern		*
 * functions in the tree rooted at x.  Assumes that pattern variables	*
 * and pattern functions already have pat field set.			*
 *----------------------------------------------------------------------*/

PRIVATE void reset_pats(EXPR *e)
{
  if(e == NULL) return;
  if(EKIND(e) == PAIR_E) {
    reset_pats(e->E1);
    reset_pats(e->E2);
    e->pat = e->E1->pat | e->E2->pat;
  }
}


/*----------------------------------------------------------------------*
 * basic_role_select_structure builds a structure for the case		*
 * where primitives are unknown.  It takes the apply nodes and		*
 * uses pattern function '_also_' to connect them, rather than 		*
 * connecting them with pair nodes.					*
 *----------------------------------------------------------------------*/

PRIVATE EXPR* basic_role_select_structure(EXPR *e)
{
  int line;
  TYPE *ty;
  EXPR *rhs, *pair, *also_expr, *result, *sse;

  sse = skip_sames(e);
  if(EKIND(sse) != PAIR_E) return e;

  line = e->LINE_NUM;
  ty   = sse->E1->ty;
  rhs  = basic_role_select_structure(sse->E2);
  pair = new_expr2(PAIR_E, sse->E1, rhs, line);
  bump_type(pair->ty = pair_t(ty, ty));
  also_expr = typed_global_sym_expr(ALSO_ID, function_t(pair->ty, ty), line);
  skip_sames(also_expr)->kind = PAT_FUN_E;
  result = apply_expr(also_expr, pair, line);
  bump_type(result->ty = ty);
  return result;
}


/*----------------------------------------------------------------------*
 * get_role_select_structure_help adds one primitive to structure.  p	*
 * is one of the apply nodes in the picture above.  The primitive is	*
 * that of p->E1.  An anonymous pattern variable in 'structure' is 	*
 * replaced by a structure with p->E2 in an appropriate place.  For	*
 * example, if p is							*
 *									*
 *             APPLY_E							*
 *            /       \							*
 *           r        ?x						*
 *									*
 * and 'structure' is							*
 *									*
 *              PAIR_E							*
 *             /      \							*
 *            ?        ?y						*
 *									*
 * and the selection list for primitive r is [R,L,L], then the		*
 * resulting structure is						*
 *									*
 *              PAIR_E							*
 *             /      \							*
 *           PAIR_E   ?y						*
 *          /      \							*
 *       PAIR_E     ?							*
 *      /      \							*
 *     ?        ?x							*
 *									*
 * Note: If p->E1 has no primitive info attached to it, then set	*
 * 'structure' to NULL.							*
 *									*
 * Also set *the_discr to the common tag and set *irregular to true if  *
 * the constructor whose part is being selected from is irregular.	*
 *----------------------------------------------------------------------*/

PRIVATE void 
get_role_select_structure_help(EXPR *p, EXPR **structure, int *the_discr,
			       Boolean *irregular, int line)
{
  int prim, discr;
  EXPR *st, *ssst;
  PART *part;
  LIST *posnlist;
  INT_LIST *sel_list;
  Boolean irregular_prim;
  
  prim = get_pat_prim_g(p->E1, &discr, &part, &irregular_prim, &sel_list);
  if(prim != PRIM_SELECT) {
    SET_EXPR(*structure, NULL);
    return;
  }
    
  /*---------------------------------------------------------*
   * Check for consistent discriminators, and set the_discr. *
   *---------------------------------------------------------*/

  if(*the_discr >= -1 && discr != *the_discr) {
    warn0(TAG_MISMATCH_ERR, line);
    SET_EXPR(*structure, NULL);
    return;
  }
  else *the_discr = discr;

  /*----------------------------*
   * Remember whether irregular *
   *----------------------------*/

  if(irregular_prim) *irregular = TRUE;
 
  /*----------------------------------------------------------*
   * The selection list is backwards for what is needed here. *
   *----------------------------------------------------------*/

  bump_list(sel_list = reverse_list(sel_list));

  /*-----------------------*
   * Install into stucture *
   *-----------------------*/

  st   = *structure;
  ssst = skip_sames(st);
  posnlist = sel_list;
  while(posnlist != NIL) {
    if(EKIND(ssst) == PAT_VAR_E) {
      RTYPE rt;
      rt.type = NULL;
      rt.role = NULL;
      if(EKIND(st) == SAME_E) drop_expr(st->E1);
      st->kind = PAIR_E;
      bump_expr(st->E1 = anon_pat_var_p(rt, line));
      bump_expr(st->E2 = anon_pat_var_p(rt, line));
    }
    st   = (posnlist->head.i == LEFT_SIDE) ? st->E1 : st->E2;
    ssst = skip_sames(st);
    posnlist = posnlist->tail;
  } /* end while */

  if(EKIND(st) == SAME_E) drop_expr(st->E1);
  st->kind = SAME_E;
  st->SAME_MODE = 0;
  st->STR = NULL;
  bump_expr(st->E1 = p->E2);
  st->LINE_NUM = st->E1->LINE_NUM;
  drop_list(sel_list);
}

/*---------------------------------------------------------------*/

EXPR* get_role_select_structure(EXPR *e, TYPE *image_type)
{
  EXPR *structure;
  int the_discr;
  Boolean irregular;

  EXPR* p    = e;
  int   line = e->LINE_NUM;
 
# ifdef DEBUG
    if(trace_pm > 1) {
      trace_t(490);
      print_expr(e, 0);
    }
# endif

  /*----------------------------------------------------------------------*
   * Initialize.  The initial structure is just an anonymous pattern var. *
   * Value -2 for the_discr indicates no knowledge.  Value -1 indicates   *
   * knowledge that there is no discrimiator. 				  *
   *----------------------------------------------------------------------*/

  the_discr = -2;
  irregular = FALSE;
  {RTYPE rt;
   rt.type = NULL;
   rt.role = NULL;
   bump_expr(structure = anon_pat_var_p(rt, line));
  }

  /*--------------------------------*
   * Try to install each primitive. *
   *--------------------------------*/

  while(EKIND(p) == PAIR_E && EKIND(p->E1) == APPLY_E 
	&& p->E1->ROLE_MOD_APPLY) {
    get_role_select_structure_help(p->E1, &structure, &the_discr,
				   &irregular, line);
    if(structure == NULL) goto no_prim;
    p = p->E2;
  }
  if(EKIND(p) == APPLY_E && p->ROLE_MOD_APPLY) {
    get_role_select_structure_help(p, &structure, &the_discr,&irregular,line);
  }

  /*-----------------------------------------------------------------*
   * If couldn't get an efficient structure, get an inefficient one. *
   *-----------------------------------------------------------------*/

 no_prim:
  if(structure == NULL) {
#   ifdef DEBUG
      if(trace_pm > 1) trace_t(492);
#   endif
      
    bump_expr(structure = basic_role_select_structure(e));
  }

  /*-------------------------------------------------------------*
   * If got an efficient structure, fix up its pat fields, apply *
   * the constructor to it, and do type inference on it. 	 *
   *-------------------------------------------------------------*/

  else {
#   ifdef DEBUG
      if(trace_pm > 1) trace_t(493);
#   endif

    /*---------------------------------------*
     * Add a converter to the front of expr. *
     *---------------------------------------*/

    {EXPR* converter = new_expr1(SPECIAL_E, NULL, line);
     converter->PRIMITIVE = 
       (the_discr < 0 && !irregular) ? PRIM_CAST :
       (the_discr < 0)               ? PRIM_WRAP :
       (irregular)                   ? PRIM_DWRAP 
	 			     : PRIM_QWRAP;
     converter->SCOPE = the_discr;
     converter->STR = "convert-for-role-select";
     bump_type(converter->ty = function_t(NULL, image_type));
     SET_EXPR(structure, apply_expr(converter, structure, line));
   }
   reset_pats(structure);
   do_patmatch_infer_pm(structure, PAT_SUBST_TYPE_ERR, line);
  }

# ifdef DEBUG
    if(trace_pm > 1) {
      trace_t(491);
      print_expr(structure, 0);
    }
# endif

  return structure;
}


/****************************************************************
 *			GET_MODIFY_STRUCTURE			*
 ****************************************************************
 * Compute the structure of expression e from the primitive	*
 * info of the functions being applied.  It is a tree, with	*
 * a left-child corresponding to LEFT_SIDE and a right-child	*
 * correspoding to RIGHT_SIDE.  At the leaves are the		*
 * expressions to be substituted.  Null subtrees represent 	*
 * parts to be taken from the original object. For example, if 	*
 * e is								*
 *								*
 *             apply						*
 *            /     \						*
 *         r'!!'    pair					*
 *                 /    \					*
 *              apply    a					*
 *             /     \						*
 *          s'!!'     pair					*
 *                   /    \					*
 *                  x      b					*
 *								*
 *								*
 * and r'!!' has selection list [R,L] and s'!!' has selection 	*
 * list [L,R], then the structure is				*
 *								*
 *                 O						*
 *               /   \						*
 *              O     O						*
 *               \   /						*
 *                a b						*
 *								*
 * indicating that a is to be the new right of the left of x, 	*
 * and b is to be the new left of the right of x.  Also return  *
 * the expression x in start_expr.  Parameter the_discr is set 	*
 * to the common discrimiator of the primitives, or to -1 if 	*
 * all have no discriminator.					*
 *								*
 * The structure returned is a list with an EXPR_L tag at each	*
 * leaf that is a substitution expr.  It will be NIL if any of	*
 * the following occur.						*
 *								*
 *   (a) One of the functions has no primitive info.		*
 *   (b) The functions have inconsistent discriminators.	*
 *   (c) One of the functions is irregular.			*
 ****************************************************************/

LIST* get_modify_structure(EXPR *e, EXPR **start_expr, int *the_discr)
{
  int prim, arg;
  PART *part;
  INT_LIST *posnlist;
  LIST *st, *structure, *sel_list;
  Boolean irregular_prim;
  Boolean seen_irregular = FALSE;
  EXPR* above_p = e;
  EXPR* p       = skip_sames(e);
  int   discr   = -2;

  bump_list(structure = list_cons(NIL,NIL));

  while(EKIND(p) == APPLY_E && p->ROLE_MOD_APPLY) {

    /*------------------------------------------*
     * Get the prim info for the '!!' function. *
     *------------------------------------------*/

    prim = get_prim_g(p->E1, &arg, &part, &irregular_prim, &sel_list);
    if(prim != PRIM_MODIFY) goto return_nil;

    /*--------------------------*
     * Check the discriminator. *
     *--------------------------*/

    if(discr >= -1 && arg != discr) {
      warn0(TAG_MISMATCH_ERR, e->LINE_NUM);
      goto return_nil;
    }
    discr = arg;

    /*-------------------------------------------------------*
     * Check for irregular, but keep going here even if this *
     * function is irregular, so can check for inconsistent  *
     * discriminators. 					     *
     *-------------------------------------------------------*/

    if(irregular_prim) seen_irregular = TRUE;

    /*----------------------------------------------------*
     * Locate the position in structure to add this expr. *
     *----------------------------------------------------*/

    st = structure;
    bump_list(sel_list = reverse_list(sel_list));
    for(posnlist = sel_list; posnlist != NIL; posnlist = posnlist->tail) {

      /*--------------------------------------*
       * Check for overlapping substitutions. *
       *--------------------------------------*/

      if(LKIND(st) == EXPR_L) {
	warn0(ROLE_MODIFY_OVERLAP_ERR, e->LINE_NUM);
	drop_list(sel_list);
	goto return_nil;
      }

      /*------------------------------------------------------*
       * Move in the direction indicated by posnlist->head.i. *
       *------------------------------------------------------*/

      {LIST** stside = (posnlist->head.i == LEFT_SIDE) 
	                 ? &(st->head.list) 
		         : &(st->tail);
       if(*stside == NIL) {
	 bump_list(*stside = list_cons(NIL,NIL));
       }
       st = *stside;
      }
    }

    /*---------------------------------------------------------------*
     * Check final position for overlapping substitutions.  To avoid *
     * overlap, the final position should be a leaf with tag LIST_L. *
     *---------------------------------------------------------------*/

    if(LKIND(st) != LIST_L || st->head.list != NIL || st->tail != NIL) {
      warn0(ROLE_MODIFY_OVERLAP_ERR, e->LINE_NUM);
      drop_list(sel_list);
      goto return_nil;
    }

    /*--------------------------------------------*
     * Install this expression in the structure.  *
     *--------------------------------------------*/

    st->kind = EXPR_L;
    bump_expr(st->head.expr = p->E2->E2);

    /*------------------*
     * Next p for loop. *
     *------------------*/

    p = skip_sames(above_p = p->E2->E1);
  }
  drop_list(sel_list);

  /*-----------------------------------------------------------*
   * If one of the functions is irregular, need to return nil. *
   *-----------------------------------------------------------*/

  if(seen_irregular) goto return_nil;
    	
  /*-----------------------------------------------------------------*
   * Have the structure.  Return it, and other info.  The start_expr *
   * is in p at the end of the loop. 				     *
   *-----------------------------------------------------------------*/

  *start_expr = above_p;
  *the_discr  = discr;
  structure->ref_cnt--;
  return structure;  

 return_nil:
  SET_LIST(structure, NIL);
  return NIL;
}


/****************************************************************
 *			GEN_ROLE_MODIFY_G			*
 ****************************************************************
 * Generate expression x/{: r1~>a1, r2~>a2, ...:}.  		*
 * e is the SINGLE_E expression, with e->E1 having the form	*
 * r2'!!'(r1'!!'(x,a1),a2) if this expression is  		*
 * x/{: r1~>a1, r2~>a2 :}. 					*
 *								*
 * The idea here is to see if all of the '!!' expressions have	*
 * primitives.  If so, then just build an expression without	*
 * any '!!' functions that does all of the substitution at once.*
 *								*
 * gen_role_modify_help_g generates code.  It assumes that the	*
 * value to be modified is on the stack, and it takes x apart	*
 * in parallel with taking structure apart.  It generates parts	*
 * of structure where appropriate.				*
 ****************************************************************/

PRIVATE void
gen_role_modify_help_g(LIST *structure)
{
  /*-----------------------------------------------------*
   * If structure is NIL, then leave stack to unchanged. *
   *-----------------------------------------------------*/

  if(structure == NIL) return;

  /*------------------------------------------------------------*
   * If structure is a leaf (with tag EXPR_L), then replace the *
   * stack top with the value of the expr in the leaf. 		*
   *------------------------------------------------------------*/

  if(LKIND(structure) == EXPR_L) {
    gen_g(POP_I);
    generate_exec_g(structure->head.expr, FALSE, 0);
  }

  /*---------------------------------------------------*
   * Otherwise, split and do replacements on children. *
   *---------------------------------------------------*/

  else {
    gen_g(SPLIT_I);
    gen_role_modify_help_g(structure->tail);
    if(structure->head.list != NIL) {
      gen_g(SWAP_I);
      gen_role_modify_help_g(structure->head.list);
      gen_g(SWAP_I);
    }
    gen_g(PAIR_I);
  }
}

/*------------------------------------------------------------*/

#ifdef DEBUG
PRIVATE void print_structure(LIST *l, int n)
{
  if(l == NIL) {
    indent(n);
    fprintf(TRACE_FILE, "NIL\n");
  }
  else if(LKIND(l) == EXPR_L) print_expr(l->head.expr, n);
  else {
    indent(n);
    fprintf(TRACE_FILE, "Pair\n");
    print_structure(l->head.list, n+1);
    print_structure(l->tail, n+1);
  }
}
#endif

/*-----------------------------------------------------------*/

void gen_role_modify_g(EXPR *e)
{
  LIST *structure;
  int the_discr;
  EXPR *start_expr;

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(487);
      print_expr(e->E1, 0);
    }
# endif

  /*---------------------------------------------*
   * Get the structure from the primitives in e. *
   *---------------------------------------------*/

  bump_list(structure = 
	    get_modify_structure(e->E1, &start_expr, &the_discr));

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(488);
      print_structure(structure, 0);
    }
# endif

  /*----------------------------------------------------------------*
   * If the structure cannot be obtained (and so is NIL), then just *
   * generate e->E1.  It will do the job.			    *
   *----------------------------------------------------------------*/

  if(structure == NIL) {
    generate_exec_g(e->E1, FALSE, 0);
  }

  /*-----------------------------------------------------------------*
   * If the structure is not NIL, then generate it.  First, generate *
   * start_expr. 						     *
   *-----------------------------------------------------------------*/

  else {
    generate_exec_g(start_expr, FALSE, 0);

    /*----------------------------------------*
     * Generate the unwrap code if necessary. *
     *----------------------------------------*/

    if(the_discr >= 0) {
      gen_g(QUNWRAP_I);
      gen_g(the_discr);
    }
 
    /*-------------------------------------------------------*
     * Take x apart in parallel with structure, and rebuild. *
     *-------------------------------------------------------*/

     gen_role_modify_help_g(structure);

    /*----------------------*
     * Rewrap if necessary. *
     *----------------------*/

    if(the_discr >= 0) {
      gen_g(QWRAP_I);
      gen_g(the_discr);
    }

    drop_list(structure);
  }
   
# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(489);
    }
# endif
} 
  

/****************************************************************
 *			GEN_UNWRAPS_G				*
 ****************************************************************
 * Possibly generate code to unwrap the top of the stack.       *
 *								*
 *  (1) q-unwrap with tag discr if discr >= 0.			*
 *								*
 *  (2) type-unwrap if do_type_unwrap is true.			*
 *								*
 * If do_type_unwrap is true, then t is the type of the 	*
 * unwrapper, so t->TY2 is the target type.			*
 * Return a list of the newly bound run-time vars.		*
 * Function the_fun is responsible for the unwraps.		*
 ****************************************************************/

PRIVATE void
gen_unwraps_g(int discr, Boolean do_type_unwrap, TYPE *t,
              EXPR *the_fun, int line)
{
  /*------------------------------------------------*
   * Generate the discriminator test if discr >= 0. *
   *------------------------------------------------*/

  if(discr >= 0) {
    gen_g(QUNWRAP_I);
    gen_g(discr);
  }

  /*---------------------------------------*
   * Generate an unwrap if do_type_unwrap. *
   *---------------------------------------*/

  if(do_type_unwrap) {
    gen_unwrap_g(t, the_fun, line);
  }
}


/****************************************************************
 *			GEN_SELECTION_G				*
 ****************************************************************
 * Generate a selection with discriminator discr and selection	*
 * information from f.  For example, if the selection list is 	*
 * [LEFT_SIDE, RIGHT_SIDE, RIGHT_SIDE], then we are asked to	*
 * take the left of the right of the right of what is on top of *
 * the stack.  So we generate RIGHT_I, RIGHT_I, LEFT_I.  A 	*
 * negative discriminator indicates no test for tag.		*
 ****************************************************************/

void gen_selection_g(int discr, EXPR *f)
{
  /*-----------------------*
   * Generate the unwraps. *
   *-----------------------*/

  gen_unwraps_g(discr, f->irregular, f->ty, f, f->LINE_NUM);

  /*-----------------------------------------------------*
   * Generate the left and right selection instructions. *
   *-----------------------------------------------------*/

  {INT_LIST *rl, *p;
   bump_list(rl = reverse_list(f->EL3));
   for(p = rl; p != NIL; p = p->tail) {
     gen_g((p->head.i == LEFT_SIDE) ? LEFT_I : RIGHT_I);
   }
  }
}


/****************************************************************
 *			GEN_WRAPS_G				*
 ****************************************************************
 * Possibly generate code to wrap the top of the stack.  	*
 *								*
 *  (1) type-wrap if do_type_wrap is true.			*
 *      t is the type to wrap with.				*
 *								*
 *  (2) q-wrap with tag discr if discr >= 0.			*
 *								*
 * Note that both kinds of wraps can be done.			*
 ****************************************************************/

PRIVATE void gen_wraps_g(int discr, Boolean do_type_wrap, TYPE *t)
{
  /*----------------------------------*
   * Generate a wrap if do_type_wrap. *
   *----------------------------------*/

  if(do_type_wrap) {
    gen_wrap_g(t);
  }

  /*---------------------------------*
   * Generate the q-wrap discr >= 0. *
   *---------------------------------*/

  if(discr >= 0) {
    gen_g(QWRAP_I);
    gen_g(discr);
  }
}


/************************************************************************
 *			GEN_MODIFY_G					*
 ************************************************************************
 * Generate a modification with discriminator discr and selection 	*
 * information from f.  For example, if the selection list is 		*
 * [LEFT_SIDE, RIGHT_SIDE, RIGHT_SIDE], then we are asked to replace	*
 * the left of the right of the right of what is second from the	*
 * top of the stack with what on top in the stack.  That is, if		*
 * x = (a,b,c,d), then we are being asked to replace c by some value	*
 * y, yielding (a,b,y,d). So we generate 				*
 *                stack (top at left)	temp				*
 *		  -------------------	------				*
 *                ((a,b,c,d), y)					*
 *    SPLIT_I     y (a,b,c,d)						*
 *    LOCK_TEMP_I (a,b,c,d)             y				*
 * >>>> unwrap code goes here if (a,b,c,d) is wrapped			*
 *    SPLIT_I     (b,c,d) a		y				*
 *    SPLIT_I     (c,d) b a		y				*
 *    SPLIT_I     d c b a		y				*
 *    SWAP_I	  c d b a		y				*
 *    POP_I	  d b a 		y				*
 *    GET_TEMP_I  y d b a						*
 *    SWAP_I	  d y b a						*
 *    PAIR_I	  (y,d) b a						*
 *    PAIR_I	  (b,y,d) a						*
 *    PAIR_I	  (a,b,y,d)						*
 * >>>> wrap code goes here if (a,b,c,d) is wrapped			*
 *    									*
 * A negative discriminator indicates no unwrap.  f->irregular		*
 * indicates whether to do type-unwrap.					*
 ************************************************************************/

PRIVATE void gen_modify_help_g(INT_LIST *posnlist)
{
  if(posnlist == NIL) {
    gen_g(POP_I);
    gen_g(GET_TEMP_I);
    return;
  }

  gen_g(SPLIT_I);
  if(posnlist->head.i == LEFT_SIDE) gen_g(SWAP_I);
  gen_modify_help_g(posnlist->tail);
  if(posnlist->head.i == LEFT_SIDE) gen_g(SWAP_I);
  gen_g(PAIR_I);
}

/*--------------------------------------------------------------*/

void gen_modify_g(int discr, EXPR *f)
{
  gen_g(SPLIT_I);
  gen_g(LOCK_TEMP_I);

  /*-------------------------------*
   * Generate the unwraps, if any. *
   *-------------------------------*/

  gen_unwraps_g(discr, 0, NULL, f, f->LINE_NUM);

  /*------------------------------------------*
   * Generate the select/modify/rebuild code. *
   *------------------------------------------*/

  {LIST *posnlist;
   bump_list(posnlist = reverse_list(f->EL3));
   gen_modify_help_g(posnlist);
   drop_list(posnlist);
  }

  /*-----------------------------------------*
   * Generate a wrap at the end if necessary *
   *-----------------------------------------*/

  gen_wraps_g(discr, 0, NULL);
}


