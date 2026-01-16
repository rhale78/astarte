/**********************************************************************
 * File:    evaluate/eval.c
 * Purpose: This package contains functions that evaluate entities.
 *          It farms out the labor to other modules, notably lazy.c,
 *          where evaluation of lazy entities (promises) is done.
 * Author:  Karl Abrahamson
 **********************************************************************/

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
 * This file provides functions that perform evaluation of lazy		*
 * entities.  The functions provided here are the top level ones.	*
 * Specific functions are provided in lazy.c.				*
 ************************************************************************/
   
#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/rdwrt.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../tables/tables.h"
#include "../show/getinfo.h"
#include "../show/prtent.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/********************************************************
 *			INDIRECT_REPLACEMENT	        *
 *			NEEDS_INDIRECTION		*
 ********************************************************
 * Indirect_replacement(p) returns a value equivalent   *
 * to an indirection to *p.  If *p does not require an  *
 * indirection, that is just *p	itself. 		*
 *							*
 * This function is called during garbage		*
 * collection as well as during evaluation, so it	*
 * must use full tag (GCTAG) and equality check		*
 * (ENT_FEQ) operations.				*
 *							*
 * needs_indirection(e) returns TRUE if e needs an	*
 * an indirection, and FALSE otherwise.			*
 *							*
 * XREF: indirect_replacement is called below and in	*
 * gc/gc.c.						*
 ********************************************************/

PRIVATE ENTITY 
indirect_replacement_help(ENTITY *p, Boolean *needs_ind)
{
  ENTITY x = *p;

  switch(GCTAG(x)) {
    /*--------------------------------*/
    case TREE_TAG:
      if(ENT_FEQ(*ENTVAL(x), NOTHING)) goto indirect;
      else goto not_indirect;
  
    /*--------------------------------*/
    case FILE_TAG:
      {register struct file_entity *fent = FILEENT_VAL(x);
       register int kind = fent->kind;
       if(kind == STDIN_FK || 
	  (kind == INFILE_FK && IS_VOLATILE_FM(fent->mode))) {
         goto indirect;
       }
       else goto not_indirect;
      }

    /*--------------------------------*/
    case LAZY_TAG:
    case LAZY_LIST_TAG:
    case LAZY_PRIM_TAG:
    indirect:
      *needs_ind = TRUE;
      return ENTP(INDIRECT_TAG, p);

    /*--------------------------------*/
    case GLOBAL_TAG:
      *needs_ind = TRUE;
      return ENTP(GLOBAL_INDIRECT_TAG, p);

    /*--------------------------------*/
    default:
    not_indirect:
      *needs_ind = FALSE;
      return x;
  }
}

/*----------------------------------------------------------*/

ENTITY indirect_replacement(ENTITY *p)
{
  Boolean n;
  return indirect_replacement_help(p, &n);
}

/*----------------------------------------------------------*/

PRIVATE Boolean needs_indirection(ENTITY e)
{
  Boolean result;
  indirect_replacement_help(&e, &result);
  return result;
}


/****************************************************************
 *			REDUCE_INDIRECTION_CHAIN		*
 ****************************************************************
 * Compress a chain of indirections so that all point		*
 * to the non-indirect entity at the end of the chain.		*
 * If the object at the end does not require an indirection,	*
 * replace the indirection by the object itself.		*
 *								*
 * Returns a pointer to the non-indirect entity at the		*
 * end of the chain.						*
 *								*
 * *p must have tag INDIRECT_TAG for this to work.		*
 ****************************************************************/

PRIVATE ENTITY* reduce_indirection_chain(ENTITY *p)
{
  ENTITY new_ent;
  ENTITY* q = ENTVAL(*p);

  /*----------------------------*
   * Find the end of the chain. *
   *----------------------------*/

  while(TAG(*q) == INDIRECT_TAG && VAL(*q) != 0) q = ENTVAL(*q);

  /*---------------------*
   * Get the new entity. *
   *---------------------*/

  new_ent = indirect_replacement(q);

  /*----------------------------------------------------*
   * Replace each thing in the chain by the new entity. *
   *----------------------------------------------------*/

  q  = ENTVAL(*p);
  *p = new_ent;
  while(TAG(*q) == INDIRECT_TAG && VAL(*q) != 0) {
    register ENTITY *r = ENTVAL(*q);
    *q = new_ent;
    q  = r;
  }
  return q;
}


