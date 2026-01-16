/****************************************************************
 * File:    show/gprint.c
 * Purpose: Print on either standard output or window
 * Author:  Karl Abrahamson
 ****************************************************************/

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
 * The interpreter needs to be able to print to files or to windows in  *
 * a uniform way.  This file provides that uniformity.  A window is     *
 * given as a small numbered FILE* value.				*
 ************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../misc/misc.h"
#ifdef SMALL_STACK
#  ifdef USE_MALLOC_H
#    include <malloc.h>
#  else
#    include <stdlib.h>
#  endif
#  include "../alloc/allocate.h"
#endif
#include "../show/gprint.h"

extern int in_visualizer;

#ifdef MSWIN
extern void out_str(char *str);
#endif

/****************************************************************
 *			VGPRINTF				*
 ****************************************************************
 * See gprintf.  This version uses a va_list for args.		*
 ****************************************************************/

#define TOT_N_WINS 0

PRIVATE void vgprintf(FILE *where, char *fmt, va_list args)
{
  if(tolong(where) <= TOT_N_WINS) {
#   ifdef SMALL_STACK
      char* str = (char *) BAREMALLOC(MAX_GPRINT_LEN + 1);
#   else
      char str[MAX_GPRINT_LEN + 1];
#   endif

    vsprintf(str, fmt, args);

#   ifdef MSWIN
      out_str(str);
#   else
#     ifdef NEVER
        if(in_visualizer) {
          if(where == NULL) where = (FILE *) INTERACT_WIN;
          print_wm(toint(where), str);
        }
        else where = stderr;
#     else
        where = stderr;
#     endif
#   endif

#   ifdef SMALL_STACK
      FREE(str);
#   endif
  }

  else {
    vfprintf(where, fmt, args);
  }
}
  

/****************************************************************
 *			GPRINTF					*
 ****************************************************************
 * Do fprintf(where, fmt, ...), but if where is a window number,*
 * then instead do output to that window.  If output is to a    *
 * window, then to total number of characters must be less than *
 * MAX_GPRINT_LEN.						*
 ****************************************************************/

void gprintf(FILE *where, char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  vgprintf(where, fmt, args);
  va_end(args);
}


/****************************************************************
 *			GPRINT_STR				*
 ****************************************************************
 * Do gprintf(where, "%s", s, k), but print the first k		*
 * characters of s, and be careful not to overflow the buffer	*
 * if s is a long string.					*
 *								*
 * If k is -1, then print all of s, up to a terminating null	*
 * character.							*
 *								*
 * Return the number of characters printed.			*
 ****************************************************************/

int gprint_str(FILE* where, char* s, int k)
{
  char buffer[MAX_GPRINT_LEN + 1];
  LONG s_len = strlen(s);

  if(k < 0 || k > s_len) k = s_len;

  /*----------------------------------------------------------*
   * Pull off pieces of length MAX_GPRINT_LEN and print them. *
   *----------------------------------------------------------*/

  while(k > MAX_GPRINT_LEN) {
    strncpy(buffer, s, MAX_GPRINT_LEN);
    buffer[MAX_GPRINT_LEN] = 0;
    gprintf(where, "%s", buffer);
    s += MAX_GPRINT_LEN;
    k -= MAX_GPRINT_LEN;
  }

  /*------------------------------------------------------*
   * Now print the last piece, whose size must be at most *
   * MAX_GPRINT_LEN.  					  *
   *------------------------------------------------------*/

  if(k > 0) {
    strncpy(buffer, s, k);
    buffer[k] = 0;
    gprintf(where, "%s", buffer);
  }
  return k;
}


/****************************************************************
 *			GGPRINTF				*
 ****************************************************************
 * Do fprintf(f, fmt, b) or sprintf(f,fmt, b), according to 	*
 * whether f is a file or a string.  f is presumed to be a file	*
 * if its offset field is negative.  The total number of 	*
 * characters to print should be less than MAX_GPRINT_LEN.	*
 ****************************************************************/

void ggprintf(FILE_OR_STR *f, char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  if(f->offset < 0) vgprintf(f->u.file, fmt, args);
  else {
#   ifdef SMALL_STACK
      char* temp = (char *) BAREMALLOC(MAX_GPRINT_LEN + 1);
#   else
      char temp[MAX_GPRINT_LEN + 1];
#   endif
    int templen;

    vsprintf(temp, fmt, args);
    templen = strlen(temp);
    if(templen <= f->len - f->offset) {
      strcat(f->u.str + f->offset, temp);
      f->offset += templen;
    }

#   ifdef SMALL_STACK
      FREE(temp);
#   endif
  }
  va_end(args);
}

