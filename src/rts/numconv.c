/*************************************************************************
 * File:    rts/numconv.c
 * Purpose: Conversion operations for numbers.
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
 * This file provides functions for converting from one numeric species *
 * to another.								*
 ************************************************************************/

#include <math.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../standard/stdtypes.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../parser/tokens.h"
#include "../rts/rts.h"
#include "../clstbl/classtbl.h"
#include "../gc/gc.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			AST_AS					*
 ****************************************************************
 * Return number a, cast to the same type as x.  ty is the	*
 * desired type of the result.  It is only consulted when the	*
 * desired type is Integer or Natural or a wrapped type.	*
 ****************************************************************/

ENTITY ast_as(ENTITY a, ENTITY x, TYPE *ty)
{
  int a_tag = TAG(a);

  /*-----------------------------*
   * If a is wrapped, unwrap it. *
   *-----------------------------*/

  if(TAG(a) == WRAP_TAG) a = ENTVAL(a)[1];

  switch(TAG(x)) {

    /*-----------------------------------*
     * Conversion to Integer or Natural. *
     *-----------------------------------*/

    case INT_TAG:
    case BIGPOSINT_TAG:
    case BIGNEGINT_TAG:
      switch(a_tag) {
	case INT_TAG:
	case BIGPOSINT_TAG:
	case BIGNEGINT_TAG:
	  if(ty == natural_type && ast_sign(a) < 0) failure = CONVERSION_EX;
          return a;

        case RATIONAL_TAG:
	  return rat_to_int(a);

	case SMALL_REAL_TAG:
	case LARGE_REAL_TAG:
           return rat_to_int(real_to_rat(a));

      }
      break;

    /*---------------------*
     * Conversion to Real. *
     *---------------------*/

    case SMALL_REAL_TAG:
    case LARGE_REAL_TAG:
      return ast_real(a);

    /*-------------------------*
     * Conversion to Rational. *
     *-------------------------*/

    case RATIONAL_TAG:
      return ast_rational(a);

    /*-------------------------------*
     * Conversion to a wrapped type. *
     *-------------------------------*/

    case WRAP_TAG:
      switch(a_tag) {
        case INT_TAG:
	case BIGPOSINT_TAG:
	case BIGNEGINT_TAG:
	  if(ast_sign(a) >= 0 && 
	     (ty == WrappedINTEGER_type ||
	      ty == WrappedRATIONAL_type ||
	      ty == WrappedREAL_type)) return wrap(a, natural_type);
	  else return wrap(a, integer_type);

	case SMALL_REAL_TAG:
	case LARGE_REAL_TAG:
	  if(ty == WrappedINTEGER_type ||
	     ty == WrappedRATRING_type ||
	     ty == WrappedRATIONAL_type) failure = CONVERSION_EX;
	  return wrap(a, real_type);

        case RATIONAL_TAG:
	  if(ty == WrappedINTEGER_type) failure = CONVERSION_EX;
	  return wrap(a, rational_type);
      }        
      break;

    default:
      die(84);
      return zero;
  }

  die(84);
  return zero;
}
      

/****************************************************************
 *			REAL_TO_RAT				*
 ****************************************************************
 * Convert from real to rational.  This is a down-conversion,   *
 * and gives the current level of precision.			*
 ****************************************************************/

ENTITY real_to_rat(ENTITY y)
{
  LARGE_REAL *al;
  ENTITY a, a_man, a_ex;
  LONG a_ex_ival;
  BIGINT_BUFFER aa;
  int a_tag;

  /*----------------------------*
   * Force the parameter large. *
   *----------------------------*/

  a = y;
  if(TAG(a) == SMALL_REAL_TAG)  a = force_large_real(a,zero);
  else if(TAG(a) != LARGE_REAL_TAG) die(86);

  al    = LRVAL(a);
  a_man = al->man;
  a_ex  = al->ex;

  /*-----------------------------------------------------------*
   * Can't convert a really large or small number to rational. *
   *-----------------------------------------------------------*/

  if(TAG(a_ex) != INT_TAG) {
    failure = CONVERSION_EX;
    return make_rat(zero, one);
  }

  /*-------------------------------------------------------*
   * Case where the exponent is 0.  Just get the mantissa. *
   *-------------------------------------------------------*/

  a_ex_ival = IVAL(a_ex);
  if(a_ex_ival == 0) {
    return ast_make_rat(a_man, one);
  }

  /*-------------------------------------------------------*
   * Case where the exponent is negative.  Shift the 	   *
   * mantissa down.					   *
   *-------------------------------------------------------*/

  if(a_ex_ival < 0) {
    int_to_array(one, &aa, &a_tag, -a_ex_ival, 0);
    return ast_make_rat(a_man, ENTP(a_tag, &aa));
  }

  /*-------------------------------------------------------*
   * Case where the exponent is positive.  Shift the 	   *
   * mantissa up.					   *
   *-------------------------------------------------------*/

  else {
    int_to_array(a_man, &aa, &a_tag, a_ex_ival, 0);
    return make_rat(ENTP(a_tag, &aa), one);
  }
}


