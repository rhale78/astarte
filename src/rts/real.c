/****************************************************************
 * File:    rts/real.c
 * Purpose: Operations on reals
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
 * This file contains assorted operations on real numbers.		*
 ************************************************************************/

#include <math.h>
#include <stdlib.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../rts/rts.h"
#include "../tables/tables.h"
#include "../gc/gc.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			ln_base_val				*
 ****************************************************************
 * ln_base_value is an approximation to ln(INT_BASE) that is 	*
 * good to normal precision.					*
 ****************************************************************/

ENTITY ln_base_val;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			AST_MAX_DBL				*
 *			AST_MIN_DBL				*
 *			AST_EXPIN_MAX				*
 *			MAX_DBL_EXP				*
 ****************************************************************
 * AST_MAX_DBL is the largest 'double' value that can be	*
 * squared without leading to an overflow.		        *
 *								*
 * AST_MIN_DBL is the 1/AST_MAX_DBL.				*
 *								*
 * AST_EXPIN_MAX is ln(AST_MAX_DBL)				*
 *								*
 * MAX_DBL_EXP is log_base(AST_MAX_DBL), where log_base is the  *
 *             logarithm to base INT_BASE.  (See entity.c for   *
 *             INT_BASE.)					*
 ****************************************************************/

PRIVATE DOUBLE AST_MAX_DBL;
PRIVATE DOUBLE AST_MIN_DBL;
PRIVATE DOUBLE AST_EXPIN_MAX;
PRIVATE int    MAX_DBL_EXP;

/****************************************************************
 *			DIGIT_RSHIFT				*
 *			DIGIT_LSHIFT				*
 ****************************************************************
 * DIGIT_RSHIFT is 2^{-INTCELL_BITS}.				*
 *								*
 * DIGIT_LSHIFT is 2^{INTCELL_BITS}.				*
 ****************************************************************/

PRIVATE DOUBLE DIGIT_RSHIFT;
PRIVATE DOUBLE DIGIT_LSHIFT;

/*===============================================================
  			CONVERSIONS
  ===============================================================*/

/****************************************************************
 *			AST_MAKE_REAL				*
 ****************************************************************
 * Make a small real from a double.				*
 ****************************************************************/

ENTITY ast_make_real(DOUBLE x)
{
  SMALL_REAL *r;
  LARGE_REAL *lr;
  DOUBLE absx;
  ENTITY e;

# ifdef USE_ISNAN
    if(isnan(x)) die(91);
#   ifdef NEVER
      if(isinf(x)) die(92);
#   endif
# endif

  /*-------------------------------------------------------------------*
   * If x is sufficiently small, then use a small real representation. *
   *-------------------------------------------------------------------*/

  absx = fabs(x);
  if(absx == 0.0 || (absx < AST_MAX_DBL && absx > AST_MIN_DBL)) {
    r = allocate_small_real();
    r->val = x;
    return ENTP(SMALL_REAL_TAG, r);
  }

  /*---------------------------------------------------------------*
   * If x is very large, then we need a large real representation. *
   *---------------------------------------------------------------*/

  else {
    lr      = allocate_large_real();
    lr->man = lr->ex = zero;
    e       = ENTP(LARGE_REAL_TAG, lr);
    double_to_floating(x, &(lr->man), &(lr->ex));
    return e;
  }
}


/****************************************************************
 *			DOUBLE_TO_FLOATING			*
 ****************************************************************
 * Set m and ex so that m * INT_BASE^ex = x.			*
 ****************************************************************/

void double_to_floating(DOUBLE x, ENTITY *m, ENTITY *ex)
{
  BIGINT_BUFFER man_buff;
  int j, tag;
  LONG e, f;
  DOUBLE y;

# ifdef USE_ISNAN
    if(isnan(x)) die(91);
#   ifdef NEVER
      if(isinf(x)) die(92);
#   endif
# endif

  /*-------------------------*
   * Special case for x = 0. *
   *-------------------------*/

  if(x == 0.0) {
    *m = *ex = zero;
    return;
  }

  /*------------------------------------------------------------*
   * In general, shift x so that it is just less than 1.  The	*
   * first loop shifts left until is it at least 1.  The second	*
   * loop then shifts right until it is less than 1.		*
   *------------------------------------------------------------*/

  e = 0;
  y = fabs(x);

  while(y < 1.0) {
    y *= DIGIT_LSHIFT;
    e--;
  }

  while(y >= 1.0) {
    y *= DIGIT_RSHIFT;
    e++;
  }

  /*------------------------------------------------------------*
   * Now accumulate the digits in an array, and convert to	*
   * a large real.						*
   *------------------------------------------------------------*/

  get_bigint_buffer((DBL_DIG + 1) >> 2, &man_buff);

  for(j = man_buff.val.size - 1; j >= 0; j--) {
    y                    *= DIGIT_LSHIFT;
    f                     = floor(y);
    man_buff.val.buff[j]  = (intcell) f;
    y                    -= (DOUBLE) f;
  }

  tag = (x >= 0) ? BIGPOSINT_TAG : BIGNEGINT_TAG;
  *m  = array_to_int(tag, &man_buff);
  *ex = ast_make_int(e - man_buff.val.size);
}


