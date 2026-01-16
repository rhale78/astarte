/************************************************************
 * File:    rts/compare.c
 * Purpose: Implement comparison operations
 * Author:  Karl Abrahamson
 ************************************************************/

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

#include <string.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../rts/rts.h"
#include "../classes/classes.h"
#include "../gc/gc.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/********************************************************
 *			AST_COMPARE_SIMPLE		*
 ********************************************************
 * Compare numbers or simple values.  Return one of the *
 * following.                                           *
 *                                                      *
 *	   0	if a == b, or if evaluation of a or b	*
 *		fails.                                  *
 *	   1	if a > b                                *
 *	   2	if a < b                                *
 *							*
 * This function will not get back to the definition 	*
 * of compare made by any program, as it does no 	*
 * dispatching, so it can only work on primitive values.*
 * It is used for numbers and ranked types.		*
 ********************************************************/

int ast_compare_simple(ENTITY a, ENTITY b)
{
  int c, b_tag, a_tag;

  if(ENT_EQ(a,b)) return 0;

  b_tag = TAG(b);
  a_tag = TAG(a);

  switch (a_tag) {

    /*-------------------------------------------------------------*
     * If both are INT_TAG values, just compare their IVAL values. *
     * Note that if a and b both have tag INT_TAG and a == b, we   *
     * will have returned 0 above.				   *
     *-------------------------------------------------------------*/

    case INT_TAG:
	if(b_tag == INT_TAG) {
	  if(IVAL(a) < IVAL(b)) {return 2;}
	  return 1;
	}
	/*------------------------------------*
	 * No break: continue with next case. *
	 *------------------------------------*/

    case SMALL_REAL_TAG:
    case LARGE_REAL_TAG:
    case RATIONAL_TAG:
    case BIGPOSINT_TAG:
    case BIGNEGINT_TAG:
    case WRAP_TAG:
        c = ast_sign((ast_sign(b) == 0) ? a : ast_subtract(a,b));
	if(c == 0) return 0;
	if(c > 0) return 1;
	return 2;

    default:
	die(124, a_tag);
	return 0;

  }
}


/********************************************************
 *			AST_COMPARE			*
 ********************************************************
 * Do a compare_simple on entities a and b.  This	*
 * version returns an ENTITY rather than an int.	*
 ********************************************************/

ENTITY ast_compare(ENTITY a, ENTITY b)
{
  return ENTU(ast_compare_simple(a,b));
}


/**********************************************************
 *	     AST_LT, AST_LE, AST_GT, AST_GE		  *
 **********************************************************
 * These functions perform less-than, less-than-or-equal, *
 * greater-than or greater-than-or-equal tests using	  *
 * compare_simple.					  *
 **********************************************************/

ENTITY ast_lt(ENTITY a, ENTITY b)
{
  return ENTU(ast_compare_simple(a,b) == 2);
}

/*------------------------------------------------------------*/

ENTITY ast_le(ENTITY a, ENTITY b)
{
  return ENTU(ast_compare_simple(a,b) != 1);
}

/*------------------------------------------------------------*/

ENTITY ast_gt(ENTITY a, ENTITY b)
{
  return ENTU(ast_compare_simple(a,b) == 1);
}

/*------------------------------------------------------------*/

ENTITY ast_ge(ENTITY a, ENTITY b)
{
  return ENTU(ast_compare_simple(a,b) != 2);
}

/*------------------------------------------------------------*/

ENTITY ast_ne(ENTITY a, ENTITY b)
{
  LONG l_time = LONG_MAX;
  return ENTU(1 - VAL(ast_equal(a, b, &l_time)));
}


/********************************************************
 *			AST_EQUAL			*
 ********************************************************
 * Return true just when a and b are equal.  a and b    *
 * should be simple values such as numbers, boxes or    *
 * qwrapped simple values; or lists or products of such *
 * values.  						*
 *							*
 * This function will not get back to the definition 	*
 * of compare made by any program, as it does no 	*
 * dispatching, so it can only work on primitive values.*
 ********************************************************/

