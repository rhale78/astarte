/*********************************************************************
 * File:    rts/io.c
 * Purpose: Implement input and output functions
 * Author:  Karl Abrahamson
 *********************************************************************/

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
 * This file is concerned with implementing the read and write 		*
 * operations for the interpreter.  Here, we deal with			*
 *    evaluating lazy files;						*
 *    outfiles;								*
 *    and printing to files.						*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#ifdef UNIX
# include <sys/types.h>
# include <unistd.h>
# include <fcntl.h>
# ifndef O_NONBLOCK
#   define O_NONBLOCK O_NDELAY
# endif
#endif
#ifdef MSWIN
# include <io.h>
# include <fcntl.h>
# include <sys/stat.h>
# include <memory.h>
# ifdef USE_MALLOC_H
#   include <malloc.h>
# else
#   include <stdlib.h>
# endif
#endif
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../tables/tables.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../show/gprint.h"
#include "../show/prtent.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/************************************************************************
 * 			file_table					*
 ************************************************************************
 * file_table is an array of structures that describe open files.     	*
 *									*
 * File-entities (tag FILE_TAG) contain a pointer to a structure that 	*
 * describes some aspects of a file, such as the name of the          	*
 * file.  That structure in turn contains the index of one of the     	*
 * structures in file_table, which gives information about the open   	*
 * file descriptor.						      	*
 * 								      	*
 * file_table is only used here and in the garbage collector, where   	*
 * it is scanned.						      	*
 ************************************************************************/

struct filed file_table[MAX_OPEN_FILES];


/******************************************************************
 *			stdin_filed				  *
 *			keyboard_buffer_chain			  *
 *			keyboard_buffer_end			  *
 ******************************************************************
 * Stdin is handled going directly to the keyboard in MSWIN, and  *
 * by using stdin in Unix.					  *
 ******************************************************************/

#ifdef MSWIN
  extern void flush_console_buff(void);
  struct keyboard_buffer* keyboard_buffer_chain = NULL;
  struct keyboard_buffer* keyboard_buffer_end = NULL;
#else
  PRIVATE struct filed stdin_filed;
#endif


/******************************************************************
 *			PRIVATE VARIABLES			  *
 ******************************************************************/

/****************************************************************
 *			stdout_filed				*
 *			stderr_filed				*
 ****************************************************************
 * stdout_file and stderr_file refer to the standard output and *
 * standard error file descriptors, which are build in init_io. *
 ****************************************************************/

PRIVATE struct filed stdout_filed, stderr_filed;


/****************************************************************
 *			next_file_stamp				*
 ****************************************************************
 * File stamps are used to indicate use times.  next_file_stamp *
 * is the next number to stamp a file with.  It is incremented  *
 * each time it is used.					*
 ****************************************************************/

PRIVATE LONG next_file_stamp = 0;


/********************************************************
 *			GET_FILE_MODE			*
 ********************************************************
 * Return the file mode for open calls from mode list   *
 * modes.  modes can contain 				*
 *							*
 *   volatileMode  = VOLATILE_INDEX_FM			*
 *   appendMode    = APPEND_INDEX_FM			*
 *   binaryMode    = BINARY_INDEX_FM			*
 * 							*
 * The defaults are nonvolatile, non-append and text.	*
 ********************************************************/

PRIVATE int get_file_mode(ENTITY modes)
{
  int modes_as_int = 0;

  while(!IS_NIL(modes)) {
    modes_as_int |= 1 << toint(IVAL(ast_head(modes)));
    modes = remove_indirection(ast_tail(modes));
  }
  return modes_as_int;
}


/****************************************************************
 *			NEW_FILED				*
 ****************************************************************
 * Enter file entity e into the file table, attached to file	*
 * descriptor fd.  Return the index in the file table where it  *
 * was entered.  If there is no index available, return -1 and 	*
 * set failure = TOO_MANY_FILES_EX.				*
 ****************************************************************/

PRIVATE int new_filed(struct file_entity *e, int fd)
{
  LONG stmin, st;
  int i, minpos, result;
  char kind, mode;
  struct filed *fild;

  /*------------------------------------------------------------------*
   * Try to find an empty record.  This loop sets result to the index *
   * found.  If no index is found, it sets minpos to the index of the *
   * least recently used file descriptor, and stmin to the time-stamp *
   * stored with index.						      *
   *------------------------------------------------------------------*/

  stmin  = LONG_MAX;
  minpos = -1;
  for(i = 0; i < MAX_OPEN_FILES; i++) { 

    /*--------------------------------------------------------------*
     * If the i-th entry has an fd field of -1, then that entry is  *
     * vacant, so use it.  We can only use outfiles and nonvolatile *
     * infiles, since we need to hang onto volatile infiles.	    *
     *--------------------------------------------------------------*/

    if(file_table[i].fd == -1) {result = i; goto out;}
    kind = file_table[i].file_rec->kind;
    mode = file_table[i].file_rec->mode;
    if(kind == OUTFILE_FK || (kind == INFILE_FK && !IS_VOLATILE_FM(mode))) {
      st = file_table[i].stamp;
      if(st < stmin) {
	stmin  = st;
	minpos = i;
      }
    }
  }

  /*-------------------------------------------------------------------*
   * There is no empty record.  If there is none that can be replaced, *
   * then we are out of luck.					       *
   *-------------------------------------------------------------------*/

  if(minpos < 0) {
    failure = TOO_MANY_FILES_EX;

#   ifdef DEBUG
      if(trace) {
	struct file_entity *fent;
	trace_i(223);
	for(i = 0; i < MAX_OPEN_FILES; i++) {
	  trace_i(224,
		 file_table[i].fd, toint(file_table[i].delay),
		 file_table[i].stamp);
	  fent = file_table[i].file_rec;
	  if(fent == NULL) trace_i(225);
	  else {
	    trace_i(226, fent->u.file_data.name);
	    trace_i(227, toint(fent->kind), fent->u.file_data.pos,
		    toint(fent->descr_index));
	  }
	}
      }
#   endif

    return -1;

  } /* end if(minpos < 0) */

  /*-------------------------------------------------------------*
   * Here, we replace the least-recently used replaceable entry. *
   *-------------------------------------------------------------*/

  fild = file_table + minpos;
  kind = fild->file_rec->kind;

  /*--------------------------------------------------------------------*
   * The file record (to which a file entity refers) has the index of   *
   * this filed record in it.  We need to set that index to -1, 	*
   * indicating that the file entity has no associated filed descriptor.*
   *--------------------------------------------------------------------*/

  fild->file_rec->descr_index = -1;

  /*------------------------------------------------------------------*
   * Close the file that we are replacing, and clear out this record. *
   *------------------------------------------------------------------*/

  if(kind == OUTFILE_FK && fild->file != NULL) fclose(fild->file);
  else if(kind == INFILE_FK) close(fild->fd);
  fild->fd = -1;
  fild->file = NULL;
  result = minpos;

 out:

  /*----------------------------------*
   * Store this file at index result. *
   *----------------------------------*/

  fild           = file_table + result;
  fild->fd       = fd;
  fild->stamp    = next_file_stamp++;
  fild->file_rec = e;
  fild->delay    = fild->was_blocked = FALSE;
  return result;
}


