/**********************************************************************
 * File:    machdata/except.h
 * Purpose: Functons for dealing with exception numbers.
 * Author:  Karl Abrahamson
 **********************************************************************/

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
 * If you add an exception to this list, be sure to update LAST_EX and  *
 * to update init_exceptions and init_exception_names in file           *
 * except.c.  The Astarte equivalents of these exceptions are given 	*
 * in the table below. 							*
 ************************************************************************/

#define TEST_EX         0  /* A failed test.				     */
#define MEM_EX          1  /* Too much memory has been requested.            */
#define ENDTHREAD_EX    2  /* A thread has stopped.     		     */
#define EMPTY_BOX_EX 	3  /* Attempt to take content of empty box 	     */
#define EMPTY_LIST_EX 	4  /* Attempt to take apart empty list 		     */
#define DOMAIN_EX     	5  /* Argument not in domain of function             */
#define GLOBAL_ID_EX	6  /* No such global id definition		     */
#define CONVERSION_EX   7  /* Error in conversion from one object to another */
#define SIZE_EX         8  /* Size mismatch                                  */
#define NO_CASE_EX	9  /* No case matches in choose.		     */
#define NO_FILE_EX	10 /* No file found				     */
#define SPECIES_EX	11 /* Unwrapper has produced a type that does not    */
			   /* match expectation                              */
#define INF_LOOP_EX     12 /* Loop in lazy evaluation 			     */
#define INTERRUPT_EX    13 /* An interrupt has occurred                      */
#define CANNOT_SEEK_EX  14 /* Cannot seek on input file                      */
#define DO_COMMAND_EX   15 /* Error in doing command                         */
#define TOO_MANY_FILES_EX 16 /* Too many files				     */
#define INPUT_EX        17 /* Bad user input                                 */
#define LIMIT_EX	18 /* Limit exceeded or instruction failed           */
#define BIND_EX		19 /* Attempt to bind-unknown on non-unknown, or to  */
			   /* bind a protected unknown with the wrong key    */
#define NO_FONT_EX	20 /* Could not open a font. 			     */
#define CLOSED_FILE_EX	21 /* Attempt to use a closed file 		     */
#define ENSURE_EX	22 /* An ensured condition has failed.  Not used     */
			   /* by primitives.		 		     */
#define REQUIRE_EX	23 /* A required condition has failed.  Not used     */
			   /* by primitives.		 		     */
#define SUBSCRIPT_EX	24 /* Subscript out of bounds			     */
#define TERMINATE_EX	25 /* The thread has been terminated by another	     */
			   /* thread.					     */
#define LISTEXHAUSTED_EX 26 /* Pull from an empty list 			     */
#define LAST_EX         25 /* Number of last standard exception              */

/************************************************
 * Astarte equivalents of exceptions 		*
 *						*
 * TEST_EX		testX			*
 * MEM_EX		memX			*
 * ENDTHREAD_EX		endThreadX		*
 * EMPTY_BOX_EX		emptyBoxX		*
 * EMPTY_LIST_EX	emptyListX		*
 * DOMAIN_EX		domainX			*
 * GLOBAL_ID_EX		noDefX			*
 * CONVERSION_EX	conversionX		*
 * SIZE_EX		sizeX			*
 * NO_CASE_EX		noCaseX			*
 * NO_FILE_EX		noFileX			*
 * SPECIES_EX		speciesX		*
 * INF_LOOP_EX		infiniteLoopX		*
 * INTERRUPT_EX		interruptX		*
 * CANNOT_SEEK_EX	cannotSeekX		*
 * DO_COMMAND_EX	systemX			*
 * TOO_MANY_FILES_EX 	tooManyFilesX		*
 * INPUT_EX		badInputX		*
 * LIMIT_EX		limitX			*
 * BIND_EX		bindUnknownX		*
 * NO_FONT_EX		noFontX			*
 * CLOSED_FILE_EX	closeFileX		*
 * ENSURE_EX		ensureX			*
 * REQUIRE_EX		requireX		*
 * SUBSCRIPT_EX		subscriptX		*
 * TERMINATE_EX		terminateX		*
 * LISTEXHAUSTED_EX	listExhaustedX		*
 ************************************************/

/*---------------------------------------------------------------------------*
 * The following are to handle special conditions such as failure.	     *
 *---------------------------------------------------------------------------*/

#define TIME_OUT_EX	1000 /* Special exception for time out               */
#define TRAPPED_EX	1001 /* Special exception for exiting at a trap      */
#define PAUSE_EX	1002 /* Special exception for a pause 		     */

/*----------------------------------------------*
 * The following are aliases used in the code.	*
 *----------------------------------------------*/

#define BAD_STRING_EX	CONVERSION_EX /* String not appropriate (such as "a" */
				      /* as integer)			     */
#define HIGH_PREC_EX	MEM_EX /* Too much precision has been requested for  */
			       /* reals					     */
#define LONG_STRING_EX  MEM_EX /* Request for a very long string denied      */
#define LARGE_ARRAY_EX  MEM_EX /* An array is too large.                     */


extern int   trap_vec_size;
extern int   next_exception;
extern struct exception_data_struct FARR exception_data[];
extern TRAP_VEC *no_trap_tv;

ENTITY	  exception_string_stdf (ENTITY ex);
void      init_exceptions 	(void);
void      init_exception_names	(void);
void      trap_by_default 	(int exc_num);
void 	  clear_trap		(int ex, struct trap_vec *tr);
void 	  set_trap		(int ex, struct trap_vec *tr);
TRAP_VEC* copy_trap_vec 	(TRAP_VEC *tr);
ENTITY    will_trap_stdf	(ENTITY ex);
