/*************************************************************************
 * File:    rts/integer.c
 * Purpose: Implement simple operations on integers
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
 * This file contains only a few operations that are concerned with	*
 * integers stored as long ints, or with converting between long int	*
 * form and internal integer (ENTITY) form.  Most operations on integers*
 * are handled by number.c or bigint.c.					*
 ************************************************************************/

#include <memory.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../tables/tables.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			AST_MAKE_INT				*
 ****************************************************************
 * Return an integer entity equal to long integer n.            *
 ****************************************************************/

ENTITY ast_make_int(LONG n)
{
  BIGINT_BUFFER rr;
  int tag;

  /*---------------------------------------------------------*
   * We can use an INT_TAG value if n is within the range of *
   * such values.					     *
   *---------------------------------------------------------*/

  if(n <= SMALL_INT_MAX && n >= SMALL_INT_MIN) return ENTI(n);

  if(n >= 0) tag = BIGPOSINT_TAG;
  else {
    tag = BIGNEGINT_TAG;
    n = -n;
  }

  /*---------------------------------------------------*
   * Copy n into the low order short ints of an array. *
   *---------------------------------------------------*/

  get_bigint_buffer(INTCELLS_IN_LONG, &rr);
  install_int(n, rr.val.buff);

  return ENTP(tag, rr.chunk);
}


/****************************************************************
 *			GET_IVAL				*
 ****************************************************************
 * Return a long int equivalent to entity a, provided a has tag *
 * INT_TAG.  Otherwise, set failure = ex, and return 0.      	*
 ****************************************************************/

LONG get_ival(ENTITY a, int ex)
{
  int tag = TAG(a);
  if(tag == INT_TAG) return IVAL(a);
  while(tag == INDIRECT_TAG || tag == GLOBAL_INDIRECT_TAG) {
    a = *ENTVAL(a);
    tag = TAG(a);
  }
  if(tag == INT_TAG) return IVAL(a);
  failure = ex;
  return 0;
}


/****************************************************************
 *			GCD					*
 ****************************************************************
 * Compute gcd(a,b) by Stein's algorithm.  Note that bigint.c	*
 * has a similar algorithm for entities.  This is for long ints.*
 ****************************************************************/

LONG gcd(LONG a, LONG b)
{
  int s;

  if(a == 0) {
    if(b == 0) {
      failure = DOMAIN_EX;
      failure_as_entity = qwrap(DOMAIN_EX, make_str("gcd(0,0)"));
    }
    return b;
  }
  if(b == 0) return a;

  /*-----------------------*
   * Get the factors of 2. *
   *-----------------------*/

  s = 0;
  while((((int)a) & 1) == 0 && (((int)b) & 1) == 0) {
    s++;
    a = a >> 1;
    b = b >> 1;
  }

  /*------------------------------------*
   * Throw away remaining factors of 2. *
   *------------------------------------*/

  while((((int)b) & 1) == 0) b = b >> 1;
  while((((int)a) & 1) == 0) a = a >> 1;

  /*---------------------------------*
   * Main part of Stein's algorithm. *
   *---------------------------------*/

  for(;;) {
    if(a >= b) {
      a -= b;
      if(a == 0) break;
      while((((int)a) & 1) == 0) a = a >> 1;
    }
    else {
      b -= a;
      if(b == 0) break;
      while((((int)b) & 1) == 0) b = b >> 1;
    }
  }

  /*----------------------------------*
   * Restore factors of 2 and return. *
   *----------------------------------*/

  if(a == 0) return b << s;
  else       return a << s;
}
