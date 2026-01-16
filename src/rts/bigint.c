/*************************************************************************
 * File:    rts/bigint.c
 * Purpose: Implement operations on integers
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
 * Integers are stored to base INT_BASE = 2^{INTCELL_BITS}.  		*
 * INTCELL_BITS is currently the same as the number of bits in a	*
 * USHORT.  By the term "digit", we mean one of the digits to base 	*
 * INT_BASE.  								*
 *									*
 * A big integer is stored as an entity with tag BIGPOSINT_TAG		*
 * or BIGNEGINT_TAG, holding a pointer to a binary chunk that holds	*
 * the integer as an array of digits.					*
 *									*
 * The arrays are stored with low order digits stored in low indices.	*
 *									*
 * This file is not concerned with arithemetic on ENTITYs.  File	*
 * number.c does that.  Instead, this file implements arithmetic on	*
 * arrays of digits, which are used by number.c and other files to	*
 * implement operations on ENTITYs.  This file also contains functions	*
 * for extracting the arrays from ENTITYs and for converting an array	*
 * to an ENTITY.							*
 *									*
 * An array is represented by one of two different structures.  	*
 *									*
 *  (1) When we are computing with an array, we need to know where the	*
 *      array starts, and how many digits it logically contains.  An	*
 *      INTCELL_ARRAY structure holds that information.  Hence, each	*
 *      function that uses an array holding an integer gets an		*
 *      INTCELL_ARRAY parameter.					*
 *									*
 *  (2) In order to keep track of a temporary array for allocation and	*
 *      freeing, we need to know information about where it came from,	*
 *      and how large it is (in terms of physical size).  A 		*
 *      BIGINT_BUFFER structure holds that information.  Such a		*
 *      buffer holds an INTCELL_ARRAY struct giving a pointer to the	*
 *      array and a logical size, plus a pointer to the chunk that 	*
 *      holds the array of digits.  The chunk holds the physical size,  *
 *      and can be used for deallocating the buffer.  See		*
 *	alloc/tempstr.c.						*
 ************************************************************************/

#include <memory.h>
#include <stdarg.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#ifdef DEBUG
# include "../show/prtent.h"
# include "../debug/debug.h"
#endif
#ifdef MSWIN
  void handlePendingMessages(void);
#endif


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			one_array				*
 ****************************************************************
 * one_array is an array that holds the number 1.  It should	*
 * not be changed.						*
 ****************************************************************/

PRIVATE intcell one_array_digit = 1;

INTCELL_ARRAY one_array = {&one_array_digit, 1};
  /* {buffer, logical size} */


/*===============================================================*
 * 		MISCELLANEOUS OPERATIONS			 *
 *===============================================================*/

/*===============================================================*
 *		ALLOCATING AND FREEING BUFFERS			 *
 *===============================================================*/

/****************************************************************
 *			GET_BIGINT_BUFFER			*
 ****************************************************************
 * Allocate a binary chunk of n intcells and put its		*
 * description into structure bf.				*
 *								*
 * (The physical size must be at least n, but the logical size  *
 * will be set to exactly n.)					*
 *								*
 * XREF: Used here, and in bitvec.c, number.c, numconv.c, 	*
 * prtnum.c, real.c -- all need to get temp buffers.		*
 ****************************************************************/

void get_bigint_buffer(LONG n, BIGINT_BUFFER *bf)
{
  register CHUNKPTR chunk = allocate_bigint_array(n);

  bf->chunk    = chunk;
  bf->val.buff = BIGINT_BUFF(chunk);
  bf->val.size = n;
}


/****************************************************************
 *			GET_TEMP_BIGINT_BUFFER			*
 ****************************************************************
 * Allocate a binary chunk of at least n intcells and put its	*
 * description into structure bf.				*
 *								*
 * (The physical size must be at least n, but the logical size  *
 * will be set to exactly n.)					*
 *								*
 * This function is the same as get_bigint_buffer, but it       *
 * allocates the buffer with a size that is an exact size of    *
 * a binary chunk so that, when it is put back, it will go into *
 * a free space list that allows use of all of the bytes in the *
 * chunk.  This function should be used to allocate chunks that *
 * will be put back shortly.					*
 ****************************************************************/

void get_temp_bigint_buffer(LONG n, BIGINT_BUFFER *bf)
{
  LONG size;

  /*-----------------------------------------------------------*
   * If n is too large, then leave alone, since the array      *
   * will be allocated as a huge array anyway, and is not      *
   * managed by the storage manager in the same way as small   *
   * arrays.  Otherwise, bump the physical size up to the next *
   * size in free_binary_data.    			       *
   *-----------------------------------------------------------*/

  if(n + SHORT_BYTES > BINARY_BLOCK_GRAB_SIZE) size = n;
  else {
    register int i;
    for(i = 0;
        free_binary_data[i].free_chunks_size < n;
        i++) {
      /* Nothing */
    }
    size = free_binary_data[i].free_chunks_size;
  }

  /*----------------------------------------------*
   * Now do the allocation and set up the header. *
   *----------------------------------------------*/

  {register CHUNKPTR chunk = allocate_bigint_array(size);
   bf->chunk    = chunk;
   bf->val.buff = BIGINT_BUFF(chunk);
   bf->val.size = n;
  }
}


/****************************************************************
 *			FREE_BIGINT_BUFFER			*
 ****************************************************************
 * Free buffer buff.						*
 ****************************************************************/

void free_bigint_buffer(BIGINT_BUFFER *buff)
{
  free_chunk(buff->chunk);
}


/****************************************************************
 *			FREE_IF_DIFF				*
 ****************************************************************
 * If x is a large integer or a large real with a large		*
 * large integer mantissa, then free the chunk to which x or	*
 * its mantissa refers provided it is not the same chunk that	*
 * is referred to by any of the other parameters or their	*
 * mantissas.							*
 *								*
 * nchks is the number of optional parameters, each of which    *
 * must have type ENTITY.  For example, to free x provided it   *
 * does not share memory with y, use				*
 *								*
 *  free_if_diff(x, 1, y);					*
 ****************************************************************/

void free_if_diff(ENTITY x, int nchks, ...)
{
  va_list ap;
  CHUNKPTR xchunk;
  int k;
  int xtag = TAG(x);

  /*-------------------------------------------------------*
   * Get the chunk to which x or its mantissa refers.  If  *
   * none, then return.					   *
   *-------------------------------------------------------*/

  if(xtag == LARGE_REAL_TAG) {
    x    = LRVAL(x)->man;
    xtag = TAG(x);
  }

  if(xtag != BIGPOSINT_TAG && xtag != BIGNEGINT_TAG) return;

  xchunk = BIVAL(x);

  /*------------------------------------------------------------*
   * Check whether x refers to the same chunk as any of the 	*
   * check parameters.  If it does, then return.		*
   *------------------------------------------------------------*/

  va_start(ap, nchks);
  for(k = 0; k < nchks; k++) {
    ENTITY c = va_arg(ap, ENTITY);
    int ctag = TAG(c);

     if(ctag == LARGE_REAL_TAG) {
       c    = LRVAL(c)->man;
       ctag = TAG(c);
     }

     if(ctag == BIGPOSINT_TAG || ctag == BIGNEGINT_TAG) {
        if(BIVAL(c) == xchunk) {
	  va_end(ap);
	  return;
	}
     }
  }
  va_end(ap);

  /*---------------------------------------------*
   * If we get here, then we can free x's chunk. *
   *---------------------------------------------*/

  free_chunk(xchunk);
}



/*===============================================================*
 *		CONVERSION TO AND FROM ARRAYS			 *
 *===============================================================*/

/****************************************************************
 *			SMALL_ARRAY_TO_INT			*
 ****************************************************************
 * This function is similar to array_to_int, below, but it      *
 * should only be used when the array is small enough that	*
 * it can be stored as an INT_TAG entity.			*
 *								*
 * This function will try to store the result into variable	*
 * *result.  On success, it returns TRUE.  If the array turns   *
 * out to be too large, FALSE is returned.			*
 *								*
 * sz is one less than the number of digits in this buffer.  It	*
 * is acceptable for sz to be negative.  That indicates that    *
 * the result is 0.						*
 ****************************************************************/

PRIVATE Boolean 
small_array_to_int(int tag, intcellptr buff, int sz, ENTITY *result)
{
  register LONG val;

  /*-------------------------------*
   * When sz < 0, the result is 0. *
   *-------------------------------*/

  if(sz < 0) {
    *result = zero;
    return TRUE;
  }

  /*------------------------------------------------------------*
   * There are two versions, depending on how many intcells fit *
   * into a LONG.  Loops are unwound for efficiency.		*
   *------------------------------------------------------------*/

# if (INTCELLS_IN_LONG == 2)

    val = buff[0];
    if(sz == 1) {
      if(buff[1] > SMALL_INT_HIGH_MAX) return FALSE;
      val += tolong(buff[1]) << INTCELL_BITS;
    }
# else /* INTCELLS_IN_LONG == 4 */
    switch(sz) {
      case 0: 
	val = buff[0];
	break;
      case 1: 
	val = buff[0] + (tolong(buff[1]) << INTCELL_BITS);
	break;
      case 2: 
        val = buff[0] + (tolong(buff[1]) << INTCELL_BITS)
	      + (tolong(buff[2]) << 2*INTCELL_BITS);
	break;
      case 3: 
	if(buff[3] > SMALL_INT_HIGH_MAX) return FALSE;
	val = buff[0] + (tolong(buff[1]) << INTCELL_BITS)
	      + (tolong(buff[2]) << 2*INTCELL_BITS)
	      + (tolong(buff[3]) << 3*INTCELL_BITS);
    }
# endif
  if(tag == BIGNEGINT_TAG) val = -val;

  *result = ENTI(val);
  return TRUE;
}