/****************************************************************
 *			FORCE_LARGE_REAL			*
 ****************************************************************
 * Return a large equivalent of a*INT_BASE^expon.		*
 ****************************************************************/

ENTITY force_large_real(ENTITY a, ENTITY expon)
{
  LARGE_REAL *lr, *alr;
  SMALL_REAL *sr;
  ENTITY x;
  int a_tag;

  /*--------------------------------------------------------------*
   * If a is already a large real, and we are not asked to modify *
   * its exponent, then just return a.				  *
   *--------------------------------------------------------------*/

  a_tag = TAG(a);
  if(a_tag == LARGE_REAL_TAG && ENT_EQ(expon, zero)) {
    return a;
  }

  lr      = allocate_large_real();
  lr->man = lr->ex = zero;
  x       = ENTP(LARGE_REAL_TAG, lr);

  /*------------------------------------------------------------*
   * If a is already a large real, then we must just modify the *
   * exponent here.						*
   *------------------------------------------------------------*/

  if(a_tag == LARGE_REAL_TAG) {
    alr     = LRVAL(a);
    lr->man = alr->man;
    lr->ex  = ast_add(alr->ex, expon);
  }

  /*---------------------------------------------------------------*
   * If a is a small real, then we must convert to large real.  The *
   * number 10.0 is a common special case, so we look it up.	   *
   *---------------------------------------------------------------*/

  else {
    sr = SRVAL(a);
    if(sr->val == 10.0) {
      *lr = *(LARGE_REAL*) ENTVAL(large_ten_real);
    }
    else {
      double_to_floating(sr->val, &(lr->man), &(lr->ex));
    }
    if(ENT_NE(expon, zero)) lr->ex = ast_add(lr->ex, expon);
  }

  return x;
}


/**********************************************************************
 *			GET_APPROX				      *
 **********************************************************************
 * Set x to a double approximation of a, and set e to the 	      *
 * appropriate exponent so that a = x*INT_BASE^e.  Make e as close    *
 * to 0 as possible, unless it is very large anyway.		      *
 **********************************************************************/

PRIVATE void get_approx(ENTITY a, DOUBLE *x, ENTITY *e)
{
  LARGE_REAL *lr;
  DOUBLE v, newv;
  LONG ee, i, j, dig;
  int a_tag;
  BIGINT_BUFFER aa;
  ENTITY ex;
  intcell a_space[INTCELLS_IN_LONG];

  /*-------------------------------------------------------------*
   * If a is already a small real, just extract the double value *
   * from it.							 *
   *-------------------------------------------------------------*/

  if(TAG(a) == SMALL_REAL_TAG) {
    *x = SRVAL(a)->val;
    *e = zero;
    return;
  }

  /*--------------------------------------------*
   * Otherwise, we need to do an approximation. *
   *--------------------------------------------*/
  
  lr = LRVAL(a);
  ex = lr->ex;
  const_int_to_array(lr->man, &aa, &a_tag, a_space);
  v  = 0.0;
  ee = aa.val.size;
  dig = (DBL_DIG + 1) >> 2;
  for(j = aa.val.size - 1, i = 0;
      j >= 0 && i < dig;
      j--, i++) {
    v = v * DIGIT_LSHIFT + aa.val.buff[j];
    ee--;
  }
  while(v > AST_MAX_DBL) {
    v *= DIGIT_RSHIFT; 
    ee++;
  }

  /*--------------------------------------------------------------*
   * We have viable values here, but want to make the exponent ex *
   * as close to 0 as possible.					  *
   *--------------------------------------------------------------*/

  ex = ast_add(ENTI(ee), ex);
  if(TAG(ex) == INT_TAG) {
    ee = IVAL(ex);
    while(ee > 0 && (newv = v * DIGIT_LSHIFT) < AST_MAX_DBL) {
      v = newv;
      ee--;
    }
    while(ee < 0 && (newv = v * DIGIT_RSHIFT) > AST_MIN_DBL) {
      v = newv;
      ee++;
    }
    ex = ast_make_int(ee);
  }

  *e = ex;
  *x = (a_tag == BIGPOSINT_TAG) ? v : -v;

}


