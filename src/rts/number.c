/*************************************************************************
 * File:    rts/number.c
 * Purpose: Implement general operations on numbers
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
 * This file provides top level support for numeric operations.  The	*
 * functions define here are typically called by the evaluator		*
 * (evaluate.c).  They are responsible for deciding what kind of number	*
 * is being operated on, and taking appropriate action.			*
 ************************************************************************/

#include <math.h>
#include <stdlib.h>
#include <memory.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../standard/stdtypes.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../tables/tables.h"
#include "../unify/unify.h"
#include "../classes/classes.h"
#ifdef DEBUG
# include "../debug/debug.h"
# include "../show/prtent.h"
#endif


/****************************************************************
 *			PRED_STDF				*
 ****************************************************************
 * Return x-1, but fail with exception domainX if x = 0.	*
 ****************************************************************/

ENTITY pred_stdf(ENTITY x)
{
  if(ast_sign(x) == 0) {
    failure = DOMAIN_EX;
    failure_as_entity = qwrap(DOMAIN_EX, make_str("predecessor of smallest"));
  }
  return ast_subtract(x, one);
}


/****************************************************************
 *			UP_CONVERT_TY				*
 ****************************************************************
 * Parameters *a and *b must be numbers of some primary species *
 * in genus REAL.  ta is the type of a, and tb is the type of   *
 * b.  Convert *a and *b to a common species by converting in   *
 * the direction Natural -> Integer -> Rational -> Real.  Store *
 * the results of the conversion back into *a and *b, and store *
 * the type of the new *a and *b into *tr.			*
 *								*
 * Note: types ta and tb must be in the type table, so that     *
 * they can be compared for equality by comparing pointers.	*
 ****************************************************************/

PRIVATE void 
up_convert_ty(ENTITY *a, ENTITY *b, TYPE *ta, TYPE *tb, TYPE** tr)
{
  ENTITY aval  = remove_indirection(*a);
  ENTITY bval  = remove_indirection(*b);
  int    a_tag = TAG(aval);
  int    b_tag = TAG(bval);

  /*----------------------------------------------------*
   * Perform the up conversion.  Note that the tags of	*
   * the standard numeric types are ordered so that the	*
   * more general types have higher numbered tags. 	*
   *----------------------------------------------------*/

  if(a_tag != b_tag) {
    if(a_tag < b_tag) {
      *a = ast_as(aval, bval, NULL);
      a_tag = b_tag;
    }
    else *b = ast_as(bval, aval, NULL);        
  }

  /*-----------------------------------------------*
   * Now get the type of the result.  We have made *
   * sure that a_tag is the tag of the result.     *
   *						   *
   * Note: if a_tag is an integer tag, then the    *
   * result type is either Natural or Integer.     *
   * If either *a or *b originally had type	   *
   * Integer, then the result type is Integer.     *
   * If both had type Natural, then the result     *
   * type is Natural.				   *
   *-----------------------------------------------*/
  
  if     (a_tag == RATIONAL_TAG)   *tr = rational_type;
  else if(a_tag >= SMALL_REAL_TAG) *tr = real_type;
  else if(ta == tb)                *tr = ta;
  else                             *tr = integer_type;
}


/****************************************************************
 *			UP_CONVERT				*
 ****************************************************************
 * It is presumed that *a has tag WRAP_TAG.  It is required     *
 * that *b also have tag WRAP_TAG, but that is checked for.     *
 *								*
 * Unwrap *a and *b, and convert the results to the same type   *
 * by converting upwards (in the direction Natural -> Integer   *
 * -> Rational -> Real).					*
 *								*
 * Store the results back into *a and *b. 			*
 *								*
 * Set *ty to the type of the result numbers.			*
 ****************************************************************/

PRIVATE void up_convert(ENTITY *a, ENTITY *b, TYPE **ty)
{
  ENTITY* ap = ENTVAL(*a);
  ENTITY* bp = ENTVAL(*b);

  if(TAG(*b) != WRAP_TAG) die(112);

  *a = ap[1];
  *b = bp[1];
  up_convert_ty(a, b, TYPEVAL(ap[0]), TYPEVAL(bp[0]), ty);
}