/****************************************************************
 *			ARRAY_TO_INT				*
 ****************************************************************
 * Return an entity equal to the value in buffer bf, 		*
 * with tag tag.  Tag should be either BIGPOSINT_TAG or 	*
 * BIGNEGINT_TAG, and indicates the sign.			*
 *								*
 * (If the result is a small number, its tag will be INT_TAG,	*
 * not BIGPOSINT_TAG or BIGNEGINT_TAG.  Nonetheless, the value	*
 * of parameter tag must be either BIGPOSINT_TAG or		*
 * BIGNEGINT_TAG.						*
 *								*
 * Array_to_int is allowed to use the chunk in buffer bf for 	*
 * the content of the result entity.  It is also allowed to	*
 * free that chunk, and use other storage for the entity.	*
 ****************************************************************/

ENTITY array_to_int(int tag, BIGINT_BUFFER *bf)
{
  LONG chunk_size;

  /*-----------------------------------*
   * buff points to the array.  i is   *
   * the index of the leftmost digit.  *
   *-----------------------------------*/

  LONG                n        = bf->val.size;
  register LONG       i        = n - 1;
  register intcellptr buff     = bf->val.buff;
           CHUNKPTR   bf_chunk = bf->chunk;

  /*----------------------------------------*
   * Skip over zero-digits at the left end. *
   *----------------------------------------*/

  while(i >= 0 && buff[i] == 0) i--;

  /*-----------------------------------------------------------------*
   * Check if this integer is small.  If so, return a small integer. *
   * If the result is a small integer, then delete bf->chunk.	     *
   *-----------------------------------------------------------------*/

  if(i < INTCELLS_IN_LONG) {
    ENTITY result;
    if(small_array_to_int(tag, buff, i, &result)) {
      free_chunk(bf_chunk);
      return result;
    }
  }

  /*------------------------------------------------------------*
   * If at most half of the bytes in the chunk are occupied,    *
   * then move this integer to a smaller chunk.			*
   *------------------------------------------------------------*/

  chunk_size = BIGINT_SIZE(bf_chunk);
  if(i < chunk_size >> 1) {
    register CHUNKPTR newchunk = allocate_bigint_array(i+1);
    longmemcpy(binary_chunk_buff(newchunk),
	       binary_chunk_buff(bf_chunk),
	       INTCELL_TO_BYTE(i+1));
    free_chunk(bf_chunk);
    bf_chunk = newchunk;
  }

  /*------------------------------------------------------------*
   * If the old chunk is to be used, be sure that unused bytes  *
   * at the left end of the chunk, beyond n digits, are         *
   * zeroed out.						*
   *------------------------------------------------------------*/

  else if(chunk_size > n) {
    longmemset(binary_chunk_buff(bf_chunk) + INTCELL_TO_BYTE(n), 0,
	       INTCELL_TO_BYTE(chunk_size - n));
  }

  /*---------------------*
   * Return the integer. *
   *---------------------*/

  return ENTP(tag, bf_chunk);
}


/****************************************************************
 *			NORMALIZE_ARRAY_TO_INT			*
 ****************************************************************
 * Convert buffer aa to an integer, but round and normalize to  *
 * keep the most significant digits.  a_tag tells the sign, and *
 * must be either BIGPOSINT_TAG or BIGNEGINT_TAG.		*
 *								*
 * Truncate to prec digits, with digits on the right being 	*
 * removed.							*
 *								*
 * Return the resulting integer. Put, in *ex, the exponent 	*
 * to shift by to restore the result to the parameter value,	*
 * with truncated digits zeroed out.    			*
 *								*
 * If a is the integer stored in array aa, and result is the	*
 * integer returned, then 					*
 *								*
 *      a ~= result * BASE^(*ex) 				*
 *								*
 * at the end, with the approximation induced by the truncation.*
 ****************************************************************/

ENTITY normalize_array_to_int(BIGINT_BUFFER *aa, int a_tag,
			      LONG prec, LONG *ex)
{
  LONG n, digits, chunk_size;
  CHUNKPTR aa_chunk;
  register LONG i, shift;
  register intcellptr buff;

  /*----------------------------------------*
   * Skip over zero digits at the left end. *
   *----------------------------------------*/

  n    = aa->val.size;
  i    = n - 1;
  buff = aa->val.buff;
  while(i >= 0 && buff[i] == 0) i--;

  /*----------------------------------------------------*
   * Skip over zero digits at the right end. They are 	*
   * accounted for by the shift.			*
   *----------------------------------------------------*/

  shift = 0;
  while(shift < i && buff[shift] == 0) shift++;

  /*--------------------------------------*
   * We want min(prec, i-shift+1) digits. *
   *--------------------------------------*/

  digits = i - shift + 1;
  if(digits > prec) {
    shift  += digits - prec;
    digits  = prec;
  }
  *ex = shift;

  /*-----------------------------------------------------------------*
   * Check if this integer is small.  If so, return a small integer. *
   *-----------------------------------------------------------------*/

  if(digits <= INTCELLS_IN_LONG) {
    ENTITY result;
    if(small_array_to_int(a_tag, buff + shift, digits - 1, &result)) {
      free_chunk(aa->chunk);
      return result;
    }
  }

  /*-----------------------------------------------------*
   * If the shift is positive or at most half of what is *
   * available in aa->chunk is used, then move to a 	 *
   * smaller chunk.		 			 *
   *-----------------------------------------------------*/

  aa_chunk   = aa->chunk;
  chunk_size = BIGINT_SIZE(aa_chunk);
  if(shift > 0 || digits <= chunk_size >> 1) {
    register CHUNKPTR newchunk = allocate_bigint_array(digits);
    longmemcpy(binary_chunk_buff(newchunk),
	       binary_chunk_buff(aa_chunk) + INTCELL_TO_BYTE(shift),
	       INTCELL_TO_BYTE(digits));
    free_chunk(aa_chunk);
    aa_chunk = newchunk;
  }

  /*----------------------------------------------------*
   * If the original chunk is used, then be sure that   *
   * unused digits are zeroed.				*
   *----------------------------------------------------*/

  else if(chunk_size > n) {
    longmemset(binary_chunk_buff(aa_chunk) + INTCELL_TO_BYTE(n), 0,
	       INTCELL_TO_BYTE(chunk_size - n));
  }

  /*---------------------*
   * Return the integer. *
   *---------------------*/

  return ENTP(a_tag, aa_chunk);
}


/****************************************************************
 *			INSTALL_INT				*
 ****************************************************************
 * Install val into intcell buffer buf.  val must be		*
 * nonnegative.							*
 ****************************************************************/

void install_int(LONG val, intcellptr buf)
{
# if (INTCELLS_IN_LONG == 2) 
    buf[0] = val & INTCELL_MASK;
    buf[1] = val >> INTCELL_BITS;
# else
    buf[0] = val & INTCELL_MASK;
    val = val >> INTCELL_BITS;
    buf[1] = val & INTCELL_MASK;
    val = val >> INTCELL_BITS;
    buf[2] = val & INTCELL_MASK;
    buf[3] = val >> INTCELL_BITS;
# endif
}


/****************************************************************
 *			INSTALL_INT_LIMITED			*
 ****************************************************************
 * Install val into intcell buffer buf.  val must be		*
 * nonnegative.	 Install a maximum of n intcells into buf.	*
 ****************************************************************/

PRIVATE void install_int_limited(LONG val, intcellptr buf, int n)
{
  register int i;

  if(n > INTCELLS_IN_LONG) n = INTCELLS_IN_LONG;

  for(i = 0; i < n; i++) {
    buf[i] = (intcell)(val & INTCELL_MASK);
    val = val >> INTCELL_BITS;
  }
}


/****************************************************************
 *			CONST_INT_TO_ARRAY			*
 ****************************************************************
 * Entity x must be an integer.  This function installs into	*
 * structure *r an array of digits holding |x|.  This array     *
 * should not be written into -- it is read only.		*
 *								*
 * If x is a large integer, then put the chunk pointer in x  	*
 * refers into structure r.					*
 *								*
 * If x is a small integer, then use buffer small_buff to hold  *
 * x.  small_buff must be allocated by the caller -- it is      *
 * not allocated here.  small_buff should hold INTCELLS_IN_LONG *
 * intcells.							*
 *								*
 * In either case, put a tag indicating the sign of x (either	*
 * BIGPOSINT_TAG or BIGNEGINT_TAG) into *tag.			*
 ****************************************************************/

void const_int_to_array(ENTITY x, BIGINT_BUFFER *r, int *tag, 
			intcellptr small_buff)
{
  if(TAG(x) == INT_TAG) {
    register LONG xval = IVAL(x);

    if(xval < 0) {
      xval = -xval;
      *tag = BIGNEGINT_TAG;
    }
    else *tag = BIGPOSINT_TAG;

    r->val.buff = small_buff;
    r->val.size = INTCELLS_IN_LONG;
    r->chunk    = NULL;
    install_int(xval, small_buff);
  }
  else {
    register CHUNKPTR chunk = BIVAL(x);
    r->chunk    = chunk;
    r->val.buff = BIGINT_BUFF(chunk);
    r->val.size = BIGINT_SIZE(chunk);
    *tag = TAG(x);
  }

  /*-----------------------*
   * Skip over leading 0's *
   *-----------------------*/

  {register LONG       k    = r->val.size;
   register intcellptr buff = r->val.buff;
   while(k > 1 && buff[k-1] == 0) k--;
   r->val.size = k;
  }
}