/****************************************************************
 *			GET_FILED				*
 ****************************************************************
 * Return a struct filed for file record r.  k is READ to	*
 * read from the file, and WRITE to write to the file.		*
 * Return NULL if no file record can be obtained.		*
 ****************************************************************/

PRIVATE struct filed* get_filed(struct file_entity *r, int k)
{
  int kind = r->kind;
  int index, fd, option;
  struct filed* result = NULL;

# ifdef GCTEST
    if(r->mark != 0) badrc("file record", r->mark, (char *) r);
# endif

  /*---------------------------------------------------------------*
   * Check that the request is reasonable. Also, handle stdout and *
   * stderr as special cases.					   *
   *---------------------------------------------------------------*/

# ifdef MSWIN
   if(kind == STDIN_FK)  {if(k == READ)  return NULL;  else goto fail;}
# else
   if(kind == STDIN_FK)  {if(k == READ)  return &stdin_filed;  else goto fail;}
# endif
  if(kind == STDOUT_FK) {if(k == WRITE) return &stdout_filed; else goto fail;}
  if(kind == STDERR_FK) {if(k == WRITE) return &stderr_filed; else goto fail;}
  if(kind == NO_FILE_FK) {
    failure = CLOSED_FILE_EX;
    return NULL;
  }

  /*------------------------------------------------------------*
   * If this file already has an active index, then return the  *
   * file descriptor at that index.  Stamp this entry as most   *
   * recently used.						*
   *------------------------------------------------------------*/

  index = r->descr_index;
  if(index >= 0) {
    result = file_table + index;
    result->stamp = next_file_stamp++;

#   ifdef DEBUG
      if(trace_extra) {
	trace_i(228, kind, index, result->fd);
      }
#   endif

    if(result->fd != -1) {
      if(kind == OUTFILE_FK) {
	if(k == WRITE) return result; else goto fail;
      }
      else if(kind == INFILE_FK) {
	if(k == READ) return result; else goto fail;
      }
    }
  }

# ifdef DEBUG
    if(trace_extra) {
      trace_i(229, kind, index, file_table[index].fd);
      if(index == 0) fprintf(TRACE_FILE, "%d\n", result->fd);
    }
# endif

  /*---------------------------------------------------------*
   * If this file does not currently have a file descriptor, *
   * then get one.  Start by opening the file.		     *
   *---------------------------------------------------------*/

  if(r->u.file_data.name == NULL) goto fail;
  option = (k == READ) ? O_RDONLY : O_WRONLY | O_APPEND;
# ifdef MSWIN
    option |= (IS_TEXT_FM(r->mode)) ? O_TEXT : O_BINARY;
# endif

  fd = open_file(r->u.file_data.name, option, 0);
  if(fd < 0) goto fail;

# ifdef UNIX
    fcntl(fd, F_SETFD, 1);  /* close on exec */
# endif

  /*----------------------------------------------*
   * Get the new file descriptor, and install fd. *
   *----------------------------------------------*/

  if(index < 0) {
    r->descr_index = index = new_filed(r,fd);
    if(index < 0) {close(fd); return NULL;}
    result = file_table + index;
  }
  else {
    result->fd = fd;
    result->delay = result->was_blocked = 0;
    result->stamp = next_file_stamp++;
  }

  /*------------------------------------------------------------*
   * For an outfile, we will use the FILE* operations, such as  *
   * fprintf, rather that direct writes.  So open fd as a FILE* *
   * value.							*
   *------------------------------------------------------------*/

# ifdef MSWIN
    {char *optstr = (IS_TEXT_FM(r->mode)) ? "a" : "ab";
     if(k == WRITE) file_table[index].file = fdopen(fd, optstr);
    }
# else
    if(k == WRITE) file_table[index].file = fdopen(fd, "a");
# endif

  /*--------------------------------------------------------------------*
   * If we are opening an infile, then we might be opening a previously *
   * opened file, partway through the file.  Do a seek to the correct   *
   * file position.							*
   *--------------------------------------------------------------------*/

  if(k == READ) {
#   ifdef DEBUG
       if(trace_extra) trace_i(230, r->u.file_data.pos);
#   endif

    if(r->u.file_data.pos != 0
       && lseek(fd, r->u.file_data.pos, SEEK_SET) < 0) {
      failure = CANNOT_SEEK_EX;
#     ifdef DEBUG
	if(trace_extra) trace_i(231);
#     endif
      return NULL;
    }
  }
  return result;

  /*-----------------------------------------------------------------*
   * At a failure, set the exception to NO_FILE_EX, and return NULL. *
   *-----------------------------------------------------------------*/

fail:
# ifdef DEBUG
    if(trace_extra) {
      trace_i(232, kind, k);
    }
# endif
  failure = NO_FILE_EX;
  failure_as_entity = qwrap(NO_FILE_EX, make_str(r->u.file_data.name));
  return NULL;
}


/********************************************************
 *		        FILE_FROM_ENTITY		*
 ********************************************************
 * Return the file that entity f refers to, or NULL if  *
 * can't get it.  For visualizer, return a window 	*
 * number.  Use only for outfiles.			*
 ********************************************************/

FILE* file_from_entity(ENTITY f)
{
  struct file_entity *fent;
  struct filed *fild;
  FILE *result;
  int kind;

  fent = FILEENT_VAL(f);
  fild = get_filed(fent, WRITE);
  if(failure >= 0) return NULL;
  result = fild->file;
  kind = fent->kind;
# ifdef NEVER
    if(in_visualizer && (kind == STDOUT_FK || kind == STDERR_FK)) {
      result = (FILE *) INTERACT_WIN;
    }
# endif
  return result;
}


