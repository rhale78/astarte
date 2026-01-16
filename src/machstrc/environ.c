/**********************************************************************
 * File:    machstrc/environ.c
 * Purpose: Implement environment.
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
 * This file contains functions that manage the environment of an	*
 * activation.  The environment holds bindings of identifiers.		*
 *									*
 * An environment binds values to names.   The names implicitly		*
 * correspond to locations in the environment, such as the third cell	*
 * in the top frame.  							*
 *									*
 * There are two kinds of environment node: local and global.  Both 	*
 * kinds hold								*
 *									*
 *	(a) A kind (either LOCAL_ENV or GLOBAL_ENV).			*
 *									*
 *	(b) A reference count, for storage management.			*
 *									*
 *	(c) A field called sz.  There are env_size[sz] cells in the	*
 *	    array of part (f). 						*
 *									*
 *	(d) A field called most_entries, holding the number of occupied	*
 *	    entries in the environment.					*
 *									*
 *	(e) A field called descr_num, holding the index in 		*
 *	    env_descriptors of the descriptor list for this environment *
 *	    node.							*
 *									*
 * A global environment node has 					*
 *									*
 *	(f) An array cells of typed entities.				*
 *									*
 *	(g) The type of the identifier that created the environment 	*
 *									*
 * A local environment node has						*
 *									*
 *	(f) An array cells of entities.					*
 *									*
 *	(g) A link to another environment, holding more bindings.	*
 *									*
 *	(h) A field num_link_entries telling how many of the cells in	*
 *	    the link environment are relevant to this environment.	*
 *									*
 *	(i) A pc field, holding the address of the instruction that did *
 *	    the last change to this node.  This is used to get a	*
 * 	    correspondence between offset in the environment and names. *
 *	    The correspondence depends on where the program is.		*
 ************************************************************************/


#include "../misc/misc.h"
#include "../machdata/except.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../error/error.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../show/prtent.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			SCAN_ENV				*
 ****************************************************************
 * Return the environment frame n from the top of env.		*
 ****************************************************************/

ENVIRONMENT* scan_env(ENVIRONMENT *env, int n)
{
  while(n > 0 && env != NULL) {env = env->link; n--;}
  if(env == NULL) die(43);
  return env;
}


/****************************************************************
 *			GLOBAL_BINDING_ENV			*
 ****************************************************************
 * Return the entity at offset k in the global environment of   *
 * env.  (The global environment is at the end of the chain 	*
 * that starts at env.)						*
 ****************************************************************/

ENTITY global_binding_env(ENVIRONMENT *env, int k)
{
  ENTITY e, e1;

  while(env != NULL && ENVKIND(env) == LOCAL_ENV) env = env->link;

  if(env == NULL) {
#   ifdef DEBUG
      if(trace) {
	trace_i(212);
      }
#   endif
    return bad_ent;
  }

  e = env->cells[k].val;
  if(TAG(e) == GLOBAL_INDIRECT_TAG) {
    e1 = ENTVAL(e)[0];
    if(TAG(e1) != GLOBAL_TAG) return e1;
  }
  return e;
}


/****************************************************************
 *			BASIC_LOCAL_BIND_ENV   			*
 ****************************************************************
 * Bind cell k in the_act.env to value a, without checking 	*
 * reference counts.  pc is the address of the instruction	*
 * that is doing the binding.					*
 ****************************************************************/

void basic_local_bind_env(int k, ENTITY a, CODE_PTR pc)
{
  ENVIRONMENT*    env  = the_act.env;
  struct envcell* cell = env->cells + k;
  env->pc = pc;
  cell->val = a;
}


/****************************************************************
 *			LOCAL_BIND_ENV				*
 ****************************************************************
 * Bind cell k in environment the_act.env to value a.  It might	*
 * be necessary to adjust reference counts and/or to copy the	*
 * environment. 						*
 *								*
 * Parameter pc is the byte-code address where this binding	*
 * is being done.  It is installed in the environment as the	*
 * most recently used program counter, so that the debug stuff	*
 * can know what the environment looks like.			*
 ****************************************************************/