/****************************************************************
 *			RAT_TO_REAL				*
 ****************************************************************
 * Conversion from Rational to Real.				*
 ****************************************************************/

ENTITY rat_to_real(ENTITY a)
{ 
  ENTITY *ap, t1, t2;

  ap = ENTVAL(a);
  t1 = int_to_real(ap[0]);
  t2 = int_to_real(ap[1]); 
  return ast_divide(t1, t2);
}


/****************************************************************
 *			RAT_TO_INT				*
 ****************************************************************
 * Conversion from Rational to Integer.  This fails on a 	*
 * fraction.							*
 ****************************************************************/

ENTITY rat_to_int(ENTITY a)
{
  ENTITY* ap = ENTVAL(a);

  if(!ENT_EQ(ap[1], one)) failure = CONVERSION_EX;
  return ap[0];
}


/****************************************************************
 *			INT_TO_RAT				*
 ****************************************************************
 * Conversion from Integer to Rational.				*
 ****************************************************************/

ENTITY int_to_rat(ENTITY a)
{
 if(TAG(a) == WRAP_TAG) a = ENTVAL(a)[1];
 return make_rat(a, one);
}


/****************************************************************
 *			INT_TO_LARGE_REAL			*
 ****************************************************************
 * Conversion from Integer to Real.  The result is in the large *
 * form, not as a double.					*
 ****************************************************************/

ENTITY int_to_large_real(ENTITY a)
{
  LARGE_REAL* r = allocate_large_real();
  r->man = a;
  r->ex  = zero;
  return ENTP(LARGE_REAL_TAG, r);
}


/****************************************************************
 *			INT_TO_REAL				*
 ****************************************************************
 * Conversion from Integer to Real.				*
 ****************************************************************/

ENTITY int_to_real(ENTITY a)
{
  LONG prec;

  /*-----------------*
   * Small integers. *
   *-----------------*/

  if(TAG(a) == INT_TAG) {
    return ast_make_real((double) IVAL(a));
  }

  /*-----------------*
   * Large integers. *
   *-----------------*/

  if(!MEMBER(TAG(a), int_tags)) die(87);
  prec = DIGITS_PRECISION;

  return round_real(int_to_large_real(a), prec);
}


/****************************************************************
 *			AST_FLOOR				*
 ****************************************************************
 * Take the floor of a rational or real.			*
 ****************************************************************/

ENTITY ast_floor(ENTITY a)
{
  int a_tag = TAG(a);

  /*-----------------------------------------*
   * If the parameter is wrapped, unwrap it. *
   *-----------------------------------------*/

  if(a_tag == WRAP_TAG) {
    a     = ENTVAL(a)[1];
    a_tag = TAG(a);
  }

  /*-----------------------------------*
   * Taking the floor of a small real. *
   *-----------------------------------*/

  if(a_tag == SMALL_REAL_TAG) {
    SMALL_REAL* a_sr  = SRVAL(a);
    double      a_val = a_sr->val;
    if(fabs(a_val) <= LONG_MAX_AS_DOUBLE) {
      return ast_make_int(floor(a_val));
    }
    a = force_large_real(a,zero);  /* Handle as large real, below. */
  }

  /*---------------------------------*
   * Taking the floor of a rational. *
   *---------------------------------*/

  else if(a_tag == RATIONAL_TAG) {
    ENTITY* d = ENTVAL(a);
    return ast_div(d[0], d[1]);
  }

  /*-----------------------------------*
   * Taking the floor of a large real. *
   *-----------------------------------*/

  else if(a_tag != LARGE_REAL_TAG) die(88);

  return floor_large_real(a);
}
      

/****************************************************************
 *			AST_CEILING				*
 ****************************************************************
 * Compute the ceiling or rational or real number a.		*
 ****************************************************************/

ENTITY ast_ceiling(ENTITY a)
{
  return ast_negate(ast_floor(ast_negate(a)));
}


/****************************************************************
 *			AST_NATURAL				*
 ****************************************************************
 * Convert a from INTEGER`a to Natural.				*
 ****************************************************************/

ENTITY ast_natural(ENTITY a)
{
  if(TAG(a) == WRAP_TAG) a = ENTVAL(a)[1];

  if(ast_sign(a) < 0) failure = CONVERSION_EX;

  return a;
}


/****************************************************************
 *			AST_INTEGER				*
 ****************************************************************
 * Convert a from INTEGER`a to Integer.				*
 ****************************************************************/

ENTITY ast_integer(ENTITY a)
{
  if(TAG(a) == WRAP_TAG) a = ENTVAL(a)[1];
  return a;
}