/********************************************************
 *			PRINT_FIRST_CHAR		*
 ********************************************************
 * Print the first character of string a on file f, and *
 * return the tail of a.  List a must be nonempty, and  *
 * must be evaluated up to the point where it can be    *
 * split.  The tail will be evaluated up to its         *
 * head/tail before returning.				*
 * 							*
 * The head of a will be evaluated here.  If evaluation *
 * times out, then the head will not be printed, and    *
 * print_first_char will return a, not the tail of a.   *
 * l_time is the time for evaluation.			*
 ********************************************************/

PRIVATE ENTITY print_first_char(FILE *f, ENTITY a, LONG *l_time)
{
  ENTITY a_head, a_tail;
  REG_TYPE mark = reg2(&a_head, &a_tail);
  ast_split(a, &a_head, &a_tail);
  IN_PLACE_EVAL(a_head, l_time);
  if(failure < 0) {
    gprintf(f, "%c", CHVAL(a_head));
    IN_PLACE_EVAL(a_tail, l_time);
    unreg(mark);
    return a_tail;
  }
  else {
    unreg(mark);
    return a;
  }
}


/********************************************************
 *			AST_PRINT_STR			*
 ********************************************************
 * Print string e on file f.  Return any part that did	*
 * not get printed due to a timeout.  Print at most     *
 * max_chars characters, and set *chars_printed to the  *
 * number of characters printed.			*
 ********************************************************/

ENTITY ast_print_str(FILE *f, ENTITY e, LONG *l_time,
		     LONG max_chars, LONG *chars_printed)
{
  ENTITY a, *ap, q;
  int tag;
  LONG num_printed = 0;
  REG_TYPE mark;

  if(max_chars <= 0) return e;

  mark = reg2(&a, &q);

  /*------------------------------------------------------------*
   * a is the part of e that has not yet been printed.		*
   *								*
   * We need to keep the head of a available, by evaling it.    *
   * Note that evaluation of a is the only place where *l_time  *
   * is decremented.  If we are asked to print a long explicit  *
   * list, we will print it all.				*
   *------------------------------------------------------------*/

  SET_EVAL(a, e, l_time);  /* a = eval(e,l_time); */

  while(!IS_NIL(a) && failure < 0) {
    if(num_printed >= max_chars) break;

    tag = TAG(a);

    /*--------------------------------------------*
     * At an append x ++ y, print x then print y. *
     *--------------------------------------------*/

    if(tag == APPEND_TAG) {
      LONG cp;
      ap = ENTVAL(a);
      q  = ast_print_str(f, ap[0], l_time, max_chars - num_printed, &cp);
      num_printed += cp;
      ap = ENTVAL(a);   /* In case ast_print_str did a GC */

      /*--------------------------------------------------------------*
       * If something was left over after doing ap[0], then defer all *
       * of ap[1].						      *
       *--------------------------------------------------------------*/

      if(!IS_NIL(q)) {
	q = quick_append(q, ap[1]);
        *chars_printed = num_printed;
	unreg(mark);
	return q;
      }

      /*-----------------------------------------------------*
       * Tail recur: return ast_print_str(f, ap[1], l_time). *
       *-----------------------------------------------------*/

      SET_EVAL(a, ap[1], l_time);  /* a = eval(ap[1], l_time); */
    }

    /*-------------------------------------------------*
     * At a STRING_TAG, print the characters of the    *
     * string. 					       *
     *-------------------------------------------------*/

    else if(tag == STRING_TAG) {
      CHUNKPTR chunk = BIVAL(a);
      char* p = STRING_BUFF(chunk);
      LONG  k = STRING_SIZE(chunk);
      LONG  n = max_chars - num_printed;
      if(n > k) n = k;
      gprint_str(f, p, n);
      num_printed += n;
      unreg(mark);
      *chars_printed = num_printed;
      if(n >= k) return nil;
      else {
        return ast_sublist1(a, n, k-n, l_time, 0, 0);
      }
    }

    /*-------------------------------------------------*
     * At a CSTR_TAG, print the characters of the      *
     * string. 					       *
     *-------------------------------------------------*/

    else if(tag == CSTR_TAG) {
      int a_len;
      char* a_str = CSTRVAL(a);
      LONG n = gprint_str(f, a_str, max_chars - num_printed);
      num_printed += n;
      *chars_printed = num_printed;
      unreg(mark);
      a_len = strlen(a_str);
      if(a_len == n) return nil;
      else return ast_sublist1(a, n, a_len - n, l_time, 0, 0);
    }

    /*----------------------------------------------------------*
     * At an ARRAY_TAG/STRING_TAG, print the characters of the	*
     * string, followed by the follower.			*
     *----------------------------------------------------------*/

    else if(tag == ARRAY_TAG) {
      ENTITY* hd = ENTVAL(a);
      ENTITY bdy = hd[1];
      if(TAG(bdy) == STRING_TAG) {
        LONG     len    = IVAL(hd[0]);
	LONG     offset = IVAL(hd[3]);
	CHUNKPTR chunk  = BIVAL(bdy);
        char*    p      = STRING_BUFF(chunk);
        LONG     n      = max_chars - num_printed;
        if(n > len) n = len;
        gprint_str(f, p + offset, n);
        num_printed += n;

	if(n < len) {
	  *chars_printed = num_printed;
          unreg(mark);
          return ast_sublist1(a, n, len-n, l_time, 0, 0);
	}

        /*---------------------------------*
         * Now continue with the follower. *
         *---------------------------------*/

        a = hd[2];
        IN_PLACE_EVAL(a, l_time);
      }

      else { /* ARRAY_TAG/t where t is not STRING_TAG */
        a = print_first_char(f, a, l_time);
        num_printed++;
      }
    }

    /*-----------------------------------------------------------*
     * For other kinds of strings, print the head and tail-recur *
     * on the tail.						 *
     *-----------------------------------------------------------*/

    else {
      a = print_first_char(f, a, l_time);
      num_printed++;
    }
  }

  *chars_printed = num_printed;
  unreg(mark);
  return a;
}


