/**********************************************************************
 * File:    evaluate/lazyprim.c
 * Purpose: Functions to lazy primitives, unknowns.  Also fairmerge,
 *          used in forcing concurrent execution.
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
 * This file provides support for lazy primitives and for unknowns.	*
 *									*
 * Each lazy primitive has a kind and two arguments, a and b.  Here	*
 * are the specifics.  When the meaning is an expression, the value	*
 * of the lazy primitive is the value of that expression.		*
 *									*
 * kind		Meaning of lazy prim (kind, a, b).			*
 * -----	----------------------------------			*
 * 									*
 * EQUAL_TMO	a == b							*
 * 									*
 * LENGTH_TMO	length(a) + b						*
 * 									*
 * MERGE_TMO	The result is a fair merge of a and b.			*
 * 									*
 * SUBLIST_TMO	  a ## (i,n), if s = false,                             *
 *                a # i       if s = true                               *
 *              where b = (s,i,n).  If s is true, the n must be 1.      *
 *									*
 * INTERN_TMO	a is a string that is to be interned into the string	*
 *		table, and b is the length of a.  Return the result of  *
 *		interning a.						*
 * 									*
 * PACK_TMO	pack(a), where b is the length of a.			*
 * 									*
 * UPTO_TMO	a upto b.						*
 * 									*
 * DOWNTO_TMO	a downto b.						*
 * 									*
 * LAZY_HEAD_TMO  head(a)						*
 * 									*
 * LAZY_TAIL_TMO  tail(a)						*
 * 									*
 * LAZY_LEFT_TMO  left(a).  If a is an unbound, unprotected unknown,	*
 *		  then bind it to a pair first.				*
 * 									*
 * LAZY_RIGHT_TMO  right(a).  If a is an unbound, unprotected unknown,	*
 *		   then bind it to a pair.				*
 * 									*
 * SCAN_FOR_TMO    Suppose scanForChar(a,l,sense,c) = (n,s,c'), where 	*
 * 		   b = (f,c,l,sense)).  Return (n+f, s, c').		*
 *		   b might not be evaluated.				*
 * 									*
 * REVERSE_TMO	reverse(a) ++ b.					*
 * 									*
 * FULL_EVAL_TMO  Same as a, but should only return after b is fully	*
 * 		  evaluated.						*
 * 									*
 * INFLOOP_TMO	The result is a failure with value INFLOOP_EX.		*
 * 									*
 * CMDMAN_TMO	This kind of lazy primitive is used for sending the	*
 * 		standard input to a process forked by the system	*
 *		function.  a is (pid,fd), where pid is the process id	*
 *		of the process that is being sent to, and fd is the	*
 *		file descriptor of the pipe to send the data on.  	*
 *		b is the data to send.  Evaluating this lazy primitive	*
 *		causes some of the data to be sent.			*
 * 									*
 * DO_CMD_TMO	a is a command to be executed, and s is its standard	*
 * 		input.  Return the standard output of that command.	*
 * 									*
 * UNKNOWN_TMO	An unknown is stored with tag UNKNOWN_TMO (for an	*
 * 		unprotected unknown) or PROTECTED_UNKNOWN_TMO (for	*
 * 		a protected unknown).  The pair (a,b) stored with it	*
 * 		are 							*
 * 		   a =  a box that is empty when the unknown is		*
 *			unbound, and holds the value of the unknown 	*
 *			when the unknown is bound,			*
 * 		   b =  the key, for a protected unknown.		*
 * 									*
 * PROTECTED_UNKNOWN_TMO See UNKNOWN_TMO.				*
 ************************************************************************/

#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/instruc.h"
#include "../evaluate/evaluate.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../tables/tables.h"
#ifdef DEBUG
# include "../debug/debug.h"
# include "../show/prtent.h"
#endif


/*************************************************************
 *			MAKE_LAZY_PRIM			     *
 *************************************************************
 * Make a lazy prim entity of kind k, with entities a and b  *
 * as auxiliary data.  See above for a discussion of lazy    *
 * primitives.                            		     *
 *************************************************************/

ENTITY make_lazy_prim(int k, ENTITY a, ENTITY b)
{
  ENTITY* r = allocate_entity(4);

  r[0] = ENTU(k);
  r[1] = a;
  r[2] = b;
  r[3] = ENTP(LAZY_PRIM_TAG, r);
  return ENTP(INDIRECT_TAG, r+3);
}


