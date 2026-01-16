/*************************************************************************
 * File:    rts/prtnum.c
 * Purpose: Convert a number to a string under format control
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
 * This file contains functions that convert numbers to strings.	*
 ************************************************************************/

#include <math.h>
#include <string.h>
#include <ctype.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../standard/stdtypes.h"
#include "../tables/tables.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../rts/rts.h"
#include "../show/prtent.h"
#include "../gc/gc.h"
#ifdef DEBUG
# include "../debug/debug.h"
#include "../show/prtent.h"
#endif

PRIVATE int     ast_num_to_str_rec(ENTITY a, ENTITY b);
PRIVATE int     int_to_str_rec    (ENTITY a, LONG n, char c, Boolean commas);
PRIVATE int     rat_to_str_rec    (ENTITY a, ENTITY b);
PRIVATE int     real_to_str_rec   (ENTITY a, ENTITY b);

/****************************************************************
 *			AST_NUM_TO_STR				*
 ****************************************************************
 * Convert number 'a' to a string with format parameter 'b'.	*
 ****************************************************************/

charptr ast_num_to_str(ENTITY a, ENTITY b)
{
  return get_temp_str(ast_num_to_str_rec(a,b));
}


/****************************************************************
 *			AST_NUM_TO_STR_REC			*
 ****************************************************************
 * Convert number a to a string with format parameter b.	*
 * Return the index in temp_str_record of the result string.	*
 ****************************************************************/

PRIVATE int ast_num_to_str_rec(ENTITY a, ENTITY b)
{

tail_recur:
  switch(TAG(a)) {
    case INT_TAG:
    case BIGPOSINT_TAG:
    case BIGNEGINT_TAG:
      {ENTITY put_commas = ast_content_stdf(PUT_COMMAS_BOX);
       return int_to_str_rec(a, get_ival(b, LONG_STRING_EX), ' ', 
			     tobool(VAL(put_commas)));
      }

    case SMALL_REAL_TAG:
    case LARGE_REAL_TAG:
      return real_to_str_rec(a, b);

    case RATIONAL_TAG:
      return rat_to_str_rec(a,b);

    case WRAP_TAG:
      a = ENTVAL(a)[1];
      goto tail_recur;
       
    default: 
      die(122);
      return 0;
  }
}


/****************************************************************
 *			AST_NUM_TO_STRNG			*
 ****************************************************************
 * Conver a number or a function or a box to a string.  This	*
 * function implements $ for numbers, functions, strings and    *
 * boxes.							*
 ****************************************************************/

ENTITY ast_num_to_strng(ENTITY a)
{
  int tag = TAG(a);

  if(tag == FUNCTION_TAG) return fun_to_str(a);
  if(tag == BOX_TAG || tag == PLACE_TAG) return boxpl_to_str(a);

  /*------------------------------------------------------------*
   * Numbers are converted to string (by $) with size parameter *
   * 0.								*
   *------------------------------------------------------------*/

  return make_str(ast_num_to_str(a, zero));
}


/****************************************************************
 *			AST_NUM_TO_STRING			*
 ****************************************************************
 * Return a $$ b.  This function implements $$ for numbers.	*
 * The possibilities are as follows.				*
 *								*
 *	  a type    formats      form				*
 *	 -------  -----------  -----------			*
 *	   							*
 *	   integer/   n          n spaces			*
 *	   natural      					*
 *								*
 *	   rational   n		 n spaces			*
 *		      (n1,n2)    num:n1/denom:n2		*
 *								*
 *	   real       n		 n spaces, exp format		*
 *		      (n,d)      n spaces, d to right of .	*
 *		      (n,d,e)    n spaces, exp format, 		*
 *				 d to right of ., e spaces for	*
 *				 exponent.			*
 ****************************************************************/

ENTITY ast_num_to_string(ENTITY a, ENTITY b)
{
  return make_str(ast_num_to_str(a,b));
}


/****************************************************************
 *			INT_TO_STR_REC				*
 ****************************************************************
 * Return the index of a string record that holds the decimal	*
 * form of x, in at least n columns, padded on the left if	*
 * necessary with character c.  If commas is true, put commas	*
 * in the number every three digits.				*
 *								*
 * int_to_str_rec1 is similar to int_to_str_rec, but it also    *
 * sets *actual to the actual number of characters in the	*
 * string.							*
 ****************************************************************/