/**********************************************************
 *			AST_PRINT_LIST			  *
 **********************************************************
 * Print list of strings e on file f.  Return any part of *
 * list e that did not get printed due to a timeout.	  *
 **********************************************************/

ENTITY ast_print_list(FILE *f, ENTITY e, LONG *l_time)
{
  ENTITY p, q, h;
  LONG hold_time = *l_time;
  REG_TYPE mark = reg3(&p, &q, &h);

  /*-----------------------------------------------*
   * Reduce the time, to prevent a deep recursion. *
   *-----------------------------------------------*/

  if(hold_time > LIST_PRIM_TIME) *l_time = LIST_PRIM_TIME;

  SET_EVAL(p, e, l_time);  /* p = eval(e,l_time); */
  while(!IS_NIL(p) && failure < 0) {

    /*-----------------------*
     * Check for a time-out. *
     *-----------------------*/

    TIME_STEP(l_time);

    /*------------------------------------------*
     * If we did not time-out, then do a print. *
     *------------------------------------------*/

    if(failure < 0) {
      LONG cp;
      h = ast_head(p);
      q = ast_print_str(f, h, l_time, LONG_MAX, &cp);
      p = ast_tail(p);

      /*------------------------------------------------------------*
       * If the print timed out, then defer what was left over from *
       * that print plus the rest of the current list of strings.   *
       *------------------------------------------------------------*/

      if(!IS_NIL(q)) {
	/*-----------------------*
	 * Time out in the print *
	 *-----------------------*/
	p = ast_pair(q, p);
	break;
      }

      /*-----------------------------------*
       * Otherwise, eval p and tail recur. *
       *-----------------------------------*/

      IN_PLACE_EVAL(p, l_time);  /* p = eval(p, l_time); */
    }
  }

  unreg(mark);
  if(hold_time > LIST_PRIM_TIME) {*l_time += hold_time - LIST_PRIM_TIME;}
  return p;
}


/********************************************************
 *			AST_FPRINT			*
 ********************************************************
 * Print list of strings e on file f, and return	*
 * any part of e that did not get printed due to a 	*
 * timeout. Presumes that f is fully evaluated.		*
 ********************************************************/

ENTITY ast_fprint(ENTITY f, ENTITY e, LONG *l_time)
{
  FILE *ff;

  if(failure >= 0) return e;
  if(*l_time <= 0) {*l_time = 0; failure = TIME_OUT_EX; return e;}
  if(TAG(f) != FILE_TAG) die(98);
  
  ff = file_from_entity(f);
  if(ff == NULL) return e;
  return ast_print_list(ff, e, l_time);
}



/****************************************************************
 *			AST_FOPEN				*
 *			AST_FOPEN_STDF				*
 ****************************************************************
 * Open file s for writing, with given mode.			*
 * Presumes that s and modes are fully evaluated.  		*
 ****************************************************************/

ENTITY ast_fopen_stdf(ENTITY s, ENTITY modes)
{
  return ast_fopen(s, get_file_mode(modes));
}

/*----------------------------------------------------------*/

ENTITY ast_fopen(ENTITY s, int mode)
{
  int fd, index, option;
  ENTITY a, result = nil;
  struct file_entity *fent;
  char *fname;
# ifdef SMALL_STACK
    char* name = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char name[MAX_FILE_NAME_LENGTH + 1];
# endif

  /*--------------------------------------------------------------------*
   * Get the name of the file in char* form.  If the file name is too   *
   * long, then fail with exception LIMIT_EX.				*
   *--------------------------------------------------------------------*/

  copy_str(name, s, MAX_FILE_NAME_LENGTH, &a);
  if(ENT_NE(a, nil)) {
    failure = LIMIT_EX; 
    goto out; /* Returns nil */
  }
  force_internal(name);

  /*-------------------------------------------*
   * Select the open mode, and open file file. *
   *-------------------------------------------*/

  option = O_WRONLY | O_CREAT;
# ifdef MSWIN
    option |= (IS_TEXT_FM(mode)) ? O_TEXT : O_BINARY;
# endif
  option |= IS_APPEND_FM(mode) ? O_APPEND : O_TRUNC;
  fname = get_full_file_name(name, 0);
  fd = (fname == NULL) ? -1 : open_file(fname, option, 0660);

  /*-----------------------------------------------------------*
   * If the file could not be opened, then fail with exception *
   * NO_FILE_EX.					       *
   *-----------------------------------------------------------*/

  if(fd == -1) {
    failure = NO_FILE_EX;
    failure_as_entity = qwrap(NO_FILE_EX, s);
    goto out; /* Returns nil */
  }

# ifdef UNIX
    fcntl(fd, F_SETFD, 1);  /* close on exec */
# endif

  /*---------------------------------------*
   * Build the file entity, as an outfile. *
   *---------------------------------------*/

  fent = alloc_file_entity();
  fent->kind = OUTFILE_FK;
  fent->mode = mode;
  fent->u.file_data.val = nil;
  fent->u.file_data.name = make_perm_str(fname);

  /*--------------------------------------------*
   * Get a file-descriptor index for this file. *
   *--------------------------------------------*/

  index = fent->descr_index = new_filed(fent, fd);
  if(index < 0) {
    close(fd); 
    goto out; /* Returns nil */
  }

  /*-------------------------------------------------------------*
   * Open the file as a FILE* value, so that we can use fprintf. *
   *-------------------------------------------------------------*/

  {char stropt[3];
   strcpy(stropt, IS_APPEND_FM(mode) ? "a" : "w");
#  ifdef MSWIN
     if(IS_BINARY_FM(mode)) strcat(stropt, "b");
#  endif
   file_table[index].file = fdopen(fd, stropt);
  }

  result = ENTP(FILE_TAG, fent);

 out:
# ifdef SMALL_STACK
    FREE(name);
# endif
  return result;
}


/**********************************************************
 *		        FLUSH_FILED			  *
 **********************************************************
 * Flush the file referred to by fild, provided it is not *
 * null and does not refer to a window.			  *
 **********************************************************/

PRIVATE void flush_filed(struct filed *fild)
{
  ULONG file_as_int = (ULONG)(fild->file);
  if(file_as_int > TOT_N_WINS) fflush(fild->file);
# ifdef MSWIN
    else if(file_as_int == INTERACT_WIN) flush_console_buff();
# endif
}


/**********************************************************
 *		        FLUSH_STDOUT			  *
 **********************************************************
 * Flush stdout and stderr.				  *
 **********************************************************/