/*********************************************************
 *			REDO_LAZY_PRIM			 *
 *********************************************************
 * Restart computation of entity e, which must have tag	 *
 * LAZY_PRIM_TAG.  Return the result of that compuation. *
 *							 *
 * *time_bound is the maximum number of steps to use.  	 *
 * It is decreased by the number of steps made. 	 *
 * 							 *
 * *no_store is set true if the result of evaluation	 *
 * should not be memoized.				 *
 *							 *
 * XREF:						 *
 *   Called in evalsup.c to evaluate a lazy primitive.	 *
 *********************************************************/

ENTITY redo_lazy_prim(ENTITY *e, LONG *time_bound, Boolean *nostore)
{
  LONG hold_time, list_prim_time, old_num_repauses;
  ENTITY *p, a, b, c, result;
  REG_TYPE mark;
  REGPTR_TYPE ptrmark;
  int tag;

  *nostore = FALSE;

  /*----------------------------------------------------*
   * Scan through bindings of unknowns.  At an unbound  *
   * unknown, time out. If a bound nonshared unknown    *
   * is encountered, set nostore = TRUE, to prevent     *
   * shared memoizing of nonshared bindings.            *
   *----------------------------------------------------*/

  p = ENTVAL(*e);
  tag = toint(NRVAL(p[0]));
  if(tag >= UNKNOWN_TMO) {
    a = scan_through_unknowns(*e, nostore);
    if(TAG(a) != LAZY_PRIM_TAG) return a;
    p   = ENTVAL(a);
    tag = toint(NRVAL(p[0]));
    if(tag >= UNKNOWN_TMO) {
      pause_this_thread(time_bound);
      *nostore = TRUE;
      return a;
    }
  }

  /*----------------------------------------------------------*
   * Get the additional information stored with this lazy     *
   * primitive.  The default result is nil.		      *
   *----------------------------------------------------------*/

  mark    = reg3(&a, &b, &c);
  ptrmark = reg1_ptrparam(&e);
  a       = p[1];
  b       = p[2];
  result  = nil;

  /*--------------------------------------------------------------------*
   * We will adjust the time downward to break off deep recursions.	*
   * The time is pushed back up below. But if there is a repause, then 	*
   * don't push the time back up, to prevent trying again and again	*
   * to get a value that is not available.				*
   *--------------------------------------------------------------------*/

  list_prim_time   = LIST_PRIM_TIME;
  old_num_repauses = num_repauses;
  hold_time = 0; /* To suppress action at end when hold_time is not used */

  switch(tag) {
    case EQUAL_TMO:

	/*----------------------------------------*
         * Compute a == b, with instruction inst. *
	 *----------------------------------------*/

	/*-----------------------------------------------------------*
	 * Set time low to force early return on long lists, to give *
	 * an opportunity to drop reference.  That is, on a long     *
         * list, the garbage collector might come in and get back    *
         * part of what we have produced here, but only if given     *
         * a chance to run.					     *
	 *-----------------------------------------------------------*/

	hold_time = *time_bound;
	if(hold_time > list_prim_time) *time_bound = list_prim_time;

	/*--------------------------*
	 * Perform the computation. *
	 *--------------------------*/

	result = ast_equal(a, b, time_bound);
	break;

    case LENGTH_TMO:

	/*------------------------*
	 * Compute length(a) + b. *
	 *------------------------*/

	/*-----------------------------------------------------------*
	 * Set time low to force early return on long lists, to give *
	 * an opportunity to drop reference. 			     *
	 *-----------------------------------------------------------*/

	hold_time = *time_bound;
	if(hold_time > list_prim_time) *time_bound = list_prim_time;

        /*--------------------------*
         * Perform the computation. *
         *--------------------------*/

	result = ast_length(a, get_ival(b,LIMIT_EX), time_bound);
	break;

    case SUBLIST_TMO:

	/*----------------------------------------------*
	 * Compute                                      *
	 *    a ## (i,n) if s is true, or		*
	 *    a #  i     if s is false                  *
	 *  where b is (s,i,n). 			*
	 *----------------------------------------------*/

	/*-----------------------------------------------------------*
	 * Set time low to force early return on long lists, to give *
	 * an opportunity to drop reference. 			     *
	 *-----------------------------------------------------------*/

	hold_time = *time_bound;
        if(hold_time > list_prim_time) *time_bound = list_prim_time;

        /*--------------------------*
         * Perform the computation. *
         *--------------------------*/

	{ENTITY i, n, s, in;
	 ast_split(b, &s, &in);
         ast_split(in, &i, &n);
	 result = ast_sublist1(a,
			      index_val(i),
			      index_val(n),
			      time_bound, TRUE, VAL(remove_indirection(s)));
        }
	break;

    case PACK_TMO:

	/*----------------------------------------------*
	 * Compute pack(a), where b is the length of a. *
	 *----------------------------------------------*/

	result = ast_pack(a, b, time_bound);
	break;

    case INTERN_TMO:

 	/*------------------------------------------*
	 * Return the result of interning string a. *
	 * b is the length of a.		    *
	 *------------------------------------------*/

        {LONG len;
	 IN_PLACE_EVAL_FAILTO(b, time_bound, fail_intern);
	 len = get_ival(b, LIMIT_EX);
	 if(failure > 0) {
	   failure = -1;
	   result = a;
	 }
	 else {
	   char* result_buff;
	   IN_PLACE_FULL_EVAL_FAILTO(a, time_bound, fail_intern);
	   result_buff = (char*) BAREMALLOC(len + 1);
	   copy_str(result_buff, a, len, &b);
	   result = make_cstr(result_buff);
	   FREE(result_buff);
	 }
	 break;

        fail_intern:
	 result = make_lazy_prim(INTERN_TMO, a, b);
	 break;
        }

    case FULL_EVAL_TMO:

      /*----------------------------------------------------------------*
       * The result is the same as a, but we should only return after   *
	 b is fully evaluated.						*
       *----------------------------------------------------------------*/

      {ENTITY defer = zero;

       /*------------------------------------------------------------*
        * Rerun full_eval.  Note that b is to be evaluated, but a is *
        * to be returned.  (b is some part of a that might need      *
        * further evaluation.)					     *
        *------------------------------------------------------------*/

       should_not_recompute++;
       IN_PLACE_FULL_EVAL2(b, time_bound, &defer);
       should_not_recompute--;

       /*--------------------------------------------------------*
        * If done, then just return a.  If evaluation timed out, *
        * then build another lazy value.			 *
        *--------------------------------------------------------*/

       if(TAG(defer) != NOREF_TAG) {
	 result = make_lazy_prim(FULL_EVAL_TMO, a, defer);
       }
       else {
         result = a;
       }
       break;
      }

    case MERGE_TMO:

	/*----------------------------------------*
	 * Compute a fair merge of lists a and b. *
	 *----------------------------------------*/

	/*-----------------------------------------------------------*
	 * Set time low to force early return on long lists, to give *
	 * an opportunity to drop reference. 			     *
	 *-----------------------------------------------------------*/

	hold_time = *time_bound;
	if(hold_time > list_prim_time) *time_bound = list_prim_time;

	/*--------------------*
	 * Perform the merge. *
	 *--------------------*/

	result = ast_merge(a, b, time_bound);
	break;

    case UPTO_TMO:

	/*---------------------------------------------------*
	 * Compute a _upto_ b.  Normally suppress memoizing. *
	 *---------------------------------------------------*/

	if(ast_compare_simple(a,b) == 1) result = nil;
	else {
	  c = ast_add(a,one);
	  if(ast_compare_simple(c,b) == 1) result = ast_pair(a, nil);
	  else {
	    result = make_lazy_prim(UPTO_TMO, ast_add(c,one), b);
	    result = ast_triple(a, c, result);
	  }
	  if(should_not_recompute == 0) {
	    *nostore = TRUE;
	    goto out1;
	  }
	}
	break;

    case DOWNTO_TMO:

	/*-----------------------------------------------------*
	 * Compute a _downto_ b.  Normally suppress memoizing. *
	 *-----------------------------------------------------*/

	if(ast_compare_simple(a,b) == 2) result = nil;
	else {
	  c = ast_subtract(a, one);
	  if(ast_compare_simple(c,b) == 2) result = ast_pair(a, nil);
	  else {
	    result = make_lazy_prim(DOWNTO_TMO, ast_subtract(c,one), b);
	    result = ast_triple(a, c, result);
	  }
	  if(should_not_recompute == 0) {
	    *nostore = TRUE;
	    goto out1;
	  }
	}
	break;

    case LAZY_HEAD_TMO:
    case LAZY_TAIL_TMO:
	SET_EVAL(c, a, time_bound);
	goto do_lazy_part;  /* below, under next case */

    case LAZY_LEFT_TMO:
    case LAZY_RIGHT_TMO:
	SET_EVAL_TO_UNPROT_UNKNOWN(c, a, time_bound);
	if(failure < 0) {
	  if(is_unprot_unknown(c)) c = split_unknown(c);
	}
	/* goto do_lazy_part; */

    do_lazy_part:
	if(failure < 0) {
	  result = (tag == LAZY_HEAD_TMO || tag == LAZY_LEFT_TMO)
		      ? ast_head(c)
		      : ast_tail(c);
	}
	else {
	  result = (failure == TIME_OUT_EX) ?
		       make_lazy_prim(tag, c, nil) : bad_ent;
	}
	break;

    case INFLOOP_TMO:
	failure = INF_LOOP_EX;
	result = *e;
	break;

#ifdef UNIX
    /*---------------------------------------------*
     * See lazyprim.doc for a discussion of these. *
     *---------------------------------------------*/

    case CMDMAN_TMO:
	result = cmdman(a, b, time_bound);
	break;

    case DOCMD_TMO:
	result = do_cmd(a, b, time_bound);
	break;
#endif

    case SCAN_FOR_TMO:
	{ENTITY the_set, the_sense, the_offset, the_leadin;
	 REG_TYPE mark1;

	 /*-----------------------------------------------------*
          * This implements standard function scanForChar.      *
	  * a and b are the two entities stored with this	*
	  * structure.						*
          *							*
	  * Suppose scanForChar(a,leadin,theset,sense) = 	*
	  * (n,s,c'), where b = (f, leadin, theset, sense)).	*
	  * Then we should return (n+f, s, c').			*
          *							*
	  * b might not be evaluated.  Evaluate it now.  Due to *
	  * the setup in scan_for_stdf, this will cause b to    *
	  * be fully evaluated.					*
	  *-----------------------------------------------------*/

	 IN_PLACE_EVAL_FAILTO(b, time_bound, scan_for_fail);

	 /*-----------------------------------------------------------*
	  * Set time low to force early return on long lists, to give *
	  * an opportunity to drop references. 			      *
	  *-----------------------------------------------------------*/

	 hold_time = *time_bound;
	 if(hold_time > list_prim_time) *time_bound = list_prim_time;

	 mark1 = reg2(&the_set, &the_sense);
	 reg2(&the_offset, &the_leadin);
         ast_split(b, &the_offset, &b);
         ast_split(b, &the_leadin, &b);
         ast_split(b, &the_set, &the_sense);

	 result = scan_for_help(a, 
				remove_indirection(the_set), 
				remove_indirection(the_offset), 
				remove_indirection(the_sense),
				remove_indirection(the_leadin), 
				time_bound);
	 unreg(mark1);
	 break;

       scan_for_fail:
         result = make_lazy_prim(SCAN_FOR_TMO, a, b);
	 break;
	}

    case REVERSE_TMO:

      /*-----------------------------------------*
       * Return the reversal of a followed by b. *
       *-----------------------------------------*/

      result = ast_reverse(a, b, time_bound);
      break;

    default:
	die(75, (char *) tag);
	result = false_ent;    /* Suppresses warning */
  }

  /*--------------------------------------------------*
   * Memoize the result, restore the time and return. *
   *--------------------------------------------------*/

  *e = result;

  /*--------------------------------------------------------------------*
   * Restore the time, if the time was artificially lowered, and a 	*
   * repause was not done.						*
   *--------------------------------------------------------------------*/

  if(hold_time > list_prim_time && old_num_repauses == num_repauses) {
    *time_bound += hold_time - list_prim_time;
  }

 out1:
  unreg(mark);
  unregptr(ptrmark);
  return result;
}