/****************************************************************
 *			INT_TO_ARRAY				*
 ****************************************************************
 * Copy a * BASE^shft into a newly allocated chunk, and put a	*
 * description of that chunk in structure r.  If shft is	*
 * negative, then the shift is to the right.	 		*
 *								*
 * Set *tag to the tag for r (BIGPOSINT_TAG or BIGNEGINT_TAG).	*
 *								*
 * If size >= 0, then at least size+1 cells will be allocated.	*
 * However, the nominal size, found in r->val.size, will be the	*
 * size necessary to store the integer, even if extra cells	*
 * were allocated to reach size+1 total.  All extra cells are	*
 * to the left of the array.  (That is, they form higher order	*
 * digits.)  							*
 *								*
 * If size < 0, than at most -size digits will be copied into	*
 * the array.  r->val.size will be the number of digits copied.	*
 *								*
 * If n is the number of digits that are necessary to store the *
 * number, so that r->val.size = n, then r->val.buff[n] is	*
 * guaranteed to exist and to hold 0. 				*
 *								*
 * a must be fully evaluated before calling int_to_array.	*
 ****************************************************************/

void int_to_array(ENTITY a, BIGINT_BUFFER *r, int *tag,
		  LONG shft, LONG size)
{
  LONG nr, truesize;
  int a_tag;
  intcellptr rp;

  a_tag = TAG(a);
  if(!MEMBER(a_tag, int_tags)) die(81);

  /*---------------------------------------------------------------*
   * Get the number of cells needed. 				   *
   *								   *
   * For a small integer (tag INT_TAG), we need INTCELLS_IN_LONG   *
   * digits for the integer, one more for r.val.buff[n], plus 	   *
   * shft for the shift.					   *
   *								   *
   * For a big integer, we need as many digits as are held in the  *
   * number's buffer, one for r->val.buff[n],  			   *
   * plus shft for the shift.		   			   *
   *								   *
   * In any event, get at least 3 digits.  nr is the number of	   *
   * digits required (so the nominal size if nr-1), and truesize   *
   * is the actual number of digits allocated, which might be more *
   * than nr.							   *
   *---------------------------------------------------------------*/

  nr = (a_tag == INT_TAG)
         ? shft + (INTCELLS_IN_LONG + 1)
	 : shft + BIGINT_SIZE(BIVAL(a)) + 1;
  if(nr < 3) nr = 3;
  if(size < 0) {
    if(nr > -size + 1) nr = -size + 1;
    truesize = nr;
  }
  else truesize = max(nr, size+1);

  /*---------------------------------------------------------------*
   * Allocate the array.  Zero out r->val.buff[n] (which is 	   *
   * rp[nr-1]). Also zero out the shft rightmost digits for the	   *
   * shift, if shft is positive, and zero out any padding on the   *
   * left.							   *
   *---------------------------------------------------------------*/

  get_bigint_buffer(truesize, r);
  rp          = r->val.buff;
  rp[nr-1]    = 0;
  if(shft > 0) longmemset((charptr)rp, 0, INTCELL_TO_BYTE(shft));
  if(nr < truesize) {
    longmemset((charptr)(rp + nr), 0, INTCELL_TO_BYTE(truesize - nr));
  }

  /*--------------------------------------*
   * Copy a small integer into the array. *
   *--------------------------------------*/

  if(a_tag == INT_TAG) {

    /*---------------------------------------------*
     * Get the absolute value of a, as a long int, *
     * and set a_tag to the value to set *tag to.  *
     * (*tag is set below.)			   *
     *---------------------------------------------*/

    LONG val = IVAL(a);
    if(val >= 0) a_tag = BIGPOSINT_TAG;
    else {
      a_tag = BIGNEGINT_TAG;
      val = -val;
    }

    /*-----------------------------------------*
     * Install in the array, and set the size. *
     *-----------------------------------------*/

    if(shft >= 0) {
      install_int_limited(val, rp+shft, nr-shft-1);
    }
    else /* shft < 0 */ {
      int shift_count = -shft;
      val = (shift_count >= INTCELLS_IN_LONG)
              ? 0
	      : val >> shift_count*INTCELL_BITS;
      install_int_limited(val, rp, nr-1);
    }
  }

  /*------------------------------------*
   * Copy a big integer into the array. *
   *------------------------------------*/

  else {
    register CHUNKPTR  a_chunk = BIVAL(a);
    intcellptr         a_buff  = BIGINT_BUFF(a_chunk);
    LONG               a_size  = BIGINT_SIZE(a_chunk);

    register LONG copy_digits;
    intcellptr to_buff, from_buff;
   
    /*-------------------------------------------------------------------*
     * When shft is negative, skip over the rightmost shft digits.	*
     * We need to copy min(nr-1, a_size + shft) digits from              *
     * a's buffer to the new buffer.  If size < 0, then we must	        *
     * copy at most -size digits.					*
     *-------------------------------------------------------------------*/

    if(shft < 0) {
      copy_digits = nr-1;
      if(copy_digits > a_size + shft) copy_digits = a_size + shft;
      to_buff   = rp;
      from_buff = a_buff - shft;
    }

    /*-----------------------------------------------------------*
     * When shft is nonnegative, we write the contents of a's    *
     * buffer shifted over by shft digits.  The number of digits *
     * to copy is min(nr - shft - 1, a_size).			*
     *-----------------------------------------------------------*/

    else /* shft >= 0 */ {
      copy_digits = nr - shft - 1;
      if(copy_digits > a_size) copy_digits = a_size;
      to_buff   = rp + shft;
      from_buff = a_buff;
    }

    if(size < 0 && copy_digits > -size) copy_digits = -size;
   
    /*--------------*
     * Do the copy. *
     *--------------*/
    
    if(copy_digits > 0) {
      longmemcpy(to_buff, from_buff, INTCELL_TO_BYTE(copy_digits));
    }
  }

  /*-----------------------------------------------------*
   * Skip over zeros at the left-hand end of the number. *
   * Remember that rp[nr-1] is the padding digit, so the *
   * leftmost genuine digit is rp[nr-2].		 *
   *-----------------------------------------------------*/

  while(nr > 2 && rp[nr-2] == 0) nr--;

  r->val.size = nr - 1;
  *tag = a_tag;
}


/*======================================================================*
 *			ARITHMETIC, COMPARISON				*
 *======================================================================*/


/****************************************************************
 *			COMPARE_ARRAY				*
 ****************************************************************
 * Return							*
 *								*
 *   -1 if a < b,                                               *
 *   0  if a = b,                                               *
 *   1  if a > b.                                               *
 *								*
 * a and b are integers stored as intcell arrays.		*
 ****************************************************************/

int compare_array(const INTCELL_ARRAY *a, const INTCELL_ARRAY *b)
{
  register LONG i;
  LONG n;
  LONG       a_size = a->size;
  LONG       b_size = b->size;
  intcellptr a_buff = a->buff;
  intcellptr b_buff = b->buff;

  n = a_size;
  if(n < b_size) n = b_size;

  for(i = n - 1; i >= 0; i--) {
    register intcell da = (i >= a_size) ? 0 : a_buff[i];
    register intcell db = (i >= b_size) ? 0 : b_buff[i];
    if(da < db) return -1;
    if(da > db) return 1;
  }
  return 0;
}


/****************************************************************
 *			NOT_ARRAY				*
 ****************************************************************
 * Form the one's complement of array a.		  	*
 * The result goes in array b, which can be a if desired.	*
 * The bytes to the left of a are presumed to hold pad, which	*
 * must be either 0 or INTCELL_MAX.				*
 ****************************************************************/

void not_array(const INTCELL_ARRAY *a, const INTCELL_ARRAY *b, intcell pad)
{
  register LONG i;
  LONG b_size;

           LONG       a_size = a->size;
  register intcellptr a_buff = a->buff;
  register intcellptr b_buff = b->buff;

  for(i = 0; i < a_size; i++) b_buff[i] = ~a_buff[i];
  
  b_size = b->size;
  pad    = ~pad;
  for(; i < b_size; i++) b_buff[i] = pad;
}


/****************************************************************
 *			OR_ARRAY				*
 ****************************************************************
 * Form a or b (bitwise).  Leave the result in c.  Array c	*
 * can be the same as array a, or have the same buffer as	*
 * array b.							*
 *								*
 * a must be the larger of the two arrays. That part of array	*
 * b that is missing is presumed to hold 'pad' in each intcell. *
 * pad must be either 0 or INTCELL_MAX. 			*
 *								*
 * Any part of c to the left of all of a is set to pad.		*
 ****************************************************************/

void or_array(const INTCELL_ARRAY *a, const INTCELL_ARRAY *b, 
	      const INTCELL_ARRAY *c, intcell pad)
{
  register LONG i;
  LONG a_size;

  register intcellptr a_buff = a->buff;
  register intcellptr b_buff = b->buff;
  register intcellptr c_buff = c->buff;
           LONG       b_size = b->size;

  for(i = 0; i < b_size; i++) c_buff[i] = a_buff[i] | b_buff[i];

  a_size = a->size;
  if(pad != 0) {
    for(i = b_size; i < a_size; i++) c_buff[i] = pad;
  }
  else if(c != a) {
    for(i = b_size; i < a_size; i++) c_buff[i] = a_buff[i];
  }

  if(c->size > a_size) {
    longmemset((charptr)(c_buff + a_size), 
	       pad & 0xff, 
	       INTCELL_TO_BYTE(c->size - a_size));
  }
}


/****************************************************************
 *			AND_ARRAY				*
 ****************************************************************
 * Form a and b (bitwise).  Leave the result in array c, which	*
 * can be the same as a or the same as b.			*
 *								*
 * a must be the larger of the two arrays. That part of array	*
 * b that is missing is presumed to  hold 'pad' in each 	*
 * intcell.  pad must be either 0 or INTCELL_MAX.		*
 *								*
 * Any part of c to the left of all of a is set to 0.		*
 ****************************************************************/

