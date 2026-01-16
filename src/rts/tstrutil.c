/*************************************************************
 * FILE:    rts/tstrutil.c
 * PURPOSE: Utilities for temporary strings.
 * AUTHOR:  Karl Abrahamson
 *************************************************************/

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
 * This file provides concatenable strings of arbitrary length for	*
 * use in converting things, particularly numbers, to strings.  It	*
 * uses the string buffers provided by alloc/tempstr.c.			*
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
 * This file implements utilities, such as 		*
 * concatenation, for managing temporary strings.	*
 ********************************************************/

#define MAX_NUMBER_TEMP_STRS 5

/********************************************************
 *		temp_str_record				*
 ********************************************************
 * When a temp buffer is allocated, a member of		*
 * temp_str_record can also be allocated to describe	*
 * the temp buffer.  The fields of temp_str_record[k]	*
 * are							*
 *							*
 *  i		the index in temp_strs of the buffer	*
 *		being described.			*
 *							*
 *  j		the block index in temp_strs[i] of 	*
 *		the buffer being described.  So the	*
 *		buffer itself is temp_strs[i].block[j].	*
 *							*
 *  size	same as temp_strs[i].size		*
 *							*
 *  in_use	set true when this temp_str_record cell *
 * 		is in use for describing a buffer.	*
 *							*
 *  where	points to the start of the buffer.	*
 *							*
 *  next	strings are generally written from	*
 *		back to front.  next points to the 	*
 *		char cell closest to the end of the	*
 *		buffer that has not yet been written	*
 *		into.					*
 ********************************************************/

PRIVATE struct temp_str_record {
  LONG size;
  SBYTE in_use, i, j;
  charptr where, next;
} temp_str_record[MAX_NUMBER_TEMP_STRS];


/********************************************************
 *		INIT_TEMP_STR_UTILS			*
 ********************************************************
 * Sets up temp_str_record at the start of computation. *
 ********************************************************/

void init_temp_str_utils(void)
{
  memset(temp_str_record, 0, 
	 MAX_NUMBER_TEMP_STRS * sizeof(struct temp_str_record));
}  


/********************************************************
 *			NEW_TEMP_STR			*
 ********************************************************
 * Return the index in temp_str_record of an available  *
 * record, and mark that record used.  Set the record 	*
 * up, by allocationg a buffer from temp_str, so that   *
 * it describes a new (empty) temporary string capable	*
 * of holding n characters (not counting the		*
 * terminating null character).				*
 *							*
 * If allocation is impossible, set failure to		*
 * LONG_STRING_EX, and return -1.			*
 ********************************************************/

int new_temp_str(LONG n)
{
  int i, j, k;
  LONG size;
  charptr buff;
  struct temp_str_record *ts;

  /*-------------------------------------------------*
   * Find a temp_str_record cell that is not in use. *
   *-------------------------------------------------*/

  for(k = 0; k < MAX_NUMBER_TEMP_STRS; k++) {
    if(!temp_str_record[k].in_use) break;
  }
  if(k == MAX_NUMBER_TEMP_STRS) {
    failure = LONG_STRING_EX;
    return -1;
  }

  /*----------------------*
   * Allocate the buffer. *
   *----------------------*/

  alloc_temp_buffer(n, &i, &j, &buff, &size);

  /*---------------------------------------------------*
   * Set up the temp_str_record, and return its index. *
   *---------------------------------------------------*/

  ts         = temp_str_record + k;
  ts->in_use = 1;
  ts->where  = buff;
  ts->next   = buff + size - 2;
  ts->size   = size;
  ts->i      = i;
  ts->j      = j;

  /*-----------------------------------------------------*
   * Null-terminate the buffer: install an empty string. *
   *-----------------------------------------------------*/

  *(buff + size - 1) = '\0';
  return k;
}
  

/********************************************************
 *			TEMP_STR_BUFFER			*
 ********************************************************
 * Return the buffer of temp string k.			*
 ********************************************************/

charptr temp_str_buffer(int k)
{
  return temp_str_record[k].where;
}


/********************************************************
 *			MAKE_TEMP_STR			*
 ********************************************************
 * Copy string s into a temporary string buffer, set up *
 * a temp_str_record entry to manage it, and    	*
 * return the index of its temp_str entry.  		*
 * Set *buff to the string buffer.			*
 *							*
 * This function presumes that s is fully evaluated.	*
 ********************************************************/

int make_temp_str(ENTITY s, char **buff)
{
  int i;
  ENTITY tl;
  char *start;
  REG_TYPE mark = reg1_param(&s);

  LONG timev = LONG_MAX;
  ENTITY len = ast_length(s, 0, &timev);
  LONG l     = get_ival(len, LIMIT_EX);

  if(failure >= 0) {
    failure = LIMIT_EX;
    *buff = NULL;
    unreg(mark);
    return -1;
  }

  else {
    i = new_temp_str(l+1);
    *buff = start = (char *)(temp_str_record[i].next - l + 1);
    copy_str(start, s, l, &tl);
    temp_str_record[i].next = start - 1;
    unreg(mark);
    return i;
  }
}