void local_bind_env(int k, ENTITY a, CODE_PTR pc)
{
  register int i;
  register ENVIRONMENT *env;
  struct envcell *cell;
  register struct envcell *p;
  int most_ent;

  env      = the_act.env;
  cell     = env->cells + k;
  most_ent = env->most_entries;

  /*--------------------------------------------------------------------*
   * Typically, k = env->most_entries.  In that case, just do the	*
   * bindings.	This case can be handled by the next one, but it is     *
   * a little faster to do it as a special case, and it is done often.	*
   *--------------------------------------------------------------------*/

  if(k == most_ent) {
    drop_env_ref(env, the_act.num_entries);
    the_act.num_entries = env->most_entries = k + 1;
    cell->refs = 1;
    goto out;
  }

  /*--------------------------------------------------------------------*
   * Bind everything between the end of the current used space and	*
   * offset k to NOTHING.  Set up reference counts. 			*
   *--------------------------------------------------------------------*/

  if(k >= most_ent) {
    for(i = most_ent, p = env->cells + i; i < k; i++, p++) {
      p->val  = NOTHING;
      p->refs = 0;
    }
    drop_env_ref(env, the_act.num_entries);
    the_act.num_entries = env->most_entries = k + 1;
    cell->refs = 1;
    goto out;
  }

  /*--------------------------------------------------------------------*
   * If there are no other references, then it is safe to modify	*
   * this cell.								*
   *--------------------------------------------------------------------*/

  if(env->ref_cnt == 1) goto out;

  /*--------------------------------------------------------------------*
   * If the cell to be changed contains NOTHING, then it is safe to 	*
   * modify this cell.  Need to update ref counts and			*
   * the_act.num_entries. 						*
   *--------------------------------------------------------------------*/

  if(ENT_EQ(cell->val, NOTHING)) {
    if(the_act.num_entries < k+1) {
      drop_env_ref(env, the_act.num_entries);
      bump_env(env, k+1);
      the_act.num_entries = k+1;
    }
    goto out;
  }

  /*-----------------------------------------------------------*
   * Multiple ref.  Find the last multi-ref cell (offset i-1). *
   *-----------------------------------------------------------*/

  drop_env_ref(env, the_act.num_entries);
  for(i = env->most_entries, p = env->cells + i - 1; 
      i > 0 && p->refs == 0; 
      i--,p--){}

  /*--------------------------------------------------------------------*
   * If k >= i, then it is safe to modify k, since nobody else cares 	*
   * about it. 								*
   *--------------------------------------------------------------------*/

  if(k >= i) {
    env->cells[the_act.num_entries - 1].refs = 1;
    goto out;
  }

  /*---------------------------------*
   * Otherwise copy the environment. *
   *---------------------------------*/

  env->most_entries = i;
  copy_the_env(k);
  env = the_act.env;
  cell = env->cells + k;

 out:

  /*-------------------------------------*
   * Store the value in the environment. *
   *-------------------------------------*/

  env->pc   = pc;
  cell->val = a;
}


/****************************************************************
 *			COPY_THE_ENV				*
 ****************************************************************
 * Copy the_act.env, putting the copy into the_act.env, and	*
 * setting the refs for the new environment for an assignment	*
 * at location k.  It is the responsibility of the caller to	*
 * fix up old environment's refs and most_entries fields, and	*
 * the new environment's pc field.				*
 ****************************************************************/

void copy_the_env(int k)
{
  int i,n,m,nl;
  ENVIRONMENT *env, *new_env;
  struct envcell *p, *q;

  env = the_act.env;
  m = n = the_act.num_entries;
  if(m <= k) m = k+1;
  new_env = allocate_local_env(env->sz);    /* Ref cnt is 1 */

  /*-------------------------*
   * Copy the cell contents. *
   *-------------------------*/

  p = new_env->cells;
  q = env->cells;
  for(i = 0; i < n; i++) {
    p->val = q->val;
    p->refs = 0;
    p++; q++;
  }

  /*---------------------------------------*
   * Copy the remaining fields, except pc. *
   *---------------------------------------*/

  nl = env->num_link_entries;
  new_env->descr_num = env->descr_num;
  new_env->most_entries  = m;
  new_env->cells[m-1].refs = 1;
  for(i = n; i < m-1; i++) {
    new_env->cells[i].refs = 0;
    new_env->cells[i].val  = NOTHING;
  }
  new_env->num_link_entries = nl;
  bump_env(new_env->link = env->link, nl);
  env->ref_cnt--;
  the_act.env            = new_env;
  the_act.num_entries    = m;

# ifdef DEBUG
    if(trace_env) {
      trace_i(213);
      if(trace_env > 1) print_env(new_env, m);
    }
# endif
}