void and_array(const INTCELL_ARRAY *a, const INTCELL_ARRAY *b, 
	       const INTCELL_ARRAY *c, intcell pad)
{
  register LONG i;
  LONG a_size;

  register intcellptr a_buff = a->buff;
  register intcellptr b_buff = b->buff;
  register intcellptr c_buff = c->buff;
           LONG       b_size = b->size;

  for(i = 0; i < b_size; i++) c_buff[i] = a_buff[i] & b_buff[i];

  a_size = a->size;
  if(pad == 0) {
    for(i = b_size; i < a_size; i++) c_buff[i] = 0;
  }
  else if(c != a) {
    for(i = b_size; i < a_size; i++) c_buff[i] = a_buff[i];
  }

  if(c->size > a_size) {
    longmemset((charptr)(c_buff + a_size), 
	       0, 
	       INTCELL_TO_BYTE(c->size - a_size));
  }
}


/****************************************************************
 *			XOR_ARRAY				*
 ****************************************************************
 * Form a and b (bitwise).  Leave the result in c, which can	*
 * be the same as a or b.					*
 *								*
 * a must be the larger of the two arrays.  That part of array	*
 * b that is missing is presumed to hold 'pad' in each intcell. *
 * pad must be either 0 or INTCELL_MAX.				*
 *								*
 * Any part of c to the left of all of a is set to 0.		*
 ****************************************************************/

void xor_array(const INTCELL_ARRAY *a, const INTCELL_ARRAY *b, 
	       const INTCELL_ARRAY *c, intcell pad)
{
  register LONG i;
  LONG a_size;
  register intcellptr a_buff = a->buff;
  register intcellptr b_buff = b->buff;
  register intcellptr c_buff = c->buff;
           LONG       b_size = b->size;

  for(i = 0; i < b_size; i++) c_buff[i] = a_buff[i] ^ b_buff[i];

  a_size = a->size;
  if(pad != 0) {
    for(i = b_size; i < a_size; i++) c_buff[i] = ~a_buff[i];
  }
  else if(c != a) {
    for(i = b_size; i < a_size; i++) c_buff[i] = a_buff[i];
  }

  if(c->size > a_size) {
    longmemset((charptr)(c_buff + a_size),
	       pad & 0xff, 
	       INTCELL_TO_BYTE(c->size - a_size));
  }

}


/****************************************************************
 *			ADD_ARRAY				*
 ****************************************************************
 * Add b to a, leaving result in c.  Array c can be the same	*
 * as array a.			 				*
 *								*
 * It is required that a.size >= b.size.  Also, add_array might *
 * need to use c.buff[a.size] for the carry out of the high	*
 * bit, so that cell should exist.  It is permissible for a and *
 * b to be the same array.					*
 ****************************************************************/

void add_array(const INTCELL_ARRAY *a, const INTCELL_ARRAY *b, 
	       const INTCELL_ARRAY *c)
{
  register LONG i, t;
  int carry;
  intcellptr abuff = a->buff;
  intcellptr bbuff = b->buff;
  intcellptr cbuff = c->buff;
  LONG       bsize = b->size;

  carry = 0;

  /*----------------------------------------------------------*
   * First add those parts of a and b where both have digits. *
   *----------------------------------------------------------*/

  for(i = 0; i < bsize; i++) {
    t = tolong(abuff[i]) + tolong(bbuff[i]) + carry;
    carry = 0;
    if(t > INTCELL_MAX) {
      carry = 1;
      t -= INTCELL_MAX + 1;
    }
    cbuff[i] = (intcell) t;
  }

  /*----------------------------------------------------------------*
   * Propogate the carry through the part of a beyond the end of b. *
   *----------------------------------------------------------------*/

  {register LONG asize = a->size;
   for(i = bsize; i < asize && carry != 0; i++) {
     t = tolong(abuff[i]) + carry;
     carry = 0;
     if(t > INTCELL_MAX) {
       carry = 1;
       t -= INTCELL_MAX + 1;
     }
     cbuff[i] = (intcell) t;
   }

   /*---------------------------------------*
    * Copy the rest of a into c, if a and   *
    * c are different buffers.		    *
    *---------------------------------------*/

   if(c != a) {
     for(; i < asize; i++) cbuff[i] = abuff[i];
   }

   /*------------------------------------------------------*
    * If there is a carry out, then we need to install it. *
    * Also zero out any extra digits in c.                 *
    *------------------------------------------------------*/   
   
   {register long csize = c->size;
    if(asize < csize) {
      cbuff[asize] = carry;
      for(i = asize + 1; i < csize; i++) cbuff[i] = 0;
    }
    else if(carry != 0) die(165);
   }
  }
}


/****************************************************************
 *			SUBTRACT_ARRAY				*
 ****************************************************************
 * Subtract b from a, leaving result in c.  Array c can be	*
 * the same as array a.			 			*
 *								*
 * It must be the case that a >= b, when both are taken as	*
 * numbers.							*
 ****************************************************************/

void subtract_array(const INTCELL_ARRAY *a, const INTCELL_ARRAY *b, 
		    const INTCELL_ARRAY *c)
{
  register LONG i, t;
  int borrow;
  intcellptr abuff = a->buff;
  intcellptr bbuff = b->buff;
  intcellptr cbuff = c->buff;
  LONG       bsize = b->size;

  borrow = 0;

  /*---------------------------------------------------*
   * First do the part where a and b both have digits. *
   *---------------------------------------------------*/

  for(i = 0; i < bsize; i++) {
    t = tolong(abuff[i]) - tolong(bbuff[i]) - borrow;
    borrow = 0;
    if(t < 0) {
      borrow = 1;
      t += INTCELL_MAX + 1;
    }
    cbuff[i] = (intcell) t;
  }

  /*-----------------------------------*
   * Propogate borrow beyond end of b. *
   *-----------------------------------*/

  {register LONG asize = a->size;
   for(i = bsize; i < asize && borrow != 0; i++) {
     t = tolong(abuff[i]) - borrow;
     borrow = 0;
     if(t < 0) {
       borrow = 1;
       t += INTCELL_MAX + 1;
     }
     cbuff[i] = (intcell) t;
   }

   /*---------------------------------------*
    * Copy the rest of a into c, if a and   *
    * c are different buffers.		    *
    *---------------------------------------*/

   if(c != a) {
     for(; i < asize; i++) cbuff[i] = abuff[i];
   }

   /*---------------------------------*
    * Zero out any extra digits in c. *
    *---------------------------------*/   
   
   {register long csize = c->size;
    for(i = asize; i < csize; i++) cbuff[i] = 0;
   }
  }
}


/****************************************************************
 *			LEFT_SHIFT				*
 ****************************************************************
 * Shift array a left k bits, where 0 <= k < INTCELL_BITS.	*
 * The low order bits of a are at the right end.		*
 * Any shift off the left end will be lost.  Shift in zero 	*
 * bits on the right end.  					*
 ****************************************************************/

void left_shift(const INTCELL_ARRAY *a, int k)
{
  register LONG j;
  register intcellptr abuff;

  if(k == 0) return;

  abuff = a->buff;

  /*------------------------------------------------------------*
   * Do the shift from left to right. The low order bits are	*
   * at the right end.						*
   *------------------------------------------------------------*/

  for(j = a->size - 1; j > 0; j--) {
    register intcellptr c = abuff + j;

    /*------------------------------------------------------------------*
     * Let b = INTCELL_BITS.  Get the low order (b-k) bits of c[0],     *
     * shifted left k bits, and the high order k bits of c[-1], shifted *
     * right (b-k) bits.  Shift right one bit first, and zero out the	*
     * sign bit, to ensure that 0's are brought in at the left end in	*
     * the right sifht.							*
     *------------------------------------------------------------------*/

    *c = (*c << k) |
       (((c[-1] >> 1) & INTCELL_MASK1) >> ((INTCELL_BITS - 1) - k));
  }

  /*-------------------------------------------------------*
   * Finish up by shifting the rightmost cell left k bits. *
   *-------------------------------------------------------*/

  abuff[0] = (abuff[0] << k);
}


/****************************************************************
 *			RIGHT_SHIFT				*
 ****************************************************************
 * Shift array a right k bits, where 0 <= k < INTCELL_BITS.     *
 * The low order bits of a are at the right end.		*
 * Any shift off the right end will be lost.			*
 ****************************************************************/

void right_shift(const INTCELL_ARRAY *a, int k)
{
  register LONG j;
  register intcellptr c;
  register intcellptr abuff;
  register LONG asize;

  if(k == 0) return;

  abuff = a->buff;
  asize = a->size;

  /*----------------------------------------------------*
   * Do the shift from right to left. The low order end *
   * of the number is the right end.                    *
   *----------------------------------------------------*/

  for(j = 0; j < asize - 1; j++) {
    c  = abuff + j;

    *c = (c[1] << (INTCELL_BITS - k)) |
      (((*c >> 1) & INTCELL_MASK1) >> (k - 1));
  }
  c  = abuff + asize - 1;
  *c = ((*c >> 1) & INTCELL_MASK1) >> (k-1);
}


/****************************************************************
 *			SHIFT_LEFT_DIGITS			*
 ****************************************************************
 * Shift array a to the left s digits, padding on the right 	*
 * with zeros.							*
 ****************************************************************/

void shift_left_digits(const INTCELL_ARRAY *a, LONG s)
{
  register LONG i;
  register intcellptr p;
  register LONG a_size = a->size;

  for(p = a->buff + a_size + s - 1, i = 0; i < a_size; p--, i++) *p = p[-s];
  for(p = a->buff, i = 0; i < s; p++, i++) *p = 0;
}