/********************************************************
 *			AST_MERGE			*
 ********************************************************
 * Merge lists a and b in a "fair" way.			*
 *							*
 * XREF: Called above to evalute MERGE_TMO prim.	*
 ********************************************************/

ENTITY ast_merge(ENTITY aa, ENTITY bb, LONG *time_bound)
{
  LONG t, half_time;
  ENTITY h, hold;
  REG_TYPE mark = reg2(&hold, &h);
  ENTITY a = aa;
  ENTITY b = bb;
  reg2_param(&a, &b);

  /*-----------------------------*
   * We will evaluate a, then b. *
   *-----------------------------*/

  while(*time_bound > 1) {
    half_time = (*time_bound) >> 1;
    if(half_time > MERGE_MAX_TIME) half_time = MERGE_MAX_TIME;
    else if(half_time < MERGE_MIN_TIME) half_time = MERGE_MIN_TIME;

    /*-------------------*
     * First evaluate a. *
     *-------------------*/

    t = half_time;
    IN_PLACE_EVAL(a, &t);

    /*--------------------------------------------------*
     * If evaluation of a succeeded, then there are two	*
     * possibilities.  If a is nil, then the merge of a *
     * and b is b.  If a is not nil, then the merge of  *
     * a and b is the list h::rest where h is the head  *
     * of a and rest is the result of merging b with    *
     * the tail of a.  Put b as the first parameter in  *
     * the recursive merge, so that it will be evaluated*
     * first next time.					*
     *							*
     * In the special case where a is an append, we     *
     * know that no evaluation is needed on the left    *
     * hand side, so just build an append for the       *
     * result.						*
     *--------------------------------------------------*/

    if(failure < 0) {
      if(IS_NIL(a)) {unreg(mark); return b;}
      if(TAG(a) == APPEND_TAG) {
	ENTITY* p = ENTVAL(a);
	h = p[1];
	hold = make_lazy_prim(MERGE_TMO, b, h);
	h = p[0];
	hold = quick_append(h, hold);
      }
      else {
	h = ast_tail(a);
	hold = make_lazy_prim(MERGE_TMO, b, h);
	h = ast_head(a);
	hold = ast_pair(h, hold);
      }
      unreg(mark);
      return hold;
    }

    /*----------------------------------------------*
     * If evaluation of a fails, then just give up. *
     *----------------------------------------------*/

    else if(failure != TIME_OUT_EX) {unreg(mark);  return nil;}

    num_thread_switches++;
    num_threads++;
    failure = -1;
    *time_bound = *time_bound - (half_time - t);

    /*------------------------------------------------*
     * Now evaluate b. This part is a mirror image of *
     * evaluation of a.				      *
     *------------------------------------------------*/

    t = half_time;
    IN_PLACE_EVAL(b, &t);
    if(failure < 0) {
      if(IS_NIL(b)) {unreg(mark); return a;}
      if(TAG(b) == APPEND_TAG) {
	ENTITY* p = ENTVAL(b);
	h = p[1];
	hold = make_lazy_prim(MERGE_TMO, a, h);
	h = p[0];
	hold = quick_append(h, hold);
      }
      else {
	h    = ast_tail(b);
	hold = make_lazy_prim(MERGE_TMO, a, h);
	h    = ast_head(b);
	hold = ast_pair(h, hold);
    }
      unreg(mark);
      return hold;
    }
    else if(failure != TIME_OUT_EX) {unreg(mark);  return nil;}

    num_thread_switches++;
    num_threads++;
    failure = -1;
    *time_bound = *time_bound - (half_time - t);
    if(*time_bound < 0) *time_bound = 0;
  }

  /*-------------------------------------------------*
   * When merge times out, it will have incremented  *
   * num_thread_switches at the last iteration.      *
   * We need to undo that, since that last iteration *
   * is not really a switch at all, but results in   *
   * merge timing-out its parent thread. 	     *
   * (num_thread_switches will be incremented when   *
   * the evaluator realizes that the parent thread   *
   * has timed out.)				     *
   *-------------------------------------------------*/

  num_thread_switches--;
  hold = make_lazy_prim(MERGE_TMO, a, b);
  unreg(mark);
  return hold;
}


