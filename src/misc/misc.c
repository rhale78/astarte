/***********************************************************************
 * File:   misc/misc.c
 * Purpose: Miscellaneous functions.
 * Author: Karl Abrahamson
 ***********************************************************************/

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

#include <sys/stat.h>
#include <stdarg.h>
#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#if defined(MSWIN) && defined(MACHINE)
# include <windows.h>
  extern HWND MainWindowHandle;
#endif
#include "../alloc/allocate.h"
#ifdef TRANSLATOR
# include "../error/error.h"
#endif
#ifdef DEBUG
# include "../debug/debug.h"
#endif
void clean_up_and_exit(int);
void va_die(int n, va_list args);

/********************************************************
 *			Functions for debugging		*
 ********************************************************/

#ifdef DEBUG
void breakhere(void) {}

void flushTrace(void) {
  if(TRACE_FILE != NULL) fflush(TRACE_FILE);
}
#endif

/****************************************************************
 *			     FNL				*
 ****************************************************************
 * Print a newline on file f.					*
 ****************************************************************/

void fnl(FILE *f) {putc('\n', f);}

/****************************************************************
 *			     FINDENT				*
 ****************************************************************
 * Print n spaces on file f.					*
 ****************************************************************/

void findent(FILE *f, int n)
{
  int i;
  for(i=1; i<=n; i++) putc(' ',f);
}


/****************************************************************
 *			DIE					*
 *			VA_DIE					*
 *			PRINT_DIE_MSG				*
 ****************************************************************
 * die(n) causes the program to die a horrible death, printing	*
 * message n.  print_die_msg(n) prints message number n in the	*
 * die-message file.  If the die-message file can't be opened,	*
 * a generic message is printed.				*
 *								*
 * die admits optional parameters that are used in the printf   *
 * call in print_die_msg.  They are parameters to format	*
 * die_msg[n].							*
 ****************************************************************/

PRIVATE void print_die_msg(int n, va_list args)
{
  /*--------------------------------------------------------------*
   * Get the message to display. This involves looking in the die *
   * message file at line n.					  *
   *--------------------------------------------------------------*/

  char msg[100];
  FILE *f;
  strcpy(msg, MESSAGE_DIR);
  strcat(msg, DIE_MSG_FILE);
  f = fopen(msg, TEXT_READ_OPEN);
  if(f != NULL) {
    int i,c;
    char *p;

    /*----------------------------------------*
     * Skip over lines until line n is found. *
     *----------------------------------------*/

    for(i = 0; i < n; i++) {
      while((c = fgetc(f)) != '\n' && c != EOF) {}
    }

    /*----------------------------------------------------*
     * Read the message, skipping the first 4 characters. *
     * (The first 4 characters of each line are used for  *
     * numbering the line.)				  *
     *----------------------------------------------------*/

    for(i = 0; i < 4; i++) fgetc(f);
    p = msg;
    while((c = fgetc(f)) != '\n' && c != EOF) {
      *(p++) = (c == '\\') ? '\n' : c;
    }
    *p = '\0';
    fclose(f);
  }
  else if(n == 136) {
    strcpy(msg, "Cannot open file %s\n");
  }
  else {
    strcpy(msg, "An internal error has occurred!");
  }

  /*----------------------*
   * Display the message. *
   *----------------------*/

# ifdef TRANSLATOR
#   ifdef UNIX
      vfprintf(ERR_FILE, msg, args);
      fputc('\n',ERR_FILE);
#   endif
#   ifdef MSWIN
      {FILE *death_file;
       char* death_file_name = 
        (char*) BAREMALLOC(strlen(MESSAGE_DIR) + strlen(DEATH_FILE_NAME) + 1);
       strcpy(death_file_name, MESSAGE_DIR);
       strcat(death_file_name, DEATH_FILE_NAME);
       death_file = fopen(death_file_name, "w");
       FREE(death_file_name);
       if(death_file != NULL) {
	 vfprintf(death_file, msg, args);
	 fputc('\n', death_file);
       }
      }
#   endif
# endif

# ifdef MACHINE
#   ifdef UNIX
      vfprintf(stderr, msg, args);
      fputc('\n', stderr);
#   endif
#   ifdef MSWIN
      {char fullmsg[150];
       vsprintf(fullmsg, msg, args);
       MessageBox(MainWindowHandle, fullmsg, "Astr Error", 
		  MB_ICONEXCLAMATION | MB_OK);
      }
#   endif
# endif
}   

/*----------------------------------------------------------------*/

