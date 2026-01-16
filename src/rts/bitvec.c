/*************************************************************************
 * File:    rts/bitvec.c
 * Purpose: Implement bit vector operations on integers
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

/****************************************************************
 * Bit vectors are stored as integers, with an integer thought  *
 * of as standing for its infinite two's complement 		*
 * representation.  For example, integer 5 stands for the	*
 * infinite vector ...0101 with infinitely many leading 0's, 	*
 * and integer -5 stands for the infinite vector ...1011 with	*
 * infinitely many leading 1's.   Since integers are stored in	*
 * sign and magnitude form, to convert from a negative number	*
 * to the complement of its  bit vector, just subtract 1 from 	*
 * the magnitude.  For example, to get from -5 to vector ...0100*
 * subtract 1 from 5.  To get from a positive number to the 	*
 * complement of its bit vector, just add one and select a 	*
 * negative sign.						*
 ****************************************************************/


#include <memory.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../evaluate/evaluate.h"
#include "../rts/rts.h"
#include "../tables/tables.h"
#include "../gc/gc.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			LOWMASK					*
 ****************************************************************
 * lowmask[k] is a bit vector with 1's in the low order k bits, *
 * and 0's in the other bits.					*
 ****************************************************************/

#if (INTCELL_BITS == 16)
intcell lowmask[] =
{
  0x0000,
  0x0001,
  0x0003,
  0x0007,
  0x000f,
  0x001f,
  0x003f,
  0x007f,
  0x00ff,
  0x01ff,
  0x03ff,
  0x07ff,
  0x0fff,
  0x1fff,
  0x3fff,
  0x7fff,
  0xffff
};
#endif


/****************************************************************
 *			BV_GET_ARRAYS				*
 ****************************************************************
 * Copy integers *a and *b into arrays ra and rb.  		*
 * Set taga to the tag of a (either BIGPOSINT_TAG or		*
 * BIGNEGINT_TAG).  Do similarly for b.				*
 *								*
 * If a or b are small, they will be put into space given by    *
 * arrays a_space and b_space.  Those arrays must be allocated	*
 * by the caller, and should have INTCELLS_IN_LONG intcells	*
 * each.							*
 *								*
 * Arrange that ra.size >= rb.size, by swapping if necessary.	*
 ****************************************************************/

PRIVATE void bv_get_arrays(ENTITY *a, ENTITY *b,
			   BIGINT_BUFFER *ra, int *taga, intcellptr a_space,
			   BIGINT_BUFFER *rb, int *tagb, intcellptr b_space)
{
  BIGINT_BUFFER rt;
  ENTITY et;
  int tagt;

  const_int_to_array(*a, ra, taga, a_space);
  const_int_to_array(*b, rb, tagb, b_space);

  /*----------------------*
   * Swap if b is larger. *
   *----------------------*/

  if(ra->val.size < rb->val.size) {
    rt   = *ra;       *ra = *rb;       *rb = rt;
    tagt = *taga;   *taga = *tagb;   *tagb = tagt;
    et   = *a;         *a = *b;         *b = et;
  }

}


/****************************************************************
 *			BV_AND_ARRAY				*
 ****************************************************************
 * Compute the "and" of the bit vectors in ra and rb, where	*
 * ra->size >= rb->size.  taga and tagb indicate the 		*
 * associated tags (BIGPOSINT_TAG for positive, BIGNEGINT_TAG	*
 * for negative).  						*
 * Put the result in rc, and the result tag in *tagc.  		*
 * It is required that rc->size >= ra->size + 1.  		*
 * rc must be a different buffer from ra and rb.		*
 ****************************************************************/