/*===============================================================
			UNKNOWNS
  ===============================================================*/

PRIVATE LONG next_protected_unknown_key = 0;

/****************************************************************
 *			EVAL_LAZY_PRIM_TO_UNKNOWN 		*
 ****************************************************************
 * Evaluate *e, which must have tag LAZY_PRIM_TAG, to one	*
 * level.  (That is, if it ends up being a pair, stop with the  *
 * pair, without evaluating the components of the pair.		*
 *								*
 * Stop at any unknown if any_unknown is nonzero, and		*
 * stop at an unprotected unknown if any_unknown is 0.	        *
 * The return value indicates one of the following about the	*
 * return value.						*
 *								*
 *   0 : The result does not have tag LAZY_PRIM_TAG.  Do not	*
 *       store the result; reprocess.				*
 *								*
 *   1 : The result does not have tag LAZY_PRIM_TAG.  Store	*
 *       the result; reprocess.  (Storing is not done here.)	*
 *								*
 *   2 : The result is an unknown that we have been asked to	*
 *       stop at.						*
 *								*
 *   3 : Finished evaluating.					*
 *								*
 *   4 : Not finished evaluating, process normally.		*
 ****************************************************************/

int eval_lazy_prim_to_unknown(ENTITY *e, ENTITY **loc,
			      Boolean any_unknown, LONG *time_bound)
{
  Boolean ovnostore = FALSE;
  Boolean nostore;
  int tmo, tag, result;
  REGPTR_TYPE ptrmark = reg1_ptrparam(&e);
  reg1_ptrparam(loc);

  /*------------------------------------------------------------------*
   * If *e is an unknown, then scan through bound unknowns to either  *
   * an unbound unknown or to something that is not an unknown at all *
   *------------------------------------------------------------------*/

  tmo = toint(NRVAL(*ENTVAL(*e)));
  if(tmo >= UNKNOWN_TMO) {
    *e = scan_through_unknowns(*e, &ovnostore);

    /*---------------------------------------------------------------*
     * Non-unknown: return now, but indicate that further evaluation *
     * might be needed.						     *
     *---------------------------------------------------------------*/

    if(TAG(*e) != LAZY_PRIM_TAG) {
      result = ovnostore ? 0 : 1;
      goto out;
    }

    /*-----------------------------------------------------------------*
     * If this is an unbound unknown of the kind where we are supposed *
     * to stop, then return now.                                       *
     *-----------------------------------------------------------------*/

    tmo = toint(NRVAL(*ENTVAL(*e)));
    if(tmo == UNKNOWN_TMO || (tmo == PROTECTED_UNKNOWN_TMO && any_unknown)) {
      result = 2;
      goto out;
    }
  }

  /*---------------------------------------------------------------------*
   * We only get past the previous part with a lazy primitive.  Evaluate *
   * that lazy primitive now.                                            *
   *---------------------------------------------------------------------*/

  *e = redo_lazy_prim(*loc, time_bound, &nostore);
  if(ovnostore) nostore = TRUE;

  /*---------------------------------------------------------*
   * Pull INDIRECT_TAGs and GLOBAL_INDIRECT_TAGs off, since  *
   * they might be left there by redo_lazy_prim.  We need to *
   * see what kind of thing we are dealing with.             *
   *---------------------------------------------------------*/

  tag = TAG(*e);
  while((tag == INDIRECT_TAG && VAL(*e) != 0) || tag == GLOBAL_INDIRECT_TAG) {
    *loc = ENTVAL(*e);
    *e = **loc;
    tag = TAG(*e);
  }
  result = nostore ? 3 : 4;

 out:
  unregptr(ptrmark);
  return result;
}