PRIVATE int 
int_to_str_rec1(ENTITY x, LONG n, char c, LONG *actual, Boolean commas)
{
  BIGINT_BUFFER aa;
  int tag, result;
  LONG r;
  register LONG i, digiti;
  
  /*----------------------------------------------------------------*
   * i counts the total number of characters that have been written *
   * into the temp string.  digiti counts the number of digits.     *
   * The two can differ due to the presence of commas.		    *
   *----------------------------------------------------------------*/

  i    = digiti = 0;
  tag  = TAG(x);
  if(!MEMBER(tag, int_tags)) die(123);
  result = new_temp_str(0);

  /*-----------------------------*
   * Handle 0 as a special case. *
   *-----------------------------*/

  if(ENT_EQ(x, zero)) {
    result = cons_temp_str('0', result); 
    i = digiti = 1;
  }

  /*--------------------------------------------------------------*
   * For a nonzero number, convert by successive divisions by 10. *
   *--------------------------------------------------------------*/

  else {
    int_to_array(x, &aa, &tag, 0, 0);
    while(aa.val.size > 0) {
      divide_by_digit(&(aa.val), 10, &(aa.val), &r);
      result = cons_temp_str('0' + tochar(r), result);
      i++; 
      digiti++;
      if(aa.val.buff[aa.val.size - 1] == 0) aa.val.size--;
      if(commas && aa.val.size > 0 && digiti % 3 == 0) {
	result = cons_temp_str(',', result);
	i++;
      }
    }
    free_bigint_buffer(&aa);
  }

  if(tag == BIGNEGINT_TAG) {result = cons_temp_str('-', result); i++;}
  while(i < n) {
    result = cons_temp_str(c, result); 
    i++;
  }
  *actual = i;
  return result;
}

/*------------------------------------------------------------*/

PRIVATE int 
int_to_str_rec(ENTITY x, LONG n, char c, Boolean commas)
{
  LONG actual;

  return int_to_str_rec1(x, n, c, &actual, commas);
}


/****************************************************************
 *			INT_TO_HEX_STR_REC			*
 ****************************************************************
 * Return the index of a string record that holds the 		*
 * hexadecimal form of x.  There might be leading zeros.	* 
 *								*
 * This function ignores the sign of x, so really shows the 	*
 * absolute value of x.						*
 *								*
 * Helper function cons_hexes conses the hex digits of cell c	*
 * onto the string in buffer rec, from right to left, adds the  *
 * number of symbols consed to variable *numsyms, and returns   *
 * the index where the string record is stored after the conses.*
 ****************************************************************/

PRIVATE int cons_hexes(intcell c, int rec, LONG *numsyms)
{
  int k;
  int result = rec;

  for(k = 0; k < HEX_DIGITS_IN_DIGIT; k++) {
    int digit = c & 0xf;
    char ch = (digit <= 9) ? '0' + digit : 'A' + digit - 10;
    result = cons_temp_str(ch, result);
    c = c >> 4;
  }
  *numsyms += HEX_DIGITS_IN_DIGIT;
  return result;
}

/*---------------------------------------------------------*/

PRIVATE int int_to_hex_str_rec(ENTITY x)
{
  BIGINT_BUFFER aa;
  intcellptr p;
  int tag, result;
  LONG numsyms;
  register LONG k;
  
  tag  = TAG(x);
  if(!MEMBER(tag, int_tags)) die(123);
  result = new_temp_str(0);

  /*-----------------------------*
   * Handle 0 as a special case. *
   *-----------------------------*/

  numsyms = 0;
  if(ENT_EQ(x, zero)) {
    result = cons_hexes(0, result, &numsyms); 
  }

  /*---------------------------------------------------*
   * For a nonzero number, convert by examining bytes. *
   *---------------------------------------------------*/

  else {
    int_to_array(x, &aa, &tag, 0, 0);
    p = aa.val.buff;
    k = aa.val.size;
    while(k > 0) {
      result = cons_hexes(*p, result, &numsyms);
      k--;
      p++;
    }
    free_bigint_buffer(&aa);
  }

  return result;
}


/****************************************************************
 *			AST_INT_TO_HEX_STR			*
 *			AST_INT_TO_HEX_STRING			*
 ****************************************************************
 * Convert integer 'a' to a string in hex format.	        *
 ****************************************************************/

charptr ast_int_to_hex_str(ENTITY a)
{
  return get_temp_str(int_to_hex_str_rec(a));
}

ENTITY ast_int_to_hex_string(ENTITY a)
{
  return make_str(ast_int_to_hex_str(a));
}



/****************************************************************
 *			RAT_TO_STR_REC				*
 ****************************************************************
 * Compute a $$ b for a rational number a.  If rat_result is    *
 * TRUE, return the result as a rational number.  If rat_result *
 * is FALSE, return the result as a real number.		*
 ****************************************************************/

