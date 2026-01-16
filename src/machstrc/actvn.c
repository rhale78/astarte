/**********************************************************************
 * File:    machstrc/actvn.c
 * Purpose: Functions for activations
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
 * This file provides a few functions for use with activations and    	*
 * continuations.       						*
 *									*
 * Activations and continuations are described in file actvn.doc.	*
 ************************************************************************/

#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../utils/lists.h"
#include "../show/prtent.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *		        ACTKINDF				*
 ****************************************************************
 * Return the kind field of activation a, but check that this   *
 * activation has not been returned to the free space pool.     *
 * This is only used in GCTEST mode.				*
 ****************************************************************/

#ifdef GCTEST

int actkindf(ACTIVATION *a)
{
  if(a->ref_cnt < 0) {
    badrc("activation", toint(a->ref_cnt), (char *) a);
  }
  return a->kind;
}

#endif


/****************************************************************
 *			COPY_ACTIVATION				*
 ****************************************************************
 * Return a copy of activation a, 				*
 * The reference count on the copy is 0.			*
 ****************************************************************/

ACTIVATION* copy_activation(ACTIVATION *a)
{
  register ACTIVATION* p = allocate_activation();

  *p         = *a;
  p->ref_cnt = 0;
  p->stack   = get_stack();
  bump_activation_parts_except_stack(p);
  return p;
}


/****************************************************************
 *			COPY_THE_ACT				*
 ****************************************************************
 * Return a copy of the_act, but whose control has been set to	*
 * NULL.  The reference count on the copy is 1.			*
 ****************************************************************/

ACTIVATION* copy_the_act()
{
  register ACTIVATION* p = allocate_activation();

  *p         = the_act;
  p->ref_cnt = 1;
  p->stack   = get_stack();
  p->control = NULL;
  bump_activation_parts_except_stack(p);
  return p;
}


/****************************************************************
 *			GET_THE_ACT				*
 ****************************************************************
 * Return a copy of the_act in the heap, with ref count 1.	*
 ****************************************************************/

ACTIVATION* get_the_act()
{
  register ACTIVATION* p = allocate_activation();

  *p           = the_act;
  p->ref_cnt   = 1;
  bump_activation_parts(p);
  return p;
}


/****************************************************************
 *			GET_ACT_BINDING_LIST			*
 *			GET_CONT_BINDING_LIST			*
 ****************************************************************
 * Return NIL if a has a null type binding list, and return the *
 * head of a->type_binding_list otherwise.			*
 ****************************************************************/

TYPE_LIST* get_act_binding_list(ACTIVATION *a)
{
  return (a->type_binding_lists == NIL )
           ? NIL 
           : a->type_binding_lists->head.list;
}

/*------------------------------------------------------*/

TYPE_LIST* get_cont_binding_list(CONTINUATION *a)
{
  return (a->type_binding_lists == NIL )
           ? NIL 
           : a->type_binding_lists->head.list;
}

#ifdef DEBUG
/********************************************************
 *			PRINT_CONTINUATION		*
 ********************************************************
 * Print continuation a in debug form on the trace	*
 * file.						*
 ********************************************************/

void print_continuation(CONTINUATION *a)
{
  if(a == NULL) {
    fprintf(TRACE_FILE, "NULL\n");
    return;
  }
  trace_i(30, toint(a->ref_cnt), toint(a->num_entries), a->program_ctr);
  print_str_list_nl(a->exception_list);
  tracenl();
  print_env(a->env, a->num_entries);
  print_continuation(a->continuation);
}


/********************************************************
 *			PRINT_ACTIVATION		*
 ********************************************************
 * Print activation a in debug form on the trace	*
 * file.						*
 ********************************************************/

void print_activation(ACTIVATION *a)
{
  if(a == NULL) {
    fprintf(TRACE_FILE, "NULL\n");
    return;
  }
  trace_i(31,toint(a->num_entries), toint(ACTKIND(a)), 
	  a->program_ctr, toint(a->ref_cnt));
  print_str_list_nl(a->exception_list);
  tracenl();
  trace_i(319);
  print_str_list_nl(a->embedding_tries);
  trace_i(321);
  print_str_list_nl(a->embedding_marks);
  print_stack(a->stack);
  print_env(a->env, a->num_entries);
  print_up_control(TRACE_FILE, a->control, 0);
  print_state(a->state_a, 0);
  print_states(a->state_hold);
  print_trapvec(a->trap_vec_a);
  print_trapvecs(a->trap_vec_hold);
  print_coroutines(a->coroutines);
  tracenl();
  print_continuation(a->continuation);
} 

#endif