/****************************************************************
 *		SCAN_THROUGH_UNKNOWNS,		 		*
 *              GEN_SCAN_THROUGH_UNKNOWNS               	*
 ****************************************************************
 * Return the value that e is bound to, possibly through  	*
 * a chain of unknowns. Set *saw_nonshared to true if any   	*
 * of the boxes in the chain scanned through is nonshared,  	*
 * false otherwise.  If possible, compress the bound path.	*
 *								*
 * For scan_through_unknowns, the tag of e must be LAZY_PRIM_TAG.*
 * For gen_scan_through_unknowns, e can have any tag.		*
 ****************************************************************/

ENTITY scan_through_unknowns(ENTITY e, Boolean *saw_nonshared)
{
  ENTITY path[MAX_UNKNOWN_PATH_COMPRESS];
  ENTITY result;
  int n = 0;                /* How many in path */
  int last_nonshared = -1;  /* Where on the path was the last nonshared box? */
  ENTITY* p = ENTVAL(e);
  int tag = toint(NRVAL(p[0]));
  *saw_nonshared = FALSE;

  /*-----------------------------------------------------------------------*
   * Scan through bound unknowns. Keep track, in saw_nonshared, of whether *
   * a nonshared unknown is encountered. If one is, let last_nonshared be  *
   * the index in the unknown chain of the last nonshared unknown. Also    *
   * keep track of the first MAX_UNKNOWN_PATH_COMPRESS bound unknowns      *
   * in the chain, so that they can be rescanned for path compression.     *
   *-----------------------------------------------------------------------*/

  result = e;
  while(tag >= UNKNOWN_TMO) {
    ENTITY binding, sibinding;
    ENTITY bx = p[1];
    if(TAG(bx) == BOX_TAG) {
      *saw_nonshared = TRUE;
      last_nonshared = n;
    }
    binding = prcontent_stdf(bx);
    if(ENT_EQ(binding, NOTHING)) {
      result = e;
      break;
    }
    else {
      sibinding = binding;
      while(TAG(sibinding) == INDIRECT_TAG) sibinding = *ENTVAL(sibinding);
      if(TAG(sibinding) != LAZY_PRIM_TAG) {
	result = binding;
	break;
      }
      else {
	if(n < MAX_UNKNOWN_PATH_COMPRESS) path[n] = bx;
	n++;
	e = binding;
	p = ENTVAL(sibinding);
	tag = toint(NRVAL(p[0]));
      }
    }
  }

  /*--------------------------------------------------------------------*
   * Compress the path.  The actions taken here are public, so don't do *
   * anything that relies on the binding of a private unknown.          *
   *--------------------------------------------------------------------*/

  {int i;
   int pathsize = n;
   if(pathsize > MAX_UNKNOWN_PATH_COMPRESS) {
     pathsize = MAX_UNKNOWN_PATH_COMPRESS;
   }
   for(i = 0; i < pathsize; i++) {
     if(TAG(path[i]) == BOX_TAG || i >= last_nonshared) {
       simple_assign_bxpl(path[i], result);
     }
   }
  }

  return result;
}