/****************************************************************
 *			AST_REAL				*
 ****************************************************************
 * Convert a to Real.  If a is already real, round it to the	*
 * current precision.						*
 ****************************************************************/

ENTITY ast_real(ENTITY a)
{

tail_recur:
  switch(TAG(a)) {
    case INT_TAG:
      return ast_make_real((double) IVAL(a));

    case BIGPOSINT_TAG:
    case BIGNEGINT_TAG:
      return int_to_real(a);

    case RATIONAL_TAG:
      {ENTITY t1, t2, *ap;

       ap = ENTVAL(a);
       t1 = int_to_real(ap[0]);
       t2 = int_to_real(ap[1]);
       return ast_divide(t1, t2);
     }

    case SMALL_REAL_TAG: 
      return a;

    case LARGE_REAL_TAG: 
      return round_real(a, DIGITS_PRECISION);

    case WRAP_TAG:
      a = ENTVAL(a)[1];
      goto tail_recur;

    default: 
      die(89);
      return zero;
  }
}


/****************************************************************
 *			AST_RATIONAL				*
 ****************************************************************
 * Convert a to Rational.					*
 ****************************************************************/

ENTITY ast_rational(ENTITY a)
{
tail_recur:
  switch(TAG(a)) {
    case INT_TAG:
    case BIGPOSINT_TAG:
    case BIGNEGINT_TAG:
      return make_rat(a, one);

    case SMALL_REAL_TAG:
    case LARGE_REAL_TAG:
      return real_to_rat(a);

    case RATIONAL_TAG:
      return a;

    case WRAP_TAG:
      a = ENTVAL(a)[1];
      goto tail_recur;

    default: 
      die(90);
      return zero;
  }
}


/****************************************************************
 *			WRAP_NUMBER				*
 ****************************************************************
 * Wrap x with a type that is acceptable both for x and for     *
 * type t->TY2.	 This should only be used when t->TY2 is        *
 * G for a standard numeric genus G.				*
 *								*
 * The type wrapped with the result is the first in the 	*
 * sequence (Natural, Integer, Rational, Real) that is		*
 * consistent with both the representation of x and with 	*
 * species t->TY2.  Prefer t->TY1 when it is primary and it     *
 * works.  If t->TY1 is secondary, prefer to keep the type      *
 * stored in x.							*
 ****************************************************************/

ENTITY wrap_number(ENTITY x, TYPE *t)
{
  int x_tag;
  TYPE *wrap_type, *preferred_wrap_type;
  TYPE*             result_type = t->TY2;
  CLASS_TABLE_CELL* result_ctc  = result_type->ctc;

  /*----------------------------*
   * Unwrap x if it is wrapped. *
   *----------------------------*/

  preferred_wrap_type = t->TY1;
  x_tag = TAG(x);
  if(x_tag == WRAP_TAG) {
    ENTITY* p = ENTVAL(x);
    preferred_wrap_type = TYPEVAL(p[0]);
    x     = p[1];
    x_tag = TAG(x);
  }

  /*----------------------------------------*
   * Get the initial type to wrap with.     *
   *----------------------------------------*/

  wrap_type = result_ctc->CTC_DEFAULT;
  if(wrap_type->ctc == natural_type->ctc && 
     preferred_wrap_type->ctc == integer_type->ctc) {
    wrap_type = integer_type;
  }

  /*-----------------------------------------------------*
   * Now adjust the wrap type upwards if necessary to    *
   * accomodate x, and adjust the representation of x if *
   * necessary to accomodate the wrap type.		 *
   *-----------------------------------------------------*/

  switch(x_tag) {
    case NOREF_TAG:
    case BIGPOSINT_TAG:
    case BIGNEGINT_TAG:
      if(wrap_type == natural_type && ast_sign(x) < 0) {
	if(ancestor_tm(result_ctc, integer_type->ctc)) {
	  wrap_type = integer_type;
        }
        else goto fail;  /* at end */
      }
      else if(wrap_type == rational_type) x = make_rat(x, one);
      else if(wrap_type == real_type) x = int_to_real(x);
      break;

    case RATIONAL_TAG:
      if(wrap_type == natural_type || wrap_type == integer_type) {
	if(ancestor_tm(result_ctc, rational_type->ctc)) {
	  wrap_type = rational_type;
        }
        else goto fail;  /* at end */
      }
      else if(wrap_type == real_type) {
	x = rat_to_real(x);
      }
      break;

    case SMALL_REAL_TAG:
    case LARGE_REAL_TAG:
      if(wrap_type == natural_type || wrap_type == integer_type
	 || wrap_type == rational_type) {
	if(ancestor_tm(result_ctc, real_type->ctc)) {
	  wrap_type = real_type;
        }
        else goto fail;  /* at end */
      }

  }
        
  return wrap(x, wrap_type);

 fail:
  failure = CONVERSION_EX;
  return zero;
}



