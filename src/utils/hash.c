/**********************************************************************
 * File:    utils/hash.c
 * Purpose: Implement hash functions
 * Author:  Karl Abrahamson
 **********************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/************************************************************************
 * This file contributes to the implementation of hash tables.		*
 *									*
 * Here are just the hash functions and functions for performing	*
 * equality tests during hash table lookup.  Functions for managing	*
 * hash tables are found in hash1.c and hash2.c				*
 *									*
 * Each hash function should produce a nonnegative integer.		*
 ************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../classes/classes.h"


/****************************************************************
 *			DEFINITIONS				*
 ****************************************************************
 * HASH_MODULUS is the modulus for computing hash functions.    *
 * It should be a fairly large prime.				*
 ****************************************************************/

#define HASH_MODULUS	16777213L


/****************************************************************
 *			COMBINE3				*
 ****************************************************************
 * combine3(K,A,B) is a hash value that is K, A and B combined. *
 ****************************************************************/

LONG combine3(int k, LONG a, LONG b)
{
  return (k ^ a ^ (b << 3)) % HASH_MODULUS;
}


/****************************************************************
 *			STRHASH					*
 ****************************************************************
 * Hash function for strings.  This function gets four bytes at *
 * a time from the string and xors it with the current value.	*
 *								*
 * This function is called a lot, so a loop is unrolled.	*
 ****************************************************************/

LONG strhash(CONST char *k)
{
  register LONG h;   /* Accumulates hash */
  register LONG b;   /* Accumulates four bytes of the string */
  register CONST char *s;  /* Scans through the string. */
  register char c;   /* Holds *s */

  if(k == NULL) return 0;

  h = 0;
  s = k;
  c = *s;
  while(c != 0) {

    /*----------------------------------------------------------*
     * Shift four bytes of s into b, but stop at the end of s.  *
     * The bytes are taken to be 7 bits. The loop is unrolled.	*
     *----------------------------------------------------------*/

    b = c;
    c = *(++s);
    if(c == 0) goto get_h;
    b = (b << 7) + c;
    c = *(++s);
    if(c == 0) goto get_h;
    b = (b << 7) + c;
    c = *(++s);
    if(c == 0) goto get_h;
    b = (b << 7) + c;
    c = *(++s);

  get_h:
    h = h ^ b;
  }

  return h;
}


/****************************************************************
 *				LPHASH				*
 ****************************************************************
 * Hash a pair of integers.					*
 ****************************************************************/

LONG lphash(LPAIR lp)
{
  return (((tolong(lp.label2)) << 16) + (tolong(lp.label1)) + 1)
         % HASH_MODULUS;
}


/****************************************************************
 *				EQ				*
 ****************************************************************
 * eq tests two hash keys by testing their num fields.  It can  *
 * be used for pointers, provided two things are the same just  *
 * when they have identical pointers.				*
 ****************************************************************/

Boolean eq(HASH_KEY s, HASH_KEY t)
{
  return s.num == t.num;
}


/****************************************************************
 *			EQUALSTR 				*
 ****************************************************************
 * equalstr tests strings for equality by comparing character   *
 * by character.  						*
 ****************************************************************/

Boolean equalstr(HASH_KEY s, HASH_KEY t)
{
  if(s.str == t.str) return TRUE;
  if(strcmp(s.str,t.str) == 0) return TRUE;
  return FALSE;
}