/*-------------------------------------------------------------*/

ENTITY gen_scan_through_unknowns(ENTITY e)
{
  Boolean b;
  ENTITY a = e;

  while(TAG(a) == INDIRECT_TAG) a = *ENTVAL(a);
  if(TAG(a) != LAZY_PRIM_TAG) return e;
  return scan_through_unknowns(a, &b);
}


/****************************************************************
 *			PRIVATE_UNKNOWN_STDF,			*
 *			PUBLIC_UNKNOWN_STDF,			*
 *			PROTECTED_PRIVATE_UNKNOWN_STDF,		*
 *			PROTECTED_PUBLIC_UNKNOWN_STDF		*
 ****************************************************************
 * Return an unknown object.  For an unprotected object,	*
 * only the unknown object itself is returned.  For a protected	*
 * unknown, a pair (u,k) is returned, where u is the unknown	*
 * and k is the write-permission key.				*
 ****************************************************************/

PRIVATE ENTITY make_unknown_stdf(int tag, ENTITY bx)
{
  ENTITY result;

  result = make_lazy_prim(tag, bx, nil);
  if(tag == PROTECTED_UNKNOWN_TMO) {
    ENTITY write_key = ENTU(next_protected_unknown_key++);
    result = make_lazy_prim(tag, bx, write_key);
    result = ast_pair(result, write_key);
  }
  else {
    result = make_lazy_prim(tag, bx, zero);
  }
  return result;
}

