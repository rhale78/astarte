/*************************************************************
 * FILE:    alloc/tempstr.c
 * PURPOSE: Storage allocator for temporary string/big int buffers.
 * AUTHOR:  Karl Abrahamson
 *************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/************************************************************************
 * Some operations (notably those in directory rts that need to work    *
 * with large integers and strings) need temporary buffers.  Those      *
 * buffers are allocated here.  They are kept here, and not		*
 * deallocated, to avoid having to use malloc and free on them.   	*
 *									*
 * This file manages an array temp_strs of buffers. 			*
 ************************************************************************/

#include <string.h>
#include <memory.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/********************************************************
 *			MAX_TEMP_STR_BLOCKS		*
 *			N_TEMP_STRS			*
 ********************************************************
 * MAX_TEMP_STR_BLOCKS is the number of different sizes *
 * of temporary strings.  If it is changed, function	*
 * init_temp_strs below must be changed as well.	*
 *							*
 * N_TEMP_STRS is the maximum number of buffers of any	*
 * given size that can be allocated.			*
 ********************************************************/

#define MAX_TEMP_STR_BLOCKS 16
#define N_TEMP_STRS 14

/********************************************************
 *			temp_strs			*
 ********************************************************
 * temp_strs holds string buffers of various sizes.     *
 *							*
 * temp_strs[i] holds N_TEMP_STRS buffers of size	*
 * temp_strs[i].size, in bytes.				*
 *							*
 * temp_strs[i].in_use[j] is 1 if the j-th buffer of	*
 * the i-th group of buffers is in use.  It is 0 if 	*
 * that buffer is not in use.				*
 *							*
 * temp_strs[i].block[j] is the j-th buffer.  It is 	*
 * NULL if this buffer has not yet been allocated from  *
 * the system.						*
 ********************************************************/

PRIVATE struct temp_str_struct {
  LONG size;			
  Boolean in_use[N_TEMP_STRS];
  char* block[N_TEMP_STRS];
} FARR temp_strs[MAX_TEMP_STR_BLOCKS];


/********************************************************
 *			INIT_TEMP_STRS			*
 ********************************************************
 * Set up the temp_strs array.  This must be called at  *
 * startup time.					*
 ********************************************************/

void init_temp_strs(void)
{
  memset(temp_strs, 0, 
         MAX_TEMP_STR_BLOCKS * sizeof(struct temp_str_struct));

  /*------------------------------------------------------------*
   * Be careful about these sizes.  Multiplication, in 		*
   * rts/bigint.c, is recursive, and needs various sizes 	*
   * of buffers.  If the buffer sizes are not set up		*
   * in the right relationships to one another, then 		*
   * the recursive calls will wind up getting too many		*
   * buffers of a given size.  Buffer sizes should slightly	*
   * less than double from one size to the next.		*
   *								*
   * There are MAX_TEMP_STR_BLOCKS sizes to set up, so the	*
   * last line here has index MAX_TEMP_STR_BLOCKS - 1.		*
   *------------------------------------------------------------*/

  temp_strs[0].size = 12L;
  temp_strs[1].size = 16L;
  temp_strs[1].size = 24L;
  temp_strs[2].size = 40L;
  temp_strs[3].size = 72L;
  temp_strs[4].size = 136L;
  temp_strs[5].size = 264L;
  temp_strs[6].size = 520L;
  temp_strs[7].size = 1032L;
  temp_strs[8].size = 2056L;
  temp_strs[9].size = 4104L;
  temp_strs[10].size = 8200L;
  temp_strs[11].size = 16392L;
  temp_strs[12].size = 32776L;
  temp_strs[13].size = 65544L;
  temp_strs[14].size = 131080L;
  temp_strs[15].size = 262152L;
}


/********************************************************
 *			ALLOC_TEMP_BUFFER		*
 ********************************************************
 * Find an available temporary buffer in temp_strs that *
 * has room for at least N+1 bytes.  Mark it used.  Set *
 *							*
 *   *II	to the index in temp_strs of the buffer *
 *		collection chosen.			*
 *							*
 *   *JJ	to the index in temp_strs[*II] of the 	*
 *		buffer.  (So *BUFF is			*
 *		temp_strs[*II].block[*JJ].)		*
 *							*
 *   *BUFF	to the buffer itself.			*
 *							*
 *   *OUT_SIZE	to the physical size of the buffer.	*
 *		(This will be at least N + 1.)		*
 *							*
 * If allocation is impossible, set failure to		*
 * LONG_STRING_EX, and set *II = *JJ = -1, and 		*
 * *BUFF = NULL.					*
 ********************************************************/

void alloc_temp_buffer(long n, int *ii, int *jj, charptr *buff, long *out_size)
{
  int i, j;
  LONG size;
  charptr where;

  /*----------------------------------------------------------*
   * Find a group in temp_strs that has large enough buffers. *
   * Return error conditions if none are large enough.	      *
   *----------------------------------------------------------*/

  for(i = 0; i < MAX_TEMP_STR_BLOCKS; i++) {
    if(temp_strs[i].size > n) break;
  }

  if(i == MAX_TEMP_STR_BLOCKS) {

#   ifdef DEBUG
      if(trace) {
	trace_i(253, n);
      }
#   endif

    failure = LONG_STRING_EX;
    *ii = *jj = -1;
    *buff = NULL;
    return;
  }

  /*----------------------------------------------*
   * We have found a buffer group.  Get its size. *
   *----------------------------------------------*/

  size = temp_strs[i].size;

  /*---------------------------------------------------------*
   * Find a buffer in group i that is not in used.  Fail if  *
   * all are in use.					     *
   *---------------------------------------------------------*/

  for(j = 0; j < N_TEMP_STRS; j++) {
    if(!temp_strs[i].in_use[j]) break;
  }

  if(j == N_TEMP_STRS) {
    failure = LIMIT_EX;
    *ii = *jj = -1;
    *buff = NULL;
    return;
  }

  /*-----------------------------------------------------*
   * Get the buffer (where).  If necessary, allocate it. *
   * Mark this buffer in use.				 *
   *-----------------------------------------------------*/

  where = temp_strs[i].block[j];
  if(where == NULL) {
    where = temp_strs[i].block[j] = alloc(size);
  }
  temp_strs[i].in_use[j] = 1;

  /*--------------------*
   * Set return values. *
   *--------------------*/

  *ii       = i;
  *jj       = j;
  *out_size = size;
  *buff     = where;
}


/********************************************************
 *			FREE_TEMP_BUFFER		*
 ********************************************************
 * Free temp buffer temp_strs[I].block[J], by setting   *
 * temp_strs[I].in_use[J] = 0.				*
 *							*
 * If an attempt is made to deallocate a buffer that	*
 * does not exist (with negative indices) do nothing.	*
 ********************************************************/

void free_temp_buffer(int i, int j)
{
  if(i < 0 || j < 0) return;
  temp_strs[i].in_use[j] = 0;
}