PRIVATE void bv_and_array(INTCELL_ARRAY *ra, int taga,
			  INTCELL_ARRAY *rb, int tagb,
			  INTCELL_ARRAY *rc, int *tagc)
{
  /*--------------------------------------------------------------------*
   * Case where a is positive.  So a has leading 0's.			*
   *									*
   * The result will have leading 0's.  If b is positive, just 		*
   * do a bitwise 'and'.  If b is negative (so it 			*
   * represents a vector with leading 1's) then get the complement	*
   * of the bit vector represented by b, do a bitwise complement of	*
   * that to get b itself, and do a bitwise and, noting that missing	*
   * part of the b vector are filled with 1's.				*
   *--------------------------------------------------------------------*/

  if(taga == BIGPOSINT_TAG) {
    if(tagb == BIGNEGINT_TAG) {
      subtract_array(rb, &one_array, rc);
      not_array(rc, rc, 0);
      and_array(rc, ra, rc, 0);
    }
    else {
      and_array(ra, rb, rc, 0);
    }
    *tagc = taga;
  }

  /*--------------------------------------------------------------------*
   * Case where a is negative.  So a has leading 1's.			*
   *									*
   * If b is positive, then the result has leading 0's.  We get the	*
   * complement of the vector represented by a by subtracting 1 from	*
   * ra, then get a by doing a bitwise complement on that, then 	*
   * 'and' the two arrays together.					*
   *									*
   * If both a and b are negative, then the result has leading 1's.	*
   * Get the complement of the vectors represented by a and b, by	*
   * subtracting 1 from each of ra and rb.  Do a bitwise or on the	*
   * result, and complement the result by adding 1 to its representation*
   * and setting the tag negative.					*
   *--------------------------------------------------------------------*/

  else {
    subtract_array(ra, &one_array, rc);
    if(tagb == BIGPOSINT_TAG) {
      not_array(rc, rc, 0);
      and_array(rc, rb, rc, 0);
      *tagc = BIGPOSINT_TAG;
    }
    else {
      BIGINT_BUFFER rd;		/* Same as rc, but reduced size */
      get_bigint_buffer(rb->size, &rd);
      subtract_array(rb, &one_array, &(rd.val)); 
      or_array(rc, &(rd.val), rc, 0);
      add_array(rc, &one_array, rc);
      free_bigint_buffer(&rd);
      *tagc = BIGNEGINT_TAG;
    }
  }
}


/****************************************************************
 *			BV_XOR_ARRAY				*
 ****************************************************************
 * Compute the xor of the bit vectors in ra and rb,		*
 * where ra->size >= rb->size.  taga and tagb indicate the 	*
 * associated tags (BIGPOSINT_TAG for positive, BIGNEGINT_TAG	*
 * for negative).  						*
 *								*
 * Put the result in rc, and the result tag in 			*
 * *tagc.  rc must have at least ra->size + 1 digits in its	*
 * buffer.  rc must be a different buffer from ra or rb.	*
 ****************************************************************/

PRIVATE void bv_xor_array(INTCELL_ARRAY *ra, int taga,
			  INTCELL_ARRAY *rb, int tagb,
			  INTCELL_ARRAY *rc, int *tagc)
{
  if(taga == BIGPOSINT_TAG) {

    /*------------------------------------------------------------------*
     * If a and b are both positive, then just xor the arrays, and	*
     * leave positive.							*
     *------------------------------------------------------------------*/

    if(tagb == BIGPOSINT_TAG) {
      xor_array(ra, rb, rc, 0);
      *tagc = taga;
    }

    /*------------------------------------------------------------------*
     * If a is positive and b is negative, then the result has leading	*
     * 1's.  We will get the correct result by taking the complement    *
     * of b (adding 1 to its absolute value), xoring with a, and then   *
     * complementing the result by adding 1 and setting the tag negative.*
     *------------------------------------------------------------------*/

    else {
      BIGINT_BUFFER rd;
      get_bigint_buffer(rb->size, &rd);
      subtract_array(rb, &one_array, &(rd.val));
      xor_array(ra, &(rd.val), rc, 0);
      add_array(rc, &one_array, rc);
      free_bigint_buffer(&rd);
      *tagc = BIGNEGINT_TAG;
    }
  }

  else {

    /*-----------------------------------------------------------------*
     * If a is negative and b is positive, then the result has leading *
     * 1's.  Complement a, xor, and complement the result, as for the  *
     * case where a is positive and b is negative.		       *
     *-----------------------------------------------------------------*/

    if(tagb == BIGPOSINT_TAG) {
      subtract_array(ra, &one_array, rc);  /* Subtract 1 from ra. */
      xor_array(rc, rb, rc, 0);
      add_array(rc, &one_array, rc);
      *tagc = taga;
    }

    /*----------------------------------------------------------*
     * If a is negative and b is negative, then the result has 	*
     * leading 0's.  Complement both and xor. (The xor of the	*
     * complements is the same as the complement of the xors.)	*
     *----------------------------------------------------------*/

    else {
      BIGINT_BUFFER rd;
      get_bigint_buffer(rb->size + 1, &rd);
      rd.val.size = rb->size;
      subtract_array(ra, &one_array, rc);  
      subtract_array(rb, &one_array, &(rd.val));  
      xor_array(rc, &(rd.val), rc, 0);
      free_bigint_buffer(&rd);
      *tagc = BIGPOSINT_TAG;
    }
  }
}


