/************************************************************************
 * File:    patmatch/pmcompl.c
 * Purpose: Check for exhaustiveness of pattern matching chooses
 * Author:  Karl Abrahamson
 ************************************************************************/

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
 * When a choose-matching construct (or a definition by cases, which	*
 * is translated internally into a choose-matching construct) is	*
 * compiled, an attempt is made to see whether the patterns are		*
 * exhaustive.  This file implements those tests.			*
 *									*
 * Much of the work is done in unify/complete.c, where polymorphic	*
 * types are compared.  This is because comparing patterns is similar	*
 * to comparing types.  So patterns are converted to types so that	*
 * the type functions can be used.					*
 *									*
 * When the parser reads a choose-matching construct, it builds 	*
 * a list [fun,target,p1,p2,...,pn] holding 				*
 *									*
 *    (1) The name fun of the function being defined, if this is a 	*
 *          Let f by ... sort of construct, or NULL if it is a 		*
 *          choose-matching construct.    				*
 *									*
 *    (2) The target of the match.					*
 *									*
 *    (3) The patterns p1,...,pn of the cases of the match.		*
 *									*
 * It adds that list to choose_matching_lists, which is a list of       *
 * lists.								*
 *									*
 * When a choose-matching construct is checked for completeness, each	*
 * of its patterns is converted to a pseudo-type.  A pseudo-type has    *
 * type TYPE*, but has some special characteristics.  A pattern part 	*
 * that is known to match only an object with tag k is represented by	*
 * a TYPE_ID_T node with TOFFSET value of k, ctc value of NULL and	*
 * special value of 1.  FAM_T nodes are similar.  When such a type or	*
 * family printed, it is shown as #k.					*
 *									*
 * The type of the target is converted to a pseudo-type that		*
 * represents all possible tag values.  A variable with 'special' field	*
 * set to 1 and TOFFSET field set to k represents a variable that	*
 * ranges over all tags 0..k-1.	 It is printed as `#k.			*
 *									*
 * To perform the completeness check, function missing_type from        *
 * unify/complete.c is used to try to find a type that is in the pseudo-*
 * type derived from the target, but is not among the pseudo-types      *
 * derived from the patterns.						*
 *									*
 *          --------- Some technical issues -----------			*
 *									*
 * When identifiers are handled in the declaration, identifiers are     *
 * also handled, in a similar way, in the expressions in 		*
 * choose_matching_lists.  See ids/ids.c.				*
 *									*
 * choose_matching_lists holds expressions as they are in the original  *
 * version produced by the parser.  But when the type inference		*
 * mechanism begins trying possible solutions to overloaded identifiers,*
 * it makes a copy of the expression before modifying it.  It also 	*
 * makes a copy of choose_matching_lists, in which each expression is   *
 * replaced by its corresponding expression in the copy.  That copy is  *
 * called copy_choose_matching_lists.  List copy_choose_matching_lists  *
 * is computed just before pattern match substitution.			*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../exprs/expr.h"
#include "../parser/tokens.h"
#include "../classes/classes.h"
#include "../tables/tables.h"
#include "../clstbl/cnstrlst.h"
#include "../unify/unify.h"
#include "../error/error.h"
#include "../patmatch/patmatch.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 * 			PUBLIC VARIABLES			*
 ****************************************************************/

/******************************************************************
 *			choose_matching_lists			  *
 *			copy_choose_matching_lists		  *
 ******************************************************************
 * Variables choose_matching_lists and copy_choose_matching_lists *
 * are described above.						  *
 ******************************************************************/

LIST* choose_matching_lists      = NULL;
LIST* copy_choose_matching_lists = NULL;


/******************************************************************
 *			PAIR_LIST and PAIR_LIST1		  *
 ******************************************************************
 * pair_list1(t,[u,v,w]) = [(t,u), (t,v), (t,w)], where the pairs *
 * in the output list are type pairs.				  *
 *								  *
 * pair_list([r,s],[u,v]) = [(r,u),(r,v),(s,u),(s,v)].  That is,  *
 * pair_list performs a kind of cartesian product.		  *
 ******************************************************************/

PRIVATE TYPE_LIST* pair_list1(TYPE *t, TYPE_LIST *b)
{
  if(b == NIL) return NIL;
  return type_cons(pair_t(t, b->head.type), pair_list1(t, b->tail));
}


PRIVATE TYPE_LIST* pair_list(TYPE_LIST *a, TYPE_LIST *b)
{
  if(a == NIL) return NIL;
  return append(pair_list1(a->head.type,b), pair_list(a->tail, b));
}


/****************************************************************
 *			INTERSECT_PT_LISTS			*
 ****************************************************************
 * intersect_pt_lists(l1,l2) forms the list of intersections of *
 * members of lists l1 and l2. Lists l1 and l2 contain 		*
 * pseudo-types.  						*
 *								*
 * Intersect_pt_list1(t,l2) forms the list formed by 		*
 * intersecting type t with each member of l2.			*
 ****************************************************************/

PRIVATE TYPE_LIST* intersect_pt_lists1(TYPE *t, TYPE_LIST *l2)
{
  TYPE *s;
  TYPE_LIST *r, *result;
  LIST *mark;

  /*-------------------------------------------------------------*
   * If there is nothing in l2, then there are no intersections. *
   *-------------------------------------------------------------*/

  if(l2 == NIL) return NIL;

  bump_list(r = intersect_pt_lists1(t, l2->tail));

  /*--------------------------------------------------------------------*
   * Add the head of l2 intersected with t to the result from the tail, *
   * if that intersection is non-null. 					*
   *--------------------------------------------------------------------*/

  s    = l2->head.type;
  mark = finger_new_binding_list();
  bump_list(result = UNIFY(s,t,1) ? type_cons(copy_type(t,0),r) : r);
  undo_bindings_u(mark);

  if(result != NIL) result->ref_cnt--;
  return result;
}  

/*------------------------------------------------------------------*/

PRIVATE TYPE_LIST* intersect_pt_lists(TYPE_LIST *l1, TYPE_LIST *l2)
{
  TYPE_LIST *r, *result;

  if(l1 == NIL) return NIL;
  bump_list(r = intersect_pt_lists1(l1->head.type, l2));
  bump_list(result = append(r, intersect_pt_lists(l1->tail, l2)));
  drop_list(r);
  if(result != NIL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			TAG_PSEUDO_TYPE				*
 ****************************************************************
 * Return the pseudo-type for tag 'tag'.  If tag is -1, return  *
 * a variable, which is a pseudo-type matching any other pseudo-*
 * type.  If tag >= 0, return a pseudo-type #tag.  pat is the   *
 * pattern whose tag is 'tag'.					*
 ****************************************************************/

PRIVATE TYPE* tag_pseudo_type(int tag, EXPR *pat)
{
  TYPE *result;

  /*--------------------------------------------------------------------*
   * We have a tag, possibly -1 (indicating a match against anything)   *
   * or 10000 (indicating a match against nothing).			*
   *--------------------------------------------------------------------*/

# ifdef DEBUG
     if(trace_pat_complete) {
	 trace_t(100, tag);
	 print_expr(pat, 1);
     }
# endif

  /*------------------------------------------------------*
   * For tags other than -1, build a pseudo-type #tag or, *
   * in the case of a family member, #tag(`a). 	   	  *
   *------------------------------------------------------*/

  if(tag >= 0) {
     TYPE* pat_ty = find_u(pat->ty);
     if(TKIND(pat_ty) == FAM_MEM_T) {
	 result          = new_type(FAM_T, NULL);
	 result->TOFFSET = tag;
	 result->special = 1;
	 result          = fam_mem_t(result, var_t(NULL));
     }

     else {
	 result          = new_type(TYPE_ID_T, NULL);
	 result->TOFFSET = tag;
	 result->special = 1;
     }

     return result;
  }

  /*----------------------------------------------------------*
   * For tag (-1), build a variable that will match anything. *
   *----------------------------------------------------------*/

  else {
     return var_t(NULL);
  }
}



