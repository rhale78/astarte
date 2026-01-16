/*****************************************************************
 * File:    show/profile.c
 * Purpose: Print profile info.
 * Author:  Karl Abrahamson
 *****************************************************************/

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

/****************************************************************
 * The functions in this file accumulate a primitive execution	*
 * profile and print it.					*
 ****************************************************************/

#include <memory.h>
#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../intrprtr/intrprtr.h"
#include "../show/profile.h"
#include "../show/gprint.h"
#include "../alloc/allocate.h"

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			printed_profile				*
 ****************************************************************
 * printed_profile is set to 1 after the execution profile has  *
 * been printed.  This prevents duplicate prints.		*
 ****************************************************************/

PRIVATE Boolean printed_profile = 0;

/****************************************************************
 *			profile_table				*
 ****************************************************************
 * Table profile_table associates with each function name a     *
 * PROFILE_INFO node.  That node has the following fields.      *
 *								*
 *   instructions_executed   Number of instructions that this   *
 *                           function has done.                 *
 *								*
 *   number_calls	     Number of times this function has  *
 *                           been called. 			*
 *								*
 *   tbl		     A hash table associating with each *
 *			     name of a function that called     *
 *			     this function the number of times  *
 * 			     taht function called this one.     *
 ****************************************************************/

PRIVATE HASH2_TABLE* profile_table = NULL;

/****************************************************************
 *			profile_name				*
 *			profile_counter				*
 ****************************************************************
 * profile_name is the name of the function that executed the   *
 * previously profiled instruction.  It is used to avoid a	*
 * lookup in the hash table.					*
 *								*
 * profile_counter points to the variable that counts the	*
 * number of instructions done by the function whose name is	*
 * in profile_name.						*
 ****************************************************************/

PRIVATE char* profile_name = NULL;

PRIVATE LONG* profile_counter = NULL;

/****************************************************************
 *			profile_file				*
 ****************************************************************
 * profile_file is the file where profile information is	*
 * written.							*
 ****************************************************************/

PRIVATE FILE* profile_file;

/****************************************************************
 *			profile_instr_count			*
 ****************************************************************
 * profile_instr_count is the total number of instructions 	*
 * done, but only after printing the profile.			*
 ****************************************************************/

PRIVATE LONG profile_instr_count;


/****************************************************************
 * 			NEW_PROFILE_CELL			*
 ****************************************************************
 * Return a new PROFILE_INFO cell, zeroed out.			*
 ****************************************************************/

PRIVATE PROFILE_INFO* new_profile_cell(void)
{
  register PROFILE_INFO* result = 
    (PROFILE_INFO *) alloc_small(sizeof(PROFILE_INFO));
  memset(result, 0, sizeof(PROFILE_INFO));
  return result;
}


/****************************************************************
 * 			INSTALL_PROFILE_INSTRUCTION		*
 ****************************************************************
 * Install profile information for an instruction about to	*
 * be executed by the_act.					*
 ****************************************************************/

void install_profile_instruction(void)
{
  HASH2_CELLPTR h;

  /*-----------------------------------------------------------------*
   * If this is a new function, then find the new profile counter    *
   * and set profile_name for this function.      		     *
   *-----------------------------------------------------------------*/

  if(profile_name != the_act.name) {
    HASH_KEY u;
    u.str = the_act.name;
    h     = insert_loc_hash2(&profile_table, u, strhash(u.str), eq);
    if(h->key.num == 0) {
      h->key.str = u.str;
      h->val.profile_info = new_profile_cell();
    }
    profile_name    = the_act.name;
    profile_counter = &(h->val.profile_info->instructions_executed);
  }

  /*----------------------------------*
   * Increment the instruction count. *
   *----------------------------------*/
 
  (*profile_counter)++;

}


/****************************************************************
 * 			INSTALL_PROFILE_CALL			*
 ****************************************************************
 * Function caller is calling function callee.  Install this    *
 * information in the table.                                    *
 ****************************************************************/

void install_profile_call(char *caller, char *callee)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  PROFILE_INFO *pi;

  u.str = callee;
  h     = insert_loc_hash2(&profile_table, u, strhash(u.str), eq);
  if(h->key.num == 0) {
    h->key.str          = u.str;
    h->val.profile_info = new_profile_cell();
  }
  pi              = h->val.profile_info;
  profile_name    = callee;
  profile_counter = &(pi->instructions_executed);
  pi->number_calls++;

  u.str = caller;  
  h = insert_loc_hash2(&(pi->tbl), u, strhash(u.str), eq);
  if(h->key.num == 0) {
    h->key.str = caller;
    h->val.num = 1;
  }
  else h->val.num++;
}


/****************************************************************
 * 			PRINT_CALL_LINE				*
 ****************************************************************
 * Print a record of the profile.				*
 ****************************************************************/

PRIVATE Boolean indent_call_line;

PRIVATE void print_call_line(HASH2_CELLPTR h)
{
  fprintf(profile_file, "          %-40s  %10ld\n", h->key.str, h->val.num);
}


/****************************************************************
 * 			PRINT_PROFILE_LINE			*
 ****************************************************************
 * Print a record of the profile.				*
 ****************************************************************/

PRIVATE void print_profile_line(HASH2_CELLPTR h)
{
  PROFILE_INFO* pi = h->val.profile_info;
  LONG num_instrs = pi->instructions_executed;

  fprintf(profile_file, "%-40s %10ld %10ld\n", h->key.str, 
	  num_instrs, pi->number_calls);
  profile_instr_count += num_instrs;

  indent_call_line = FALSE;
  if(pi->tbl != NULL) {
    sort_int_val_hash2(pi->tbl);
    scan_and_clear_hash2(&(pi->tbl), print_call_line);
  }
  fprintf(profile_file, "\n");
}


/****************************************************************
 * 			PRINT_PROFILE				*
 ****************************************************************
 * Print the execution profile in profile_table.		*
 ****************************************************************/

void print_profile(void)
{
  if(printed_profile || profile_table == NULL) return;

  printed_profile = 1;
  profile_instr_count = 0;
  profile_file = fopen("ast.prof", TEXT_WRITE_OPEN);
  if(profile_file == NULL) {
    gprintf(STDERR, "Cannot open ast.prof\n");
  }
  else {
    fprintf(profile_file, 
	    "Each function is given with instructions executed and calls,\n"
            "followed by a list of functions that called it, and number of\n"
            "calls by each other function.\n\n");
    fprintf(profile_file,
            "Function                               Instructions      Total\n"
            "                                       executed          calls\n\n");
    sort_profile_val_hash2(profile_table);
    scan_and_clear_hash2(&profile_table, print_profile_line);
    fprintf(profile_file, "\n Total instructions: %ld\n", 
	    profile_instr_count);
    fclose(profile_file);
  }
}