/****************************************************************
 *			SHIFT_RIGHT_DIGITS			*
 ****************************************************************
 * Shift array a to the right s digits.	Ensure that the 	*
 * digit to the left of the leftmost used digit in array a 	*
 * after the shift holds 0.					*
 ****************************************************************/

void shift_right_digits(const INTCELL_ARRAY *a, LONG s)
{
  register LONG i;
  register LONG n_minus_s = a->size - s;
  register intcellptr abuff = a->buff;

  for(i = 0; i < n_minus_s; i++) abuff[i] = abuff[i+s];
  abuff[(a->size > s) ? a->size - s : 0] = 0;
}


/****************************************************************
 *			MULT_BY_DIGIT				*
 ****************************************************************
 * Compute a*d and put the result back into a. d must be a	*
 * single intcell digit, and must be positive.	a must have	*
 * an extra digit, set to 0, beyond its left end.  If that      *
 * digit is set nonzero, then the product has grown by a digit. *
 ****************************************************************/

void mult_by_digit(const INTCELL_ARRAY *a, intcell d)
{
  register LONG j;
  register ULONG carry;
  register ULONG sum;

  intcellptr abuff = a->buff;
  LONG       na    = a->size;

  /*-------------------------------------------------------*
   * Perform standard multiplication. Addition is inlined. *
   *-------------------------------------------------------*/

  carry = 0;
  for(j = 0; j < na; j++) {
    sum      = d * abuff[j] + carry;
    abuff[j] = (intcell)(sum & INTCELL_MASK);
    carry    = (intcell)((sum >> INTCELL_BITS) & INTCELL_MASK);
  }
  abuff[j] = (intcell) carry;
}


/****************************************************************
 *			STANDARD_MULT_ARRAY			*
 ****************************************************************
 * Compute a * b, and put the result in c. 			*
 * Buffer c must have size at least a.size + b.size + 1, and    *
 * must be zeroed out.						*
 *								*
 * The algorithm is the standard (grade shool) algorithm.	*
 ****************************************************************/

PRIVATE void
standard_mult_array(const INTCELL_ARRAY *a, const INTCELL_ARRAY *b, 
		    intcellptr c)
{
  register LONG i;
  register ULONG carry, d;

  intcellptr          abuff = a->buff;
  LONG                na    = a->size;
  register intcellptr bbuff = b->buff;
  register LONG       nb    = b->size;

  /*-------------------------------------*
   * Skip over leading zeros in factors. *
   *-------------------------------------*/

  while(abuff[na-1] == 0 && na > 1) na--;
  while(bbuff[nb-1] == 0 && nb > 1) nb--;

  /*-------------------------------------------------------*
   * Perform standard multiplication. Addition is inlined. *
   *-------------------------------------------------------*/

  for(i = 0; i < na; i++) {
    carry = 0;
    d     = abuff[i];
    if(d != 0) {
      register LONG j, k;
      register ULONG sum;
      k = i;
      for(j = 0; j < nb; j++) {
	sum    = c[k] + (d * bbuff[j] + carry);
	c[k++] = (intcell)(sum & INTCELL_MASK);
	carry  = (intcell)((sum >> INTCELL_BITS) & INTCELL_MASK);
      }
      c[k] = (intcell) carry;
    }
  } /* end for(i = ...) */
}


/****************************************************************
 *			MULT_ARRAY				*
 ****************************************************************
 * Compute aa * bb, and put the result in c.  			*
 * Buffer c must have length at least aa->size + bb->size + 1. 	*
 *                                                              *
 * The algorithm is a divide-and-conquer algorithm.		*
 ****************************************************************/

void mult_array(const INTCELL_ARRAY *aa, const INTCELL_ARRAY *bb, intcellptr c)
{
  /*---------------------------------------------------------------*
   * We will want to change the size of the buffers, so copy them. *
   *---------------------------------------------------------------*/

  INTCELL_ARRAY a = *aa;
  INTCELL_ARRAY b = *bb;

  LONG a_size = a.size;
  LONG b_size = b.size;

  /*-----------------------------*
   * Zero out the result array.  *
   *-----------------------------*/

  longmemset(c, 0, INTCELL_TO_BYTE(a_size + b_size + 1));

  /*------------------------------------------------------------*
   * Skip over leading zeros in the input numbers. Only do this *
   * if doing so is a good idea.  (Unbalancing the numbers for  *
   * a small reduction in size does not pay off.)  Say that the *
   * adjustment is a good idea if the sizes were already 	*
   * unequal, or if the adjustment cuts one of the numbers in	*
   * half.		                			*
   *------------------------------------------------------------*/

  {register LONG i = a_size;
   register LONG j = b_size;
   register intcellptr a_buff = a.buff;
   register intcellptr b_buff = b.buff;
   while(a_buff[i-1] == 0 && i > 1) i--;
   while(b_buff[j-1] == 0 && j > 1) j--;
   if(a_size != b_size || i <= a_size >> 1 || j <= b_size >> 1) {
     a.size = a_size = i;
     b.size = b_size = j;
   }
  }

  /*-------------------------------------------------------*
   * If either array is small, do the standard algorithm.  *
   * It is more efficient for small arrays.		   *
   *-------------------------------------------------------*/

  if(a_size < 12 || b_size < 12) {
    standard_mult_array(&a, &b, c);
    return;
  }

  /*-----------------------------------------------------------------*
   * For MSWIN, check for messages.  A multiplication can be a long  *
   * computation. 						     *
   *-----------------------------------------------------------------*/

# ifdef MSWIN
    handlePendingMessages();
# endif

  /*--------------------------*
   * Make a the larger array. *
   *--------------------------*/

  if(a_size < b_size) {
    INTCELL_ARRAY tt = a; 
    a      = b; 
    b      = tt;
    a_size = a.size;
    b_size = b.size;
  }

  /*------------------------------------------------------------*
   * If sizes of a and b are different, chop the larger one (a) *
   * up.  Chop a up into sections of length b->size.  Multiply 	*
   * by b as if b were a single digit in base 			*
   * INT_BASE^{b->size}.					*
   *------------------------------------------------------------*/

  if(a_size > b_size) {
    LONG k, twicenb, dig;
    BIGINT_BUFFER t4;

    /*------------------------------------------------------------*
     * Run the standard algorithm on big digits. Each "digit" has *
     * b->size intcells.  Buffer t4 is used to hold the		  *
     * individual "digit" multiplication results.		  *
     *------------------------------------------------------------*/

    twicenb = b_size + b_size;
    get_temp_bigint_buffer(twicenb + 1, &t4);
    for(k = 0; k < a_size; k += b_size) {
      INTCELL_ARRAY a_piece, c_tail;

      /*--------------------------------------------------------*
       * Multiply b by a piece of a that is b.size digits long. *
       *--------------------------------------------------------*/

      a_piece.buff = a.buff + k;
      a_piece.size = min(b_size, a_size - k);
      mult_array(&a_piece, &b, t4.val.buff);  /* t4 = a_piece * b. */
      t4.val.size = a_piece.size + b_size;

      /*-------------------------------------------------------------*
       * Add the product t4 into the result buffer c, shifted to the *
       * left an appropriate amount.				     *
       *-------------------------------------------------------------*/

      dig = a_size + b_size - k;
      if(dig > twicenb) dig = twicenb;
      c_tail.buff = c + k;
      c_tail.size = dig;
      add_array(&c_tail, &(t4.val), &c_tail);
    }
    free_bigint_buffer(&t4);
    return;
  } /* end if(a_size > b_size) */

  /*------------------------------------------------------------*
   * Here, we do the case where a->size = b->size, and both are *
   * fairly large.						*
   *								*
   * We chop each of a and b in half, by taking half of the	*
   * digits in each part.  This yields low-order parts  	*
   * a0 and b0, and high order parts a1 and b1.  Then do three	*
   * recursive multiplications. 				*
   *								*
   *   p1 = (a0 + a1) * (b0 + b1)				*
   *   p2 = a0 * b0						*
   *   p3 = a1 * b1						*
   *								*
   * Two subtractions yields					*
   *								*
   *   t = p1 - p2 - p3 = a0*b1 + b0*a1.			*
   *								*
   * Now we can get the product by adding up p2, p3 and t, with	*
   * t and p3 shifted over appropriate amounts.			*
   *------------------------------------------------------------*/

  else /* a_size == b_size */ {
    BIGINT_BUFFER p1, a_temp, b_temp;
    INTCELL_ARRAY a0, b0, a1, b1, cc;

    LONG losize       = a_size>>1;
    LONG hisize       = a_size - losize;
    LONG twice_hisize = hisize + hisize;
    LONG twice_losize = losize + losize;

    /*--------------------------------------------------*
     * Allocate temporaries, and zero out p1.  		*
     * We will set p1 = (a0+a1)*(b0+b1).  p1 will have  *
     * 2*hisize+1 digits. 				*
     *--------------------------------------------------*/

    {LONG buffsize = a_size + 3;
     get_temp_bigint_buffer(buffsize, &p1);
     get_temp_bigint_buffer(buffsize, &a_temp);
     get_temp_bigint_buffer(buffsize, &b_temp);
     longmemset(p1.val.buff, 0, INTCELL_TO_BYTE(buffsize));
    }

    /*------------------------*
     * Get a0, a1, b0 and b1. *
     *------------------------*/

    a0.buff = a.buff;
    b0.buff = b.buff;
    a0.size = b0.size = losize;

    a1.buff = a.buff + losize;
    b1.buff = b.buff + losize;
    a1.size = b1.size = hisize;

    /*-------------------------------------------------------------*
     * Compute the sums (a0 + a1) and (b0 + b1), and put them into *
     * a_temp and b_temp, resp.					   *
     *								   *
     * After the addition, bump up the sizes of a_temp and b_temp, *
     * since they might have grown at the addition. 	   	   *
     *-------------------------------------------------------------*/

    add_array(&a1, &a0, &(a_temp.val));
    add_array(&b1, &b0, &(b_temp.val));
    a_temp.val.size = b_temp.val.size = hisize + 1;

    /*------------------------------------------------------------*
     * Now take the product (a0 + a1)(b0+b1), and put it into p1. *
     *------------------------------------------------------------*/

    mult_array(&(a_temp.val), &(b_temp.val), p1.val.buff);
    p1.val.size = a_temp.val.size + b_temp.val.size;

    /*----------------------------------------*
     * Set c = a0*b0.  c has 2*losize digits. *
     *----------------------------------------*/

    mult_array(&a0, &b0, c);
    cc.buff = c;
    cc.size = twice_losize;

    /*--------------------------------------------------*
     * Set a_temp = a1*b1.  a_temp has 2*hisize digits. *
     *--------------------------------------------------*/

    mult_array(&a1, &b1, a_temp.val.buff);
    a_temp.val.size = twice_hisize;

    /*---------------------------------------------------*
     * Set p1 = a0*b1 + b0*a1 by subtracting.	 	 *
     * Now p1 has losize + hisize + 1 = a_size+1 digits. *
     *---------------------------------------------------*/

    subtract_array(&(p1.val), &cc, &(p1.val));
    subtract_array(&(p1.val), &(a_temp.val), &(p1.val));
    p1.val.size = a_size + 1;

    /*--------------------------------------------------*
     * Add up the result, accumulating it in c. 	*
     * Note that a0*b0 is already in the c array. 	*
     *--------------------------------------------------*/

    {INTCELL_ARRAY c_tail1, c_tail2;
     c_tail1.buff = c + losize;
     c_tail1.size = a_size + 1;
     c_tail2.buff = c + twice_losize;
     c_tail2.size = twice_hisize;
     add_array(&c_tail1, &(p1.val), &c_tail1);
     add_array(&c_tail2, &(a_temp.val), &c_tail2);
    }

    /*------------------------*
     * Free the temp buffers. *
     *------------------------*/

    free_bigint_buffer(&p1);
    free_bigint_buffer(&a_temp);
    free_bigint_buffer(&b_temp);
  } /* end else(a_size == b_size) */
}