/****************************************************************
 *			EVAL					*
 ****************************************************************
 * eval evaluates ee until at least its uppermost part is	*
 * explicit.  For example, it ee is an ordered pair, the 	*
 * components of the pair will not be evaluated.   		*
 *								*
 * Time out if more steps than *time_bound are made.  		*
 * *time_bound is decreased by the number of steps made.	*
 *								*
 * eval1 is similar to eval, but it assumes that the tag	*
 * of ee is a lazy one.						*
 *								*
 * Use the macros in evaluate.h to use these functions.  They	*
 * are used throughout the interpreter.				*
 ****************************************************************/

ENTITY eval(ENTITY ee, LONG *time_bound)
{
  if(!MEMBER(TAG(ee), all_lazy_tags)) return ee;
  else return eval1(ee, time_bound);
}

/*----------------------------------------------------------*/

ENTITY eval1(ENTITY ee, LONG *time_bound)
{
  ENTITY      e       = ee;
  ENTITY*     loc     = &e;
  REG_TYPE    mark    = reg1_param(&e);
  REGPTR_TYPE ptrmark = reg1_ptrparam(&loc);

  /*-------------------------------------*
   * The for loop is for tail recursion. *
   *-------------------------------------*/

  for(;;) {
    if(failure >= 0) {
      if(failure == TIME_OUT_EX) goto out; else goto failout;
    }

    switch(TAG(e)) {

      /*--------------------------------*/
      case INDIRECT_TAG:
	if(VAL(e) == 0) {
	  /*---------*
	   * NOTHING *
	   *---------*/
	  *loc = e;
	  unreg(mark);
	  unregptr(ptrmark);
	  return zero;
	}

        /*------------------------------------------------------*
         * Reduce the chain, and tail recur on the non-indirect *
         * entity at the end of the chain.			*
         *------------------------------------------------------*/

	loc = reduce_indirection_chain(loc);
	e   = *loc;
	break;

      /*--------------------------------*/
      case GLOBAL_INDIRECT_TAG:

        /*------------------------------------------------------*
         * To evaluate a global, use global_eval, and then tail *
         * recur on its result, since global_eval might return  *
         * a lazy value.					*
         *------------------------------------------------------*/

	loc = ENTVAL(e);
	/* No break: continue with next case. */

      /*--------------------------------*/
      case GLOBAL_TAG:
	e   = global_eval(loc, time_bound);
	break;

      /*--------------------------------*/
      case LAZY_TAG:
	e = lazy_eval(loc, time_bound);
	goto finish_lazy;

      /*--------------------------------*/
      case LAZY_LIST_TAG:
	e = lazy_list_eval(loc, time_bound);
        /*goto finish_lazy; */

      finish_lazy:

	/*--------------------------------------------------------------------*
	 * Normally, the result of a lazy computation is stored, or memoized, *
	 * unless it calls for suppressing such memoization.  When the result *
	 * is still lazy, however, and so needs an indirection, we must      *
	 * store the result to give us a place to point the indirection to.  *
	 * To tell whether the value has been memoized, just compare e to    *
         * *loc.  If no indirection is needed, then it is safe to set loc    *
	 * to &e, since no indirection will point to it.                     *
	 *-------------------------------------------------------------------*/

	if(ENT_NE(e, *loc)) {
	  if(needs_indirection(e)) *loc = e;
	  else loc = &e;
	}

	if(failure < 0 && --(*time_bound) <= 0) {
	  *time_bound = 0;
	  failure     = TIME_OUT_EX;
	}

        /*-----------------------------------------------------------*
         * Now tail recur on the result of lazy evaluation, since it *
         * might still be lazy.  Don't need to do the stuff at the   *
	 * bottom of the loop, since it was done above.		     *
         *-----------------------------------------------------------*/

	continue;

      /*--------------------------------*/
      case LAZY_PRIM_TAG:

        /*----------------------------------------------------------*
         * To evaluate lazy primitive, use redo_lazy_prim, and then *
	 * tail recur on the result, since it might still be lazy.  *
         *----------------------------------------------------------*/

	{Boolean nostore;
	 e = redo_lazy_prim(loc, time_bound, &nostore);

	 /*-----------------------------------------------------------*
	  * If the lazy primitive calls for no memoizing, force loc   *
	  * to be &e.  e should not need indirection.		      *
	  * But if memoizing is called for, then store.		      *
	  *-----------------------------------------------------------*/

	 if(nostore) {
	   if(needs_indirection(e)) {
	     loc  = allocate_entity(1);
	     *loc = e;
	   }
	   else loc = &e;
         }
	 else *loc = e;

	 break;
	}

      /*--------------------------------*/
      case TREE_TAG:
	if(eval_tree(loc, e, time_bound, FALSE)) {

	  /*---------------------------------------------------------*
	   * eval_tree says that evaluation is finished or has timed *
	   * out.  Memoize the result and return.  Note that, if     *
	   * evaluation has timed out, then we need an indirection.  *
	   *---------------------------------------------------------*/

	  e = *loc;
	  if(TAG(e) == TREE_TAG && ENT_EQ(*ENTVAL(e), NOTHING)) {
	    e = ENTP(INDIRECT_TAG, loc);
	  }
	  goto really_out;
	}
	else {

	  /*--------------------------------------------*
	   * eval_tree says to keep trying. Tail recur. *
	   *--------------------------------------------*/

	  e = *loc;
	  break;
	}

      /*--------------------------------*/
      case FILE_TAG:

	/*-------------------------------------------------------------------*
	 * Evaluate an infile by reading the file.  An outfile is always     *
	   already evaluated.  file_eval will tell us if this is an outfile. *
	 *-------------------------------------------------------------------*/

	{Boolean is_infile;
	 e = file_eval(loc, &is_infile, time_bound);
	 if(!is_infile) goto out;

	 /*---------------------------------------------------*
	  * An infile might still be lazy.  Tail recur on it. *
	  *---------------------------------------------------*/

	 break;
	}

      /*--------------------------------*/
      case FAIL_TAG:
        failure_as_entity = *ENTVAL(e);
	failure           = qwrap_tag(failure_as_entity);
	goto out;

      /*--------------------------------*/
      default:
	goto out;

    } /* end switch */

    /*-------------------------------------------------------------------*
     * Get ready to go back and try again.  This involves memoizing what *
     * we have so far, and stepping the time counter.                    *
     *-------------------------------------------------------------------*/

    if(failure < 0) {
      *loc = e;
      if(--(*time_bound) <= 0) {
	*time_bound = 0;
	failure     = TIME_OUT_EX;
      }
    }
  } /* end for */

 out:
  /*------------------------------------*
   * Memoize and continue with failout. *
   *------------------------------------*/

  *loc = e;

 failout:

  /*---------------------------------------------------------------------*
   * Add an indirection if one is required and continue with really_out. *
   *---------------------------------------------------------------------*/

  e = indirect_replacement(loc);

 really_out:

  /*--------------------------------------------------------*
   * Unregister the variables and return the computed value *
   *--------------------------------------------------------*/

  unreg(mark);
  unregptr(ptrmark);
  return e;
}