/****************************************************************
 *			BV_NOT_ARRAY				*
 ****************************************************************
 * Do the effect of a bitwise complement of array ra,		*
 * by flipping the sign and adding or subtracting 1.  taga 	*
 * should be the tag of ra (BIGPOSINT_TAG or 			*
 * BIGNEGINT_TAG).						*
 *								*
 * The result is put into array rr, and the result tag will be  *
 * put into tagr.  rr must have at least ra->size + 1 digits    *
 * in its buffer.	      					*
 ****************************************************************/

PRIVATE void bv_not_array(INTCELL_ARRAY *ra, int taga,
			  INTCELL_ARRAY *rr, int *tagr)
{
  if(taga == BIGPOSINT_TAG) {
    add_array(ra, &one_array, rr);
  }
  else {
    subtract_array(ra, &one_array, rr);
  }
  *tagr = (BIGPOSINT_TAG + BIGNEGINT_TAG) - taga;
}


/****************************************************************
 *			BV_AND					*
 ****************************************************************
 * Return the bitwise 'and' of bit vectors a and b.		*
 ****************************************************************/

ENTITY bv_and(ENTITY a, ENTITY b)
{
  BIGINT_BUFFER ra, rb, rc;
  int taga, tagb, tagc;
  intcell a_space[INTCELLS_IN_LONG], b_space[INTCELLS_IN_LONG];

  bv_get_arrays(&a, &b, &ra, &taga, a_space, &rb, &tagb, b_space);
  get_bigint_buffer(ra.val.size + 1, &rc);
  bv_and_array(&(ra.val), taga, &(rb.val), tagb, &(rc.val), &tagc);
  return array_to_int(tagc, &rc);
}


/****************************************************************
 *			BV_OR					*
 ****************************************************************
 * Return the bitwise 'or' of bit vectors a and b.		*
 ****************************************************************/

ENTITY bv_or(ENTITY a, ENTITY b)
{
  BIGINT_BUFFER ra, rb, rc, rd, re;
  int taga, tagb, tagc, tagd, tage;
  intcell a_space[INTCELLS_IN_LONG], b_space[INTCELLS_IN_LONG];

  bv_get_arrays(&a, &b, &ra, &taga, a_space, &rb, &tagb, b_space);
  get_bigint_buffer(ra.val.size + 1, &re);
  if(taga == BIGPOSINT_TAG && tagb == BIGPOSINT_TAG) {
    or_array(&(ra.val), &(rb.val), &(re.val), 0);
    tage = BIGPOSINT_TAG;
  }
  else {
    get_bigint_buffer(ra.val.size + 1, &rc);
    get_bigint_buffer(rb.val.size + 1, &rd);
    bv_not_array(&(ra.val), taga, &(rc.val), &tagc);
    bv_not_array(&(rb.val), tagb, &(rd.val), &tagd);
    bv_and_array(&(rc.val), tagc, &(rd.val), tagd, &(re.val), &tage);
    bv_not_array(&(re.val), tage, &(re.val), &tage);
    free_bigint_buffer(&rd);
    free_bigint_buffer(&rc);
  }
  return array_to_int(tage, &re);
}