/****************************************************************
 *			COMPAT_STDF				*
 ****************************************************************
 * Return compatify(a,b), where a and b are numbers each of a   *
 * primary species in genus REAL.  That is, return a pair of	*
 * numbers (a',b') so that a = a' and b = b' (in value), but a' *
 * and b' have the same species.  Conversion is upwards, in the	*
 * direction Natural -> Integer -> Rational -> Real.		*
 *								*
 * Parameter t is species (A,B), where A is the species of a 	*
 * and B is the species of b.					*
 *								*
 * Additionally, push (T,T) onto the type stack, where T is the *
 * species of a' and b'.					*
 ****************************************************************/

ENTITY compat_stdf(ENTITY pr, TYPE* t)
{
  TYPE *ty;
  ENTITY a, b;

  ast_split(pr, &a, &b);
  IN_PLACE_FIND_U_NONNULL(t);
  up_convert_ty(&a, &b, t->TY1, t->TY2, &ty);
  push_a_type(pair_t(ty, ty));
  return ast_pair(a, b);
}


/****************************************************************
 *			AST_ADD					*
 ****************************************************************
 * Return a + b.  a and b can be any kind of numbers, provided  *
 * they are of the same kind.					*
 ****************************************************************/

ENTITY ast_add(ENTITY a, ENTITY b)
{
  /*------------------------------------------------------------*
   * wrap_type is set to a type to wrap with the result if the  *
   * result should be wrapped.	It is NULL if no wrap should be *
   * done at the end.						*
   *------------------------------------------------------------*/

  int a_tag, b_tag;
  ENTITY result;
  TYPE* wrap_type = NULL;   

tail_recur:
  b_tag = TAG(b);
  a_tag = TAG(a);

  switch(a_tag) {
    case RATIONAL_TAG:
      result = add_rat(a,b);
      break;

    case INT_TAG:
      if(b_tag == INT_TAG) {
	result = ast_make_int(IVAL(a) + IVAL(b));
	break;
      }
      /* No break: continue with BIGPOSINT_TAG. */

    case BIGPOSINT_TAG:
    case BIGNEGINT_TAG:
      {BIGINT_BUFFER aa, bb, cc;
      intcell a_space[INTCELLS_IN_LONG], b_space[INTCELLS_IN_LONG];

       const_int_to_array(a, &aa, &a_tag, a_space);
       const_int_to_array(b, &bb, &b_tag, b_space);

       /*---------------------------------------------------------------*
	* If a and b have the same sign, then just add their absolute   *
	* values, and attach appropriate sign.				*
	*---------------------------------------------------------------*/

       if(a_tag == b_tag) {
	 if(aa.val.size >= bb.val.size) {
	   get_bigint_buffer(aa.val.size + 1, &cc);
	   add_array(&(aa.val), &(bb.val), &(cc.val));
	 }
	 else {
	   get_bigint_buffer(bb.val.size + 1, &cc);
	   add_array(&(bb.val), &(aa.val), &(cc.val));
	 }
	 result = array_to_int(a_tag, &cc);
       }

       /*---------------------------------------------------------------*
	* If a and b have opposite signs, then subtract the smaller one *
	* from the larger one, and attach the appropriate sign.		*
	*---------------------------------------------------------------*/

       else {
	 if(compare_array(&(aa.val), &(bb.val)) >= 0) {
	   get_bigint_buffer(aa.val.size, &cc);
	   subtract_array(&(aa.val), &(bb.val), &(cc.val));
	   result = array_to_int(a_tag, &cc);
	 }
	 else {
	   get_bigint_buffer(bb.val.size, &cc);
	   subtract_array(&(bb.val), &(aa.val), &(cc.val));
	   result = array_to_int(b_tag, &cc);
	 }
       }
       break;
      }

    case SMALL_REAL_TAG:
      if(b_tag == SMALL_REAL_TAG && DIGITS_PRECISION == 0) {
	SMALL_REAL* a_sr = SRVAL(a);
	SMALL_REAL* b_sr = SRVAL(b);
	result = ast_make_real(a_sr->val + b_sr->val);
	break;
      }
      /* No break: continue with LARGE_REAL_TAG. */

    case LARGE_REAL_TAG:
      result = op_real(a, b, ADD);
      break;

    case WRAP_TAG:
      up_convert(&a, &b, &wrap_type);
      a_tag = TAG(a);
      b_tag = TAG(b);
      goto tail_recur;

    default:
      die(112);
      return zero;
  }

  /*----------------------------------------------------*
   * If we unwrapped the parameters, then we need to	*
   * wrap the result.					*
   *----------------------------------------------------*/

  if(wrap_type != NULL) return wrap(result, wrap_type);
  else return result;
}