/****************************************************************
 *			ROUND_REAL				*
 ****************************************************************
 * Return a rounded to prec digits of precision.  		*
 ****************************************************************/

ENTITY round_real(ENTITY a, LONG prec)
{
  LARGE_REAL *lr, *new_lr;
  SMALL_REAL *sr;
  DOUBLE v;
  LONG l, exi;
  int a_tag;
  BIGINT_BUFFER aa;
  ENTITY man, new_man, ex;

  a_tag = TAG(a);

  /*-----------------------------------------------------------*
   * If a is a small real, then we can't round it any smaller. *
   *-----------------------------------------------------------*/

  if(a_tag == SMALL_REAL_TAG) return a;

  /*---------------------------------------------*
   * If a is not a real at all, we have trouble. *
   *---------------------------------------------*/

  if(a_tag != LARGE_REAL_TAG) die(93);

  lr = LRVAL(a);
  ex = lr->ex;

  /*----------------------------------------------------------------*
   * If prec is small, then we are rounding to SMALL_REAL status.   *
   * We can only build a small real if the exponent is not too	    *
   * large.	   						    *
   *----------------------------------------------------------------*/

  if(prec <= DECIMAL_TO_DIGIT(DBL_DIG) && TAG(ex) == INT_TAG) {
    exi = labs(IVAL(ex));
    if(exi > MAX_DBL_EXP) goto do_large;
    get_approx(a, &v, &ex);
    if(ENT_EQ(ex, zero)) {
      sr      = allocate_small_real();
      sr->val = v;
      return ENTP(SMALL_REAL_TAG, sr);
    }
  }

  /*---------------------------------------------------*
   * Here, prec is large or the exponent is too large. *
   *---------------------------------------------------*/

 do_large:

  man = lr->man;

  /*------------------------------------------------------------*
   * If the mantissa is a small integer, then we already have a *
   * rounded to as small a value as we can get.                 *
   *------------------------------------------------------------*/

  if(TAG(man) == INT_TAG) return a;

  /*------------------------------------------------------------*
   * Similarly, if the mantissa has at most prec digits, then a *
   * is small enough.  Don't try to round below 4 digits.	*
   *------------------------------------------------------------*/

  l = BIGINT_SIZE(BIVAL(man));
  if(l <= prec || l <= 4) return a;

  /*-----------------------------------------------------*
   * Here, we must cut some precision from the mantissa. *
   *-----------------------------------------------------*/

  int_to_array(man, &aa, &a_tag, 0, 0);
  new_man = normalize_array_to_int(&(aa), a_tag, prec, &exi);
  new_lr      = allocate_large_real();
  new_lr->man = new_man;
  new_lr->ex  = ast_add(lr->ex, ENTI(exi));
  return ENTP(LARGE_REAL_TAG, new_lr);
}


/****************************************************************
 *			PULL_APART_REAL				*
 ****************************************************************
 * Return pair (m,e), where x = m*base^e is the representation  *
 * of real number x.  m and e are integers.			*
 ****************************************************************/

ENTITY pull_apart_real(ENTITY x)
{
  ENTITY* p;
  if(TAG(x) == SMALL_REAL_TAG) {
    p = allocate_entity(2);
    double_to_floating(SRVAL(x)->val, p, p+1);
  }
  else p = ENTVAL(x);

  return ENTP(PAIR_TAG, p);
}


/*===============================================================
 			ARITHMETIC
  ===============================================================*/

/****************************************************************
 *			ADDSUB_FLOATING				*
 ****************************************************************
 * Let 								*
 *     r = result_man * INT_BASE^result_ex, 			*
 *     a = a_man * INT_BASE^a_ex				*
 *     b = b_man * INT_BASE^b_ex				*
 *								*
 * Set result_man and result_ex as follows.			*
 *								*
 *        op          r						*
 *      ------     ------					*
 *        ADD         a + b					*
 *        SUBTRACT    a - b					*
 *								*
 * prec is the desired presion, in blocks, of the result.	*
 ****************************************************************/