/****************************************************************
 *			WEAK_EVAL				*
 ****************************************************************
 * Similar to eval, but with special modes.  Parameter mode is  *
 * actually an OR of the following conditions.			*
 *								*
 *   mode     meaing						*
 *  ------   --------						*
 *    1      don't evaluate files.  				*
 *								*
 *    2      stop when it is evident whether ee is an		*
 *           unknown or not an unknown.				*
 *								*
 *    4      When ee is found to be a protected unknown,	*
 *	     time-out.						*
 *								*
 * Use weak_eval1 via macros SET_WEAK_EVAL, IN_PLACE_WEAK_EVAL, *
 * etc. in evaluate.h.						*
 *								*
 * XREF: This is called in rts/product.c to do evaluations 	*
 * in functions that do seeks on files, and in			*
 * evaluate/lazyprim.c to handle unknowns.			*
 ****************************************************************/

ENTITY weak_eval(ENTITY ee, LONG mode, LONG *time_bound)
{
  if(!MEMBER(TAG(ee), all_weak_lazy_tags)) return ee;
  else if((mode & 1) == 0 && TAG(ee) == FILE_TAG) return ee;
  else return weak_eval1(ee, mode, time_bound);
}


/*----------------------------------------------------------*/

ENTITY weak_eval1(ENTITY ee, LONG mode, LONG *time_bound)
{
  ENTITY      e       = ee;
  ENTITY*     loc     = &e;
  REG_TYPE    mark    = reg1_param(&e);
  REGPTR_TYPE ptrmark = reg1_ptrparam(&loc);

  for(;;) {
    if(failure >= 0) {
      if(failure == TIME_OUT_EX) goto out; else goto failout;
    }

    switch(TAG(e)) {

      /*--------------------------------*/
      case INDIRECT_TAG:
	if(VAL(e) == 0) {
	  /*---------*
	   * NOTHING *
	   *---------*/
	  *loc = e;
	  unreg(mark);
	  unregptr(ptrmark);
	  return zero;
	}
	loc = reduce_indirection_chain(loc);
	e   = *loc;
	break;

      /*--------------------------------*/
      case GLOBAL_INDIRECT_TAG:
	loc = ENTVAL(e);
	e   = global_eval(loc, time_bound);
	break;

      /*--------------------------------*/
      case LAZY_TAG:
	e = lazy_eval(loc, time_bound);
	goto finish_lazy;

      /*--------------------------------*/
      case LAZY_LIST_TAG:
	e = lazy_list_eval(loc, time_bound);
        /* goto finish_lazy; */

      finish_lazy:

	/*--------------------------------------------------------------------*
	 * Normally, the result of a lazy computation is stored, or memoized, *
	 * unless it calls for suppressing such memoization.  When the result *
	 * is still lazy, however, and so needs an indirection, we must      *
	 * store the result to give us a place to point the indirection to.  *
	 * To tell whether the value has been memoized, just compare e to    *
         * *loc.  If no indirection is needed, then it is safe to set loc    *
	 * to &e, since no indirection will point to it.                     *
	 *-------------------------------------------------------------------*/

	if(ENT_NE(e, *loc)) {
	  if(needs_indirection(e)) *loc = e;
	  else loc = &e;
	}

	if(failure < 0 && --(*time_bound) <= 0) {
	  *time_bound = 0;
	  failure     = TIME_OUT_EX;
	}
	continue;

      /*--------------------------------*/
      case LAZY_PRIM_TAG:
        if(mode & 2) {
	  int w = eval_lazy_prim_to_unknown(&e, &loc, !(mode & 4), time_bound);
	  switch(w) {
	    case 0: if(needs_indirection(e)) {
	              loc  = allocate_entity(1);
		      *loc = e;
		    }
		    else loc = &e;
		    continue;

	    case 1: *loc = e;
		    continue;

	    case 2: loc = allocate_entity(1);
		    goto out;

	   case 3: if(needs_indirection(e)) {
		     loc = allocate_entity(1);
		     goto out;
		   }
		   else goto really_out;

	   case 4: break;
	 }
	}
	
        else {
          Boolean nostore;
	  e = redo_lazy_prim(loc, time_bound, &nostore);
	  if(nostore) {
	    if(needs_indirection(e)) {
	      loc  = allocate_entity(1);
	      *loc = e;
	    }
	    else loc = &e;
          }
	  else *loc = e;
	}
	break;

      /*--------------------------------*/
      case TREE_TAG:
	if(eval_tree(loc, e, time_bound, FALSE)) {

	  /*---------------------------------------------------------*
	   * eval_tree says that evaluation is finished or has timed *
	   * out.  Memoize the result and return.  Note that, if     *
	   * evaluation has timed out, then we need an indirection.  *
	   *---------------------------------------------------------*/

	  e = *loc;
	  if(TAG(e) == TREE_TAG && ENT_EQ(*ENTVAL(e), NOTHING)) {
	    e = ENTP(INDIRECT_TAG, loc);
	  }
	  goto really_out;
	}
	else {

	  /*--------------------------------*
	   * eval_tree says to keep trying. *
	   *--------------------------------*/

	  e = *loc;
	  break;
	}

      /*--------------------------------*/
      case FILE_TAG:

	if(mode & 1) goto out;
        else {
	  Boolean is_infile;
	  e = file_eval(loc, &is_infile, time_bound);
	  if(!is_infile) goto out;
	  break;
	}

      /*--------------------------------*/
      case FAIL_TAG:
        failure_as_entity = *ENTVAL(e);
	failure           = qwrap_tag(failure_as_entity);
	goto out;

      /*--------------------------------*/
      default:
	goto out;

    } /* end switch */

    /*-------------------------------------------------------------------*
     * Get ready to go back and try again.  This involves memoizing what *
     * we have so far, and stepping the time counter.                    *
     *-------------------------------------------------------------------*/

    if(failure < 0) {
      *loc = e;
      if(--(*time_bound) <= 0) {
	*time_bound = 0;
	failure     = TIME_OUT_EX;
      }
    }
  } /* end for */

 out:
  /*------------------------------------*
   * Memoize and continue with failout. *
   *------------------------------------*/

  *loc = e;

 failout:

  /*---------------------------------------------------------------------*
   * Add an indirection if one is required and continue with really_out. *
   *---------------------------------------------------------------------*/

  e = indirect_replacement(loc);

 really_out:

  /*--------------------------------------------------------*
   * Unregister the variables and return the computed value *
   *--------------------------------------------------------*/

  unreg(mark);
  unregptr(ptrmark);
  return e;
}