/****************************************************************
 *			DIVIDE_BY_DIGIT				*
 ****************************************************************
 * Divide the integer in array a by digit d.  We require that   *
 * a->size > 0.  The quotient is left in b, and the remainder 	*
 * is put in r.	 Array b can be the same as a.			*
 *								*
 * It is required that d <= INT_BASE/2.				*
 ****************************************************************/

void divide_by_digit(const INTCELL_ARRAY *a, LONG d, 
		     const INTCELL_ARRAY *b, LONG *r)
{
  register LONG i,x;
  register intcellptr abuff = a->buff;
  register intcellptr bbuff = b->buff;

  i = a->size-1;
  x = abuff[i];
  while(i >= 0) {
    bbuff[i--] = (intcell)(x/d);
    x %= d;
    if(i >= 0) x = (x << INTCELL_BITS) + abuff[i];
  }
  *r = x;
}


/****************************************************************
 *			NEWTON_RECIP				*
 ****************************************************************
 * Compute array yinv, with y->size digits, and integer exp 	*
 * such that 							*
 *								*
 *     1/y ~= yinv * BASE^{-exp} 				*
 *								*
 * where ~= indicates approximate equality.			*
 *								*
 * The algorithm is Newton's method for reciprocal.		*
 ****************************************************************/

PRIVATE void
newton_recip(INTCELL_ARRAY *y, BIGINT_BUFFER *yinv, LONG *exp)
{
  /*------------------------------------------------------------*
   * Newton's method for computing the reciprocal is as		*
   * follows.  Suppose we already have a value z that is close	*
   * to 1/y.  Compute a new value z' that is closer to 1/y	*
   * as follows.						*
   *								*
   *     z' = 2*z - (z^2)*y					*
   *								*
   * Note that if z = 1/y, then z' = z.  That is, 1/y is a 	*
   * fixed point.  If z has n digits of precision, as an	*
   * approximation to 1/y, then z' has 2n digits of precision.	*
   *								*
   * We need to work with integers, not real numbers.  So we	*
   * store z as a*BASE^{-k}, for some integers a and k.		*
   *------------------------------------------------------------*/


  INTCELL_ARRAY p, a;
  BIGINT_BUFFER t0, t1, t2;
  int donecnt, improved_init;
  LONG m, k, d_update, newd, kminusm, kminusm_minus_d_update;

  newd = 3*y->size + 4;
  get_temp_bigint_buffer(newd, &t0);
  get_temp_bigint_buffer(newd, &t1);
  get_temp_bigint_buffer(newd, &t2);

  /*--------------------------------------------------------------*
   * We don't want to use the full value y, since in the early	  *
   * stages we are only getting rough approximations to yinv	  *
   * anyway.  Using a shorter approximation of y is cheaper.	  *
   *								  *
   * The current approximation of y is p*BASE^m.  To start, use   *
   * a single digit approximation.  That approximation is improved*
   * to two digits shortly, and it is with the two digit	  *
   * approximation that we begin Newton's method.		  *
   *								  *
   * Keep in mind that our integer arrrays are refered to by a	  *
   * pointer to their low order ends, so to get the initial one	  *
   * digit approximation, make p point to the high order digit,	  *
   * and set m to the number of digits that are ignored.  	  *
   * (That is, pretend all but the high order digit are 0.)	  *
   *--------------------------------------------------------------*/

  m      = y->size - 1;
  p.buff = y->buff + m;
  p.size = 1;

  /*------------------------------------------------------------*
   * Get the initial approximation to the reciprocal of y.	*
   * The approximation is always kept as a*BASE^{-k}.  So we	*
   * always expect a*p to be an approximation to BASE^{k}.  	*
   *								*
   * Approximation a will always have one more digit than p, so *
   * that its leftmost bit is surely 0.  That way, when we	*
   * double it, we don't need to make the buffer larger.	*
   * The initial approximation has two digits.			*
   *								*
   * t0 is the buffer that holds a.  Allocate it.  Set up a	*
   * in the high order part of that buffer.  It will grow	*
   * toward the low order part as more precision is added.	*
   *------------------------------------------------------------*/

  longmemset(t0.val.buff, 0, INTCELL_TO_BYTE(newd));
  a.buff = t0.val.buff + 3*y->size - 1;
  a.size = 2;

  /*----------------------------------------------------*
   * Here, we complete setting up the two digits of	*
   * the initial approximation of a.  We must be	*
   * careful to get enough precision in the initial	*
   * approximation, since newton's method will double	*
   * the number of digits at each iteration.  If we	*
   * start with a poor approximation, the algorithm	*
   * takes too long to get up to speed.			*
   *							*
   * A first approximation to 1/y is floor(BASE^2/s),	*
   * where s is the single digit of p.  As long as s is	*
   * reasonably large, this will be ok, since then a*p	*
   * is s*floor(BASE^2/s), which is about BASE^2.	*
   * That means that a*y is about BASE^(y->size + 1).	*
   *							*
   * But if s is small, then this approximation is too  *
   * crude.  In that case, we get another digit from	*
   * y, and do the same kind of thing with a two-digit	*
   * approximation s.					*
   *----------------------------------------------------*/

  {register LONG s,t;
   s = *(p.buff);
   if(s < HALF_DIGIT_PLUS_BIT) {

     /*------------------------*
      * We need another digit. *
      *------------------------*/

     s = (s << (INTCELL_BITS-2)) + (p.buff[-1] >> 2);
     t = ALMOST_SQ_BASE/s;
     k = y->size;
   }
   else {
     t = ALMOST_SQ_BASE/(s>>2);
     k = y->size+1;
   }
   a.buff[0] = (intcell)(t & INTCELL_MASK);
   a.buff[1] = (intcell)(t >> INTCELL_BITS);
  } /* end block */

  /*----------------------------------------------------*
   * Update approximations.				*
   * Double the number of digits in each iteration. 	*
   *----------------------------------------------------*/

  donecnt       = 0;
  improved_init = 0;
  while(donecnt < 2) {

    /*----------------------------------------------------------*
     * We will get d_update more digits in the approximations.  *
     *								*
     * Our initial approximation is not all that great.		*
     * If p.size = 2, then do two iterations to improve initial *
     * estimate to an appropriate precision.  This is necessary	*
     * or the algorithm lags behind where it should be.  The	*
     * correction should be done early, while we are using few	*
     * digits, rather than later, when a more expensive		*
     * operation would be needed to make the correction.        *
     *----------------------------------------------------------*/

    if(p.size == 2 && !improved_init) {
      d_update = 0;
      improved_init = 1;
    }
    else {
      d_update = min(p.size, y->size - p.size);
    }

    /*----------------------------------------------------------*
     * Get d_update more digits in the approximation to y. We   *
     * must improve y before trying to improve yinv.		*
     *----------------------------------------------------------*/

    p.size  += d_update;
    p.buff  -= d_update;
    m       -= d_update;
    kminusm  = k - m;
    kminusm_minus_d_update = kminusm - d_update;

   /*-------------------------------------------------------*
    * Set t2 = a*a*y, using the current approximation of y. *
    *-------------------------------------------------------*/

    mult_array(&a, &a, t1.val.buff);
    t1.val.size = a.size + a.size;
    mult_array(&(t1.val), &p, t2.val.buff);
    t2.val.size = t1.val.size + p.size;

    /*----------------------------------------------------------*
     * Newton's method tells us to set the new approximation	*
     * yinv_{n+1} to be 					*
     *								*
     *   yinv_{n+1}  = 2*yinv_n - (yinv_n)^2 * y		*
     *								*
     * where yinv_n is the current approximation.  Since yinv	*
     * is being approximated by a*BASE^{-k}, and we are using	*
     * p*BASE^{m} as our approximation of y, we want to have	*
     * the new a value a' and exponent k' chosen so that	*
     *								*
     *   a'*BASE^{-k'} = 					*
     *        2a*BASE^{-k} - a^2*BASE^{-2k} * p * BASE^{m}	*
     *								*
     * We will have k' = k + d_update, so we need		*
     *								*
     *   a' = (2a*BASE^{k-m} - a*a*p) * BASE^{m - k + d_update) *
     *  							*
     * To do this, first set a = 2a*BASE^{k-m}.  Note that 	*
     * shifting the pointer to the right will suffice to 	*
     * multiply by a power of the base.				*
     *----------------------------------------------------------*/

    add_array(&a, &a, &a);
    a.buff -= kminusm;
    a.size += kminusm;

    /*----------------------------------------------------------*
     * Now subtract t2 from a.  Then multiply by 		*
     * BASE^{m - k + d_update} to complete the computation of 	*
     * the new a value.  It is always the case that k-m >	*
     * d_update, so m - k - d_update is a negative number. To   *
     * back up, set the digits that end up to the right of the  *
     * point to 0 again.			                *
     *----------------------------------------------------------*/

    subtract_array(&a, &(t2.val), &a);
    longmemset(a.buff, 0, INTCELL_TO_BYTE(kminusm_minus_d_update));
    a.buff += kminusm_minus_d_update;
    a.size -= kminusm_minus_d_update;
    k      += d_update;

    /*------------------------------------------------------------*
     * If we are looking at all of y, then increment the count of *
     * iterations using all of y.  We need to do two iterations   *
     * at full precision to complete the computation.             *
     *------------------------------------------------------------*/

    if(p.size >= y->size) donecnt++;
  } /* end while(donecnt < 2) */

  /*------------------------------------------------------------*
   * We are finished, and have the final a and k values, where  *
   * 								*
   *   1/y ~= a*BASE^{-k}.					*
   *								*
   * Set up the yinv buffer. 					*
   *------------------------------------------------------------*/

  yinv->val   = a;
  yinv->chunk = t0.chunk;
  *exp        = k;
  if(yinv->val.buff[yinv->val.size - 1] == 0) yinv->val.size--;

  /*------------------------------------------------------------*
   * Free the temp buffers. Don't free t0, since it holds yinv. *
   *------------------------------------------------------------*/

  free_bigint_buffer(&t1);
  free_bigint_buffer(&t2);
}