int rat_to_str_rec(ENTITY a, ENTITY b)
{
  LONG n, n1, n2, actual2;
  int num_as_str_rec, result = 0;
  Boolean denom_is_one, put_commas;
  ENTITY *ap, h, t;

  put_commas = tobool(VAL(ast_content_stdf(PUT_COMMAS_BOX)));
  ap = ENTVAL(a);

  /*------------------------------------------------------------------*
   * Extract format information.  n is the total number of characters *
   * to use, n1 is the number of characters to use for the numerator, *
   * and n2 is the number to use for the denominator.  If n2 is 0,    *
   * then use the fewest possible characters for each.		      *
   *------------------------------------------------------------------*/

  if(MEMBER(TAG(b), int_tags)) {
    n = get_ival(b, LONG_STRING_EX);
    n2 = 0;
  }
  else {
    ast_split(b, &h, &t);
    n1 = get_ival(h, LONG_STRING_EX);
    n2 = get_ival(t, LONG_STRING_EX);
    n  = n1 + n2 + 1;
  }

  /*------------------------------------------------------------*
   * Write the denominator into the temp string (at the end), 	*
   * preceded by a "/", unless the denominator is 1, in which 	*
   * case it is not written.	     				*
   *------------------------------------------------------------*/

  denom_is_one = TRUE;
  if(ENT_NE(ap[1], one)) {
    denom_is_one = FALSE;
    result = int_to_str_rec1(ap[1], n2, ' ', &actual2, put_commas);
    result = cons_temp_str('/', result);
    n1 = n - 1 - actual2;
    if(n1 < 0) n1 = 0;
  }
  else n1 = n;

  /*----------------------*
   * Write the numerator. *
   *----------------------*/

  num_as_str_rec = int_to_str_rec(ap[0], n1, ' ', put_commas);
  return denom_is_one ? num_as_str_rec 
                        : cat_temp_str(num_as_str_rec, result);
}


/************************************************************************
 *			SHIFT_LEFT_POWER				*
 ************************************************************************
 * Shift a left by s^k for some k, to make it as close to 1 as possible *
 * without being larger than 1.						*
 *									*
 * Return k in parameter k and the result of shifting in parameter      *
 * result.								*
 ************************************************************************/

PRIVATE void 
shift_left_power(ENTITY a, ENTITY s, ENTITY *k, ENTITY *result)
{
  ENTITY prod, kk, ssq;

  /*-----------------------------------------------------------------*
   * The algorithm is to try shifting by s, s^2, s^4, s^8, etc., in  *
   * order to get large shifts where possible.			     *
   *-----------------------------------------------------------------*/

  prod = ast_mult(a, s);
  if(ast_compare_simple(prod, one_real) != 1) {
    ssq = ast_mult(s, s);
    shift_left_power(a, ssq, &kk, result);
    if(ENT_EQ(kk, zero)) {
      free_if_diff(*result, 2, a, ssq);
      free_if_diff(ssq, 1, s);
      *k      = one;
      *result = prod;
    }
    else {
      free_if_diff(prod, 2, a, s);
      *k   = ast_add(kk, kk);
      prod = ast_mult(*result, s);
      if(ast_compare_simple(prod, one_real) != 1) {
        free_if_diff(*result, 3, a, ssq, prod);
        free_if_diff(ssq, 1, s);
	*k      = ast_add(*k, one);
	*result = prod;
      }
      else {
        free_if_diff(prod, 2, *result, s);
	free_if_diff(ssq, 2, *result, s);
      }
    }
  }
  else {
    free_if_diff(prod, 2, a, s);
    *k      = zero;
    *result = a;
  }
}        


/************************************************************************
 *			SHIFT_RIGHT_POWER				*
 ************************************************************************
 * Shift a right by s^k for some k, to make it as close to 1 as		*
 * possible without it becoming smaller than 1.  Return k in parameter	*
 * k and the result of shifting in parameter result.			*
 ************************************************************************/