/****************************************************************
 *			AST_SUBTRACT				*
 ****************************************************************
 * Return a - b.  a and b can be any kind of number, provided   *
 * they are the same kind.					*
 ****************************************************************/

ENTITY ast_subtract(ENTITY a, ENTITY b)
{
  int a_tag, b_tag;
  ENTITY result;
  TYPE* wrap_type = NULL;

tail_recur:
  b_tag = TAG(b);
  a_tag = TAG(a);

  switch(a_tag) {
    case RATIONAL_TAG:
      result = subtract_rat(a,b);
      break;

    case INT_TAG:
      if(b_tag == INT_TAG) {
	result = ast_make_int(IVAL(a) - IVAL(b));
	break;
      }
      /* No break: continue with BIGPOSINT_TAG. */

    case BIGPOSINT_TAG:
    case BIGNEGINT_TAG:
      {BIGINT_BUFFER aa, bb, cc;
      intcell a_space[INTCELLS_IN_LONG], b_space[INTCELLS_IN_LONG];

       const_int_to_array(a, &aa, &a_tag, a_space);
       const_int_to_array(b, &bb, &b_tag, b_space);

       /*---------------------------------------------------------*
	* If a and b have opposite signs, then add their absolute *
	* values, and attach an appropriate sign.		  *
	*---------------------------------------------------------*/

       if(a_tag != b_tag) {
	 if(aa.val.size >= bb.val.size) {
	   get_bigint_buffer(aa.val.size + 1, &cc);
	   add_array(&(aa.val), &(bb.val), &(cc.val));
	 }
	 else {
	   get_bigint_buffer(bb.val.size + 1, &cc);
	   add_array(&(bb.val), &(aa.val), &(cc.val));
	 }
	 result = array_to_int(a_tag, &cc);
       }

       /*-------------------------------------------------------------*
	* If a and b have the same sign, then subtract their absolute *
	* values, and attach an appropriate sign.		      *
	*-------------------------------------------------------------*/

       else {
	 if(compare_array(&(aa.val), &(bb.val)) >= 0) {
	   get_bigint_buffer(aa.val.size, &cc);
	   subtract_array(&(aa.val), &(bb.val), &(cc.val));
	   result = array_to_int(a_tag, &cc);
	 }
	 else {
	   get_bigint_buffer(bb.val.size, &cc);
	   subtract_array(&(bb.val), &(aa.val), &(cc.val));
	   result = array_to_int((BIGPOSINT_TAG + BIGNEGINT_TAG) - a_tag,&cc);
	 }
       }
       break;
      }

    case SMALL_REAL_TAG:
      if(b_tag == SMALL_REAL_TAG && DIGITS_PRECISION == 0) {
	SMALL_REAL* a_sr = SRVAL(a);
	SMALL_REAL* b_sr = SRVAL(b);
	result = ast_make_real(a_sr->val - b_sr->val);
	break;
      }
      /* No break: continue with LARGE_REAL_TAG. */

    case LARGE_REAL_TAG:
      result = op_real(a, b, SUBTRACT);
      break;

    case WRAP_TAG:
      up_convert(&a, &b, &wrap_type);
      a_tag = TAG(a);
      b_tag = TAG(b);
      goto tail_recur;

    default:
      die(113);
      return zero;
  }

  /*----------------------------------------------------*
   * If we unwrapped the parameters, then we need to	*
   * wrap the result.					*
   *----------------------------------------------------*/

  if(wrap_type != NULL) return wrap(result, wrap_type);
  else return result;
}