void flush_stdout(void)
{
  flush_filed(&stdout_filed);
  flush_filed(&stderr_filed);
# ifdef MSWIN
    flush_console_buff();
# endif

}


/**********************************************************
 *		        FLUSH_FILE_STDF			  *
 **********************************************************
 * Flush file f.  f must be fully evaluated.  Returns (). *
 **********************************************************/

ENTITY flush_file_stdf(ENTITY f)
{
  struct file_entity *fent;

  /*------------------------------------------------------------*
   * If we are being asked to flush an internal file, there is  *
   * nothing to do.						*
   *------------------------------------------------------------*/

  if(TAG(f) != FILE_TAG) return hermit;
  fent = FILEENT_VAL(f);
  if(fent->kind == STDOUT_FK) flush_filed(&stdout_filed);
  else if(fent->kind == STDERR_FK) flush_filed(&stderr_filed);
  else {
    int index = fent->descr_index;
    if(index >= 0) flush_filed(file_table + index);
  }
  return hermit;
}


/**********************************************************
 *		        CLOSE_FILED			  *
 **********************************************************
 * Close the file referred to by fild, provided it is not *
 * null and does not refer to a window.	 This is only	  *
 * used for outfiles.					  *
 **********************************************************/

PRIVATE void close_filed(struct filed *fild)
{
  if(((ULONG)(fild->file)) > TOT_N_WINS) fclose(fild->file);
}


/************************************************************
 *			AST_FCLOSE			    *
 ************************************************************
 * Close output file f.  Assumes that f is fully evaluated. *
 ************************************************************/

ENTITY ast_fclose(ENTITY f)
{
  struct file_entity *fent;
  int index;

  if(TAG(f) != FILE_TAG) die(99);
  fent = FILEENT_VAL(f);
  index = fent->descr_index;

  /*------------------------------------------------------------------*
   * If this file has an active index, then close down the file-table *
   * entry to which that index refers.  Otherwise, there is nothing   *
   * to do in the file table.					      *
   *------------------------------------------------------------------*/

  if(index >= 0) {
    close_filed(file_table + index);
    file_table[index].fd = -1;
    fent->descr_index = -1;
    file_table[index].file = NULL;
    file_table[index].file_rec = NULL;
  }

  /*-------------------------------*
   * Mark this file entity closed. *
   *-------------------------------*/

  fent->u.file_data.name = NULL;
  fent->kind = NO_FILE_FK;
  return hermit;
}


/************************************************************
 *			CLOSE_ALL_OPEN_FILES		    *
 ************************************************************
 * Close all open files in file_table.			    *
 ************************************************************/

void close_all_open_files(void)
{
  int i;
  for(i = 0; i < MAX_OPEN_FILES; i++) {
    if(file_table[i].fd >= 0) {
      if(file_table[i].file != NULL) fclose(file_table[i].file);
      else close(file_table[i].fd);
      file_table[i].fd = -1;
    }
  }

# ifndef MSWIN
    fclose(stdout);
    fclose(stderr);
# else
    close_filed(&stdout_filed);
    close_filed(&stderr_filed);
# endif
}


/****************************************************************
 *			AST_READ_FILE				*
 ****************************************************************
 * Return a list whose content is the contents of file fd,	*
 * named name.  mode is an 'or' of				*
 *	BINARY_FM		for binary (nothing for text)	*
 *	VOLATILE_FM		for a volatile file		*
 * Note: The returned list is lazy, and is a FILE_TAG entity.	*
 *								*
 * If this function is called with fd = -1, then it will cause  *
 * a failure with exception NO_FILE_EX.				*
 ****************************************************************/

ENTITY ast_read_file(int fd, int mode, char *name)
{
  struct file_entity *fent;
  int index;

  /*---------------------------------*
   * Check for failure to open file. *
   *---------------------------------*/

  if(fd == -1) {
    failure = NO_FILE_EX;
    failure_as_entity = qwrap(NO_FILE_EX, make_str(name));
    return nil;
  }

  /*-------------------------------------------------------*
   * Build the file entity. Get a file-table index for it. *
   *-------------------------------------------------------*/

  fent       = alloc_file_entity();
  fent->kind = INFILE_FK;
  fent->mode = mode;
  fent->descr_index = index = new_filed(fent, fd);
  if(index < 0) return nil;
  fent->u.file_data.pos = 0;
  fent->u.file_data.val = true_ent;
  fent->u.file_data.name = (name == NULL) ? NULL : make_perm_str(name);

  /*----------------------------------------------------------------*
   * We need to return an indirection to a volatile infile, but the *
   * file node itself is adequate for a nonvolatile file.	    *
   *----------------------------------------------------------------*/

  if(IS_VOLATILE_FM(mode)) {
    register ENTITY* p = allocate_entity(1);
    *p = ENTP(FILE_TAG, fent);
    return ENTP(INDIRECT_TAG, p);
  }
  else {
    return ENTP(FILE_TAG, fent);
  }
}


/********************************************************
 *			AST_INPUT_STDF			*
 ********************************************************
 * Return a list whose contents is the contents of	*
 * file s, with chosen modes.  Presumes that s and 	*
 * modes are fully evaluated.				*
 ********************************************************/

ENTITY ast_input_stdf(ENTITY s, ENTITY modes)
{
  return ast_input(s, get_file_mode(modes));
}


/****************************************************************
 *			AST_INPUT				*
 ****************************************************************
 * Return a list whose contents is the contents of file s.	*
 * s must be fully evaluated.  mode is the mode of the file to  *
 * be opened.  It should be a combination of VOLATILE_FM and	*
 * BINARY_FM. (Leaving them out leads to the default of text	*
 * and nonvolatile mode.)					*
 ****************************************************************/