/****************************************************************
 *			PATTERN_TO_TYPES			*
 ****************************************************************
 * pattern_to_types converts a pattern expression to a list of  *
 * pseudo-types representing the kinds of things that the 	*
 * pattern might match. 					*
 *								*
 * Pattern_to_types1 is the same, but takes an additional       *
 * parameter.							*
 *								*
 *   exb is a table that binds pattern variables to types.  It  *
 *       is used to ensure that the same type variable is used	*
 *	 for each occurrence of a pattern variable.		*
 ****************************************************************/

PRIVATE TYPE_LIST* 
pattern_to_types1(EXPR *pat, HASH2_TABLE **ex_b)
{
  EXPR_TAG_TYPE kind;
  TYPE_LIST *result;

  pat  = skip_sames(pat);
  kind = EKIND(pat);

  /*----------------------------------------------------------------*
   * A constant is assumed to match nothing of any importance.  Tag *
   * 10000 is used to indicate a match with nothing.  An exception  *
   * is made for constants that occur in constructor lists for      *
   * species, such as true and false.				    *
   *----------------------------------------------------------------*/

  if(is_fixed_pat(pat)) {
    int tag;
    if(kind == GLOBAL_ID_E) {
      tag = tag_name_to_tag_num(pat->ty, pat->STR);
      if(tag < 0) tag = 10000;
    }
    else tag = 10000;
    bump_list(result = type_cons(tag_pseudo_type(tag, pat), NIL));
    goto out;
  }

  switch(kind) {
   case PAIR_E:

    /*-----------------------------------------------------------------*
     * At a pair, build a pair from the converted components. Since    *
     * the output of pattern_to_types1 is a list, we need to form      *
     * the cartesian product of the lists, which is done by pair_list. *
     *-----------------------------------------------------------------*/

    {LIST *l1,*l2;
     bump_list(l1 = pattern_to_types1(pat->E1, ex_b));
     bump_list(l2 = pattern_to_types1(pat->E2, ex_b));
     bump_list(result = pair_list(l1, l2));
     drop_list(l1);
     drop_list(l2);
     goto out;
    }

   case PAT_FUN_E:

    /*----------------------------------------------------------------*
     * At a PAT_FUN_E (which might be a pattern function or a pattern *
     * constant) build a singleton list holding the pseudo-type that  *
     * represents the tag associated with this pattern function.  If  *
     * there is no such tag, then pat_fun_tag will return -1,         *
     * representing a pattern that matches everything.		      *
     *----------------------------------------------------------------*/

    bump_list(result = 
	      type_cons(tag_pseudo_type(pat_fun_tag(pat, pat->ty), pat), 
			NIL));
    goto out;

   case APPLY_E:
    {EXPR *patfun, *arg;
     char *pf_name;
     Boolean higher_order = FALSE;

     /*------------------------------------------------------------*
      * Get the pattern function.  If this expression has the form *
      * f a b c..., with applications embedded on the left, then   *
      * set higher_order to TRUE.				   *
      *------------------------------------------------------------*/

     patfun = skip_sames(pat->E1);
     while(EKIND(patfun) == APPLY_E) {
       higher_order = TRUE;
       patfun       = skip_sames(patfun->E1);
     }
     pf_name = patfun->STR;
     arg     = skip_sames(pat->E2);

     /*-----------------------------------------------------------------*
      * Handle orelse, thenalso, withalso, also.  Patterns formed with 	*
      * 'also' can only match targets that match both of the argument 	*
      * patterns.  Patterns formed with orelse, thenalso, withalso match*
      * patterns matched by either of the arguments.			*
      *-----------------------------------------------------------------*/

     if(!higher_order && EKIND(arg) == PAIR_E &&
	(strcmp(pf_name, "orelse") == 0 ||
	 strcmp(pf_name, "thenalso") == 0 ||
	 strcmp(pf_name, "withalso") == 0 ||
	 strcmp(pf_name, "also") == 0)) {
       TYPE_LIST *l1, *l2;
       bump_list(l1 = pattern_to_types1(arg->E1, ex_b));
       bump_list(l2 = pattern_to_types1(arg->E2, ex_b));
       if(strcmp(pf_name, "also") == 0) {
	 result = intersect_pt_lists(l1,l2);
	 if(result == NIL) {
	   warn1(ALSO_CONFLICT_ERR, NULL, pat->LINE_NUM);
           bump_list(result = type_cons(tag_pseudo_type(10000, pat), NIL));
           goto out;
	 }
       }
       else {
	 result = append(l1, l2);
       }
       bump_list(result);
       drop_list(l1);
       drop_list(l2);
       goto out;
     } 

     /*-----------------------------------------------------------*
      * Handle other pattern functions.  First get the tag of the *
      * pattern function.					  *
      *-----------------------------------------------------------*/

     bump_list(result = 
	       type_cons(tag_pseudo_type(pat_fun_tag(patfun, pat->ty), pat),
			 NIL));
     goto out;
    }

  case WHERE_E:

    /*---------------------------------------------------------------*
     * Pattern a where b is presumed to match just those things that *
     * a matches.						     *
     *---------------------------------------------------------------*/

    bump_list(result = pattern_to_types1(pat->E1, ex_b));
    goto out;

  case PAT_VAR_E:

    /*----------------------------------------------------------*
     * A pattern variable is presumed to match anything.  Its   *
     * pseudo-type is a variable `a.  But use the table to get  *
     * the same variable for each occurrence of a given pattern *
     * variable.						*
     *----------------------------------------------------------*/

    {HASH2_CELLPTR h;
     HASH_KEY u;
     u.num = tolong(pat);
     h     = insert_loc_hash2(ex_b, u, inthash(u.num), eq);
     if(h->key.num == 0) {
       h->key.num = u.num;
       bump_type(h->val.type = var_t(NULL));
     }
     bump_list(result = type_cons(h->val.type, NIL));
     goto out;
    }

  default:

    /*----------------------------------------------------------*
     * An unknown kind of pattern is presumed to match nothing. *
     *----------------------------------------------------------*/

    bump_list(result = type_cons(tag_pseudo_type(10000, pat), NIL));
    goto out;

  } /* end switch */

out:
# ifdef DEBUG
    if(trace_pat_complete) {
      LIST *p;
      trace_t(101);
      for(p = result; p != NIL; p = p->tail) {
	fprintf(TRACE_FILE, "  type: ");
	trace_ty( pat->ty);
	fprintf(TRACE_FILE, "  tag-type: ");
        fprint_tag_type(TRACE_FILE, p->head.type, pat->ty, TRUE);
	tracenl();
      }
      trace_t(102);
      print_expr(pat,1);
    }
# endif

  if(result != NIL) result->ref_cnt--;
  return result;
}