/****************************************************************
 *			AST_ABSDIFF				*
 ****************************************************************
 * Return |a-b|.  a and b can be any kind of number, provided   *
 * they have the same kind.					*
 ****************************************************************/

ENTITY ast_absdiff(ENTITY a, ENTITY b)
{
  return ast_abs(ast_subtract(a,b));
}


/****************************************************************
 *			AST_NEGATE				*
 ****************************************************************
 * Return -a.  a can be any kind of number.			*
 ****************************************************************/

ENTITY ast_negate(ENTITY a)
{
  ENTITY result;
  TYPE* wrap_type = NULL;

tail_recur:
  switch(TAG(a)) {
    case INT_TAG:
      result = ENTI(-IVAL(a));
      break;

    case BIGPOSINT_TAG:
      result = ENTP(BIGNEGINT_TAG, BIVAL(a));
      break;

    case BIGNEGINT_TAG:
      result = ENTP(BIGPOSINT_TAG, BIVAL(a));
      break;

    case RATIONAL_TAG:
      {ENTITY* ap = ENTVAL(a);
       result = make_rat(ast_negate(ap[0]), ap[1]);
       break;
      }

    case SMALL_REAL_TAG:
      {SMALL_REAL* a_sr = SRVAL(a);
       result = ast_make_real(-a_sr->val);
       break;
      }

    case LARGE_REAL_TAG:
      {LARGE_REAL* apr = LRVAL(a);
       LARGE_REAL* rp  = allocate_large_real();

       rp->man  = ast_negate(apr->man);
       rp->ex   = apr->ex;
       result = ENTP(LARGE_REAL_TAG, rp);
       break;
      }

    case WRAP_TAG:
      {ENTITY* ap = ENTVAL(a);
       a          = ap[1];
       wrap_type  = TYPEVAL(ap[0]);
       goto tail_recur;
      }

    default:
      die(114);
      return zero;
  }

  /*----------------------------------------------------*
   * If we unwrapped the parameter, then we need to	*
   * wrap the result.					*
   *----------------------------------------------------*/

  if(wrap_type != NULL) return wrap(result, wrap_type);
  else return result;
}


/****************************************************************
 *			AST_ABS					*
 ****************************************************************
 * Return |a|.  a can be any kind of number.			*
 ****************************************************************/

ENTITY ast_abs(ENTITY a)
{
  ENTITY result;
  TYPE* wrap_type = NULL;

tail_recur:
  switch(TAG(a)) {

    case INT_TAG:
      result = ENTI(labs(IVAL(a)));
      break;

    case BIGNEGINT_TAG:
      result = ENTP(BIGPOSINT_TAG, BIVAL(a));
      break;

    case BIGPOSINT_TAG:
      result = a;
      break;

    case RATIONAL_TAG:
      {ENTITY* ap = ENTVAL(a);

       if(ast_sign(ap[0]) >= 0) result = a;
       else result = make_rat(ast_negate(ap[0]), ap[1]);
       break;
      }

    case SMALL_REAL_TAG:
      {SMALL_REAL* a_sr = SRVAL(a);
       result = ast_make_real(fabs(a_sr->val));
       break;
      }

    case LARGE_REAL_TAG:
      {LARGE_REAL* apr = LRVAL(a);
       if(ast_sign(apr->man) >= 0) result = a;
       else {
	 LARGE_REAL* rp = allocate_large_real();
	 rp->man = ast_negate(apr->man);
	 rp->ex  = apr->ex;
	 result = ENTP(LARGE_REAL_TAG, rp);
       }
       break;
      }

    case WRAP_TAG:
      {ENTITY* ap = ENTVAL(a);
       a          = ap[1];
       wrap_type  = TYPEVAL(ap[0]);
       goto tail_recur;
      }

    default:
      die(115);
      return zero;
  }

  /*----------------------------------------------------*
   * If we unwrapped the parameter, then we need to	*
   * wrap the result.					*
   *----------------------------------------------------*/

  if(wrap_type != NULL) return wrap(result, wrap_type);
  else return result;
}