/****************************************************************
 *			BV_XOR					*
 ****************************************************************
 * Return the bitwise xor of bit vectors a and b.		*
 ****************************************************************/

ENTITY bv_xor(ENTITY a, ENTITY b)
{
  BIGINT_BUFFER ra, rb, rc;
  int taga, tagb, tagc;
  intcell a_space[INTCELLS_IN_LONG], b_space[INTCELLS_IN_LONG];

  bv_get_arrays(&a, &b, &ra, &taga, a_space, &rb, &tagb, b_space);
  get_bigint_buffer(ra.val.size + 1, &rc);
  bv_xor_array(&(ra.val), taga, &(rb.val), tagb, &(rc.val), &tagc);
  return array_to_int(tagc, &rc);
}


/****************************************************************
 *			BV_MIN_ARRAY				*
 ****************************************************************
 * Return the index of the rightmost 1-bit in r, and set that	*
 * bit to 0.  If there is no 1, then return -1.			*
 ****************************************************************/

PRIVATE LONG bv_min_array(INTCELL_ARRAY *r)
{
  register LONG k;
  register LONG       r_size = r->size;
  register intcellptr r_buff = r->buff;

  for(k = 0; k < r_size; k++) {
    if(r_buff[k] != 0) {
      intcell t    = r_buff[k];
      intcell mask = 1;
      LONG    m    = INTCELL_BITS * k;

      while((t & mask) == 0) {m++; mask = mask << 1;}
      r_buff[k] &= ~mask;
      return m;
    }
  }
  return -1;
}


/****************************************************************
 *			BV_MAX_ARRAY				*
 ****************************************************************
 * Return the index of the leftmost 1-bit in r, and set that	*
 * bit to 0.  If there is no 1, then return -1.			*
 ****************************************************************/

PRIVATE LONG bv_max_array(INTCELL_ARRAY *r)
{
  register LONG k;
  register intcellptr rbuff = r->buff;

  for(k = r->size - 1; k >= 0; k--) {
    if(rbuff[k] != 0) {
      intcell t    = rbuff[k];
      intcell mask = 1 << (INTCELL_BITS - 1);
      LONG    m    = INTCELL_BITS * k + (INTCELL_BITS - 1);

      while((t & mask) == 0) {m--; mask = mask >> 1;}
      rbuff[k] &= ~mask;
      return m;
    }
  }
  return -1;
}


/*****************************************************************
 *			BV_MIN, BV_MAX				 *
 *****************************************************************
 * Bv_min(a) returns a pair (m,a') where m is the smallest index *
 * of a 1 bit in |a|, and a' is the result of setting that 1-bit *
 * to 0.  a' has the same sign as a.  Bv_max(a) is similar, but  *
 * finds the largest index of a 1-bit.				 *
 *****************************************************************/

PRIVATE ENTITY bv_min_max(ENTITY a, Boolean do_min)
{
  BIGINT_BUFFER ra;
  LONG m;
  int taga, tag;
  ENTITY new_a;

  int_to_array(a, &ra, &taga, 0, 0);
  if(taga == BIGNEGINT_TAG) bv_not_array(&(ra.val), taga, &(ra.val), &tag);

  m = (do_min ? bv_min_array : bv_max_array)(&(ra.val));
  if(m < 0) {
    failure = DOMAIN_EX;
    failure_as_entity =
      qwrap(DOMAIN_EX, make_str("Cannot extract 1 bit from zero bit vector"));
   free_bigint_buffer(&ra);
   return nil;
  }
  else {
    if(taga == BIGNEGINT_TAG) bv_not_array(&(ra.val), tag, &(ra.val), &tag);
    new_a  = array_to_int(taga, &ra);
    return ast_pair(ast_make_int(m), new_a);
  }
}

/*---------------------------------------------------------------*/

ENTITY bv_min(ENTITY a)
{
  return bv_min_max(a, TRUE);
}

/*---------------------------------------------------------------*/

ENTITY bv_max(ENTITY a)
{
  return bv_min_max(a, FALSE);
}