PRIVATE void 
shift_right_power(ENTITY a, ENTITY s, ENTITY *k, ENTITY *result)
{
  ENTITY prod, kk, ssq;

  /*---------------------------------------------------------------*
   * The algorithm is to shift right by s, s^2, s^4, s^8, etc., in *
   * order to get large shifts.					   *
   *---------------------------------------------------------------*/

  prod = ast_divide(a, s);
  if(ast_compare_simple(prod, one_real) != 2) {
    ssq = ast_mult(s, s);
    shift_right_power(a, ssq, &kk, result);
    if(ENT_EQ(kk, zero)) {
      free_if_diff(*result, 2, a, ssq);
      free_if_diff(ssq, 1, s);      
      *k      = one;
      *result = prod;
    }
    else {
      free_if_diff(prod, 2, a, s);
      *k   = ast_add(kk, kk);
      prod = ast_divide(*result, s);
      if(ast_compare_simple(prod, one_real) != 2) {
        free_if_diff(*result, 3, a, ssq, prod);
        free_if_diff(ssq, 1, s);
	*k      = ast_add(*k, one);
	*result = prod;
      }
      else {
        free_if_diff(prod, 2, *result, s);
	free_if_diff(ssq, 2, *result, s);
      }
    }
  }
  else {
    free_if_diff(prod, 2, a, s);
    *k      = zero;
    *result = a;
  }
}        


/****************************************************************
 *			GET_REAL_FORMAT_PARAMS			*
 ****************************************************************
 * Entity b is an integer format to $$ for reals, and x is the  *
 * value being converted to a string.  Set the format parameters*
 * n, d and e as described in the body of real_to_str_rec.	*
 ****************************************************************/

PRIVATE void get_real_format_params(ENTITY b, ENTITY x,
				   LONG *n, LONG *d, LONG *e)
{
  LONG b_as_int;

  b_as_int = get_ival(b, LONG_STRING_EX);
  if(failure > 0) return;

  /*--------------------------------------------------------------------*
   * Case where b is positive.  Then use exponential format, with	*
   * b total spaces.  This only works in a reasonable way for		*
   * b >= 9, so force b up to 7.					*
   *--------------------------------------------------------------------*/

  if(b_as_int > 0) {
    if(b_as_int < 7) b_as_int = 7;
    *n = b_as_int;
    *e = min(4, b_as_int - 5);
    *d = b_as_int - 4 - *e;
  }

  /*--------------------------------------------------------------------*
   * If b = 0, then choose parameters according to the current		*
   * precision and the magnitude of x.  The idea is to use		*
   * fixed point format if that is reasonable, and to use		*
   * exponential format if that is really required for the number	*
   * to look nice.							*
   *									*
   * We use fixed point format when the precision is p, and 		*
   * 0.01 <= |x| <= 10^(p+2), and when x = 0.				*
   *--------------------------------------------------------------------*/

  else {
    LONG    dec_prec  = get_dec_precision();
    Boolean use_fixed = FALSE;
    ENTITY  abs_x     = ast_abs(x);
    int     sign_x    = ast_sign(x);

    if(sign_x == 0) use_fixed = TRUE;
    else if(ast_compare_simple(dollar_fixp_low, abs_x) != 1) {
      ENTITY dollar_fixp_high = ten_real_to(dec_prec + 2);
      if(ast_compare_simple(abs_x, dollar_fixp_high) != 1) {
        use_fixed = TRUE;
      }
    }

    if(use_fixed) {
      ENTITY xx = abs_x;
      *e = 0;
      *n = dec_prec + 2;
      *d = dec_prec;

      /*----------------------------------------------------------------*
       * While xx >= 1, divide xx by 10 and subtract 1 from *d.  This	*
       * effectively moves digits from the right side to the left	*
       * side of the decimal point to accomodate the integer part	*
       * of the number.							*
       *----------------------------------------------------------------*/

      while(ast_compare_simple(xx, one_real) != 2) {
        (*d)--;
        xx = ast_divide(xx, ten_real);
      }

      /*----------------------------------------------------------------*
       * Now handle the case of small but nonzero numbers.  These are	*
       * shown as 0.0..., with leading 0's after the decimal point.	*
       * We need to add extra digits for these.				*
       *----------------------------------------------------------------*/

      if(sign_x != 0) {
        xx = ast_mult(xx, ten_real);
        while(ast_compare_simple(xx, one_real) == 2) {
          (*d)++;
          (*n)++;
          xx = ast_mult(xx, ten_real);
	}
      } 
    }
    else {
      *e = 2;
      *n = dec_prec + 5;
      *d = dec_prec - 1;
    }
  }
}


/****************************************************************
 *			REAL_TO_STR_REC				*
 ****************************************************************
 * Return the index of a string record for aa $$ b, for a real	*
 * number aa.  If b is too large, set failure = LONG_STRING_EX  *
 * and return index -1 (which indicates an empty string.)	*
 ****************************************************************/