/****************************************************************
 *			DIVIDE_FROM_RECIP			*
 ****************************************************************
 * x is an array of x->size digits, but must have at least 	*
 * x->size + 1 available, with the high digit holding 0.  	*
 *								*
 * y is an array of (x->size - pad) digits.			*
 *								*
 * yinv is an array of x->size digits. 				*
 *								*
 * 0 <= pad <= k.						*
 *								*
 * k is an exponent, so that 					*
 *								*
 *        1/y ~= yinv * BASE^{pad-k}.       			*
 *								*
 * Divide x by y, placing the quotient in q and		 	*
 * the remainder in r.	q and r are allocated here.		*
 ****************************************************************/

PRIVATE void
divide_from_recip(INTCELL_ARRAY *x, INTCELL_ARRAY *y, INTCELL_ARRAY *yinv,
		  LONG k, LONG pad,
		  BIGINT_BUFFER *q, BIGINT_BUFFER *r)
{
  BIGINT_BUFFER t0, t1, t2;
  LONG n;

  /*-------------------*
   * Allocate buffers. *
   *-------------------*/

  n  = 3*x->size + 3;
  get_temp_bigint_buffer(n, &t0);
  get_temp_bigint_buffer(n, &t1);
  get_temp_bigint_buffer(n, &t2);

  /*-------------------------------------*
   * Set t1 ~= (1/y) * x * BASE^{k-pad}. *
   *-------------------------------------*/

  mult_array(yinv, x, t1.val.buff);

  /*------------------------------------------------------------*
   * The quotient is approximately t1, shifted by pad-k digits. *
   *------------------------------------------------------------*/

  q->val.buff  = t1.val.buff + k - pad;
  q->val.size  = x->size + yinv->size - k + pad;
  q->chunk     = t1.chunk;

  /*------------------------------------------------------------*
   * q is now close to the desired quotient.  Adjust q. 	*
   * To do this, we need to compute y*q, and compare it to x.  	*
   * Begin by setting t2 = y*q					*
   *------------------------------------------------------------*/

  mult_array(&(q->val), y, t2.val.buff);
  t2.val.size = y->size + q->val.size;

  /*--------------------------------------------------------------*
   * If the quotient is too big, reduce it as needed. This should *
   * not happen, but I include it just to be sure.		  *
   *--------------------------------------------------------------*/

  while(compare_array(&(t2.val), x) > 0) {
    subtract_array(&(q->val), &one_array, &(q->val));
    subtract_array(&(t2.val), y, &(t2.val));
  }

  /*----------------------------------------------------------------*
   * Now, surely, t2 has at most x->size digits, since it is	    *
   * smaller than x.						    *
   *----------------------------------------------------------------*/

  t2.val.size = x->size;

  /*------------------------------------------*
   * Compute the remainder, and put it in t0. *
   *------------------------------------------*/

  t0.val.size = x->size;
  subtract_array(x, &(t2.val), &(t0.val));

  /*---------------------------------------------------------*
   * If the remainder is too big, reduce it and increase the *
   * quotient as needed. 				     *
   *---------------------------------------------------------*/

  while(compare_array(y, &(t0.val)) != 1) {
    add_array(&(q->val), &one_array, &(q->val));
    subtract_array(&(t0.val), y, &(t0.val));
  }

  /*------------------------------------------*
   * Store the remainder. It is less than y.  *
   *------------------------------------------*/

  r->val.buff  = t0.val.buff;
  r->val.size  = y->size;
  r->chunk     = t0.chunk;

  /*--------------------------------------------------------------*
   * Free temp buffer t2. Buffers t0 and t1 are used for r and q. *
   *--------------------------------------------------------------*/

  free_bigint_buffer(&t2);
}


/****************************************************************
 *			DIVIDE_LARGE_INT			*
 ****************************************************************
 * Divide |a| by |b|, leaving the quotient in array q 	 	*
 * and the remainder in r.  The space for q is			*
 * allocated here.						*
 ****************************************************************/