/*--------------------------------------------------------------*/

ENTITY private_unknown_stdf(ENTITY herm_unused)
{
  return make_unknown_stdf(UNKNOWN_TMO, ast_new_box());
}

/*--------------------------------------------------------------*/

ENTITY protected_private_unknown_stdf(ENTITY herm_unused)
{
  return make_unknown_stdf(PROTECTED_UNKNOWN_TMO, ast_new_box());
}

/*--------------------------------------------------------------*/

ENTITY public_unknown_stdf(ENTITY herm_unused)
{
  return make_unknown_stdf(UNKNOWN_TMO, ast_new_place());
}

/*--------------------------------------------------------------*/

ENTITY protected_public_unknown_stdf(ENTITY herm_unused)
{
  return make_unknown_stdf(PROTECTED_UNKNOWN_TMO, ast_new_place());
}


/********************************************************
 *		SPLIT_UNKNOWN				*
 ********************************************************
 * Set u to a pair of unknowns, if u is an unprotected	*
 * unknown.  Return the result.  TAG(u) must be		*
 * INDIRECT_TAG.					*
 ********************************************************/

ENTITY split_unknown(ENTITY u)
{
  ENTITY* p = ENTVAL(u);
  ENTITY uu = *p;

  ENTITY *q, u1, u2, box, pair;
  int tmo, box_tag;

  if(TAG(uu) != LAZY_PRIM_TAG) return u;
  q   = ENTVAL(uu);
  tmo = toint(NRVAL(q[0]));
  if(tmo != UNKNOWN_TMO) return u;

  box     = q[1];
  box_tag = TAG(box);
  if(box_tag == BOX_TAG) {
    u1 = private_unknown_stdf(hermit);
    u2 = private_unknown_stdf(hermit);
  }
  else {
    u1 = public_unknown_stdf(hermit);
    u2 = public_unknown_stdf(hermit);
  }

  pair = ast_pair(u1, u2);
  simple_assign_bxpl(box, pair);

  return pair;
}


/************************************************************************
 *			UNKNOWNQ_STDF,					*
 *			PROTECTED_UNKNOWNQ_STDF,			*
 *			UNPROTECTED_UNKNOWNQ_STDF			*
 ************************************************************************
 * Test for unknowns.  unknownq_stdf(x) is true if x is an		*
 * unknown (protected or unprotected).  protected_unknownq_stdf(x)	*
 * is true if x is a protected unknown.   See macros			*
 * is_any_unknown(e), is_prot_unknown(e) and is_unprot_unknown(e)	*
 * in evaluate.h.							*
 ************************************************************************/