/****************************************************************
 *			BV_SET_BITS				*
 ****************************************************************
 * Suppose that info is pair(l,k).				*
 *								*
 * Return the result of setting to k the bits of a that are	*
 * at indices given by the members of list l.			*
 *								*
 * Both a and info must be fully evaluated before starting.     *
 ****************************************************************/

#define MAX_SET_BITS_HOLD_SIZE 32

ENTITY bv_set_bits(ENTITY a, ENTITY info)
{
  BIGINT_BUFFER ra;
  int taga, i, j;
  Boolean kk;
  ENTITY result, p, h, t, l, k;
  LONG largest, h_as_long, size, hold[MAX_SET_BITS_HOLD_SIZE];
  LONG l_time = LONG_MAX;
  REG_TYPE mark = reg3(&result, &p, &h);
  reg3(&t, &l, &k);
  reg1_param(&a);

  /*-----------------------------------------------------*
   * Get list l and the bit kk to set the given bits to. *
   *-----------------------------------------------------*/

  ast_split(info, &l, &k);
  kk = tobool(VAL(remove_indirection(k)));

  /*------------------------------------------------------------*
   * Copy the contents of list l, as long ints, into		*
   * array hold, and get the largest value.			*
   *								*
   * If the list is too long, so that there is not room in      *
   * array hold, then stop copying, but continue to scan the 	*
   * list to find out the largest value in the list.		*
   * If a member of list l is too large, fail and return.	*
   *								*
   * Even though l must be fully evaluated, we evaluate its     *
   * members just in case.  It is possible to encounter a bound *
   * unknown in a fully evaluated structure.			*
   *------------------------------------------------------------*/

  p       = l;
  i       = 0;
  largest = 0;
  IN_PLACE_EVAL(p, &l_time);

  while(!IS_NIL(p)) {
    ast_split(p, &h, &t);
    IN_PLACE_EVAL(h, &l_time);
    h_as_long = get_ival(h, MEM_EX);

    if(failure >= 0) {unreg(mark); return zero;}
    if(largest < h_as_long) largest = h_as_long;
    if(i < MAX_SET_BITS_HOLD_SIZE) hold[i] = h_as_long;
    i++;
    p = t;
    IN_PLACE_EVAL(p, &l_time);
  }

  /*------------------------------------------------------------*
   * Copy a to an array, making sure there is enough room	*
   * for all of the new bits.					*
   *------------------------------------------------------------*/

  size = (largest >> LOG_INTCELL_BITS) + 1;
  int_to_array(a, &ra, &taga, 0, size+1);
  if(size > ra.val.size) ra.val.size = size;

  /*------------------------------------------------------------*
   * If a is negative, then what we have in ra is the two's 	*
   * complement of the vector represented by a.  Get the 	*
   * vector in positive form.					*
   *------------------------------------------------------------*/

  if(taga == BIGNEGINT_TAG) {
    subtract_array(&(ra.val), &one_array, &(ra.val));
    not_array(&(ra.val), &(ra.val), 0);
  }

  /*------------------------------------------------------------*
   * If all of the members of l fit into array hold, then set	*
   * the bits from that array.					*
   *------------------------------------------------------------*/

  if(i <= MAX_SET_BITS_HOLD_SIZE) {
    for(j = 0; j < i; j++) {
      register LONG b = hold[j];
      if(kk) {
	ra.val.buff[b >> LOG_INTCELL_BITS] |=
	  1 << toint(b & (INTCELL_BITS - 1));
      }
      else {
	ra.val.buff[b >> LOG_INTCELL_BITS] &=
	 ~(1 << toint(b & (INTCELL_BITS - 1)));
      }
    }
  }

  /*------------------------------------------------------------*
   * If array hold is too small to hold the entire list l, then *
   * traverse l again.						*
   *------------------------------------------------------------*/

  else {
    l_time = LONG_MAX;
    p = l;
    IN_PLACE_EVAL(p, &l_time);
    while(TAG(p) != NOREF_TAG) {  /* while p != nil */
      ast_split(p, &h, &t);
      IN_PLACE_EVAL(h, &l_time);
      h_as_long = get_ival(h, MEM_EX);
      if(kk) {
	ra.val.buff[h_as_long >> LOG_INTCELL_BITS] |=
	  1 << toint(h_as_long & (INTCELL_BITS - 1));
      }
      else {
	ra.val.buff[h_as_long >> LOG_INTCELL_BITS] &=
	 ~(1 << toint(h_as_long & (INTCELL_BITS - 1)));
      }
      p = t;
      IN_PLACE_EVAL(p, &l_time);
    }
  }

  /*-------------------------------------------*
   * Convert back to an integer, and return.   *
   * If necessary, return to leading 1's form. *
   *-------------------------------------------*/

  if(taga == BIGNEGINT_TAG) {
    not_array(&(ra.val), &(ra.val), 0);
    add_array(&(ra.val), &one_array, &(ra.val));
  }
  result = array_to_int(taga, &(ra));
  unreg(mark);
  return result;
}