PRIVATE void addsub_floating(ENTITY a_man, ENTITY a_ex,
			     ENTITY b_man, ENTITY b_ex,
			     ENTITY *result_man, ENTITY *result_ex,
			     LONG prec, int op)
{
  ENTITY exdif, rex;
  BIGINT_BUFFER aa, bb, cc;
  LONG d, ex, al, bl;
  int a_tag, b_tag;
  LONG k;
  intcell b_space[INTCELLS_IN_LONG];

  /*-------------------------------------------------*
   * Handle the case where one of the operands is 0. *
   *-------------------------------------------------*/

  if(ENT_EQ(b_man, zero)) {
returnA:
    *result_man = a_man;
    *result_ex  = a_ex;
    return;
  }

  if(ENT_EQ(a_man, zero)) {
returnB:
    *result_man = (op == ADD) ? b_man : ast_negate(b_man);
    *result_ex  = b_ex;
    return;
  }

  /*----------------------------*
   * Get the amount d to shift. *
   *----------------------------*/

  exdif  = ast_subtract(b_ex, a_ex);
  d      = get_ival(exdif, 0);

  /*------------------------------------------------------------*
   * get_ival will have failed if the shift is a large integer. *
   *								*
   * If the shift is very large, just return the larger value.  *
   * (The addition or subtraction does not make a difference at *
   * the chosen precision.					*
   *------------------------------------------------------------*/

  if(failure == 0) {
    failure = -1;
    if(ast_sign(exdif) > 0) goto returnB;  /* Above -- returns a. */
    else                    goto returnA;  /* Above -- returns b or -b. */
  }

  /*-----------------------------------------*
   * Get the sizes and precision, in digits. *
   *-----------------------------------------*/

  al = (TAG(a_man) == INT_TAG)
	  ? 2
	  : BIGINT_SIZE(BIVAL(a_man));
  bl = (TAG(b_man) == INT_TAG)
	  ? 2
	  : BIGINT_SIZE(BIVAL(b_man));
  k = prec + 2;

  /*---------------------------*
   * If b << a, then return a. *
   *---------------------------*/

  if(al - bl > k + d) goto returnA;  /* Above -- returns a. */

  /*---------------------------------*
   * if a << b, then return b or -b. *
   *---------------------------------*/

  if(bl - al > k - d) goto returnB;  /* Above -- returns b or -b. */


  /*----------------------------------------*
   * General case: get a and b into arrays. *
   *----------------------------------------*/

  if(d < 0) /* a has the larger exponent */ {

    /*------------------------------------------*
     * Get a and b, with a shifted to the left. *
     *------------------------------------------*/

    const_int_to_array(b_man, &bb, &b_tag, b_space);
    int_to_array(a_man, &aa, &a_tag, -d, 0);
    rex = b_ex;
  }

  else /* b has the larger exponent. */ {

    /*------------------------------------------*
     * Get a and b, with b shifted to the left. *
     *------------------------------------------*/

    const_int_to_array(a_man, &aa, &a_tag, b_space);
    int_to_array(b_man, &bb, &b_tag, d, 0);
    rex = a_ex;
  }

  /*--------------------------------------------------------------*
   * Compute the sum or difference. This involves shifting one of *
   * them according to exponents.				  *
   *--------------------------------------------------------------*/

  if(op == ADD && a_tag == b_tag || op == SUBTRACT && a_tag != b_tag) {
    if(aa.val.size >= bb.val.size) {
      get_bigint_buffer(aa.val.size + 1, &cc);
      add_array(&(aa.val), &(bb.val), &(cc.val));
    }
    else {
      get_bigint_buffer(bb.val.size + 1, &cc);
      add_array(&(bb.val), &(aa.val), &(cc.val));
    }
    *result_man = normalize_array_to_int(&cc, a_tag, prec, &ex);
  }
  else {
    if(compare_array(&(aa.val), &(bb.val)) >= 0) {
      get_bigint_buffer(aa.val.size, &cc);
      subtract_array(&(aa.val), &(bb.val), &(cc.val));
      *result_man = normalize_array_to_int(&cc, a_tag, prec, &ex);
    }
    else {
      get_bigint_buffer(bb.val.size, &cc);
      subtract_array(&(bb.val), &(aa.val), &(cc.val));
      *result_man = normalize_array_to_int
			(&cc, (BIGPOSINT_TAG + BIGNEGINT_TAG) - a_tag,
			 prec, &ex);
    }
  }

  *result_ex = (ENT_EQ(*result_man, zero)) ? zero : ast_add(rex, ENTI(ex));

  if(d < 0) {
    free_bigint_buffer(&aa);
  }
  else {
    free_bigint_buffer(&bb);
  }

}