ENTITY ast_input(ENTITY s, int mode)
{
  int fd;
  char *fname;
  ENTITY z, result = nil;
# ifdef SMALL_STACK
    char* name = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char name[MAX_FILE_NAME_LENGTH + 1];
# endif

  /*---------------------------------------------*
   * Get the name of the file as a char* string. *
   *---------------------------------------------*/

  copy_str(name, s, MAX_FILE_NAME_LENGTH, &z);
  force_internal(name);
  if(!IS_NIL(z)) {
    failure = LIMIT_EX;
    goto out; /* Returns nil */
  }

  /*--------------------------------------------*
   * Get the full path name, and open the file. *
   *--------------------------------------------*/

  fname = get_full_file_name(name, 0);
  if(fname == NULL) {
    fd = -1;
    fname = name;
  }
  else {
#   ifdef MSWIN
      {int openmode = O_RDONLY | (IS_TEXT_FM(mode) ? O_TEXT : O_BINARY);
       fd = open_file(fname, openmode, 0);
      }
#   else
       fd = open_file(fname, O_RDONLY, 0);
#   endif
  }

# ifdef UNIX
    if(fd >= 0) fcntl(fd, F_SETFD, 1);  /* close on exec. */
# endif

  /*-----------------------------------*
   * Get the file using ast_read_file. *
   *-----------------------------------*/

  result = ast_read_file(fd, mode, fname);
  if(failure >= 0 && fd >= 0) close(fd);

 out:
# ifdef SMALL_STACK
    FREE(name);
# endif
  return result;
}


/********************************************************
 *			FILE_EVAL			*
 ********************************************************
 * Evaluate file entity f.  Set is_infile=TRUE if f is 	*
 * an input file, and FALSE if f is an output file.	*
 ********************************************************/

ENTITY file_eval(ENTITY *f, Boolean *is_infile, LONG *l_time)
{
  int kind;
  struct file_entity *fent;

  fent = FILEENT_VAL(*f);

# ifdef GCTEST
    if(fent->mark != 0) badrc("file record", fent->mark, (char *) fent);
# endif

  kind = fent->kind;
  if(kind == INFILE_FK || kind == STDIN_FK) {
    *is_infile = TRUE;
    return input_eval(f, kind, fent->mode, l_time);
  }
  else {
    *is_infile = FALSE;
    return *f;
  }
}


/**********************************************************
 *		 GET_STDIN_CHARS			  *
 *		 PUT_STDIN_CHAR		  		  *
 *		 FREE_KEYBOARD_BUFFER			  *
 **********************************************************
 * Get_stdin_chars(buff,n) gets up to n characters from   *
 * the keyboard and puts them in buff.  It returns the    *
 * number of characters copied to buff on success, and	  *
 * -1 if there is  no character available.  This is	  *
 * similar to the action of 'read' on a nonblocking file. *
 *							  *
 * Put_stdin_char puts a character that has been typed at *
 * the keyboard into the buffer.			  *
 *							  *
 * Free_keyboard_buffer frees the space occupied by the   *
 * keyboard buffer.					  *
 **********************************************************/

#ifdef MSWIN
void put_stdin_char(unsigned int c)
{
  if(keyboard_buffer_end != NULL) {

    /*--------------------------*
     * Possibly get a new node. *
     *--------------------------*/

    if(keyboard_buffer_end->back == KEYBOARD_BUFFER_SIZE) {
      struct keyboard_buffer *newnode =
	(struct keyboard_buffer *) BAREMALLOC(sizeof(struct keyboard_buffer));
      memset(newnode, 0, sizeof(struct keyboard_buffer));
      keyboard_buffer_end->next = newnode;
      keyboard_buffer_end = newnode;
    }

    /*------------------------*
     * Install the character. *
     *------------------------*/

    keyboard_buffer_end->buffer[keyboard_buffer_end->back++] = (char) c;
  }
}

/*----------------------------------------------------------------*/

int get_stdin_chars(char *buff, int n)
{
  int i;

  /*-----------------------------------------*
   * Flush the console buffer print a prompt *
   *-----------------------------------------*/

  flush_console_buff();

  i = 0;
  while(i < n && keyboard_buffer_chain != NULL) {

    /*-----------------------------------------*
     * Might need to move to next buffer node. *
     *-----------------------------------------*/

    if(keyboard_buffer_chain->front == KEYBOARD_BUFFER_SIZE) {
      struct keyboard_buffer* next = keyboard_buffer_chain->next;
      if(next != NULL) {
	FREE(keyboard_buffer_chain);
	keyboard_buffer_chain = next;
      }
      else break;
    }

    /*--------------------------------------*
     * Check to see if a char is available. *
     *--------------------------------------*/

    if(keyboard_buffer_chain->front >= keyboard_buffer_chain->back) {
      break;
    }

    /*-------------------------------------------*
     * Get the char and update the chain.  Loop. *
     *-------------------------------------------*/

    buff[i++] = keyboard_buffer_chain->buffer[keyboard_buffer_chain->front++];

  } /* end while */

  /*-----------------------------------------------------*
   * If nothing is available, behave like read on a file *
   * that does not block -- return -1.			 *
   *-----------------------------------------------------*/

  if(i == 0) return -1;

  /*--------------------------------------------------*
   * Otherwise, return the number of characters read. *
   *--------------------------------------------------*/

  else return i;
}

/*--------------------------------------------------------------*/

void free_keyboard_buffer(void)
{
  struct keyboard_buffer *p;
  for(p = keyboard_buffer_chain; p != NULL;) {
    struct keyboard_buffer *next = p->next;
    FREE(p);
    p = next;
  }
  keyboard_buffer_chain = NULL;
}
#endif


/********************************************************
 *			INPUT_EVAL			*
 ********************************************************
 * Return the content of the input file at address loc, *
 * with the first part explicit.  Sets failure = 	*
 * TIME_OUT_EX and *l_time = 0 if input is blocked and 	*
 * there is more than one thread.			*
 ********************************************************/

#ifdef MSWIN
# define READ_BUFF(n, fild, fd, buff)\
    n = (fild == NULL) ? get_stdin_chars(buff, INPUT_BUFFER_SIZE)\
		       : read(fd, buff, INPUT_BUFFER_SIZE)
#else
# define READ_BUFF(n, fild, fd, buff)\
    n = read(fd, buff, INPUT_BUFFER_SIZE)
#endif

/*--------------------------------------------------------------*/