/****************************************************************
 *			BV_FIELD				*
 ****************************************************************
 * Return the subvector of vector v starting at bit n, and      *
 * having length m, where info is pair (n,m).			*
 ****************************************************************/

ENTITY bv_field(ENTITY v, ENTITY info)
{
  BIGINT_BUFFER r;
  int vtag;
  ENTITY n, m;
  LONG nn, mm, size, shift;

  ast_split(info, &n, &m);
  nn = get_ival(n, MEM_EX);
  mm = get_ival(m, MEM_EX);
  if(mm == 0 || failure >= 0) return zero;

  /*--------------------------------------------------------------------*
   * If v >= 0, then copy v into an array, but shift right by		*
   * floor(n/INTCELL_BITS) bits, and cut off length at			*
   * ceiling((m+n)/BITS_INT_CELL) bits. 				*
   *--------------------------------------------------------------------*/

  shift = nn >> LOG_INTCELL_BITS;
  size  = (mm + nn + (INTCELL_BITS - 1)) >> LOG_INTCELL_BITS;

  if(ast_sign(v) >= 0) {
    int_to_array(v, &r, &vtag, -shift, -size);
  }

  /*--------------------------------------------------------------------*
   * If v < 0, then copy v into an array, getting at least m+n bits.    *
   * Complement it to put it into positive form.  			*
   * Then shift right by the same number of digits as would have been   *
   * done for the case v >= 0.						*
   *--------------------------------------------------------------------*/

  else {
    int_to_array(v, &r, &vtag, 0, size);
    subtract_array(&(r.val), &one_array, &(r.val));
    not_array(&(r.val), &(r.val), 0);
    shift_right_digits(&(r.val), shift);
  }

  /*--------------------------------------------------------------------*
   * Now shift right by the remaining nn - shift*INTCELL_BITS bits.	*
   *--------------------------------------------------------------------*/

  right_shift(&(r.val), toint(nn - shift*INTCELL_BITS));

  /*------------------------------------------------------------*
   * Now mask out high order digit and return the result.	*
   * If mm is too large, then suppress the mask.		*
   *------------------------------------------------------------*/

  size = mm >> LOG_INTCELL_BITS;
  if(size < r.val.size) {
    int d = toint(mm & (INTCELL_BITS - 1));
    r.val.buff[size] &= lowmask[d];
    r.val.size = size + 1;
  }

  return array_to_int(BIGPOSINT_TAG, &r);
}


/****************************************************************
 *			BV_MASK					*
 ****************************************************************
 * Return a vector of nn 1's, with infinitely many leading 0's. *
 ****************************************************************/