PRIVATE int 
real_to_str_rec(ENTITY aa, ENTITY b)
{
  char ch;
  int sgn, result;
  LONG leftd, i, n, d, e, original_precision;
  Boolean put_commas;
  ENTITY h, t, a, newa, fl, fr, fri, ex;

  a = aa;
  put_commas = tobool(VAL(ast_content_stdf(PUT_COMMAS_BOX)));

  /*------------------------------------------------------------*
   * Get the format parameters n, d, e 				*
   *   n = total number of spaces,				*
   *   d = number of digits to right of point,			*
   *   e = number of digits for exponent, including sign,	*
   *       excluding E.						*
   *------------------------------------------------------------*/

  if(MEMBER(TAG(b), int_tags)) {

    /*------------------------------*
     * A single number as a format. *
     *------------------------------*/

    get_real_format_params(b, aa, &n, &d, &e);
  }

  else { /* !MEMBER(TAG(b), int_tags) */
    ast_split(b, &h, &t);
    n = get_ival(h, LONG_STRING_EX);
    t = remove_indirection(t);
    if(TAG(t) == PAIR_TAG) {

      /*----------------------------------------------------------------*
       * A triple format (n,d,e).  Note that val of e does not include	*
       * the sign, so add 1 for the sign.				*
       *----------------------------------------------------------------*/

      ast_split(t, &h, &t);
      d = get_ival(h, LONG_STRING_EX);
      e = get_ival(t, LONG_STRING_EX) + 1;
    }
    else {

      /*------------------------*
       * A double format (n,d). *
       *------------------------*/

      d = get_ival(t, LONG_STRING_EX);
      e = 0;
    }

  } /* end else (!MEMBER(TAG(b), int_tags)) */

  /*-------------------------------------------------------*
   * It is possible to fail because a number is too large. *
   * ------------------------------------------------------*/

  if(failure > 0) return -1;

  /*-------------------------------------------------*
   * We always show at least one digit to the right  *
   * of the decimal place (unless b = 0, in which    *
   * case we might not show a point at all), and     *
   * don't want that to be confusing, so force d up  *
   * to 1 if smaller.       			     *
   *						     *
   * n and e must each be nonnegative, so force them *
   * as well.					     *
   *-------------------------------------------------*/

  if(d < 1) d = 1;
  if(n < 0) n = 0;
  if(e < 0) e = 0;

  /*-------------------------------------*
   * We can adjust the precision down    *
   * to max(4, DECIMAL_TO_DIGIT(n)).	 *
   *-------------------------------------*/

  original_precision = DIGITS_PRECISION;
  {LONG new_precision = DECIMAL_TO_DIGIT(n);
   if(new_precision < 4) new_precision = 4;
   if(new_precision < original_precision) {
     simple_assign_bxpl(DIGITS_PRECISION_BOX, ast_make_int(new_precision));
   }
  }

  /*------------------------------------*
   * Get the sign, and set a to abs(a). *
   *------------------------------------*/

  sgn = ast_sign(a);
  if(sgn < 0) a = ast_negate(a);

  /*------------------------------------------------------------*
   * If exponent format, shift so that it has one digit		*
   * to the left of the decimal point. Set ex to the exponent	*
   * to display that compensates for this shift.		*
   *------------------------------------------------------------*/

  if(e != 0) {
    ex   = zero;
    if(ast_sign(a) != 0) {
      ENTITY k;

      shift_right_power(a, ten_real, &ex, &newa);
      shift_left_power (newa, ten_real, &k, &a);
      ex = ast_subtract(ex, k);
      while(ast_compare_simple(a, one_real) == 1) {
        newa  = ast_divide(a, ten_real);
	free_if_diff(a, 3, aa, newa, ten_real);
	a     = newa;
        ex    = ast_add(ex, one);
      }
      while(ast_compare_simple(a, one_real) == 2) {
        newa  = ast_mult(ten_real, a);
	free_if_diff(a, 3, aa, newa, ten_real);
	a     = newa;
        ex = ast_subtract(ex, one);
      }
    }
  }

  /*--------*
   * Round. *
   *--------*/

  t    = ten_real_to(d);
  fl   = ast_divide(half_real, t);
  newa = ast_add(a, fl);
  a    = newa;
  if(e != 0 && ast_compare_simple(a, ten_real) != 2) {
    newa  = ast_divide(a, ten_real);
    a  = newa;
    ex = ast_add(ex, one);
  }

  /*------------------------------------------------------------*
   * Get the parts to be printed.  The number looks like	*
   *								*
   *   fl.fri							*
   *								*
   * in fixed point format, and					*
   *								*
   *   fl.friEex						*
   *								*
   * in exponential format.					*
   *------------------------------------------------------------*/

  fl  = ast_floor(a);
  fr  = ast_real(fl);
  fr  = ast_subtract(a, fr);
  fri = ast_mult(t, fr);
  fri = ast_floor(fri);

  /*---------------------------------------------------------*
   * Print the number into the temp str, from right to left. *
   * Start with the exponent, if there is any.		     *
   *---------------------------------------------------------*/

  if(e != 0) {
    t = ast_abs(ex);
    result = int_to_str_rec(t, e-1, '0', FALSE); 
    ch     = (ast_sign(ex) >= 0) ? '+' : '-';
    result = cons_temp_str(ch, result);
    result = cons_temp_str('E', result);
    leftd  = n - e - d - 2;
  }
  else {
    result = new_temp_str(0);
    leftd  = n - d - 1;
  }

  /*--------------------------------------------------------------------*
   * Now write the part that is to the right of the decimal point.	*
   * If b = 0, then any trailing zeros in this should be removed. 	*
   *--------------------------------------------------------------------*/
  
  {int fri_rec = int_to_str_rec(fri, d, '0', FALSE);
   if(ENT_EQ(b, zero)) {
     remove_trailing(fri_rec, '0');
   }
   result = cat_temp_str(fri_rec, result);
  }

  /*------------------------------------------------------------*
   * Add the decimal point, as long as there is at least one	*
   * digit to the right of the decimal point.			*
   *------------------------------------------------------------*/

  {char nextch = *grab_temp_str(result);
   if('0' <= nextch && nextch <= '9') {
     result = cons_temp_str('.', result);
   }
  }

  /*-------------------------------------------------------------*
   * Now add the integer part, to the left of the decimal point, *
   * and pad with spaces if necessary.				 *
   *-------------------------------------------------------------*/

  if(ENT_EQ(fl, zero) && sgn < 0) {
    result = multicons_temp_str("-0", result);
    for(i = leftd-2; i > 0; i--) result = cons_temp_str(' ', result);
  }
  else {
    if(sgn < 0) fl = ast_negate(fl);
    result = cat_temp_str(int_to_str_rec(fl, leftd, ' ', put_commas), result);
  }

  simple_assign_bxpl(DIGITS_PRECISION_BOX, ast_make_int(original_precision));

  /*-------------------------------------*
   * If b = 0, then skip leading blanks. *
   *-------------------------------------*/

  if(ENT_EQ(b, zero)) {
    remove_leading(result, ' ');
  }

  return result;
}