/****************************************************************
 *			GET_ENTITY_PRECISION			*
 ****************************************************************
 * Return the approximate number of digits (base INT_BASE) of	*
 * precision in real number a.					*
 ****************************************************************/

PRIVATE LONG get_entity_precision(ENTITY a)
{
  if(TAG(a) == SMALL_REAL_TAG) return DECIMAL_TO_DIGIT(DBL_DIG);
  else {
    ENTITY a_man = LRVAL(a)->man;
    if(TAG(a_man) == INT_TAG) {
      if(labs(IVAL(a_man)) > USHORT_MAX) return 2;
      else return 1;
    }
    else return BIGINT_SIZE(BIVAL(a_man));
  }
}


/****************************************************************
 *			OP_REAL					*
 ****************************************************************
 * Return a result depending on the value of op.		*
 *								*
 *        op   		result					*
 *       ----  		-------					*
 *       ADD     	a + b 					*
 *       SUBTRACT     	a - b					*
 *       MULTIPLY     	a * b					*
 *       DIVIDE     	a / b					*
 ****************************************************************/

ENTITY op_real(ENTITY a, ENTITY b, int op)
{
  ENTITY as, bs, al, bl, a_man, b_man, result_as_entity;
  LONG prec;
  int a_tag, b_tag, tag;
  LONG exi;
  LARGE_REAL *ap, *bp, *result;

  if(failure >= 0) return zero_real;

  if(!MEMBER(TAG(a), real_tags) || !MEMBER(TAG(b), real_tags)) {
    die(94);
  }

  prec = DIGITS_PRECISION;
  as   = round_real(a, prec);
  bs   = round_real(b, prec);

  /*------------------------------------------------------------*
   * Case where precision is low.  This requests operations to  *
   * be done at double precision.			      	*
   *------------------------------------------------------------*/

  if(prec <= DECIMAL_TO_DIGIT(DBL_DIG)) {
    if(TAG(as) == SMALL_REAL_TAG) {
      if(TAG(bs) == SMALL_REAL_TAG) {
	DOUBLE d, ad, bd;

	ad = SRVAL(as)->val;
	bd = SRVAL(bs)->val;
	switch(op) {
	  case ADD:
	    d = (ad) + (bd);
	    break;

	  case SUBTRACT:
	    d = (ad) - (bd);
	    break;

	  case MULTIPLY:
	    d = (ad) * (bd);
	    break;

	  case DIVIDE:
	    if(bd == 0.0) {
	      failure = DOMAIN_EX;
	      failure_as_entity = divide_by_zero_ex;
	      return zero_real;
	    }
	    d = (ad) / (bd);
	    break;

	  default: {
	    die(95);
	    d = 0.0;
	  }
	}
	return ast_make_real(d);
      }
    }
  } /* end if(prec is small) */

  /*-------------------------------------------------------------*
   * High precision operations.  Note that we can get here with  *
   * a or b being a SMALL_REAL.  For uniformity below, we force  *
   * each to be represented in LARGE_REAL format.		 *
   *-------------------------------------------------------------*/

  al    = force_large_real(as, zero);
  bl    = force_large_real(bs, zero);
  ap    = LRVAL(al);
  bp    = LRVAL(bl);

  /*---------------------------------------------------------*
   * Build the result now.  The parts are filled in below.   *
   *---------------------------------------------------------*/

  result           = allocate_large_real();
  result->man      = result->ex = zero;
  result_as_entity = ENTP(LARGE_REAL_TAG, result);

  /*---------------------------*
   * Addition and subtraction. *
   *---------------------------*/

  if(op <= SUBTRACT) {
     addsub_floating(ap->man, ap->ex, bp->man, bp->ex,
		     &(result->man), &(result->ex), prec, op);
  }

  /*-----------------*
   * Multiplication. *
   *-----------------*/

  else if(op == MULTIPLY) {
    BIGINT_BUFFER aa, bb, cc;
    intcell a_space[INTCELLS_IN_LONG], b_space[INTCELLS_IN_LONG];

    const_int_to_array(ap->man, &aa, &a_tag, a_space);
    const_int_to_array(bp->man, &bb, &b_tag, b_space);
    get_bigint_buffer(aa.val.size + bb.val.size + 1, &cc);
    tag = (a_tag == b_tag) ? BIGPOSINT_TAG : BIGNEGINT_TAG;
    mult_array(&(aa.val), &(bb.val), cc.val.buff);
    result->man = normalize_array_to_int(&cc, tag, prec, &exi);
    result->ex = ast_add(ap->ex, bp->ex);
    result->ex = ast_add(ENTI(exi), result->ex);
  }

  /*-----------*
   * Division. *
   *-----------*/

  else {
    BIGINT_BUFFER qq;
    ENTITY r;
    LONG pad_digits;
    Boolean padded_a;

    /*----------------------*
     * Fail on divide by 0. *
     *----------------------*/

    if(ENT_EQ(bp->man, zero)) {
      failure           = DOMAIN_EX;
      failure_as_entity = divide_by_zero_ex;
      return zero_real;
    }

    a_man = ap->man;
    b_man = bp->man;

    /*----------------------------------------------------------------*
     * Get the tags.  Only use large tags (not INT_TAG).  We will be  *
     * comparing tags, so we use BIGNEGINT_TAG to indicate a negative *
     * number and BIGPOSINT_TAG to indicate a positive number.        *
     *----------------------------------------------------------------*/

    a_tag = TAG(a_man);
    if(a_tag == INT_TAG) {
      a_tag = IVAL(a_man) < 0 ? BIGNEGINT_TAG : BIGPOSINT_TAG;
    }
    b_tag = TAG(b_man);
    if(b_tag == INT_TAG) {
      b_tag = IVAL(b_man) < 0 ? BIGNEGINT_TAG : BIGPOSINT_TAG;
    }

    /*------------------------------------------------------------------*
     * We need to pad a's mantissa on the right so that the remainder	*
     * in integer division is small, and can be ignored.		*
     *------------------------------------------------------------------*/

    pad_digits = prec + 2 + get_entity_precision(bl) 
                 - get_entity_precision(al);
    if(pad_digits > 0) {
      BIGINT_BUFFER a_man_buff;
      int_to_array(a_man, &a_man_buff, &a_tag, pad_digits, 0);
      a_man = array_to_int(a_tag, &a_man_buff);
      padded_a = TRUE;
    }
    else {
      padded_a = FALSE;
      pad_digits = 0;
    }

    /*-------------------------------------------*
     * Now do the division and build the result. *
     *-------------------------------------------*/

    divide_large_int(a_man, b_man, &qq, &r);
    tag = (a_tag == b_tag) ? BIGPOSINT_TAG : BIGNEGINT_TAG;
    result->man = normalize_array_to_int(&qq, tag, prec, &exi);
    result->ex  = ast_subtract(ap->ex, bp->ex);
    result->ex  = ast_add(ENTI(exi - pad_digits), result->ex);
    free_if_diff(r, 3, a_man, b_man, result->man);
    if(padded_a) free_if_diff(a_man, 1, result->man);

  } /* end else(division) */
    
  return result_as_entity;
}