/****************************************************************
 *			AST_ODD					*
 ****************************************************************
 * Return true if a is odd, false if a is even.			*
 ****************************************************************/

ENTITY ast_odd(ENTITY a)
{
  int a_tag = TAG(a);
  if(a_tag == WRAP_TAG) {
    a     = ENTVAL(a)[1];
    a_tag = TAG(a);
  }
  if(!MEMBER(a_tag, int_tags)) die(116);
  if(a_tag == INT_TAG) return ENTU(((int)IVAL(a)) & 1);
  else return ENTU(BIGINT_BUFF(BIVAL(a))[0] & 1);
}


/****************************************************************
 *			AST_MULT				*
 ****************************************************************
 * Return a*b.  a and b can be any kind of number, provided	*
 * they are the same kind.					*
 ****************************************************************/

ENTITY ast_mult(ENTITY a, ENTITY b)
{
  ENTITY result;
  TYPE* wrap_type = NULL;
  int   a_tag     = TAG(a);
  int   b_tag     = TAG(b);

tail_recur:
  switch(a_tag) {
    case RATIONAL_TAG:
      result = mult_rat(a,b);
      break;

    case INT_TAG:
      if(b_tag == INT_TAG) {
	LONG a_ival = IVAL(a);
	LONG b_ival = IVAL(b);
	if(labs(a_ival) < SHRT_MAX && labs(b_ival) < SHRT_MAX) {
	  result = ast_make_int(a_ival * b_ival);
	  break;
	}
      }
      /* No break: continue with BIGPOSINT_TAG */

    case BIGNEGINT_TAG:
    case BIGPOSINT_TAG:
      {BIGINT_BUFFER aa, bb, cc;
       intcell a_space[INTCELLS_IN_LONG], b_space[INTCELLS_IN_LONG];

       const_int_to_array(a, &aa, &a_tag, a_space);
       const_int_to_array(b, &bb, &b_tag, b_space);
       get_bigint_buffer(aa.val.size + bb.val.size + 1, &cc);
       mult_array(&(aa.val), &(bb.val), cc.val.buff);
       a_tag = (a_tag == b_tag) ? BIGPOSINT_TAG : BIGNEGINT_TAG;
       result = array_to_int(a_tag, &cc);
       break;
      }

    case SMALL_REAL_TAG:
      if(b_tag == SMALL_REAL_TAG && DIGITS_PRECISION == 0) {
	SMALL_REAL* a_sr = SRVAL(a);
	SMALL_REAL* b_sr = SRVAL(b);
	result = ast_make_real(a_sr->val * b_sr->val);
	break;
      }
      /* No break: continue with LARGE_REAL_TAG. */

    case LARGE_REAL_TAG:
      result = op_real(a,b,MULTIPLY);
      break;

    case WRAP_TAG:
      up_convert(&a, &b, &wrap_type);
      a_tag = TAG(a);
      b_tag = TAG(b);
      goto tail_recur;

    default:
	die(117);
	return zero;
  }

  /*----------------------------------------------------*
   * If we unwrapped the parameters, then we need to	*
   * wrap the result.					*
   *----------------------------------------------------*/

  if(wrap_type != NULL) return wrap(result, wrap_type);
  else return result;
}


/****************************************************************
 *			AST_DIVIDE				*
 ****************************************************************
 * Return a/b.  a and b must either be rational or real.	*
 ****************************************************************/