/****************************************************************
 *			AST_STR_TO_INT				*
 ****************************************************************
 * Return string s, as an integer.  This implements functions	*
 * stringToNatural and stringToInteger, but in the case where   *
 * the parameter is a C string, not an ENTITY.			*
 ****************************************************************/

ENTITY ast_str_to_int(char *s)
{
  char *p;
  int d, sign;
  ENTITY result;

  result = zero;
  p      = s;
  while(isspace(*p)) p++;

  /*---------------------------------*
   * Can't convert the empty string. *
   *---------------------------------*/

  if(p[0] == '\0') {
    failure = CONVERSION_EX;
    return zero;
  }

  /*---------------*
   * Get the sign. *
   *---------------*/

  sign = 1;
  if(*p == '-') {
    sign = -1;
    p++;
  }
  else if(*p == '+') p++;

  /*--------------------------------------------*
   * Accumulate the number. If it has k decimal *
   * digits, then it has about 			*
   * DECIMAL_TO_DIGIT(k) internal digits.  Get  *
   * a few extra.				*
   *--------------------------------------------*/

  {LONG k = strlen(p);
   LONG buf_size    = DECIMAL_TO_DIGIT(k) + 5;
   BIGINT_BUFFER rb;
   intcell tmp_arr_buff[2];
   INTCELL_ARRAY tmp_arr;
   tmp_arr.size = 1;
   tmp_arr.buff = tmp_arr_buff;

   get_bigint_buffer(buf_size, &rb);
   longmemset(rb.val.buff, 0, INTCELL_TO_BYTE(buf_size));
   rb.val.size = 1;
   for(; *p != '\0'; p++) {
     d = *p - '0';
     if(d < 0 || d > 9) break;
     tmp_arr_buff[0] = d;
     mult_by_digit(&(rb.val), 10);
     if(rb.val.buff[rb.val.size - 1] != 0) rb.val.size++;
     add_array(&(rb.val), &tmp_arr, &(rb.val));
     if(rb.val.buff[rb.val.size - 1] != 0) rb.val.size++;
     if(rb.val.size > buf_size) die(135);
   }
   result = array_to_int(BIGPOSINT_TAG, &(rb));
  }

  /*-----------------------------------*
   * Check that there is no junk left. *
   *-----------------------------------*/

  while(isspace(*p)) p++;
  if(*p != '\0') {
    failure = BAD_STRING_EX;
    return zero;
  }

  /*-----------------------------------------*
   * Return the result after restoring sign. *
   *-----------------------------------------*/

  if(sign != 1) result = ast_negate(result);

  return result;
}


