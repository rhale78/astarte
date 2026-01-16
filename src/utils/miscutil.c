/**********************************************************************
 * File:    utils/miscutil.c
 * Purpose: Miscellaneous utilities
 * Author:  Karl Abrahamson
 **********************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

#include "../misc/misc.h"
#include "../utils/miscutil.h"

/****************************************************************
 *			SORTOF_RANDOM_BIT			*
 ****************************************************************
 * Return a pseudo-random bit.  This is a VERY poor quality     *
 * generator, but is quick and simple.  It just uses the bits   *
 * of a counter as the pseudo-random bits.			*
 ****************************************************************/

PRIVATE int random_cnt = 2, random_bits = 2;

Boolean sortof_random_bit(void)
{
  if(random_bits == 1) {
    random_cnt++;
    if(random_cnt > 132) random_cnt = 2;
    random_bits = random_cnt;
  }
  {register Boolean result;
   result = random_bits & 1;
   random_bits >>= 1;
   return result;
 }
}