ENTITY ast_divide(ENTITY a, ENTITY b)
{
  ENTITY result;
  TYPE* wrap_type = NULL;
  int   a_tag     = TAG(a);
  int   b_tag     = TAG(b);

tail_recur:
  switch(a_tag) {
    case RATIONAL_TAG:
      result = divide_rat(a,b);
      break;

    case SMALL_REAL_TAG:
      if(b_tag == SMALL_REAL_TAG && DIGITS_PRECISION == 0) {
	SMALL_REAL* a_sr = SRVAL(a);
	SMALL_REAL* b_sr = SRVAL(b);
	if(b_sr->val == 0.0) {
	  failure = DOMAIN_EX;
	  failure_as_entity = divide_by_zero_ex;
	  result = zero_real;
	}
	else result = ast_make_real(a_sr->val / b_sr->val);
	break;
      }
      /* No break: continue with LARGE_REAL_TAG */

    case LARGE_REAL_TAG:
      result = op_real(a,b,DIVIDE);
      break;

    case WRAP_TAG:
      up_convert(&a, &b, &wrap_type);
      a_tag = TAG(a);
      b_tag = TAG(b);
      goto tail_recur;

    default:
      die(118);
      return zero;
  }

  /*----------------------------------------------------*
   * If we unwrapped the parameters, then we need to	*
   * wrap the result.					*
   *----------------------------------------------------*/

  if(wrap_type != NULL) return wrap(result, wrap_type);
  else return result;
}


/****************************************************************
 *			AST_RECIPROCAL				*
 ****************************************************************
 * Return 1/a.  a must be either rational or real.		*
 ****************************************************************/

ENTITY ast_reciprocal(ENTITY a)
{
  return ast_divide(ast_as(one, a, NULL), a);
}


/****************************************************************
 *			AST_DIVIDE_INT				*
 ****************************************************************
 * Set *q and *r to the quotient and remainder when a is divided*
 * by b.  a and b must be integers, and b must be positive.	*
 * (There is no test for b < 0 --- that is assumed false.       *
 * There is a test for b = 0.)					*
 ****************************************************************/

void ast_divide_int(ENTITY a, ENTITY b, ENTITY *q, ENTITY *r)
{
  int a_tag, b_tag;
  TYPE* wrap_type = NULL;

  /*--------------------------*
   * Check for division by 0. *
   *--------------------------*/

  if(ENT_EQ(b, zero)) {
    *r = *q = zero;
    failure           = DOMAIN_EX;
    failure_as_entity = divide_by_zero_ex;
    return;
  }

  /*-----------------------------------------------------------*
   * Get a and b, and their tags, unwrapping a if necessary.   *
   * Note that b must have type Natural, so cannot be wrapped. *
   *-----------------------------------------------------------*/

  a_tag = TAG(a);
  if(a_tag == WRAP_TAG) {
    ENTITY* ap = ENTVAL(a);
    a          = ap[1];
    a_tag      = TAG(a);
    wrap_type  = TYPEVAL(ap[0]);
  }
  b_tag = TAG(b);

  /*------------------------*
   * Handle small integers. *
   *------------------------*/

  if(a_tag == INT_TAG && b_tag == INT_TAG) {
    LONG a_val = IVAL(a);
    LONG b_val = IVAL(b);
    if(a_val >= 0) {
      *q = ENTI(a_val / b_val);
      *r = ENTI(a_val % b_val);
    }
    else {
      LONG r1 = (-a_val) % b_val;
      LONG q1 = -((-a_val) / b_val);
      if(r1 != 0) {
	r1 = b_val - r1;
	q1 = q1 - 1;
      }
      *r = ENTI(r1);
      *q = ENTI(q1);
    }
  }

  /*------------------------*
   * Handle large integers. *
   *------------------------*/

  else {
    BIGINT_BUFFER qq;

    divide_large_int(a, b, &qq, r);

    /*--------------------------------------------------------------*
     * divide_large_int has divided the absolute values of a and b. *
     * Here, we correct for the sign.                               *
     *--------------------------------------------------------------*/

    if((a_tag == INT_TAG && IVAL(a) >= 0) || a_tag == BIGPOSINT_TAG) {
      *q = array_to_int(BIGPOSINT_TAG, &qq);
    }
    else {
      if(ENT_NE(*r, zero)) {
	*r = ast_subtract(b, *r);
	add_array(&(qq.val), &one_array, &(qq.val));
      }
      *q = array_to_int(BIGNEGINT_TAG, &qq);
    }
  }

  /*-------------------------------------------------------*
   * If we unwrapped a, then we need to wrap the quotient. *
   *-------------------------------------------------------*/

  if(wrap_type != NULL) *q = wrap(*q, wrap_type);
}