ENTITY ast_equal(ENTITY aa, ENTITY bb, LONG *time_bound)
{
  int b_tag, a_tag;
  ENTITY result;
  ENTITY a = aa;
  ENTITY b = bb;
  REG_TYPE mark = reg2_param(&a, &b);

  /*---------------------------------*
   * The loop is for tail recursion. *
   *---------------------------------*/

  for(;;) {

    /*------------------------------------------------------*
     * Make at least the left part of a and b accessible.   *
     * If that times out, make a lazy primitive to continue *
     * the computation.					    *
     *------------------------------------------------------*/

    IN_PLACE_EVAL_FAILTO(a, time_bound, fail1);
    IN_PLACE_EVAL_FAILTO(b, time_bound, fail1);
    goto got_ab;  /* Just below. */

  fail1:
    if(failure == TIME_OUT_EX) {
      result =  make_lazy_prim(EQUAL_TMO, a, b);
    }
    else result = false_ent;
    goto out;

  got_ab:

    /*------------------------------------------------------*
     * If a and b have identical representations (including *
     * identical pointers for lists or pairs) then they are *
     * surely equal.					    *
     *------------------------------------------------------*/

    if(ENT_EQ(a,b)) {
      result =  true_ent; 
      goto out;
    }

    b_tag = TAG(b);
    a_tag = TAG(a);
    result = false_ent;	/* default result. */

    switch (a_tag) {

      /*------------------------------------------------------*
       * Boxes and small integers are only    		      *
       * equal if they have identical representations.        *
       * Since the identical representation test above failed,*
       * a and b must not be equal.        		      *
       *------------------------------------------------------*/

      case BOX_TAG:
      case PLACE_TAG:
      case INT_TAG:
	goto out;

      /*--------------------------------------------------*
       * A WRAP_TAG can indicate a wrapped box or number. *
       * For a number, just see if a - b = 0.  For a box, *
       * unwrap a and b, and repeat the test.		  *
       *--------------------------------------------------*/

      case WRAP_TAG:
	{ENTITY* ap = ENTVAL(a);
	 TYPE*   t  = (TAG(ap[0]) == TYPE_TAG) ? TYPEVAL(ap[0]) : any_box;
	 if(TKIND(t) == FAM_MEM_T) { /* box */
	   a = ap[1];
	   if(TAG(b) == WRAP_TAG) b = ENTVAL(b)[1];
	   break;  /* tail recur */
	 }
	 else {
	   result = ENTU(ast_sign(ast_subtract(a,b)) == 0);
	   goto out;
	 }
	}

      /*---------------------------*
       * For reals, just subtract. *
       *---------------------------*/

      case SMALL_REAL_TAG:
      case LARGE_REAL_TAG:
	result = ENTU(ast_sign(ast_subtract(a,b)) == 0);
	goto out;

      case BIGNEGINT_TAG:
      case BIGPOSINT_TAG:

        /*--------------------------------------------------*
         * If b has a different tag from a, then it cannot  *
	 * possibly be the same as  a.  Otherwise, just     *
         * check whether the difference is 0.		    *
         *--------------------------------------------------*/

	if(b_tag == a_tag) {
	  result = ENTU(ast_sign(ast_subtract(a,b)) == 0);
        }
	goto out;

      /*--------------------------------------------------------*
       * Two CSTR_TAG strings are equal only if their 	 	*
       * pointers are the same, since they point to the same	*
       * entry in the string table.  If we have one CSTR_TAG	*
       * and one STRING_TAG, handle as under STRING_TAG. If we  *
       * have a more general combination, handle as a general   *
       * list. 							*
       *--------------------------------------------------------*/

      case CSTR_TAG:
	if(b_tag == CSTR_TAG) goto out;
        else if(b_tag == STRING_TAG) {
	  ENTITY tmp = a; a = b; b = tmp;
	  a_tag = STRING_TAG;
	  b_tag = CSTR_TAG;
	  goto string_tag;  /* Just below, case STRING_TAG */
        }
        else goto general_list; /* Below case PAIR_TAG */

      case STRING_TAG:
      string_tag:

        /*-------------------------------------*
         * A string.  Compare the byte arrays  *
         * if both have tag STRING_TAG, or if  *
         * b is a CSTR_TAG.		       *
         *-------------------------------------*/

	if(b_tag == STRING_TAG || b_tag == CSTR_TAG) {
	  CHUNKPTR a_chunk;
	  int a_len, b_len;
	  charptr a_buff, b_buff;

	  a_chunk = BIVAL(a);
	  a_len   = STRING_SIZE(a_chunk);
	  a_buff  = STRING_BUFF(a_chunk);
          if(b_tag == STRING_TAG) {
	    CHUNKPTR b_chunk = BIVAL(b);
	    b_len   = STRING_SIZE(b_chunk);
	    b_buff  = STRING_BUFF(b_chunk);
          }
          else {
            b_buff = CSTRVAL(b);
            b_len  = strlen(b_buff);
	  }

	  if(a_len != b_len) {result = false_ent; goto out;}

	  result = ENTU(strncmp(a_buff, b_buff, a_len) == 0);
	  goto out;
	}

	/*------------------------------------*
	 * No break; continue with next case, *
	 * which handles general lists.	      *
	 *------------------------------------*/

      /*--------------------------------------------------------*
       * Lists and pairs are handled by comparing the heads and *
       * tails.  If a is a pair and b is nil (tag NOREF_TAG)    *
       * then a and b are not equal.				*
       *--------------------------------------------------------*/

      /*case QUINT_TAG:*/
      case QUAD_TAG:
      case TRIPLE_TAG:
      case PAIR_TAG:    /* Also RATIONAL_TAG */
      case APPEND_TAG:
      case ARRAY_TAG:
      case TREE_TAG:
      general_list:
	if(b_tag == NOREF_TAG) goto out;
	TIME_STEP(time_bound);

	/*--------------------*
	 * Compare the heads. *
	 *--------------------*/

	if(VAL(ast_equal(ast_head(a), ast_head(b), time_bound)) == 0) {
	  goto out;
	}

	/*----------------------------------*
	 * Tail-recur to compare the tails. *
	 *----------------------------------*/

	a = ast_tail(a);
	b = ast_tail(b);
	break;

      default:
	die(125);

    } /* end switch */

  } /* end for */

 out:
  unreg(mark);
  return result;
}