void va_die(int n, va_list args)
{
  print_die_msg(n,args);
  clean_up_and_exit(1);
}

/*----------------------------------------------------------------*/

void die(int n, ...)
{
  va_list args;

  va_start(args, n);
  print_die_msg(n,args);
  clean_up_and_exit(1);
}


/*******************************************************************
 *			INIT_ERR_MSGS				   *
 *******************************************************************
 * Read a message file named file_name, having num_lines lines.    *
 * *which should be set to point to the allocated array.	   *
 *								   *
 * The messages file consists of lines where the first four	   *
 * characters of each line are ignored, and are intended to hold   *
 * the line number.  Lines whose first character is #, and	   *
 * empty lines, are ignored.   If there are not four characters    *
 * in the line, the line is considered to be a null message.       *
 *******************************************************************/

void init_err_msgs(char ***which, char *file_name, int num_lines)
{
  int c;
  unsigned int buf_size, i, j, k;
  struct stat stat_buf;
  char *buf;

  /*----------------*
   * Open the file. *
   *----------------*/

  FILE *f = fopen(file_name, TEXT_READ_OPEN);
  if(f == NULL) {
    die(136, file_name);
  }

  /*-----------------------------------------------------------------*
   * Allocate two arrays.  *which is an array of char* values, where *
   * ((*which)[i]) points to the i-th line of the file.  buf is a    *
   *  buffer where the characters of the lines are stored. 	     *
   *-----------------------------------------------------------------*/

  fstat(fileno(f), &stat_buf);
  buf_size = (unsigned int)(stat_buf.st_size - 4*num_lines + 4);
  buf = (char *) alloc(buf_size);
  *which = (char **) alloc(num_lines * sizeof(char *));

  /*------------------------------------------------------------*
   * Copy the lines from the file into the buffer, and set the  *
   * pointers in *which.					*
   *------------------------------------------------------------*/

  j = k = c = 0;
  for(;;) {
    c = getc(f);
    if(c == EOF) break;

    /*---------------------------------------------------*
     * Ignore lines that start with '#' and empty lines. *
     *---------------------------------------------------*/

    if(c == '#' || c == '\n') {
      while (c != '\n' && c != EOF) c = getc(f);
      continue;
    }

    /*-----------------------------------------------------------------*
     * Install the pointer to the current buffer position into *which. *
     *-----------------------------------------------------------------*/

    (*which)[k] = buf + j;

    /*------------------------------------------------------------------*
     * Skip over the first four characters of the line.  We have done   *
     * one read, above, so do three more.  If there are not four	*
     * characters, make a null entry.				        *
     *------------------------------------------------------------------*/

    for(i = 0; i < 3; i++) {
      if(getc(f) == '\n') goto null_terminate_the_line;
    }

    /*--------------------------------*
     * Copy the line into buffer buf. *
     *--------------------------------*/

    for(;;) {
      c = getc(f);
      if(c == '\n' || c == EOF) break;
      if(c == '\\') c = '\n';
      buf[j++] = c;
    }

 null_terminate_the_line:
    buf[j++] = '\0';

    if(j > buf_size) {
      fclose(f);
      die(137, file_name);
    }
    if(k >= (unsigned) num_lines) {
      fclose(f);
      die(168, file_name, k);
    }
    k++;
  }
  fclose(f);
}


/********************************************************
 *			Memory functions		*
 ********************************************************
 * These are here because I want to use this with	*
 * 16-bit windows, and the memset and memcpy functions	*
 * there are no good.					*
 ********************************************************/

#ifdef BITS16

void longmemset(void HUGEPTR s, int c, long n)
{
  if(n <= UINT_MAX) memset(s, c, (size_t)(n));
  else {
    register long m = n;
    register long i = 0;

    while(m > 0) {
      register size_t k = (m > UINT_MAX) ? UINT_MAX : (size_t) m;
      memset(((charptr) s)+i, c, k);
      m -= k;
      i += k;
    }
  }
}

/*----------------------------------------------------------------*/

void longmemcpy(void HUGEPTR dest, void HUGEPTR src, long n)
{
  if(n <= UINT_MAX) memcpy(dest, src, (size_t)(n));
  else {
    register long m = n;
    register long i = 0;

    while(m > 0) {
      register size_t k = (m > UINT_MAX) ? UINT_MAX : (size_t) m;
      memcpy(((charptr) dest)+i, ((charptr) src)+i, k);
      m -= k;
      i += k;
    }
  }
}

#endif