/****************************************************************
 *			AST_DIV					*
 ****************************************************************
 * Return a div b.  a and b must be integers, and b must be     *
 * positive.							*
 ****************************************************************/

ENTITY ast_div(ENTITY a, ENTITY b)
{
  ENTITY q,r;

  ast_divide_int(a,b,&q,&r);
  return q;
}


/****************************************************************
 *			AST_MOD					*
 ****************************************************************
 * Return a mod b.  a and b must be integers, and b must be     *
 * positive.							*
 ****************************************************************/

ENTITY ast_mod(ENTITY a, ENTITY b)
{
  ENTITY q,r;

  ast_divide_int(a,b,&q,&r);
  return r;
}


/****************************************************************
 *			AST_DIVIDE_BY_2				*
 ****************************************************************
 * Return a div 2.  a must be an integer, and cannot be wrapped.*
 ****************************************************************/

ENTITY ast_divide_by_2(ENTITY a)
{
  int a_tag;
  BIGINT_BUFFER aa, bb;
  ENTITY result;
  intcell a_space[INTCELLS_IN_LONG];

  a_tag = TAG(a);
  if(!MEMBER(a_tag, int_tags)) die(119);

  if(a_tag == INT_TAG) return ENTI(IVAL(a) >> 1);

  const_int_to_array(a, &aa, &a_tag, a_space);
  get_bigint_buffer(aa.val.size, &bb);
  memcpy(bb.val.buff, aa.val.buff, INTCELL_TO_BYTE(aa.val.size));
  right_shift(&(bb.val), 1);
  result = array_to_int(a_tag, &bb);
  return result;
}


/****************************************************************
 *			AST_GCD					*
 ****************************************************************
 * Return the greatest common divisor of a and b.  a and b must *
 * be integers, not both 0.					*
 ****************************************************************/

ENTITY ast_gcd(ENTITY a, ENTITY b)
{
  ENTITY result;
  TYPE* wrap_type = NULL;
  int   a_tag     = TAG(a);

  /*-------------------------------------*
   * Unwrap the parameters if necessary. *
   *-------------------------------------*/

  if(a_tag == WRAP_TAG) {
    up_convert(&a, &b, &wrap_type);
    a_tag = TAG(a);
  }

  /*------------------------*
   * Handle small integers. *
   *------------------------*/

  if(a_tag == INT_TAG && TAG(b) == INT_TAG) {
    result = ENTI(gcd(labs(IVAL(a)), labs(IVAL(b))));
  }

  /*------------------------------------------------------------*
   * Note that a and b cannot both be 0 here, since otherwise 	*
   * both of them would be small.				*
   *------------------------------------------------------------*/

  else if(ENT_EQ(a, zero)) result = b;
  else if(ENT_EQ(b, zero)) result = a;

  else result = big_gcd(ast_abs(a), ast_abs(b));

  /*----------------------------------------------------*
   * If we unwrapped the parameters, then we need to	*
   * wrap the result.					*
   *----------------------------------------------------*/

  if(wrap_type != NULL) return wrap(result, wrap_type);
  else return result;
}


/****************************************************************
 *			AST_ZEROP				*
 ****************************************************************
 * Return true if x = 0.					*
 ****************************************************************/

ENTITY ast_zerop(ENTITY x)
{
  if(ast_sign(x) == 0) return true_ent;
  else return false_ent;
}


/****************************************************************
 *			AST_SIGN				*
 ****************************************************************
 * Return 							*
 *     0 if a = 0						*
 *     1 if a > 0						*
 *    -1 if a < 0						*
 ****************************************************************/