/****************************************************************
 *			AST_STR_TO_HEX_INT			*
 ****************************************************************
 * Return string s, as a natural number.  Here, s is a		*
 * hexadecimal integer.						*
 *								*
 * Helper function install_hex_digits gets up to 		*
 * min(maxchars, HEX_DIGITS_IN_DIGIT) hexadecimal digits from   *
 * the beginning of string p, and installs them into *buff as   *
 * an intcell value.  It returns the number of hex digits read  *
 * from p.							*
 ****************************************************************/

PRIVATE int 
install_hex_digits(charptr p, intcellptr buff, int maxchars)
{
  int d, i, shft;
  intcell c;

  c = 0;
  shft = 0;
  for(i = 0; i < HEX_DIGITS_IN_DIGIT && i < maxchars; i++) {
     d = p[-i];
     if('0' <= d && d <= '9') d -= '0';
     else if('a' <= d && d <= 'f') d -= ('a' - 10);
     else if('A' <= d && d <= 'F') d -= ('A' - 10);
     else {
       failure = BAD_STRING_EX;
       break;
     }

     c += d << shft;
     shft += 4;
  }
  *buff = c;
  return i;
}

/*---------------------------------------------------*/

PRIVATE ENTITY ast_str_to_hex_int(char *s)
{
  ENTITY result;
  LONG k, digits, s_len, buf_size;
  char *p;
  BIGINT_BUFFER rb;

  while(isspace(*s)) s++;

  /*---------------------------------*
   * Can't convert the empty string. *
   *---------------------------------*/

  if(s[0] == '\0') {
    failure = CONVERSION_EX;
    return zero;
  }

  /*----------------------------------------------------*
   * Locate the rightmost nonwhite character in s.	*
   * p points to it.					*
   *----------------------------------------------------*/

  s_len = strlen(s);
  p     = s + s_len - 1;
  while(isspace(*p)) p--;

  /*-----------------*
   * Get the buffer. *
   *-----------------*/

  s_len = p - s + 1;
  buf_size = HEX_TO_DIGIT(s_len) + 1;
  get_bigint_buffer(buf_size, &rb);

  /*-------------------------------------*
   * Scan the string from right to left. *
   *-------------------------------------*/

  k = digits = 0;
  while(k < s_len && failure < 0) {
    k += install_hex_digits(p-k, rb.val.buff + digits, s_len - k);
    digits++;
  }
  rb.val.size = digits;
  result = array_to_int(BIGPOSINT_TAG, &(rb));

  return result;
}


/****************************************************************
 *			AST_HEX_TO_NAT				*
 ****************************************************************
 * Convert string s into a natural number, where s is in hex.	*
 * Presumes that s is fully evaluated.  			*
 ****************************************************************/

ENTITY ast_hex_to_nat(ENTITY s)
{
  char *ss;
  return ast_str_to_hex_int((char*)(get_temp_str(make_temp_str(s, &ss))));
}


/****************************************************************
 *			AST_STRING_TO_NAT			*
 ****************************************************************
 * Convert string s into a natural number, where s is in 	*
 * decimal.  Presumes that s is fully evaluated.  		*
 ****************************************************************/

ENTITY ast_string_to_nat(ENTITY s)
{
  ENTITY result = ast_string_to_int(s);
  if(ast_sign(result) == -1) {
    failure = CONVERSION_EX;
    return zero;
  }
  else return result;
}


/****************************************************************
 *			AST_STRING_TO_INT			*
 ****************************************************************
 * Convert s to an integer, where s is in decimal.		*
 * Presumes that s is fully evaluated.				*
 ****************************************************************/

ENTITY ast_string_to_int(ENTITY s)
{
  char *ss;
  return ast_str_to_int((char*)(get_temp_str(make_temp_str(s, &ss))));
}


/****************************************************************
 *			AST_STR_TO_RAT				*
 ****************************************************************
 * Convert string s to a rational number.  The form of s is the *
 * form of a constant in an Astarte program, such as 12.34E56.  *
 * 								*
 * If rat_result is true, return the result as a rational	*
 * number.  If rat_result is false, return the result as a	*
 * real number.							*
 ****************************************************************/