ENTITY input_eval(ENTITY *loc, int kind, int mode, LONG *l_time)
{
  unsigned char str[INPUT_BUFFER_SIZE + 1];
  int fd = 0, n, index;
  ENTITY ae, result, *ind;
  REG_TYPE mark;
  struct file_entity *fent, *newfent;
  struct filed *fild;

  /*---------------------------------------------------------------*
   * We must have a FILE_TAG entity, or there is trouble.  Get the *
   * struct file_entity to which this entity refers.		   *
   *---------------------------------------------------------------*/

  if(TAG(*loc) != FILE_TAG) die(100);
  fent = FILEENT_VAL(*loc);

# ifdef DEBUG
    if(trace_extra) {
      trace_i(233);
      trace_i(234, fent);
    }
# endif

  /*-----------------------------------------------------------*
   * If the val field of fent is anything except true, then it *
   * holds the value of this entity, so there is no need to    *
   * evaluate.						       *
   *							       *
   * When we use the node, clear its mark field to prevent it  *
   * from being cleared out by the next garbage collection.    *
   *-----------------------------------------------------------*/

  if(ENT_NE(fent->u.file_data.val, true_ent)) {
    fent->mark = 0;
    return *loc = fent->u.file_data.val;
  }

# ifdef DEBUG
    if(trace_extra) {
      trace_i(233);
      trace_i(235, num_threads, fent->descr_index);
    }
# endif

  /*-----------------------------------------------------*
   * Get a file descriptor for this file, if necessary.  *
   *-----------------------------------------------------*/

  fild = get_filed(fent, READ);
  if(failure >= 0) return nil;
  if(fild != NULL) fd = fild->fd;

  /*----------------------------------------------------------------------*
   * Update nonblocking status of the file.  If this is the only process, *
   * then we can afford to block.  Otherwise, don't block.                *
   *----------------------------------------------------------------------*/

# ifdef UNIX
    {int opt;
     if(num_threads == 1) {
       if(fild->delay) {
	 opt = fcntl(fd, F_GETFL, 0);
	 fcntl(fd, F_SETFL, opt & ~O_NONBLOCK);
	 fild->delay = FALSE;
       }
     }
     else {
       if(!(fild->delay)) {
	 opt = fcntl(fd, F_GETFL, 0);
	 fcntl(fd, F_SETFL, opt | O_NONBLOCK);
	 fild->delay = TRUE;
       }
     }
    }
# endif

  /*------------------------------------*
   * Flush any prompt that was printed. *
   *------------------------------------*/

  flush_stdout();

  /*-------------------------------------*
   * Read some characters from the file. *
   *-------------------------------------*/

  READ_BUFF(n, fild, fd, str);

  /*---------------------------------------------------------------*
   * If there is nothing left, then close the file and return nil. *
   *---------------------------------------------------------------*/

  if(n == 0) {

#   ifdef DEBUG
      if(trace_extra) {
	trace_i(233);
        trace_i(236, fd);
      }
#   endif

    if(fild != NULL) {    
      close(fd);
      fild->fd = -1;
      fild->file_rec = NULL;
    }
    fent->descr_index = -1;
    fent->u.file_data.val = nil;
    return *loc = nil;
  }

  /*----------------------------------------------------------------*
   * If input is blocked,  time out, and return the file. Indicate  *
   * that a timeout occurred due to blocked input.		    *
   *----------------------------------------------------------------*/

  if(n < 0) {

#   ifdef DEBUG
      if(trace_extra) {
        trace_i(233);
        trace_i(237);
      }
#   endif

    pause_this_thread(l_time);
    input_was_blocked = TRUE;
    if(fild != NULL) fild->was_blocked = 1;
    return *loc;
  }

  /*---------------------------------------------------------*
   * If we got at least one character, then build the result *
   *---------------------------------------------------------*/

  str[n] = '\0';   /* Null-terminate the string. */

  if(fild != NULL) fild->was_blocked = 0;       /* Wasn't blocked. */

  mark = reg2(&ae, &result);
  ae   = make_strn(str, n);

  /*------------------------------------------------------------*
   * Build the file for continuing to read next time.  For	*
   * a volatile file or stdin, if we keep the file entity	*
   * referring to the volatile file or stdin.  If we just	*
   * read ae, then we get					*
   *								*
   *     ae ++ indirect-to(f)					*
   *								*
   * where f is the file entity, which will read the rest of    *
   * the file.							*
   *------------------------------------------------------------*/

  if(kind == STDIN_FK || IS_VOLATILE_FM(mode)) {
    *(ind = allocate_entity(1)) = ENTP(FILE_TAG, fent);
    result = ENTP(INDIRECT_TAG, ind);
    result = quick_append(ae, result);
  }

  /*------------------------------------------------------------*
   * If we are reading a nonvolatile file, the we take a 	*
   * different approach.  The old file entity keeps its old 	*
   * meaning, but gets its current (just read) value installed  *
   * in its val field.  If we have just read ae, then that val  *
   * is just ae ++ f, where f is a newly created file entity    *
   * refering to this infile, with a different offset.	Note    *
   * that we need to clear out the fd field of the original     *
   * file entity, since that fd is now used in the new file     *
   * entity, which reads the rest of the file.			*
   *------------------------------------------------------------*/

  else {
    newfent                      = alloc_file_entity();
    newfent->kind                = INFILE_FK;
    newfent->mode                = mode;
    newfent->descr_index = index = fent->descr_index;
    newfent->u.file_data.pos     = fent->u.file_data.pos + n;
    newfent->u.file_data.val     = true_ent;
    newfent->u.file_data.name    = fent->u.file_data.name;
    file_table[index].file_rec   = newfent;

    result = ENTP(FILE_TAG, newfent);     /* Hold onto newfent */
    result = quick_append(ae, result);

    fent->u.file_data.val = result;
    fent->mark            = 0;
    fent->descr_index     = -1;
  }

# ifdef DEBUG
    if(trace_extra) {
      trace_i(233);
      trace_i(239);
      trace_print_entity(result);
      tracenl();
    }
# endif

  unreg(mark);
  return *loc = result;
}


/*********************************************************
 *			FILE_SUBLIST			 *
 *********************************************************
 * *inlist is a FILE_TAG entity.  We want to compute a 	 *
 * suffix of *inlist by skipping over the *skip leading  *
 * characters.  Set *inlist and *skip so that the suffix *
 * described by them is invariant.  For example, set     *
 * *skip = 0 and *inlist to the suffix, or leave *skip   *
 * at its current value and set *inlist to an equivalent *
 * (non-filetag) entity. 				 *
 *							 *
 * We want to make some progress in reaching the point	 *
 * where *skip is 0.  That progress is typically made	 *
 * by replacing a file by its contents.			 *
 *							 *
 * *loc is an address in the heap where *inlist is found,*
 * or is NULL if no such address is known.		 *
 *							 *
 * Return TRUE on success, FALSE on failure.  When 	 *
 * the computation fails, failure is set.		 *
 *********************************************************/