int ast_sign(ENTITY a)
{

tail_recur:
  switch(TAG(a)) {
    case INT_TAG:
     {register LONG val = IVAL(a);
      if(val == 0) return 0;
      if(val < 0) return -1;
      return 1;
     }

    case BIGPOSINT_TAG:
      return 1;

    case BIGNEGINT_TAG:
      return -1;

    case RATIONAL_TAG:
      return ast_sign(ENTVAL(a)[0]);

    case SMALL_REAL_TAG:
      {SMALL_REAL* a_sr = SRVAL(a);
       if(a_sr->val < 0) return -1;
       if(a_sr->val > 0) return 1;
       return 0;
      }

    case LARGE_REAL_TAG:
      return ast_sign(LRVAL(a)->man);

    case WRAP_TAG:
      a = ENTVAL(a)[1];
      goto tail_recur;

    default:
      die(120);
      return 0;
  }
}


/****************************************************************
 *			AST_SIGN_AS_ENT				*
 ****************************************************************
 * Sign function, but return an entity.				*
 ****************************************************************/

ENTITY ast_sign_as_ent(ENTITY a)
{
  return ENTI(ast_sign(a));
}


/****************************************************************
 *			AST_POWER				*
 ****************************************************************
 * ast_power(x,p) computes x^p, where p is an integer (possibly	*
 * negative).							*
 *								*
 * ast_power_pos(x,p) is the same, but requires p > 0.		*
 ****************************************************************/

ENTITY ast_power_pos(ENTITY x, ENTITY p)
{
  ENTITY s,ss;

  if(ENT_EQ(p, one)) return x;

  if(VAL(ast_odd(p)) != 0) {
    s  = ast_subtract(p, one);
    ss = ast_power_pos(x, s);
    free_if_diff(s, 2, p, ss);
    s  = ast_mult(x, ss);
    free_if_diff(ss, 3, x, s, p);
  }
  else {
    s  = ast_divide_by_2(p);
    ss = ast_power_pos(x, s);
    free_if_diff(s, 2, p, ss);
    s  = ast_mult(ss, ss);
    free_if_diff(ss, 3, x, s, p);
  }

  return s;
}

/*-----------------------------------------------------------------*/

ENTITY ast_power(ENTITY x, ENTITY p)
{
  ENTITY result;
  TYPE* wrap_type = NULL;

  /*---------------------------------*
   * Unwrap the power, if necessary. *
   *---------------------------------*/

  if(TAG(p) == WRAP_TAG) p = ENTVAL(p)[1];

  /*----------------------------------------------*
   * Unwrap the base, if necessary, keeping track *
   * of its type if it is wrapped.		  *
   *----------------------------------------------*/

  if(TAG(x) == WRAP_TAG) {
    ENTITY* xp = ENTVAL(x);
    x         = xp[1];
    wrap_type = TYPEVAL(xp[0]);
  }

  /*------------------------------------*
   * Handle p = 0.  Watch out for 0^0.  *
   *------------------------------------*/

  if(ast_sign(p) == 0) {
    if(ast_sign(x) == 0) {
      failure = DOMAIN_EX;
      failure_as_entity = qwrap(DOMAIN_EX, make_str("0^0"));
    }
    result = ast_as(one, x, natural_type);
  }

  /*---------------*
   * Handle p > 0. *
   *---------------*/

  else if(ast_sign(p) >= 0) result = ast_power_pos(x, p);

  /*---------------*
   * Handle p < 0. *
   *---------------*/

  else {
    ENTITY recip_result = ast_power_pos(x, ast_negate(p));
    result = ast_reciprocal(recip_result);
    free_if_diff(recip_result, 3, x, p, result);
  }

  /*----------------------------------------------------*
   * If we unwrapped the base, then we need to		*
   * wrap the result.					*
   *----------------------------------------------------*/

  if(wrap_type != NULL) return wrap(result, wrap_type);
  else return result;
}

