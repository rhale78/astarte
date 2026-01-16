/*************************************************************************
 * File:    rts/rational.c
 * Purpose: Operations on rationals
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
 * This file provides basic operations on rational numbers.		*
 ************************************************************************/

#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../evaluate/evaluate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			MAKE_RAT				*
 ****************************************************************
 * Return rational number a/b.  a and b must be integers.	*
 *								*
 * DON'T USE THIS DIRECTLY, UNLESS YOU ARE SURE WHAT YOU ARE	*
 * DOING.  USE AST_MAKE_RAT INSTEAD.  make_rat requires that	*
 * b > 0 and the a and b have no common factors.		*
 ****************************************************************/

ENTITY make_rat(ENTITY a, ENTITY b)
{
  if(ENT_EQ(b, zero)) {
    failure = DOMAIN_EX;
    failure_as_entity = divide_by_zero_ex;
    return zero_rat;
  }
  else {
    register ENTITY *p;

    p 	  = allocate_entity(2);
    p[0]  = a;
    p[1]  = b;
    return ENTP(RATIONAL_TAG, p);
  }
}



/****************************************************************
 *			AST_REDUCE_RAT				*
 ****************************************************************
 * Return the result of reducing rational number a, by dividing *
 * numerator and denominator by their greatest common divisor.	*
 ****************************************************************/

PRIVATE ENTITY ast_reduce_rat(ENTITY a)
{
  ENTITY g, num, denom, newnum, newdenom, *ap;

  if(failure >= 0) return zero_rat;

  if(TAG(a) != RATIONAL_TAG) die(101);

  ap    = ENTVAL(a);
  num   = ap[0];
  denom = ap[1];
  if(ENT_EQ(num, zero)) return zero_rat;

  g = ast_gcd(num, denom);
  if(ENT_EQ(g, one)) return a;

  newnum = ast_div(num, g);
  newdenom = ast_div(denom, g);
  return make_rat(newnum, newdenom);
}


/****************************************************************
 *			AST_MAKE_RAT				*
 ****************************************************************
 * Make a reduced rational number a/b.  The reduced number has	*
 * a positive denominator, and has no nontrivial common factors *
 * between numerator and denominator.				*
 ****************************************************************/

ENTITY ast_make_rat(ENTITY a, ENTITY b)
{
  /*------------------------------*
   * Unwrap a and b if necessary. *
   *------------------------------*/

  if(TAG(a) == WRAP_TAG) a = ENTVAL(a)[1];
  if(TAG(b) == WRAP_TAG) b = ENTVAL(b)[1];

  /*--------------------------------------------*
   * Make sure that the denominator is positive *
   *--------------------------------------------*/

  if(ast_sign(b) < 0) {
    a = ast_negate(a);
    b = ast_negate(b);
  }
  return ast_reduce_rat(make_rat(a,b));
}


/****************************************************************
 *			AST_MAKE_RAT1				*
 ****************************************************************
 * Return a rational number 1/a.			        *
 ****************************************************************/

ENTITY ast_make_rat1(ENTITY a)
{
  return ast_make_rat(one, a);
}


/****************************************************************
 *			ADD_RAT					*
 ****************************************************************
 * Return the sum of rational numbers a and b.			*
 ****************************************************************/

ENTITY add_rat(ENTITY a, ENTITY b)
{
  ENTITY *ap, a_denom, *bp, b_denom;
  ENTITY t1, t2;

  ap           = ENTVAL(a);
  a_denom      = ap[1];
  bp           = ENTVAL(b);
  b_denom      = bp[1];
  t1           = ast_mult(ap[0], b_denom);	/* adjusted a numerator */
  t2           = ast_mult(bp[0], a_denom);      /* adjusted b numerator */
  t1           = ast_add(t1, t2);               /* sum or adjusted numerators*/
  t2           = ast_mult(a_denom, b_denom);    /* common denominator */
  return ast_make_rat(t1, t2);

  return t1;
}


/****************************************************************
 *			SUBTRACT_RAT				*
 ****************************************************************
 * Return the difference a - b of rational numbers a and b.	*
 ****************************************************************/

ENTITY subtract_rat(ENTITY a, ENTITY b)
{
  ENTITY *ap, a_denom, *bp, b_denom;
  ENTITY t1, t2;

  ap         = ENTVAL(a);
  a_denom    = ap[1];
  bp         = ENTVAL(b);
  b_denom    = bp[1];
  t1	     = ast_mult(ap[0],b_denom);
  t2         = ast_mult(bp[0],a_denom);
  t1	     = ast_subtract(t1, t2);
  t2         = ast_mult(a_denom, b_denom);
  return ast_make_rat(t1, t2);
}


/****************************************************************
 *			MULT_RAT				*
 ****************************************************************
 * Return the product a*b of rational numbers a and b.		*
 ****************************************************************/

ENTITY mult_rat(ENTITY a, ENTITY b)
{
  ENTITY *ap, *bp, result_num, result_denom;

  if(TAG(a) != RATIONAL_TAG || TAG(b) != RATIONAL_TAG) die(102);

  ap           = ENTVAL(a);
  bp           = ENTVAL(b);
  result_num   = ast_mult(ap[0], bp[0]);
  result_denom = ast_mult(ap[1], bp[1]);
  return  ast_make_rat(result_num, result_denom);
}


/****************************************************************
 *			DIVIDE_RAT				*
 ****************************************************************
 * Return the quiotient a/b of rational numbers a and b.	*
 ****************************************************************/

ENTITY divide_rat(ENTITY a, ENTITY b)
{
  ENTITY *ap, *bp, result_num, result_denom;

  ap           = ENTVAL(a);
  bp           = ENTVAL(b);
  result_num   = ast_mult(ap[0], bp[1]); /* numerator */
  result_denom = ast_mult(ap[1], bp[0]); /* denominator */
  return ast_make_rat(result_num, result_denom);
}