/****************************************************************
 *			EXIT_SCOPE_ENV				*
 ****************************************************************
 * Remove references to all but the first k bindings of		*
 * the_act.env.							*
 ****************************************************************/

void exit_scope_env(int k)
{
  struct envcell *p;
  int i;

  if(the_act.num_entries <= k) return;

  drop_env_ref(the_act.env, the_act.num_entries);
  the_act.num_entries = k;
  if(k > 0) {
    bump_env_ref(the_act.env, k);
  }

  for(i = the_act.env->most_entries, p = the_act.env->cells + i - 1;
      i > 0 && p->refs == 0; 
      i--, p--) {}
  the_act.env->most_entries = i;
}


/****************************************************************
 *			TYPE_ENV				*
 ****************************************************************
 * Return the type stored at offset n in the global environment *
 * at the end of chain env.					*
 ****************************************************************/

TYPE* type_env(ENVIRONMENT *env, int n)
{
  ENTITY e;
  int tag;

  while(env != NULL && ENVKIND(env) == LOCAL_ENV) env = env->link;

  if(env == NULL) {
#   ifdef DEBUG
      if(trace) {
	trace_i(214);
      }
#   endif
    return NULL;
  }

  e = env->cells[n].val;
  tag = TAG(e);
  if(tag == TYPE_TAG) return TYPEVAL(e);
  if(tag == GLOBAL_INDIRECT_TAG) return TYPEVAL(ENTVAL(e)[1]);
  die(44, (char *) tag);
  return NULL;
}


/****************************************************************
 *			ENVKINDF				*
 ****************************************************************
 * Return the kind of env, but report an error if the ref count	*
 * is negative.							*
 ****************************************************************/

#ifdef GCTEST

int envkindf(ENVIRONMENT *env)
{
  if(env->ref_cnt < 0) {
    badrc("environment", toint(env->ref_cnt), (char *)env);
  }
  return env->kind;
}

#endif


#ifdef DEBUG
/********************************************************
 *			LONG_PRINT_ENV			*
 ********************************************************
 * Print environment e in long form on the trace file,  *
 * indented n spaces.					*
 ********************************************************/

void long_print_env(ENVIRONMENT *e, int n)
{
  ENVIRONMENT *p;
  int i;

  for(p = e; p != NULL && p->kind != GLOBAL_ENV; p = p->link) {
    indent(n);
    trace_i(38,
	   toint(p->sz), toint(p->most_entries), toint(p->num_link_entries),
	   p->ref_cnt, p->link);
    for(i = 0; i < p->most_entries; i++) {
      indent(n);
      fprintf(TRACE_FILE, "(%d)", toint(p->cells[i].refs));
      long_print_entity(p->cells[i].val, n+1, 0);
    }
  }
  if(p != NULL) {
    indent(n);
    trace_i(39,
	   toint(p->sz), toint(p->most_entries), toint(p->ref_cnt));
    for(i = 0; i < p->most_entries; i++) {
      indent(n);
      long_print_entity(p->cells[i].val, n+1, 0);
    }
  }
}


/********************************************************
 *			PRINT_ENV			*
 ********************************************************
 * Print the first ne entries in environment e in 	*
 * shortened form on the trace file.			*
 ********************************************************/

void print_env(ENVIRONMENT *e, int ne)
{
  ENVIRONMENT *p;
  int i;

  fprintf(TRACE_FILE, "env[ne=%d]:", ne);
  for(p = e; p != NULL; p = p->link) {
    fprintf(TRACE_FILE, "[me=%d rc=%d]\n", 
	    toint(p->most_entries), toint(p->ref_cnt));
    for(i = 0; i < ne; i++) {
      trace_print_entity(p->cells[i].val);
      if(p->cells[i].refs > 0) {
	fprintf(TRACE_FILE, "<%d--", toint(p->cells[i].refs));
      }
      tracenl();
    }
    ne = p->num_link_entries;
    fprintf(TRACE_FILE, "|[ne=%d]", ne);
  }
  tracenl();
}


#endif