void divide_large_int(ENTITY a, ENTITY b,
		      BIGINT_BUFFER *q, ENTITY *r)
{
  BIGINT_BUFFER aa, bb, pa, pb, binv, x, rr, qq;
  intcell aspace[INTCELLS_IN_LONG], bspace[INTCELLS_IN_LONG];
  intcellptr pap;
  LONG m, xsize, bexp, rem, bytenb, twonb;
  int a_tag, b_tag;
  Boolean did_alloc;

  /*----------------------------*
   * Convert a and b to arrays. *
   *----------------------------*/

  const_int_to_array(a, &aa, &a_tag, aspace);
  const_int_to_array(b, &bb, &b_tag, bspace);

  /*---------------------------------------------------------*
   * If b has more digits than a, then the quotient is 0 and *
   * remainder is |a|.					     *
   *---------------------------------------------------------*/

  if(bb.val.size > aa.val.size) {
    *r = ast_abs(a);
    get_bigint_buffer(1, q);
    q->val.buff[0] = 0;
    goto out;
  }

  /*-----------------------------------------------------*
   * If b has only one digit, then use divide_by_digit.  *
   * It is much faster than the general scheme involving *
   * Newton's method.					 *
   *-----------------------------------------------------*/

  if(bb.val.size == 1 && *(bb.val.buff) <= (INT_BASE>>1)) {
    get_bigint_buffer(aa.val.size, q);
    divide_by_digit(&(aa.val), *(bb.val.buff), &(q->val), &rem);
    *r = ENTI(rem);
    goto out;
  }

  /*----------------------------------------------------------------*
   * Here is the general case.  We could just pad b out to the same *
   * size as a, invert it, and multiply.  But it is cheaper to	    *
   * do "long division", using division by newton's method to do    *
   * the basic steps.  That is what is done here.		    *
   * To imagine the long division, imagine that we are working in   *
   * base INT_BASE^{b->size}, so that b represents a single digit.  *
   * So we are dividing by a one "digit" number.		    *
   *----------------------------------------------------------------*/

  /*----------------------------------------------------------------*
   * Need to pad b on the right with bb.val.size zeros, for 	    *
   * inversion. That will give us a better approximation to 1/b	    *
   * when we get the reciprocal.  To divide by a one digit number,  *
   * you need to perform arithmetic on two digit numbers.	    *
   * Buffer pb is the padded/shifted version of bb.		    *
   *----------------------------------------------------------------*/

  bytenb = INTCELL_TO_BYTE(bb.val.size);
  twonb  = bb.val.size + bb.val.size;
  get_temp_bigint_buffer(twonb, &pb);
  longmemset((char *) (pb.val.buff), 0, bytenb);
  longmemcpy((char *) (pb.val.buff + bb.val.size),
	     (char *) (bb.val.buff),
	     bytenb);

  /*-------------------------------------------------------------*
   * Need to pad a with 0's on the left to next multiple of	 *
   * bb.val.size in size. For simplicity, just add bb.val.size   *
   * to its size, since a few extra 0's don't hurt.		 *
   * Buffer pa is the padded version of aa.			 *
   *-------------------------------------------------------------*/

  get_bigint_buffer(aa.val.size + bb.val.size + 1, &pa);
  pa.val.size--;
  longmemcpy((char *) (pa.val.buff),
	     (char*) (aa.val.buff),
	     INTCELL_TO_BYTE(aa.val.size));
  longmemset((char *) (pa.val.buff + aa.val.size), 0, bytenb + 1);

  /*------------------------------------------------------------*
   * pap points to the current position where we are doing the  *
   * long division.						*
   *------------------------------------------------------------*/

  m   = aa.val.size % bb.val.size;
  pap = pa.val.buff + aa.val.size - ((m == 0) ? bb.val.size : m);

  /*--------------*
   * Compute 1/b. *
   *--------------*/

  newton_recip(&(pb.val), &binv, &bexp);

  /*------------------------------------------------------------*
   * Allocate array x.  We will put the quotient in pa and the 	*
   * remainder in x at the end.  			  	*
   *								*
   * As the division proceeds, we will have one or two "digits"	*
   * from a in x as our current value to operate on.		*
   * Subtractions are done in-place in x.  When x is too small, *
   * another "digit" is shifted into x.  To start, put the 	*
   * leftmost "digit" of a into x.				*
   *------------------------------------------------------------*/

  xsize = twonb;
  get_bigint_buffer(xsize + 1, &x);
  x.val.size = xsize;
  x.val.buff[xsize] = 0;
  longmemcpy((char *)(x.val.buff), (char *) pap, bytenb);
  longmemset((char *)(x.val.buff + bb.val.size), 0, bytenb);

  /*----------------------------*
   * Perform the long division. *
   *----------------------------*/

  while(pap >= pa.val.buff) {

    /*--------------------------------------------------------*
     * Compute x/b.					      *
     * If x<b, then the quotient is 0 and the remainder is x. *
     * That will generally call for shifting in another digit.*
     *--------------------------------------------------------*/

    if(compare_array(&(x.val), &(bb.val)) < 0) {

      /*-------*
       * x < b *
       *-------*/

      rr.val.buff = x.val.buff;
      rr.val.size = bb.val.size;
      longmemset(pap, 0, bytenb);  /* Store a 0 digit in the quotient */
      did_alloc = FALSE;
    }
    else {

      /*--------*
       * x >= b *
       *--------*/

      divide_from_recip(&(x.val), &(bb.val), &(binv.val),
			bexp, bb.val.size, &qq, &rr);
      longmemcpy(pap, qq.val.buff, bytenb);
      did_alloc = TRUE;
    }

    /*-----------------------------*
     * Shift another digit into x. *
     *-----------------------------*/

    pap -= bb.val.size;
    if(pap >= pa.val.buff) {
      longmemcpy(x.val.buff + bb.val.size, rr.val.buff, bytenb);
      longmemcpy(x.val.buff, pap, bytenb);
    }
    else {
      longmemset(x.val.buff + bb.val.size, 0,           bytenb);
      longmemcpy(x.val.buff,               rr.val.buff, bytenb);
    }

    if(did_alloc) {
      free_bigint_buffer(&rr);
      free_bigint_buffer(&qq);
    }
  } /* end while(pap >= pa.val.buf) */

  /*------------------------------------------------------------*
   * Get the quotient and remainder.  The remainder cannot have *
   * more digits than b, and the quotient cannot have more      *
   * digits than a.						*
   *------------------------------------------------------------*/

  x.val.size  = bb.val.size;
  *r          = array_to_int(BIGPOSINT_TAG, &x);
  *q          = pa;
  q->val.size = aa.val.size;

  free_bigint_buffer(&pb);
  free_bigint_buffer(&binv);

out:
}


/****************************************************************
 *			BIG_GCD					*
 ****************************************************************
 * Return gcd(a,b), by Stein's algorithm.			*
 ****************************************************************/

ENTITY big_gcd(ENTITY a, ENTITY b)
{
  /*------------------------------------------------------------*
   * Stein's algorithm works as follows.			*
   *								*
   * (1) If a and b are both even, then 			*
   *								*
   *        gcd(a,b) = 2*gcd(a/2, b/2)				*
   *								*
   *     This is implemented below by pulling off common	*
   *     factors of 2 and remembering how many are pulled off.	*
   *								*
   * (2) If a is odd and b is even, then			*
   *								*
   *	    gcd(a,b) = gcd(a,b/2)				*
   *								*
   * (3) If both a and b are both odd, and a >= b, then		*
   *								*
   *	    gcd(a,b) = gcd(a-b, b)				*
   *								*
   *     Notice that a-b is even, so we will be able to divide	*
   *	 it by 2.						*
   *								*
   * (4) gcd(0,b) = b.						*
   *								*
   * (5) Rules that follow from the above by the symmetry of	*
   *     gcd are also used.					*
   *------------------------------------------------------------*/


  BIGINT_BUFFER aa, bb, *result_buffer;
  ENTITY result;
  register intcellptr aar, bbr, al, bl;
  LONG i, bs, ds;
  int a_tag, b_tag;

  /*------------------------------------------------------------*
   * Copy a and b into arrays. 				 	*
   * al and bl point to the leftmost digits of aa and bb.	*
   * aa.val.buff and bb.val.buff point to the rightmost		*
   * digits of aa and bb. They are duplicated in aar and bbr	*
   * for efficiency.						*
   *------------------------------------------------------------*/

  int_to_array(a, &aa, &a_tag, 0, 0);
  int_to_array(b, &bb, &b_tag, 0, 0);
  aar = aa.val.buff;
  bbr = bb.val.buff;
  al  = aar + aa.val.size - 1;
  bl  = bbr + bb.val.size - 1;

  /*--------------------------------------------------------------------*
   * Any 0 digits at the low order end of both indicate common factors	*
   * of INT_BASE.  Pull them off, and remember how many there are in 	*
   * ds.  (ds is the digit shift.)					*
   *--------------------------------------------------------------------*/

  ds = 0;
  while(*aar == 0 && *bbr == 0) {aar++; bbr++; ds++;}

  /*-------------------------------------------------------------*
   * Get common factors of 2. Remember how many there are in bs. *
   * (bs is the bit shift.)					 *
   *-------------------------------------------------------------*/

  bs          = 0;
  aa.val.size = al - aar + 1;
  bb.val.size = bl - bbr + 1;
  aa.val.buff = aar;
  bb.val.buff = bbr;
  while((*aar & 1) == 0 && (*bbr & 1) == 0) {
    bs++;
    right_shift(&(aa.val), 1);
    right_shift(&(bb.val), 1);
  }

  /*--------------------------------------------------------------------*
   * Remove remaining factors of 2 if one of the numbers is still even. *
   * Shrink at left end if possible.					*
   *--------------------------------------------------------------------*/

  while(*bbr == 0) bbr++;
  bb.val.size = bl - bbr + 1;
  bb.val.buff = bbr;
  while((*bbr & 1) == 0) right_shift(&(bb.val), 1);

  while(*aar == 0) aar++;
  aa.val.size = al - aar + 1;
  aa.val.buff = aar;
  while((*aar & 1) == 0) right_shift(&(aa.val), 1);

  while(*al == 0) al--;
  while(*bl == 0) bl--;

  /*------------------------------------------------------------*
   * Main part of Stein's algorithm. At the top of this loop,   *
   * aa and bb are both odd.					*
   *------------------------------------------------------------*/

  while(aar <= al && bbr <= bl) {
    aa.val.size = al - aa.val.buff + 1;
    aa.val.buff = aar;
    bb.val.size = bl - bbr + 1;
    bb.val.buff = bbr;

    /*------------------------------------------------------*
     * If aa is the larger value, then replace aa by aa-bb. *
     * Then pull off factors of two.			    *
     *------------------------------------------------------*/

    if(compare_array(&(aa.val), &(bb.val)) >= 0) {
       subtract_array(&(aa.val), &(bb.val), &(aa.val));
       while(aar <= al && *aar == 0) aar++;
       aa.val.size = al - aar + 1;
       aa.val.buff = aar;
       if(aa.val.size > 0) {
	 while((*aar & 1) == 0) right_shift(&(aa.val), 1);
	 while(*al == 0) al--;
       }
    }

    /*------------------------------------------------------*
     * If bb is the larger, then replace bb by bb - aa, and *
     * get rid of factors of 2.				    *
     *------------------------------------------------------*/

    else {
      subtract_array(&(bb.val), &(aa.val), &(bb.val));
      while(bbr <= bl && *bbr == 0) bbr++;
      bb.val.size = bl - bbr + 1;
      bb.val.buff = bbr;
      if(bb.val.size > 0) {
	while((*bbr & 1) == 0) right_shift(&(bb.val), 1);
	while(*bl == 0) bl--;
      }
    }
  } /* end while(aar <= al && bbr <= bl) */

  /*----------------------------------------------*
   * Now we have reduced one of the numbers to 0. *
   * Get the result, without the factors of 2.    *
   * Increase the size so that the left shifts	  *
   * below will not overflow the buffer.	  *
   *----------------------------------------------*/

  result_buffer = (al < aar) ? &bb : &aa;
  result_buffer->val.size++;

  /*------------------------------------------------------------*
   * Put in the factors of 2 and return the result. Notice that *
   * the digits of a or b to the right of aar or bbr are all 0, *
   * and gcd(a,b) cannot be larger than a or b.  Hence, it is   *
   * safe to move result's buffer pointer to the right as a	*
   * way of multiplying by INT_BASE^{ds}.			*
   *------------------------------------------------------------*/

  for(i = 0; i < bs; i++) {
    left_shift(&(result_buffer->val), 1);
  }
  result_buffer->val.buff -= ds;
  result_buffer->val.size += ds;
  result = array_to_int(BIGPOSINT_TAG, result_buffer);

  if(result_buffer == &aa) free_bigint_buffer(&bb);
  else free_bigint_buffer(&aa);
  return result;
}