ENTITY ast_str_to_rat(char *s, Boolean rat_result)
{
  char *p, *digits;
  int num_digits, exsign, ch, digits_index;
  LONG  e;
  ENTITY exponent, shift, num;

  /*-------------------------------------------*
   * Get a temp array to hold the digits of s. *
   *-------------------------------------------*/

  digits_index = new_temp_str(strlen(s)+1);
  digits       = (char*) temp_str_buffer(digits_index);

  /*---------------------------------------------------------------*
   * Copy the part to the left of the decimal point. Since we have *
   * copied s to digits in the make_temp_str call, just count      *
   * the digits.						   *
   *---------------------------------------------------------------*/

  num_digits = 0;
  for(p = s; 
      (ch = *p), (ch >= '0' && ch <= '9') || ch == '-' || ch == '+';
      p++) {
    digits[num_digits++] = ch;
  }

  /*------------------------------------------------------------*
   * Copy the part to the right of the decimal point. We are	*
   * shifting the number to the left here.  Keep track of the 	*
   * number of digits of the shift in e.  (e will be negative,  *
   * indicating that a negative shift is required to restore    *
   * the number.)						*
   *------------------------------------------------------------*/

  e = 0;
  if(*p == '.') {
    p++;
    for(; 
	(ch = *p), (ch >= '0' && ch <= '9');
	p++) {
      e--;
      digits[num_digits++] = ch;
    }
  }

  /*--------------------------------------------------------------------*
   * Null terminate the string of digits, and convert it to an integer. *
   * Then free the digits array.					*
   *--------------------------------------------------------------------*/

  digits[num_digits] = '\0';
  num = ast_str_to_int(digits);
  get_temp_str(digits_index);		/* Free the buffer. */

  if(failure >= 0) {
 err_ret:
    return zero_rat;
  }

  /*----------------------------------------------------------------*
   * Get the exponent part of s. This must not be too large, or we  *
   * can't represent it.					    *
   *----------------------------------------------------------------*/

  exponent = zero;
  if(*p == 'e' || *p == 'E') {
    exsign = 1;
    p++;
    if(*p == '-') {exsign = -1; p++;}
    else if(*p == '+'){p++;}
    for(; *p != '\0'; p++) {
      int d = *p - '0';
      if(d < 0 || d > 9) break;
      exponent = ast_mult(exponent, ten);
      exponent = ast_add(exponent, ENTI(d));
    }
    if(exsign < 0) exponent = ast_negate(exponent);
  }

  /*-----------------------------------*
   * Check that there is no junk left. *
   *-----------------------------------*/

  while(isspace(*p)) p++;
  if(*p != '\0') {
    failure = CONVERSION_EX;
    goto err_ret;
  }

  /*------------------------------------------------------------*
   * Compute the exponent shift.  This is a number to multiply  *
   * or divide by to effect the shift.  Then multiply or	*
   * divide num by it.  The first case is for a rational number *
   * as result.						        *
   *------------------------------------------------------------*/

  if(rat_result) {
    LONG expon = get_ival(exponent, MEM_EX);
    if(failure < 0) {
      LONG shift_exp = e + expon; 
      shift = ten_int_to(absval(shift_exp));
      if(shift_exp >= 0) {
        num = ast_mult(num, shift);
        num = ast_make_rat(num, one);
      }
      else {
        num = ast_make_rat(num, shift);
      }
    }
  }

  /*-------------------------------------*
   * The next case is for a real result. *
   *-------------------------------------*/

  else {
    LONG expon;

    exponent = ast_add(exponent, ENTI(e));
    expon    = get_ival(exponent, TEST_EX);
    if(failure < 0) shift = ten_real_to(expon);
    else {
      failure = -1;
      shift   = ast_power(ten_real, exponent);
    }
    num = int_to_real(num);
    num = ast_mult(num, shift);
  }    

  return num;
}


/****************************************************************
 *			AST_STRING_TO_RAT			*
 ****************************************************************
 * Return $(s), where s is a rational number.			*
 * Presumes s is fully evaluated.				*
 ****************************************************************/

ENTITY ast_string_to_rat(ENTITY s)
{
  char *ss;
  return ast_str_to_rat((char*)(get_temp_str(make_temp_str(s, &ss))), TRUE);
}


/****************************************************************
 *			AST_STRING_TO_REAL			*
 ****************************************************************
 * Return $(s), where s is a real number.			*
 * Presumes s is fully evaluated.				*
 ****************************************************************/

ENTITY ast_string_to_real(ENTITY s)
{
  char *ss;
  return ast_str_to_rat((char *)(get_temp_str(make_temp_str(s, &ss))), FALSE);
}