/****************************************************************
 *			FLOOR_LARGE_REAL			*
 ****************************************************************
 * Return the floor of a.  a must be a large real.		*
 ****************************************************************/

ENTITY floor_large_real(ENTITY a)
{
  int a_tag, s;
  LONG e;
  BIGINT_BUFFER aa;
  ENTITY man, ex;
  LARGE_REAL *ap;
  intcell a_space[INTCELLS_IN_LONG];

  ap  = LRVAL(a);
  man = ap->man;
  ex  = ap->ex;
  s   = ast_sign(man);

  /*--------------------------------------------------------------*
   * If the exponent is 0, then the number is already an integer, *
   * and the mantissa is that integer.				  *
   *--------------------------------------------------------------*/

  if(ENT_EQ(ex, zero)) return man;

  /*-----------------------------------------------*
   * If the mantissa is 0, then this number is 0.  *
   *-----------------------------------------------*/

  if(ENT_EQ(man, zero)) return zero;

  /*---------------------------------------------------------------*
   * If the exponent is a large negative integer, then this number *
   * is surely <1 in absolute value, regardless of how large the   *
   * matissa is.  (We can't store that many digits.)  So the floor *
   * is either 0 or -1.						   *
   *---------------------------------------------------------------*/

  if(TAG(ex) == BIGNEGINT_TAG) {
less_than_one:
    return (s < 0) ? ENTI(-1) : zero;
  }

  /*--------------------------------------------------------------*
   * If the exponent has large positive integer, then this number *
   * is too large to convert to an integer.			  *
   *--------------------------------------------------------------*/

  if(TAG(ex) != INT_TAG) {
    failure = CONVERSION_EX;
    return zero;
  }

  /*------------------------------------------------------------*
   * If the exponent is a "small" positive number, then convert *
   * using the shift capability of int_to_array.		*
   *------------------------------------------------------------*/

  e = get_ival(ex, 0);
  if(e > 0) {
    int_to_array(man, &aa, &a_tag, e, 0);
    return array_to_int(a_tag, &aa);
  }

  /*-------------------------------------------------------------*
   * If the exponent is a "small" negative integer, then get     *
   * the mantissa in an array.  If the exponent has sufficiently *
   * large absolute value, then this number is less than 1 in	 *
   * absolute value, so  the floor is 0 or -1.			 *
   *-------------------------------------------------------------*/

  const_int_to_array(man, &aa, &a_tag, a_space);
  if(-e >= aa.val.size) {
    goto less_than_one;  /* Above, under another case. */
  }

  /*--------------------------------------------------------------*
   * Here, the absolute value of e is small enough. Shift the     *
   * array.  For negative numbers x, floor(x) = -(floor(-x) + 1), *
   * so we add 1.						  *
   *--------------------------------------------------------------*/

  {register LONG i;
   LONG nn  = aa.val.size + e;
   intcellptr aa_buff = aa.val.buff;
   intcellptr rr_buff;
   BIGINT_BUFFER rr;

   get_bigint_buffer(nn + 1, &rr);
   rr_buff  = rr.val.buff;

   for(i = 0; i < nn; i++) rr_buff[i] = aa_buff[i-e];
   rr_buff[nn] = 0;
   if(s < 0) add_array(&(rr.val), &one_array, &(rr.val));
   return array_to_int(a_tag, &rr);
  }
}