ENTITY bv_mask(ENTITY nn)
{
  BIGINT_BUFFER r;
  int npatch;
  LONG ncells;

  LONG n = get_ival(nn, MEM_EX);
  if(failure >= 0 || n == 0) return zero;

  /*----------------------------------*
   * Get an array to hold the result. *
   *----------------------------------*/

  ncells = (n-1)/INTCELL_BITS + 1;
  get_bigint_buffer(ncells, &r);

  /*-------------------*
   * Fill it with 1's. *
   *-------------------*/

  longmemset(r.val.buff, 0xff, INTCELL_TO_BYTE(ncells));

  /*---------------------------------------------------------*
   * Partially fill the most significant cell, if necessary. *
   *---------------------------------------------------------*/

  npatch = toint(n % INTCELL_BITS);
  if(npatch != 0) {
    register intcell mask = 0;
    mask = ~((~mask) << npatch);
    r.val.buff[ncells-1] = mask;
  }

  /*-----------------*
   * Get the ENTITY. *
   *-----------------*/

  return array_to_int(BIGPOSINT_TAG, &r);
}


/****************************************************************
 *			BV_SHL					*
 ****************************************************************
 * Shift bit-vector a to the left b bits.  Both a and		*
 * b must be fully evaluated.					*
 ****************************************************************/

ENTITY bv_shl(ENTITY a, ENTITY b)
{
  LONG digit_shift, n;
  int bit_shift, tag;
  BIGINT_BUFFER r;

  /*-------------------------------------*
   * Get the shift amount as an integer. *
   *-------------------------------------*/

  n = get_ival(b, MEM_EX);
  if(failure >= 0) return zero;
  if(n == 0) return a;

  /*----------------------------------------------------------------*
   * See how many digits and how many bits within a digit to shift. *
   *----------------------------------------------------------------*/

  bit_shift   = toint(n % INTCELL_BITS);
  digit_shift = n / INTCELL_BITS;

  /*------------------------------------------------------------*
   * Put a into an array, doing the digit shift in the process. *
   *------------------------------------------------------------*/

  int_to_array(a, &r, &tag, digit_shift, 0);

  /*-------------------*
   * Do the bit shift. *
   *-------------------*/

  r.val.size++;
  left_shift(&(r.val), bit_shift);

  return array_to_int(tag, &r);
}


/****************************************************************
 *			BV_SHR					*
 ****************************************************************
 * Shift bit-vector a to the right b bits.  Both a and		*
 * b must be fully evaluated.					*
 ****************************************************************/

ENTITY bv_shr(ENTITY a, ENTITY b)
{
  LONG digit_shift, n;
  int s, bit_shift, tag;
  BIGINT_BUFFER r;

  s = ast_sign(a);

  /*------------------------------------------------------------*
   * Get the shift as an integer.  If the shift is very large,	*
   * the result is a vector of all 0's or all 1's, depending	*
   * on the sign of a.						*
   *------------------------------------------------------------*/

  n = get_ival(b, TEST_EX);
  if(failure >= 0) {
    failure = -1;
    return (s < 0) ? ENTI(-1) : zero;
  }
  if(n == 0) return a;

  /*-------------------------------------------------------------*
   * Get the number of intcells and the number of bits to shift. *
   *-------------------------------------------------------------*/

  digit_shift = n / INTCELL_BITS;
  bit_shift = toint(n % INTCELL_BITS);

  /*--------------------*
   * Put a in an array. *
   *--------------------*/

  int_to_array(a, &r, &tag, 0, 0);

  /*----------------------------------------------------------------*
   * If we are shifting to the right more digits than are in a, the *
   * result is either all 0's or all 1's, depending on the sign of  *
   * a.								    *
   *----------------------------------------------------------------*/

  if(r.val.size <= digit_shift) {
    return (s < 0) ? ENTI(-1) : zero;
  }

  /*------------------------------------------------------------*
   * Otherwise, do the shift first by digits, and then add the	*
   * remaining bits.  If a is negative, we need to do the shift *
   * on a correct form, not on the magnitude.  So subtract 1,   *
   * shift, and then add 1 in that case.			*
   *------------------------------------------------------------*/

  else {
    if(s < 0) subtract_array(&(r.val), &one_array, &(r.val));
    shift_right_digits(&(r.val), digit_shift);
    r.val.size -= digit_shift;
    right_shift(&(r.val), bit_shift);
    if(s < 0) add_array(&(r.val), &one_array, &(r.val));
    return array_to_int(tag, &r);
  }
}