Boolean file_sublist(ENTITY *inlist, ENTITY **loc,
		     LONG *skip, LONG *time_bound)
{
  struct filed *fild;
  int fd, kind;
  LONG pos;
  char *name;
  ENTITY fe;
  REG_TYPE mark1;
  struct file_entity* fent = FILEENT_VAL(*inlist);

  /*-----------------------------------------------------------*
   * If this file is already in memory, look in memory.  Leave *
   * *skip alone, and set *inlist to the memory value.         *
   *							       *
   * Clear the mark field to prevent clearing at the next      *
   * garbage collection. 				       *
   *-----------------------------------------------------------*/

  if(ENT_NE(fent->u.file_data.val, true_ent)) {
     fent->mark = 0;
     *inlist = fent->u.file_data.val;
     return TRUE;
  }

  /*----------------------------------------------------*
   * We have a choice of doing a seek or doing this in  *
   * a more naive way.	The naive way is just to 	*
   * evaluate the file and set *inlist to the value.	*
   *							*
   * If volatile or at start, don't seek.  Also		*
   * don't seek if a text file in dos, since don't	*
   * know how many end-of-lines there are. Each end-of  *
   * line is a CR-LF pair, but becomes only one character*
   * on input.						*
   *----------------------------------------------------*/

   kind = fent->kind;
   if (kind != INFILE_FK ||
       IS_VOLATILE_OR_DOSTEXT_FM(fent->mode) ||
       *skip == 0) {
     *inlist = eval((*loc == NULL) ? *inlist
				   : ENTP(INDIRECT_TAG, *loc), time_bound);
     *loc = NULL;
     return TRUE;
   }

   /*-----------------------------------------------------------*
    * Here, we have decided to do a seek.  Get the name of the 	*
    * file and the offset from the start.			*
    *-----------------------------------------------------------*/

   mark1 = reg1(&fe);
   name  = fent->u.file_data.name;
   pos   = fent->u.file_data.pos;

   /*----------------*
    * Open the file. *
    *----------------*/

#  ifdef MSWIN
     fd = open_file(name,
		    O_RDONLY | IS_TEXT_FM(fent->mode) ? O_TEXT : O_BINARY,
		    0);
#  else
     fd = open_file(name, O_RDONLY, 0);
#  endif
#  ifdef UNIX
     if(fd >= 0) fcntl(fd, F_SETFD, 1);  /* Set for close-on-exec.*/
#  endif

   /*--------------------------------------------------------*
    * Get an internal file record for this file, and get the *
    * file descriptor fd for it.			     *
    *--------------------------------------------------------*/

   fe   = ast_read_file(fd, fent->mode, name);
   fent = FILEENT_VAL(fe);
   fild = get_filed(fent, READ);
   if(failure >= 0) {
     unreg(mark1);
     return FALSE;
   }
   fd = fild->fd;

   /*-----------------------------------------------------*
    * Bump the start to pos + i, since that is where the  *
    * sublist starts.  Then do a seek.			  *
    *-----------------------------------------------------*/

   fent->u.file_data.pos = pos + *skip;
   if(lseek(fd, fent->u.file_data.pos, SEEK_SET) < 0) {
     failure = CANNOT_SEEK_EX;
     unreg(mark1);
     return FALSE;
   }

   *inlist = fe;
   *skip   = 0;
   *loc    = NULL;
   unreg(mark1);
   return TRUE;
}


/********************************************************
 *			OUTFILE_DOLLAR_STDF		*
 ********************************************************
 * Return a string that describes e (an outfile).  	*
 * The return value is an entity.			*
 ********************************************************/

ENTITY outfile_dollar_stdf(ENTITY e)
{
  if(TAG(e) != FILE_TAG) {
    return make_str("(the write end of a pipe)");
  }
  else {
    char result[MAX_FILE_NAME_LENGTH + 20];

    return make_str(lazy_name(result, e));
  }
}


/********************************************************
 *			INIT_IO				*
 ********************************************************
 * Build stdin, trueStdin, stdout, stderr.		*
 *							*
 * Initialize the file table.				*
 ********************************************************/

PRIVATE void 
init_outfile(struct filed *fild, char *redirect, int fd, Boolean force)
{
  if(redirect == NULL) {
#   ifndef MSWIN
      fild->file = (fd == 1) ? stdout : stderr;
      fild->fd = fileno(fild->file);
#   else
      fild->fd = 0;
      fild->file = (FILE *) INTERACT_WIN;
#   endif
  }
  else {
    if(!force) {
      struct stat stat_buf;
      if(stat_file(redirect, &stat_buf) == 0) {
	if(!yn_query("File %s exists.  Overwrite?[y|n] ", redirect)) {
	  clean_up_and_exit(1);
	}
      }
    }
    fild->file = fopen_file(redirect, TEXT_WRITE_OPEN);
    if(fild->file == NULL) die(136, redirect);
    fild->fd = fileno(fild->file);
  }
  fild->delay = FALSE;
  fild->stamp = 0;
  fild->file_rec = NULL;
}

/*-----------------------------------------------------------*/

void init_io()
{
  int i;

# ifndef MSWIN
    stdin_filed.fd       = 0;
    stdin_filed.file     = stdin;
    stdin_filed.delay    = FALSE;
    stdin_filed.stamp    = 0;
    stdin_filed.file_rec = NULL;
# endif

# ifdef MSWIN
    keyboard_buffer_chain = keyboard_buffer_end =
      (struct keyboard_buffer *) BAREMALLOC(sizeof(struct keyboard_buffer));
    memset(keyboard_buffer_chain, 0, sizeof(struct keyboard_buffer));
# endif

  init_outfile(&stdout_filed, stdout_redirect, 1, force_stdout_redirect);
  if(stdout_redirect != NULL && stderr_redirect != NULL
     && full_file_name(stdout_redirect, 0) == 
        full_file_name(stderr_redirect, 0)) {
    stderr_filed = stdout_filed;
  }
  else {
    init_outfile(&stderr_filed, stderr_redirect, 2, force_stderr_redirect);
  }

  for(i = 0; i < MAX_OPEN_FILES; i++) {
    file_table[i].fd   = -1;
    file_table[i].file = NULL;
  }
}