/*===============================================================
			OTHER FUNCTIONS
 ================================================================*/

/****************************************************************
 *			SQRT_REAL				*
 ****************************************************************
 * Return the square root of a, to normal (double) precision.   *
 ****************************************************************/

ENTITY sqrt_real(ENTITY a)
{
  DOUBLE x, s0;
  ENTITY z, e;

  /*--------------------------*
   * Fail on a negative input *
   *--------------------------*/

  if(ast_sign(a) < 0) {
    failure = DOMAIN_EX;
    failure_as_entity = qwrap(DOMAIN_EX, 
			      make_str("real sqrt of a negative number"));
    return zero_real;
  }

  /*---------------------------------------------------------------*
   * If a is a small real, we can take its square root using sqrt. *
   *---------------------------------------------------------------*/

  if(TAG(a) == SMALL_REAL_TAG) {
    return ast_make_real(sqrt(SRVAL(a)->val));
  }

  /*--------------------------------------------*
   * If a is large, then get it as a double and	*
   * an exponent, and compute the square root 	*
   * in that form.				*
   *--------------------------------------------*/

  get_approx(a, &x, &e);
  if(VAL(ast_odd(e))) {
    if(ast_sign(e) == 1) x *= DIGIT_LSHIFT;
    else                 x *= DIGIT_RSHIFT;
  }
  e  = ast_divide_by_2(e);
  s0 = sqrt(x);

  /*---------------------------------*
   * Get the result as a large real. *
   *---------------------------------*/

  z = ast_make_real(s0);
  z = force_large_real(z,e);

  return z;
}


/****************************************************************
 *			EXP_REAL   				*
 ****************************************************************
 * Return exp(x), to normal precision, where x is real.  	*
 ****************************************************************/