/****************************************************************
 *			FULL_EVAL				*
 ****************************************************************
 * Full_eval evaluates ee completely, and returns the result	*
 * of the evaluation.  Recomputations are suppressed, so that   *
 * the result will be available without further evaluation.     *
 *								*
 * full_eval1 is like full_eval, but assumes that the tag is	*
 * a lazy one.							*
 *								*
 * full_eval2 is similar to both, but returns with		*
 * defer = zero if there was no time-out, and defer set to	*
 * a sub-object of ee (with tag different from NOREF_TAG)	*
 * that needs to be evaluated if there				*
 * was a time-out.  NOTE: defer must be a pointer to a variable	*
 * in the run-time stack, not into the heap, since it is not	*
 * registered with the garbage collector.  If it is in the 	*
 * heap, the garbage collector might take it away.		*
 *								*
 * All full_eval functions return 0 when asked			*
 * to evaluate NOTHING.						*
 *								*
 * Use the macros in evaluate.h to use these functions.		*
 ****************************************************************/

ENTITY full_eval(ENTITY ee, LONG *time_bound)
{
  if(!MEMBER(TAG(ee), full_eval_tags)) return ee;
  else return full_eval1(ee, time_bound);
}

/*---------------------------------------------------------------*/

ENTITY full_eval1(ENTITY ee, LONG *time_bound)
{
  ENTITY e, result, defer;
  REG_TYPE mark = reg2(&e, &defer);

  /*---------------------------------------------------------*
   * Suppress recomputation, do the evaluation, and restore  *
   * recomputation status.  This is done because, when       *
   * fully evaluating, we don't want anything left over      *
   * being lazy.	 				     *
   *---------------------------------------------------------*/

  should_not_recompute++;
  e = full_eval2(ee, time_bound, &defer);
  should_not_recompute--;

  /*------------------------------------------------------------------*
   * If full_eval2 has not deferred anything, then just return its    *
   * result.  							      *
   *------------------------------------------------------------------*/

  if(TAG(defer) == NOREF_TAG) {

    /*------------------------------------------------------------------*
     * full_eval2 can leave bound nonshared unknowns in place.  We	*
     * do not want the top level result to be such a thing, so		*
     * we do an eval here to get the top level evaluated.		*
     *------------------------------------------------------------------*/

    if(MEMBER(TAG(e), all_lazy_tags)) {
      LONG l_time = LONG_MAX;
      result = eval1(e, &l_time);
    }
    else result = e;
  }

  /*------------------------------------------------------------------*
   * If full_eval2 has deferred something, then build a full_eval     *
   * lazy primitive to finish the job.				      *
   *------------------------------------------------------------------*/

  else /* TAG(defer) != NOREF_TAG */ {
    result = make_lazy_prim(FULL_EVAL_TMO, e, defer);
  }

  unreg(mark);
  return result;
}