Boolean unknownq(ENTITY x, int mode)
{
  int tag;
  ENTITY *p;
  ENTITY a = gen_scan_through_unknowns(x);

  if(TAG(a) != LAZY_PRIM_TAG) return FALSE;

  p   = ENTVAL(a);
  tag = toint(NRVAL(p[0]));

  if(tag == PROTECTED_UNKNOWN_TMO) {
    if(mode & 2) return TRUE;
  }
  else if(tag == UNKNOWN_TMO) {
   if(mode & 1) return TRUE;
  }

  return FALSE;
}

/*--------------------------------------------------------------*/

ENTITY unknownq_stdf(ENTITY x)
{
  return ENTU(unknownq(x,3));
}

/*--------------------------------------------------------------*/

ENTITY protected_unknownq_stdf(ENTITY x)
{
  return ENTU(unknownq(x, 2));
}

/*--------------------------------------------------------------*/

ENTITY unprotected_unknownq_stdf(ENTITY x)
{
  return ENTU(unknownq(x, 1));
}


/************************************************************************
 *			BIND_UNKNOWN           				*
 ************************************************************************
 * bind_unknown(u,v,mode,time_bound) attempts to bind unknown u to	*
 * v.  mode determines which kind of binding to perform.		*
 *									*
 * mode = 0 : Unprotected.						*
 * mode = 1 : Protected.					    	*
 *								    	*
 * If the binding does not succeed, then bind_unknown sets failure  	*
 * to BIND_EX.							    	*
 ************************************************************************/

void bind_unknown(ENTITY u, ENTITY v, int mode, LONG *time_bound)
{
  ENTITY uu, vv, key;
  int tag;
  REG_TYPE mark = reg3(&uu, &vv, &key);

  /*------------------------------------------------------------*
   * If this is a protected unknown, then split u to get the	*
   * unknown to bind and the write-key. 			*
   *------------------------------------------------------------*/

  uu = u;
  vv = v;
  if(mode) {
    IN_PLACE_EVAL_FAILTO(uu, time_bound, out);
    ast_split(uu, &uu, &key);
    key = remove_indirection(key);
  }

  /*-------------------------------------------------------------------*
   * Scan through bindings of uu, and check that result is an unknown. *
   * We must evaluate uu here, up to unknowns. 			       *
   *-------------------------------------------------------------------*/

  IN_PLACE_EVAL_TO_ANY_UNKNOWN_FAILTO(uu, time_bound, out);

  uu = gen_scan_through_unknowns(uu);
  if(TAG(uu) != LAZY_PRIM_TAG) goto fail;

  tag = toint(NRVAL(ENTVAL(uu)[0]));
  if(tag < UNKNOWN_TMO) goto fail;

  /*---------------------------------------------------------------------*
   * Check the write-key, if there is one.  If there is none, then check *
   * that the unknown is unprotected. 					 *
   *---------------------------------------------------------------------*/

  if(mode) {
    if(tag == UNKNOWN_TMO) goto fail;
    if(ENT_NE(key, ENTVAL(uu)[2])) goto fail;
  }
  else {
    if(tag == PROTECTED_UNKNOWN_TMO) goto fail;
  }
  if(failure >= 0) goto out;

  /*--------------------------------------------------------------------*
   * Check if vv is the same as uu.  If not, bind uu to vv. Need	*
   * to eval vv to an unknown, since otherwise we don't know		*
   * whether uu is the same as vv.  Bind uu to vv, not vvv, in		*
   * case there are public unknowns bound to private unknowns. 		*
   *--------------------------------------------------------------------*/

  IN_PLACE_EVAL_TO_ANY_UNKNOWN_FAILTO(vv, time_bound, out);

  {ENTITY vvv = gen_scan_through_unknowns(vv);
   reg1_param(&vvv);
   if(ENT_NE(uu, vvv)) simple_assign_bxpl(ENTVAL(uu)[1], vv);
   goto out;
  }

fail:
  failure = BIND_EX;

out:
  unreg(mark);
}


/****************************************************************
 *			SAME_UNKNOWN_STDF			*
 ****************************************************************
 * Return true if x and y are the same unknown, where		*
 * e = (x,y).							*
 ****************************************************************/

ENTITY same_unknown_stdf(ENTITY e)
{
  int tx;
  ENTITY *px, x, y, xx, yy;
  ENTITY result = false_ent;

  ast_split(e, &x, &y);
  xx = gen_scan_through_unknowns(x);
  yy = gen_scan_through_unknowns(y);
  if(TAG(xx) != LAZY_PRIM_TAG || TAG(yy) != LAZY_PRIM_TAG
    || ENT_NE(xx, yy)) goto out;
  px = ENTVAL(xx);
  tx = toint(NRVAL(px[0]));
  if(tx < UNKNOWN_TMO) goto out;
  result = true_ent;

out:
  return result;
}