TYPE_LIST* pattern_to_types(EXPR *pat)
{
  TYPE_LIST *result;
  HASH2_TABLE* ex_b = NULL;

  bump_list(result = pattern_to_types1(pat, &ex_b));
  scan_and_clear_hash2(&ex_b, drop_hash_type);
  if(result != NIL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			TYPE_TO_TAG_TYPE			*
 ****************************************************************
 * Convert type t to a pseudo-type representing all kinds of    *
 * things that are members of type t.				*
 ****************************************************************/

TYPE* type_to_tag_type(TYPE *t)
{
  TYPE_TAG_TYPE kind;
  TYPE *result;

  t    = find_u(t);
  kind = TKIND(t);

  switch(kind) {

    /*---------------------------------------------*
     * Structured types: handle parts recursively. *
     *---------------------------------------------*/

    case PAIR_T:
    case FAM_MEM_T:
    case FUNCTION_T:
      result = new_type2(kind, 
		         type_to_tag_type(t->TY1), 
		         type_to_tag_type(t->TY2));
      break;
  
    case TYPE_ID_T:
    case FAM_T:

      /*------------------------------------------------------------*
       * At family or type, build a pseudo-variable.  If the family *
       * or type has k options, build variable `#k.		    *
       *------------------------------------------------------------*/

      {int tag;
       TYPE_TAG_TYPE rkind;

       rkind  = (kind == TYPE_ID_T) ? TYPE_VAR_T : FAM_VAR_T;
       result = new_type(rkind, NULL_T);
       tag    = num_type_tags(t->ctc);
       if(tag > 0) {
	 result->special = 1;
	 result->TOFFSET = tag;
       }
       break;
      }

    case TYPE_VAR_T:
    case FAM_VAR_T:

      /*-----------------------------------------------------------*
       * A variable is replaced by a copy of itself. We don't know *
       * anything about what pseudo-types it includes.		   *
       *-----------------------------------------------------------*/

      result = copy_type(t,0);
      break;

    default: 
      result = t;
      break;

  } /* end switch */

# ifdef DEBUG
    if(trace_pat_complete) {
      trace_t(103);
      trace_ty( t);
      fprintf(TRACE_FILE, "\nout = ");
      trace_ty( result);
      tracenl();
    }
# endif

  return result;
}


/********************************************************************
 *			GET_MIRROR				    * 
 ********************************************************************
 * Return the type obtained by replacing some pair types by 	    *
 * function types.  The pairs that are replaced are those at the    *
 * top level that corresponds to a top-level pair nodes in e that   *
 * are marked by their extra bit being set.  For example, if the    *
 * top two pair nodes are marked, then type (A,B,C) is replaced by  *
 * A -> (B -> C).						    *
 ********************************************************************/

PRIVATE TYPE* get_mirror(EXPR* e)
{
  e = skip_sames(e);

  if(EKIND(e) != PAIR_E || !e->extra) return e->ty;

  else return function_t(e->ty->TY1, get_mirror(e->E2));
}


/********************************************************************
 *			SHOW_MISSING_CASE			    * 
 ********************************************************************
 * Show fun followed by tag-type fun_type.  If fun is NULL, just    *
 * show fun_type.						    *
 ********************************************************************/

PRIVATE void 
show_missing_case(FILE *f, TYPE *mt, char *fun, TYPE *fun_type)
{ 
  if(fun != NULL) {
    fprintf(f, "%s ", fun);
  }
  fprint_tag_type(f, mt, fun_type, FALSE);
}


/********************************************************************
 *			CHECK_PM_GROUP				    * 
 ********************************************************************
 * Check a single list this_group from copy_choose_matching_lists.  *
 * This_group represents a single choose-matching construct.  It    *
 * has the form [fun,target,p1,p2,...,pn] where 		    *
 *								    *
 *    fun       is the name of the function being defined, if this  *
 *              is a definition by cases, and is NULL otherwise.    *
 *								    *
 *    target    is the target of the matches, and		    *
 *								    *
 *    p1,...,pn are the patterns.				    *
 ********************************************************************/

PRIVATE void check_pm_group(EXPR_LIST *this_group)
{
  char *fun_name;
  EXPR *target;
  TYPE_LIST *pat_types;
  EXPR_LIST *p, *pats, *target_and_pats;
  TYPE *target_class, *mt, *fun_type;

# ifdef DEBUG
    if(trace_pat_complete) trace_t(104);
# endif

  /*------------------------------------------------------*
   * Get the target, and convert its type to a tag class. *
   *------------------------------------------------------*/

  fun_name        = this_group->head.str;
  target_and_pats = this_group->tail;
  target          = target_and_pats->head.expr;
  bump_type(target_class = type_to_tag_type(target->ty));

  /*-------------------------------------------------------*
   * The mirror type for printing is the function type, if *
   * this is the definition of a function, or is the       *
   * type of the target otherwise.			   *
   *							   *
   * For functions, the mirror type is obtained by	   *
   * replacing pair nodes in the type by function nodes,   *
   * when they are in a location that corresponds to a     *
   * pair expression in the target that is marked with its *
   * extra bit being set.  See function list_to_expr in    *
   * exprutil.c for the setting of the extra bit.	   *
   *							   *
   * Note that the function type must be obtained from one *
   * of the patterns, since the target might have been     *
   * forced into an identifier.				   *
   *-------------------------------------------------------*/

  pats     = target_and_pats->tail;
  fun_type = (pats == NULL) ? target->ty : get_mirror(pats->head.expr);

  /*---------------------------------*
   * Convert each pattern to a type. *
   *---------------------------------*/

  pat_types = NIL;
  for(p = pats; p != NIL; p = p->tail) {
    TYPE_LIST *pl;
    bump_list(pl = pattern_to_types(p->head.expr));
    SET_LIST(pat_types, append(pl, pat_types));
    drop_list(pl);
  }

# ifdef DEBUG
    if(trace_pat_complete) {
      LIST *pp;
      trace_t(448);
      trace_t(105);
      print_expr(target, 1);
      trace_t(448);
      trace_t(106);
      for(pp = pat_types; pp != NIL; pp = pp->tail) {
	fprint_tag_type(TRACE_FILE, pp->head.type, target->ty, TRUE);
	tracenl();
      }
      trace_t(448);
      trace_t(107);
      trace_ty( target_class);
      tracenl();
    }
# endif

  /*--------------------------*
   * Look for a missing case. *
   *--------------------------*/

  replace_null_vars(&target_class);
  bump_type(mt = missing_type(pat_types, target_class));

  /*------------------------------------*
   * If mt is not null, give a warning. *
   *------------------------------------*/

  if(mt != NULL) {
    warn1(PM_INCOMPLETE_ERR, NULL, target->LINE_NUM);
    err_print(MISSING_CASE_ERR);
    show_missing_case(ERR_FILE, mt, fun_name, fun_type);    
    if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
      show_missing_case(LISTING_FILE, mt, fun_name, fun_type);
    }
    err_nl();
    if(err_flags.novice) {
      err_print(PM_INCOMPLETE_EXPLAIN_ERR);
    }
  }

  drop_type(mt);
  drop_type(target_class);
  drop_list(pat_types);
} 


/****************************************************************
 *			CHECK_PM_COMPLETENESS			*
 ****************************************************************
 * Check for missing cases in pattern match choices.  		*
 * copy_choose_matching_lists must be set before entering 	*
 * check_pm_completeness.					*
 ****************************************************************/

void check_pm_completeness(void)
{
  EXPR_LIST *this_group;

# ifdef DEBUG
    if(trace_pat_complete) {
      LIST *p, *q;
      trace_t(109);
      for(p = copy_choose_matching_lists; p != NIL; p = p->tail) {
	for(q = p->head.list; q != NIL; q = q->tail) {
	  if(LKIND(q) == STR_L) trace_t(582, q->head.str);
	  else print_expr(q->head.expr, 1);
	}
	trace_t(110);
      }
    }
# endif

  for(this_group = copy_choose_matching_lists; 
      this_group != NIL; 
      this_group = this_group->tail) {
    if(this_group != NIL) check_pm_group(this_group->head.list);
  }
}