/*---------------------------------------------------------------*/

ENTITY full_eval2(ENTITY ee, LONG *time_bound, ENTITY *defer)
{
  /*--------------------------------------------------------------------*
   * ent_to_return is the value that should be returned, and 		*
   * ent_to_return_loc is a pointer to a word that holds		*
   * the same value as ent_to_return.  If the value of 			*
   * ent_to_return is an entity that needs an indirection,		*
   * such as a lazy entity, then ent_to_return_loc is a pointer		*
   * into the heap.  Otherwise, it can point either into the		*
   * heap or into the stack.  The relationship between			*
   * ent_to_return and ent_to_return_loc is maintained.			*
   *									*
   * As full_eval2 goes through the entity, it keeps variable		*
   * ent_to_eval holding the entity that it is currently evaluating.	*
   * ent_to_eval_loc is a pointer to a word that contains 		*
   * the same value as ent_to_eval, and it points into the		*
   * heap if ent_to_eval needs an indirection.				*
   *									*
   * When an evaluation of ent_to_eval is done, the result is memoized  *
   * into location ent_to_eval_loc.					*
   *--------------------------------------------------------------------*/

  ENTITY      ent_to_return       = ee;
  ENTITY*     ent_to_return_loc   = &ent_to_return;
  ENTITY      ent_to_eval         = ee;
  ENTITY*     ent_to_eval_loc     = &ent_to_eval;

  /*--------------------------------------------------------------------*
   * The variables need to be registered with the garbage collector,    *
   * since we perform evaluations.                                      *
   *--------------------------------------------------------------------*/

  REG_TYPE    mark    = reg2_param(&ent_to_return, &ent_to_eval);
  REGPTR_TYPE ptrmark = reg1_ptrparam(&ent_to_return_loc);
  reg1_ptrparam(&ent_to_eval_loc);

  /*----------------------------------*
   * The default is to defer nothing. *
   *----------------------------------*/

  *defer = zero;

  /*---------------------------------*
   * The loop is for tail recursion. *
   *---------------------------------*/

  for(;;) {
    TIME_STEP(time_bound);
    if(failure >= 0) goto out;

    switch(TAG(ent_to_eval)) {

      /*--------------------------------*/
      case INDIRECT_TAG:
        /*-------------------------------------------------------*
         * We cannot evaluate NOTHING.  Just return zero for it. *
         *-------------------------------------------------------*/

	if(VAL(ent_to_eval) == 0) {
	  ent_to_eval = zero;
	  ent_to_eval_loc = &ent_to_eval;
	  goto out;
	}

        /*--------------------------------------------------------------*
         * At an indirection, just go past a chain of indirections and  *
         * see what is at the end of the chain.                         *
	 *--------------------------------------------------------------*/

        else {
	  ent_to_eval_loc = reduce_indirection_chain(ent_to_eval_loc);
	  ent_to_eval     = *ent_to_eval_loc;
	  break;
	}

      /*--------------------------------*/
      case GLOBAL_INDIRECT_TAG:
	ent_to_eval_loc = ENTVAL(ent_to_eval);
	/* No break: continue with GLOBAL_TAG */

      /*--------------------------------*/
      case GLOBAL_TAG:
	ent_to_eval     = global_eval(ent_to_eval_loc, time_bound);
	break;

      /*--------------------------------*/
      case LAZY_TAG:
	ent_to_eval = lazy_eval(ent_to_eval_loc, time_bound);
	break;

      /*--------------------------------*/
      case LAZY_LIST_TAG:
	ent_to_eval = lazy_list_eval(ent_to_eval_loc, time_bound);
	break;

      /*--------------------------------*/
      case LAZY_PRIM_TAG:
	{Boolean nostore;
	 ENTITY ent_to_eval_at_end, *ent_to_eval_loc_at_end, *p;
	 int tag;

	 p = ENTVAL(ent_to_eval);
         if(NRVAL(*p) == FULL_EVAL_TMO) {
	   ent_to_eval_loc = p + 2;
           ent_to_eval     = *ent_to_eval_loc;
	 }
	 else {
	   ent_to_eval = redo_lazy_prim(ent_to_eval_loc, 
					time_bound, &nostore);

	   /*--------------------------------------------------------*
	    * Pull INDIRECT_TAGs and GLOBAL_INDIRECT_TAGs off, since *
	    * they might be left there by redo_lazy_prim.  We do     *
	    * not change ent_to_eval yet, but just find out what is  *
	    * at the end of any indirection chain.	 	     *
	    *--------------------------------------------------------*/

	   tag                    = TAG(ent_to_eval);
	   ent_to_eval_at_end     = ent_to_eval;
	   ent_to_eval_loc_at_end = ent_to_eval_loc;
	   while((tag == INDIRECT_TAG && VAL(ent_to_eval_at_end) != 0)
		 || tag == GLOBAL_INDIRECT_TAG) {
	     ent_to_eval_loc_at_end = ENTVAL(ent_to_eval_at_end);
	     ent_to_eval_at_end     = *ent_to_eval_loc_at_end;
	     tag                    = TAG(ent_to_eval_at_end);
	   }

	   /*-----------------------------------------------------------*
	    * If the result of evaluation is an unbound unknown, then	*
	    * full evaluation is impossible.  redo_lazy_prim will have  *
	    * forced a time-out.  Just get out now.			*
	    *-----------------------------------------------------------*/

	   if(tag == LAZY_PRIM_TAG && 
	      NRVAL(*ENTVAL(ent_to_eval_at_end)) >= UNKNOWN_TMO) {
	     goto out;
	   }

	   /*---------------------------------------------------------*
	    * If we can't memoize the result, then alter 	      *
	    * ent_to_eval_loc to prevent storing at the old location. *
	    * ent_to_eval should not need indirection here.	      *
	    *							      *
	    * If memoizing is called for, then store.		      *
	    *---------------------------------------------------------*/

	   if(nostore) {
	     if(needs_indirection(ent_to_eval)) {
	       ent_to_eval_loc  = allocate_entity(1);
	       *ent_to_eval_loc = ent_to_eval;
	     }
	     else ent_to_eval_loc = &ent_to_eval;
	   }
	   else {
	     *ent_to_eval_loc = ent_to_eval;
	   }
	 }
	 break;  /* tail recur */
       }

      /*--------------------------------*/
      case FILE_TAG:
        /*--------------------------------------------------------------*
         * An outfile is already evaluated.  Infiles need to		*
         * be forced all the way down their length.			*
         *--------------------------------------------------------------*/

	{Boolean is_infile;
	 ent_to_eval = file_eval(ent_to_eval_loc, &is_infile, time_bound);
	 if(!is_infile) goto out;
	 break;
	}

      /*--------------------------------*/
      case FAIL_TAG:
        failure_as_entity = *ENTVAL(ent_to_eval);
	failure           = qwrap_tag(failure_as_entity);
	goto out;

      /*--------------------------------*/
      case PAIR_TAG:
	 /*---------------------------------------------------------*
	  * For a pair, evaluate both parts.  Be sure to tail recur *
	  * on the right-hand member, since that is where the depth *
	  * usually is.						    *
	  *---------------------------------------------------------*/

	{ENTITY*  pair     = ENTVAL(ent_to_eval);
	 ENTITY   tmpdefer = zero;
	 REG_TYPE mark1    = reg1_param(&tmpdefer);
	 REGPTR_TYPE ptrmark1 = reg1_ptrparam(&pair);

	 IN_PLACE_FULL_EVAL2(pair[0], time_bound, &tmpdefer);

	 /*----------------------------------------------------------*
	  * If the left-hand member timed out, and left a deferral,  *
	  * then build a deferral that includes this deferral and    *
	  * the right-hand member of this pair. 		     *
	  *----------------------------------------------------------*/

	 if(TAG(tmpdefer) != NOREF_TAG) {
	   *defer = ast_pair(tmpdefer, ENTP(INDIRECT_TAG, pair + 1));
	   goto out;
	 }

	 /*---------------------------------------------------------*
	  * If evaluation of the left-hand member failed, then just *
	  * return.						    *
	  *---------------------------------------------------------*/

	 if(failure >= 0) goto out;

	 /*---------------------------------------------------------*
	  * If evaluation of the left-hand member succeeded, then   *
          * evaluate the right-hand member by going around the loop.*
	  *---------------------------------------------------------*/

	 ent_to_eval_loc = pair + 1;
	 ent_to_eval     = *ent_to_eval_loc;
	 unreg(mark1);
	 unregptr(ptrmark1);
	 break;
       }

      /*--------------------------------*/
      case TRIPLE_TAG:
      case QUAD_TAG:

	/*------------------------------------*
	 * Just split and handle like a pair. *
	 *------------------------------------*/

	{ENTITY*  loc      = ENTVAL(ent_to_eval);
	 ENTITY   tmpdefer = zero;
	 REG_TYPE mark1    = reg1_param(&tmpdefer);
	 REGPTR_TYPE ptrmark1 = reg1_ptrparam(&loc);

	 IN_PLACE_FULL_EVAL2(loc[0], time_bound, &tmpdefer);

	 ent_to_eval     = BLOCK_TAIL(ent_to_eval);
         ent_to_eval_loc = &ent_to_eval;

	 if(TAG(tmpdefer) != NOREF_TAG) {
	   *defer = ast_pair(tmpdefer, ent_to_eval);
	   goto out;
	 }
	 if(failure >= 0) goto out;

	 unreg(mark1);
	 unregptr(ptrmark1);
	 break;
	}

      /*--------------------------------*/
      case APPEND_TAG:

        /*---------------------------------------------------*
	 * Fully evaluate the left-hand list, and then loop  *
	 * on the right-hand list.			     *
         *---------------------------------------------------*/

	{ENTITY*  loc      = ENTVAL(ent_to_eval);
	 ENTITY   tmpdefer = zero;
	 REG_TYPE mark1    = reg1_param(&tmpdefer);
	 REGPTR_TYPE ptrmark1 = reg1_ptrparam(&loc);

	 IN_PLACE_FULL_EVAL2(loc[0], time_bound, &tmpdefer);

         ent_to_eval_loc = loc + 1;
	 ent_to_eval     = *ent_to_eval_loc;

	 if(TAG(tmpdefer) != NOREF_TAG) {
	   *defer = ast_pair(tmpdefer, ent_to_eval);
	   goto out;
	 }
	 if(failure >= 0) goto out;

	 unreg(mark1);
	 unregptr(ptrmark1);
	 break;
	}

      /*--------------------------------*/
      case TREE_TAG:

	/*------------------------------------------------------------*
	 * Eval ent_to_eval, and replace it by a pair.  Note that we  *
	 * might have stripped an indirection from this tree in a     *
	 * previous iteration, and that indirection needs to be	      *
	 * restored for IN_PLACE_EVAL. 				      *
	 *------------------------------------------------------------*/

	if(ENT_EQ(*ENTVAL(ent_to_eval), NOTHING)) {
	  ent_to_eval = ENTP(INDIRECT_TAG, ent_to_eval_loc);
	  IN_PLACE_EVAL_FAILTO(ent_to_eval, time_bound, out);
	  *ent_to_eval_loc = ent_to_eval;
	}

        /*--------------------------------------------------------------*
         * Now ent_to_eval has been evaluated to its top level.  	*
	 * Continue on the parts.					*
	 *--------------------------------------------------------------*/

	{ENTITY hd, tl, tmpdefer = zero;
	 REG_TYPE mark1 = reg2(&hd, &tl);
         reg1_param(&tmpdefer);
	 ast_split(ent_to_eval, &hd, &tl);

	 IN_PLACE_FULL_EVAL2(hd, time_bound, &tmpdefer);
	 if(TAG(tmpdefer) != NOREF_TAG) {
	   *defer = ast_pair(tmpdefer, tl);
	   goto out;
	 }
	 if(failure >= 0) goto out;

	 ent_to_eval_loc = &ent_to_eval;
	 ent_to_eval     = tl;
	 unreg(mark1);
	 break;
	}

      /*--------------------------------*/
      case ARRAY_TAG:
	{ENTITY* hd = ENTVAL(ent_to_eval);
	 switch(TAG(hd[1])) {
	   case BOX_TAG:
	     goto out;
	   
	   case PLACE_TAG:
	   case STRING_TAG:
	     ent_to_eval_loc = hd + 2;
	     ent_to_eval     = *ent_to_eval_loc;
	     break;

	   case INDIRECT_TAG:
	   case ENTITY_TOP_TAG:
	     {int i;
	      int         len      = IVAL(hd[0]);
	      ENTITY*     bdy      = ENTVAL(hd[1]);
	      ENTITY      tmpdefer = zero;
	      REG_TYPE    mark1    = reg1_param(&tmpdefer);
	      REGPTR_TYPE mark2    = reg1_ptrparam(&bdy);

	      for(i = 0; i < len; i++) {
		IN_PLACE_FULL_EVAL2(bdy[i], time_bound, &tmpdefer);
		if(TAG(tmpdefer) != NOREF_TAG) {
		  *defer = ast_pair(tmpdefer, 
				    array_sublist(ent_to_eval, i+1, len-(i+1),
						  time_bound, FALSE));
		  goto out;
		}
	      }

	      ent_to_eval_loc = hd + 2;
	      ent_to_eval     = *ent_to_eval_loc;
	      unreg(mark1;)
	      unregptr(mark2);
	      break;
	     }
	 } /* end switch(TAG(hd[1])) */
	 break;
        }

      /*--------------------------------*/
     case WRAP_TAG:
	ent_to_eval_loc = ENTVAL(ent_to_eval) + 1;
	ent_to_eval     = *ent_to_eval_loc;
	break;

      /*--------------------------------*/
      case QWRAP0_TAG:
      case QWRAP1_TAG:
      case QWRAP2_TAG:
      case QWRAP3_TAG:
	ent_to_eval_loc = ENTVAL(ent_to_eval);
	ent_to_eval     = *ent_to_eval_loc;
	break;

      /*--------------------------------*/
      default:
	goto out;

    } /* end switch */

  } /* end for */

 out:

  /*--------------------------------------------------------------------*
   * ent_to_return might still point to an indirection or global	*
   * indirection, since ent_to_eval is what got changed.  Skip		*
   * over that chain now.  
   *--------------------------------------------------------------------*/

  {int tag = TAG(ent_to_return);
   while((tag == INDIRECT_TAG && VAL(ent_to_return) != 0)
	 || tag == GLOBAL_INDIRECT_TAG) {
     ent_to_return_loc = ENTVAL(ent_to_return);
     ent_to_return     = *ent_to_return_loc;
     tag               = TAG(ent_to_return);
   }

   /*------------------------------------------------------------*
    * If ent_to_return is a FULL_EVAL_TMO lazy prim, then we     *
    * want to return the value of that, to prevent reevaluation. *
    *------------------------------------------------------------*/

   while(tag == LAZY_PRIM_TAG) {
     ENTITY* p = ENTVAL(ent_to_return);
     if(NRVAL(*p) == FULL_EVAL_TMO) {
       ent_to_return_loc = p + 1;
       ent_to_return     = *ent_to_return_loc;
       tag               = TAG(ent_to_return);
     }
     else break;
   }
  }

  /*-------------------------------------------------*
   * Now add any indirection that needs to be added. *
   *-------------------------------------------------*/

  ent_to_return = indirect_replacement(ent_to_return_loc);

  /*------------------------------------------------------------*
   * If timed out and did not defer ent_to_eval, then defer	*
   * it now.							*
   *------------------------------------------------------------*/

  if(failure == TIME_OUT_EX && TAG(*defer) == NOREF_TAG) {
    *defer = indirect_replacement(ent_to_eval_loc);
  }

  unreg(mark);
  unregptr(ptrmark);
  return ent_to_return;
}


