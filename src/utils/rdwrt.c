/**********************************************************************
 * File:    utils/rdwrt.c
 * Purpose: Functions for reading, writing, putting and getting
 *          small integers.
 * Author:  Karl Abrahamson
 **********************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/**********************************************************************
 * The .aso file contains integers stored in three bytes.  This file  *
 * manages writing and reading such integers.  It also manages	      *
 * putting such integers into byte arrays, and getting them out.      *
 **********************************************************************/

#include "../misc/misc.h"
#include "../utils/rdwrt.h"
#include "../intrprtr/intrprtr.h"
#include "../generate/generate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/********************************************************************
 * Each integer is stored in three bytes, biased by CODED_INT_BIAS. *
 * That is, to store n, actually store n + CODED_INT_BIAS.	    *
 ********************************************************************/

#define CODED_INT_BIAS 0x800000L


/********************************************************
 *			GET_INT_M			*
 ********************************************************
 * Gets an integer from file f, represented as three	*
 * bytes (low, mid, high), and returns it. 		*
 * This function is only used by the interpreter.	*
 *							*
 * The integer obtained is nonnegative.			*
 ********************************************************/

#ifdef MACHINE

LONG get_int_m(FILE *f)
{
  register LONG l,m,h;

  l = fgetuc(f);
  m = fgetuc(f);
  h = fgetuc(f);
  return l + (m << 8) + (h << 16) - CODED_INT_BIAS;
}

#endif

/********************************************************
 *			WRITE_INT_M			*
 ********************************************************
 * Writes integer N into file F, as a three byte 	*
 * integer, such that get_int_m would read it.		*
 *							*
 * N must be nonnegative.				*
 ********************************************************/

#ifdef TRANSLATOR

void write_int_m(package_index n, FILE *f)
{
  register unsigned int l,m,h;

# ifdef DEBUG
    if(trace_gen) trace_s(41, n);
# endif

  n += CODED_INT_BIAS;
  l  = ((int) n) & 0xFF;
  m  = ((int)(n >> 8)) & 0xFF;
  h  = ((int)(n >> 16)) & 0xFF;
  fprintf(f, "%c%c%c", l, m, h);
}

#endif

/*****************************************************************
 *			GEN_INT_M				 *
 *****************************************************************
 * Place integer N at location *C, and move *C over the integer. *
 * No alignment is necessary.  					 *
 *								 *
 * TRANSLATOR ONLY: If C is null, then write N to the code file  *
 * instead using write_g.				 	 *
 *								 *
 * N must be nonnegative.					 *
 *****************************************************************/

void gen_int_m(CODE_PTR *c, package_index n)
{
  register int byte0, byte1, byte2;

# ifdef DEBUG
    if(trace_gen) trace_s(42, n, *c);
# endif  
 
  n += CODED_INT_BIAS;
  byte0 = ((int)n) & 0xFF;
  byte1 = ((int)(n >> 8)) & 0xFF;
  byte2 = ((int)(n >> 16)) & 0xFF;

#ifdef TRANSLATOR
  if(c == NULL) {
    write_g(byte0); 
    write_g(byte1); 
    write_g(byte2);
  }
  else 
#endif
  { 
    register CODE_PTR s;
    s      = *c;
    *s     = byte0;
    *(s+1) = byte1;
    *(s+2) = byte2;
    (*c)  += 3;
  }
}


/********************************************************
 *			PUT_INT_M			*
 ********************************************************
 * Places integer N at address S, in such a way that 	*
 * next_int_m(S) would return it, and returns the	*
 * number of bytes used.				*
 *							*
 * N must be nonnegative.				*
 ********************************************************/

#ifdef MACHINE

int put_int_m(CODE_PTR s, package_index n)
{
  gen_int_m(&s, n);
  return 3;
}

#endif

/********************************************************
 *			NEXT_INT_M			*
 ********************************************************
 * Returns the integer stored at location *S by 	*
 * gen_int_m, and sets *S to the address just after the *
 * coded integer.					*
 ********************************************************/

LONG next_int_m(CODE_PTR *s) 
{ 
  register LONG n;
  register CODE_PTR ss;

  ss = *s;
  n  = (tolong(*ss)) + ((tolong(*(ss+1))) << 8) + ((tolong(*(ss+2))) << 16) 
       - CODED_INT_BIAS;
  (*s) += 3;
  return n;
}
