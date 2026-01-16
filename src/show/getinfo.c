/*********************************************************************
 * File:    show/getinfo.c
 * Purpose: Get information about environments and source code from
 *          program counter.
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
 * When the interpreter's package reader reads a .aso file, it stores   *
 * information about environments and other things.  The functions      *
 * in this file are used to store and get that information.		*
 ************************************************************************/

#include <memory.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../clstbl/typehash.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../machdata/entity.h"
#include "../machstrc/machstrc.h"
#include "../show/printrts.h"
#include "../show/getinfo.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 * The data structures that these functions look at are 	*
 * constructed by intrprtr/package.c.  See that file for a	*
 * description of them.						*
 ****************************************************************/

/********************************************************
 *			PUT_LINE_RECORD			*
 ********************************************************
 * Function put_line_record in file intrprtr/package.c  *
 * stores the information about lines that is extracted *
 * by the following functions.  See that function for   *
 * more detail. 					*
 ********************************************************/

/****************************************************************
 *			GET_LINE_REC				*
 ****************************************************************
 * Return the line record for program counter pc.  pd must be   *
 * the package descriptor for the package that contains program *
 * counter pc, as would be returned by get_pd_entry.		*
 ****************************************************************/

struct line_rec* get_linerec(struct package_descr *pd, CODE_PTR pc)
{
  int lo, hi, mid;
  LONG key;
  struct line_rec *lines;

  /*----------------------------------------------*
   * The algorithm is binary search on pd->lines. *
   *----------------------------------------------*/

  lines = pd->lines;
  key   = pc - pd->begin_addr;
  lo    = 0;
  hi    = pd->log_lines_size - 1;

  while(lo < hi) {
    mid = (lo + hi + 1) / 2;
    if(lines[mid].offset <= key) lo = mid;
    else hi = mid - 1;
  }
  return lines + lo;
}


/****************************************************************
 *			GET_LINE_INFO				*
 ****************************************************************
 * Set pack_name, file_name and line to the package name, file	*
 * name and line number, respectively, of the code that is at   *
 * program counter pc.						*
 ****************************************************************/

void get_line_info(CODE_PTR pc, char **pack_name, char **file_name,
		   int *line)
{
  struct package_descr *pd;
  struct line_rec *lr;

  /*------------------------------------------------------*
   * Get the descriptor for the package that contains pc. *
   *------------------------------------------------------*/

  pd = get_pd_entry(pc);

  /*-----------------------------------------------------------*
   * If this package does not exist, return something at least *
   * somewhat sensible. 				       *
   *-----------------------------------------------------------*/

  if(pd == NULL) {
    *pack_name = "Unknown";
    *file_name = NULL;
    *line      = 0;
    return;
  }

  /*---------------------------------------------------------------*
   * Get the package name.  If there is are separate interface and *
   * implementation packages, then decide which one contains pc.   *
   *---------------------------------------------------------------*/

  if(pd->imp_name != NULL) {
    LONG offset = pc - pd->begin_addr;
    *pack_name = (offset >= pd->imp_offset) ? pd->imp_name : pd->name;
  }
  else {
    *pack_name = pd->name;
  }

  /*---------------------*
   * Get the other info. *
   *---------------------*/

  *file_name = pd->file_name;
  lr         = get_linerec(pd, pc);
  *line      = lr->line;
}


/****************************************************************
 *			PUT_ID_DESCRIPTION			*
 ****************************************************************
 * Function put_id_description in file intrprtr/package.c 	*
 * stores the information that is extracted by the functions    *
 * below.  See that function for more detail. 			*
 ****************************************************************/

/****************************************************************
 *			GET_ONE_ENV_INFO			*
 ****************************************************************
 * Put the name of id k of environment env in info->name, and 	*
 * place its type in info->type.				*
 *								*
 * The name and type depend not only on env, but also on the	*
 * current program counter.  pc_offset is the offset from the   *
 * beginning of the current package of the current program	*
 * counter.  It can be obtained from the current proram counter *
 * using get_pd_entry: if pc is the current program counter and *
 * pd is the result of get_pd_entry(pc), then pc_offset is	*
 * pc - pd->begin_addr.						*
 *								*
 * bl must be the binding list to be used when getting   	*
 * the type.							*
 *								*
 * info->type is in the type table, so does not need to have	*
 * its reference count dropped.					*
 ****************************************************************/

void get_one_env_info(NAME_TYPE *info, ENVIRONMENT *env,
		      LONG pc_offset, LIST *bl, int k)
{
  struct env_descr *ed;
  int descr_num;
  char* name = NULL;
  CODE_PTR type_instrs = NULL;

  info->name = NULL;
  info->type = NULL;
  descr_num  = env->descr_num;
  if(descr_num < 0 || descr_num >= next_env_descr_num) {
    return;
  }

  for(ed = env_descriptors[descr_num]; ed != NULL; ed = ed->next) {
    if(ed->pc_offset < pc_offset && ed->env_offset == k) {
      name = ed->name;
      type_instrs = ed->type_instrs;
    }
  }

  info->name = name;
  if(type_instrs != NULL) {
    TYPE *t;
    bump_type(t = 
	      eval_type_instrs_with_binding_list(type_instrs, env, bl));
    bump_type(info->type = type_tb(t));
    drop_type(t);
  }
  else {
    info->type = NULL;
  }
}


/****************************************************************
 *			GET_ENV_INFO				*
 ****************************************************************
 * For each of the first num_entries identifiers in environment	*
 * env, put the name of id k in info[k].name, and place its	*
 * type in info[k].type.  If there is no such id, then		*
 * info[i].name is set to null, as is info[k].type.		*
 *								*
 * pc_offset must be set to the offset from the begining of the *
 * current package of the current program counter.  See		*
 * description of get_one_env_info for how to get pc_offset.	*
 *								*
 * bl must be the binding list to be used when getting   	*
 * the type.							*
 *								*
 * info[k].type is in the type table, for each k, so does not 	*
 * need to have its reference count dropped.			*
 ****************************************************************/

void get_env_info(NAME_TYPE *info, ENVIRONMENT *env, int num_entries,
		  LONG pc_offset, LIST *bl)
{
  struct env_descr *ed;
  NAME_TYPE *inf;
  int i, descr_num;

# ifdef DEBUG
    if(trace) {
      trace_i(219);
      trace_i(220, num_entries, pc_offset);
      print_type_list_separate_lines_without_constraints(bl, " ");
      trace_i(219);
      trace_i(221, env->descr_num);
    }
# endif

  memset(info, 0, num_entries*sizeof(NAME_TYPE));
  
  descr_num = env->descr_num;
  if(descr_num < 0 || descr_num >= next_env_descr_num) return;

  for(ed = env_descriptors[descr_num]; ed != NULL; ed = ed->next) {
    if(ed->pc_offset < pc_offset) {
      inf              = info + ed->env_offset;
      inf->name        = ed->name;
      inf->type_instrs = ed->type_instrs;

#     ifdef DEBUG
        if(trace) {
	  trace_i(219);
	  trace_i(222, ed->env_offset, ed->name);
	}
#     endif
    }
  }

  for(i = 0; i < num_entries; i++) {
    if(info[i].type_instrs != NULL) {
      TYPE* t;
      bump_type(t =
	 eval_type_instrs_with_binding_list(info[i].type_instrs, env, bl));
      bump_type(info[i].type = type_tb(t));
      drop_type(t);
    }
    else {
      info[i].type = NULL;
    }
  }
}      