/********************************************************
 *			GET_TEMP_STR			*
 ********************************************************
 * Return the string in record k, and mark it unused.	*
 * 							*
 * YOU MUST BE VERY CAREFUL WITH THIS.  IT DOES NOT	*
 * COPY THE BUFFER.  DO NOT DO ANY ALLOCATIONS WITH	*
 * NEW_TEMP_STR UNTIL THIS STRING IS NO LONGER NEEDED.  *
 ********************************************************/

charptr get_temp_str(int k)
{
  struct temp_str_record *ts;

  if(k < 0) return "";
  ts = temp_str_record + k;
  ts->in_use = 0;
  free_temp_buffer(ts->i, ts->j);
  return ts->next + 1;
}


/********************************************************
 *			GRAB_TEMP_STR			*
 ********************************************************
 * Return the string in record k, but do not mark that	*
 * string as unused.					*
 * 							*
 * YOU MUST BE VERY CAREFUL WITH THIS.  IT DOES NOT	*
 * COPY THE BUFFER.  IT ALLOWS YOU TO CHANGE THE 	*
 * CHARACTERS IN PLACE.					*
 ********************************************************/

charptr grab_temp_str(int k)
{
  if(k < 0) return "";
  return temp_str_record[k].next + 1;
}


/********************************************************
 *			REMOVE_LEADING			*
 ********************************************************
 * Remove all leading occurrences of character c from 	*
 * buffer k.						*
 ********************************************************/

void remove_leading(int k, char c)
{
  struct temp_str_record *ts;
  charptr *next;

  if(k < 0) return;

  ts = temp_str_record + k;
  next = &(ts->next);
  while((*next)[1] == c) (*next)++;
}


/********************************************************
 *			REMOVE_TRAILING			*
 ********************************************************
 * Remove all trailing occurrences of character c from 	*
 * buffer k.						*
 ********************************************************/

void remove_trailing(int k, char c)
{
  struct temp_str_record *ts;
  charptr start, nd, p;

  if(k < 0) return;

  ts    = temp_str_record + k;
  start = ts->next;
  nd    = ts->where + ts->size - 2;
  p     = nd;
  while(p > start && *p == c) p--;
  if(p != nd) {
    while(p > start) *(nd--) = *(p--);
    ts->next = nd;
  }
}


/********************************************************
 *			CONS_TEMP_STR			*
 ********************************************************
 * Place ch at the start of the temporary string in	*
 * record k.  This might cause the record to be moved.	*
 * Return the index of the new record.			*
 ********************************************************/

int cons_temp_str(char ch, int k)
{
  struct temp_str_record *ts, *new_ts;
  charptr *next, cpy_to;
  LONG size;
  int new_rec;

  if(k < 0) return -1;
  ts = temp_str_record + k;
  next = &(ts->next);

  /*-------------------------------------*
   * If necessary, move to a new buffer. *
   *-------------------------------------*/

  if(*next < ts->where) {
    size     = ts->size;
    new_rec  = new_temp_str(size + 1);
    if(new_rec < 0) return 0;

    new_ts = temp_str_record + new_rec;
    cpy_to = new_ts->where + new_ts->size - size;
    strcpy((char *)cpy_to, (char *)ts->where);
    next  = &(new_ts->next);
    *next = cpy_to - 1;
    get_temp_str(k);  /* Free k. */
    k = new_rec;
  }

  /*------------------------*
   * Install the character. *
   *------------------------*/

  **next = ch;
  --(*next);
# ifdef NEVER
  *((*next)--) = ch; /*-- doesn't compile correctly under Borland!!  Done
		        above.*/
# endif
  return k;
}


/********************************************************
 *			MULTICONS_TEMP_STR		*
 ********************************************************
 * Copy string s to the front of the string at record	*
 * k.  This might cause the record to move.		*
 * Return the index of the new record.			*
 ********************************************************/

int multicons_temp_str(char *s, int k)
{
  char *p;

  for(p = s; *p != '\0'; p++) /* Nothing */;
  for(p--; p >= s; p--) k = cons_temp_str(*p, k);
  return k;
}


/********************************************************
 *			CAT_TEMP_STR			*
 ********************************************************
 * Copy the string at record i to the front of the 	*
 * string at record k, and free record i.  Return the	*
 * index of the new record.				*
 ********************************************************/

int cat_temp_str(int i, int k)
{
  struct temp_str_record *ts;
  charptr s, p;

  if(i < 0) return -1;
  ts = temp_str_record + i;
  s = ts->next;
  p = ts->where + ts->size - 2;
  for(; p > s; p--) {
    k = cons_temp_str(*p, k);
  }
  get_temp_str(i);  /* Free i. */
  return k;
}