ENTITY exp_real(ENTITY x)
{
  ENTITY e, r;
  DOUBLE x_val;
  int halving_count;

  /*--------------------------*
   * Check for low precision. *
   *--------------------------*/

  if(TAG(x) == SMALL_REAL_TAG) {
    x_val = SRVAL(x)->val;
    if(fabs(x_val) <= AST_EXPIN_MAX) {
      return ast_make_real(exp(x_val));
    }
  }

  /*-----------------------------------------------*
   * Force x to a small real.  If x is very large, *
   * then e^x will overflow.                       *
   *-----------------------------------------------*/

  get_approx(ast_abs(x), &x_val, &e);
  if(ENT_NE(e, zero)) {
    failure = LIMIT_EX;
    return zero;
  }

  /*------------------------------------------------------*
   * Scale x down to where we can compute e^x using the	  *
   * exp function.					  *
   *------------------------------------------------------*/

  halving_count = 0;
  while(x_val > AST_EXPIN_MAX) {
    x_val *= 0.5;
    halving_count++;
  }
  
  /*------------------------------------------*
   * Get the answer, and correct for halving. *
   *------------------------------------------*/

  r = ast_make_real(exp(x_val));
  while(halving_count != 0) {
    r = ast_mult(r, r);
    halving_count--;
  }

  /*----------------------------*
   * Correct for the sign of x. *
   *----------------------------*/

  if(ast_sign(x) < 0) {
    r = ast_reciprocal(r);
  }

  return r;
}  


/****************************************************************
 *			LN_REAL    				*
 ****************************************************************
 * Return ln(x), to normal (double) precision.			*
 ****************************************************************/

ENTITY ln_real(ENTITY x)
{
  DOUBLE x_val;
  ENTITY x_exp, r, s;

  /*-------------------------------------------------*
   * We can't take the log of a non-positive number. *
   *-------------------------------------------------*/

  if(ast_sign(x) <= 0) {
    failure = DOMAIN_EX;
    failure_as_entity = qwrap(DOMAIN_EX, 
			      make_str("ln of a nonpositive number"));
    return zero_real;
  }

  if(TAG(x) == SMALL_REAL_TAG) {
    return ast_make_real(log(SRVAL(x)->val));
  }
  
  get_approx(x, &x_val, &x_exp);
  r = ast_make_real(log(x_val));
  if(ENT_NE(x_exp, zero)) {
    s = int_to_real(x_exp);
    s = ast_mult(s, ln_base_val);
    r = ast_add(r, s);
  }

  return r;
}


/****************************************************************
 *			SIN_REAL   				*
 ****************************************************************
 * Return sin(x), to normal (double) precision.			*
 ****************************************************************/

ENTITY sin_real(ENTITY x)
{
  ENTITY x_exp;
  DOUBLE x_val;

  if(TAG(x) == SMALL_REAL_TAG) {
    return ast_make_real(sin(SRVAL(x)->val));
  }

  get_approx(x, &x_val, &x_exp);
  if(ENT_NE(x_exp, zero)) {
    failure = LIMIT_EX;
    return zero;
  }

  return ast_make_real(sin(x_val));
}  


/****************************************************************
 *			ATAN_REAL				*
 ****************************************************************
 * Return invtan(x), to normal (double) precision.		*
 ****************************************************************/

ENTITY atan_real(ENTITY x)
{
  DOUBLE x_val;
  ENTITY x_exp;

  if(TAG(x) == SMALL_REAL_TAG) {
    return ast_make_real(atan(SRVAL(x)->val));
  }

  get_approx(x, &x_val, &x_exp);
  if(ENT_EQ(x_exp, zero)) return ast_make_real(atan(x_val));
  else if(ast_sign(x) > 0) return ast_make_real(1.57079632679489661923);
  else return ast_make_real(-1.57079632679489661923);
}


/****************************************************************
 *			INIT_REAL				*
 ****************************************************************/

void init_real(void)
{
  int i;
  DOUBLE log_base = log((DOUBLE) INT_BASE);


  AST_MAX_DBL = sqrt(DBL_MAX) * 0.99;
  AST_MIN_DBL = 1.0/AST_MAX_DBL;
  AST_EXPIN_MAX = log(AST_MAX_DBL);
  MAX_DBL_EXP = AST_EXPIN_MAX/log_base;
  if(MAX_DBL_EXP < 1) die(97);

  DIGIT_RSHIFT = 1.0;
  for(i = 0; i < INTCELL_BITS; i++) {
    DIGIT_RSHIFT *= 0.5;
  }
  DIGIT_LSHIFT = 1.0/DIGIT_RSHIFT;

  ln_base_val = ast_make_real(log_base);  
}

