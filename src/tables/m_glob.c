/**********************************************************************
 * File:    tables/m_glob.c
 * Purpose: Global identifier insertion and setup for machine.
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
 * This file is concerned with two aspects of global id handling in	*
 * the interpreter.  The first is handling of the table of global	*
 * id definitions.							*
 *									*
 * The interpreter assigns an index to each global id.  Table		*
 * entity_name_table maps identifier names to corresponding indices.	*
 * The interpreter also keeps an array called outer_bindings that holds,*
 * for each index, a pair of tables, called the poly table and the mono	*
 * table.  Function insert_global sets up the outer_bindings entry for	*
 * an identifier.							*
 *									*
 * The poly table holds the (polymorphic) definitions for an id,	*
 * as they are found in the package.  It is the primary source of	*
 * definitions.	 The nodes of the poly table have type			*
 * GLOBAL_TABLE_NODE, with the following fields.			*
 *									*
 *  mode	The mode of the definition.  (This is just the low      *
 *              order byte of the define_mode.)				*
 *									*
 *  packnum	The number of the package that made the definition	*
 *									*
 *  V		The type of the definition				*
 *									*
 *  offset	The offset in the package where the code for this	*
 *		definition begins					*
 *									*
 *  ty_instrs	A pointer to a sequence of instructions that build 	*
 *		type V. 						*
 *									*
 *  dummy	This is true if this entry is a dummy entry, which	*
 *		cannot be used to get a definition.  Such entries	*
 *		are made, for example, when entering an IRREGULAR_DCL_I *
 *		declaration into the table.				*
 *									*
 *  start	A pointer to the global id node where copyihg should 	*
 *		start when doing a Let{copy} declaration.		*
 *									*
 *  next	The next node.						*
 *									*
 * Unfortunately, lookup in the poly table is very slow, since the	*
 * actual type desired must be unified with each available polymorphic  *
 * type.  The mono table speeds up search.  It is a hash table, keyed 	*
 * by types, associating an entity with each type.  When an id, with a	*
 * given type, is looked up in the poly table, an entry is made for it	*
 * in the mono table, so that subsequent lookups of that id of the same	*
 * type will be fast.							*
 *									*
 * Note: When doing a lookup in the mono table, types are compared	*
 * for equality using pointer equality testing.  It is critical		*
 * therefore that types be stored only once.  Function type_tb(t)	*
 * returns the unique entry for type t.					*
 *									*
 * Lookup in the poly table and the mono table are handled by		*
 * function global_eval, in evaluate/lazy.c.  The functions here	*
 * are concerned with setting up outer_bindings, and the poly table.	*
 *									*
 * When execution of a declaration is started, an environment is	*
 * created holding the globals (with specific types) that are used	*
 * in that declaration.  The second aspect of global handled here is	*
 * setting up that environment.  The setup is handled by 		*
 * function setup_globals.						*
 ************************************************************************/


#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include "../lexer/modes.h"
#include "../alloc/allocate.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/rdwrt.h"
#include "../utils/filename.h"
#include "../unify/unify.h"
#include "../machdata/entity.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/instruc.h"
#include "../evaluate/evaluate.h"
#include "../tables/tables.h"
#include "../clstbl/typehash.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../show/printrts.h"
#include "../gc/gc.h"
#ifdef DEBUG
# include "../debug/debug.h"
# include "../show/prtent.h"
#endif

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			outer_bindings				*
 ****************************************************************
 * outer_bindings[i] tells about the identifier with index i.	*
 * It includes 							*
 *    outer_bindings[i].name  	    (the name of the id), 	*
 *    outer_bindings[i].mono_table  (the mono table for this id)*
 *    outer_bindings[i].poly_table  (the poly table for this id)*
 ****************************************************************/

GLOBAL_HEADER_PTR outer_bindings;    /* Table of global ids */ 

/****************************************************************
 *			next_ent_num				*
 ****************************************************************
 * next_ent_num is the next index (in outer_bindings) to be	*
 * assigned to an id.						*
 ****************************************************************/

int next_ent_num;

/****************************************************************
 *			theCommandLine				*
 ****************************************************************
 * theCommandLine is the value of id commandLine.  It is set	*
 * in intrprtr.c.						*
 ****************************************************************/

ENTITY theCommandLine;

/****************************************************************
 *			set_start_loc				*
 ****************************************************************
 * set_start_loc is for communication with read_to_end (in	*
 * package.c.)							*
 *								*
 * When read_to_end reads a GET_GLOBAL_I instruction, it must	*
 * be careful when that instruction occurs in a definition	*
 * with mode COPY_MODE, as would occur in			*
 *								*
 *     Let{copy} a  = b.					*
 *								*
 * when b is fetched.  It is necessary to know where in b's 	*
 * poly table to start the search, to avoid getting		*
 * definitions that are made after the copy declaration.	*
 * A pointer to the poly table node for b is placed in the	*
 * 'start' field for the poly table entry for a that is being	*
 * created by this definition.  set_start_loc is set to the	*
 * address of the 'start' field if such a link is necessary, 	*
 * and to NULL if not.  read_to_end will then patch the		*
 * content of *set_start_loc.					*
 ****************************************************************/

GLOBAL_TABLE_NODE** set_start_loc = NULL;

/****************************************************************
 *			do_overlap_tests			*
 ****************************************************************
 * If do_overlap_tests is true, then each time an entry is made *
 * into the outer id table, it is tested against other entries  *
 * to see if there is an unacceptable overlap.			*
 * do_overlap_tests is set false (in intrprtr.c) when reading   *
 * the standard package, and for all packages if a quick load   *
 * is requested.						*
 ****************************************************************/

Boolean do_overlap_tests;


/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			entity_name_table			*
 ****************************************************************
 * entity_name_table associates, with each id, its index in	*
 * array outer_bindings.					*
 ****************************************************************/

PRIVATE HASH2_TABLE* entity_name_table = NULL;

/****************************************************************
 *			outer_bindings_size			*
 ****************************************************************
 * outer_bindings_size is the physical size of outer_bindings.  *
 * It is used to determine when to reallocate outer_bindings.	*
 ****************************************************************/

PRIVATE int outer_bindings_size;


/****************************************************************
 *			ENT_STR_TB				*
 ****************************************************************
 * Returns the index in outer_bindings of global id s.  If s    *
 * has not yet been assigned an offset, then one is assigned to *
 * it.								*
 ****************************************************************/

SIZE_T ent_str_tb(char *s)
{
  HASH2_CELLPTR c;
  HASH_KEY u;
  char *ss;
  LONG h;
  GLOBAL_HEADER_PTR p;

  /*------------------------------------*
   * Locate s in the entity name table. *
   *------------------------------------*/

  u.str = s;
  h     = strhash(s);
  c     = insert_loc_hash2(&entity_name_table, u, h, equalstr);

  /*-----------------------------------*
   * If s is there, return its number. *
   *-----------------------------------*/

  if(c->key.num != 0) return c->val.num;

  /*---------------------------------------------*
   * If s is not yet in the table, put it there. *
   *---------------------------------------------*/

  c->key.str = ss = make_perm_str(s);

  /*------------------------------------------------------------------*
   * Assign s an index, and put a record in outer_bindings at that    *
   * offset. 							      *
   *------------------------------------------------------------------*/

  if(next_ent_num >= outer_bindings_size) reallocate_outer_bindings();
  p             = outer_bindings + next_ent_num;
  p->name       = ss;
  p->poly_table = NULL;
  p->mono_table = NULL;
  return(c->val.num = next_ent_num++);
}


/****************************************************************
 *			INIT_MONO_GLOBALS			*
 ****************************************************************
 * We insert global "commandLine" only in the mono table.  It	*
 * has no poly_table entry.					*
 ****************************************************************/

PRIVATE UBYTE strlist_build[5] = 
  {STAND_T_I, 0 /* replaced below */, LIST_T_I, LIST_T_I, END_LET_I};

void init_mono_globals()
{
  strlist_build[1] = Char_std_num;
  mono_global("commandLine", strlist_build, 
	      type_tb(list_t(type_tb(string_type))), 
	      theCommandLine);
}


/********************************************************
 *			MONO_LOOKUP			*
 ********************************************************
 * Look up outer_bindings[name_index], of type t, in the*
 * monomorphic global id table.  If found, then set e   *
 * to the value found and return TRUE. If not found,    *
 * return FALSE.					*
 *							*
 * Used in tables/m_glob.c to look up a global, and 	*
 * in global_eval (above) to evaluate a global.		*
 ********************************************************/

Boolean mono_lookup(TYPE *t, LONG name_index, ENTITY *e)
{
  HASH2_TABLE *tab;
  HASH2_CELLPTR h;
  HASH_KEY typekey;

# ifdef DEBUG
    if(t->THASH == 0) {
      trace_i(111, outer_bindings[name_index].name);
      long_print_ty(t, 1);
    }
# endif

  /*------------------------------------------*
   * Get the table in which to do the lookup. *
   *------------------------------------------*/

  tab = outer_bindings[name_index].mono_table;

# ifdef DEBUG
    if(trace_global_eval) {
       if(tab == NULL) trace_i(112);
     }
# endif

  /*--------------------------------------------*
   * Do the lookup if there is a nonnull table. *
   *--------------------------------------------*/

  if(tab != NULL) {
    typekey.type = t;
    h = locate_hash2(tab, typekey, t->THASH, eq);
    if(h->key.num != 0) {

#     ifdef DEBUG
        if(trace_global_eval) {
          trace_i(113);
          trace_print_entity(h->val.entity);
	  tracenl();
        }
#     endif

      *e = h->val.entity;
      return TRUE;
    }

#   ifdef DEBUG
      if(trace_global_eval)  trace_i(114, t->THASH);
#   endif

  }

  /*-------------------------------------------------------*
   * If the table is null, then it is empty, so this id is *
   * not present.					   *
   *-------------------------------------------------------*/

  return FALSE;
}


/****************************************************************
 *			MONO_GLOBAL				*
 ****************************************************************
 * Insert global id 'name' into the mono table, with only a     *
 * token entry in the poly table.				*
 ****************************************************************/

void mono_global(char *name, CODE_PTR type_instrs, TYPE *t, ENTITY val)
{
  SIZE_T name_index;
  HASH2_TABLE **tabl;
  HASH2_CELLPTR h;
  HASH_KEY u;

  name_index = ent_str_tb(name);
  bump_type(t);
  SET_TYPE(t, type_tb(t));
  insert_global(name_index, type_instrs, t, 0, 0, 0, 1);
  tabl          = &(outer_bindings[name_index].mono_table);
  u.type        = t;
  h             = insert_loc_hash2(tabl, u, t->THASH, eq);
  h->key        = u;
  h->val.entity = val;
  drop_type(t);
}


/****************************************************************
 *			REPORT_DEFINITION_CONFLICT		*
 ****************************************************************
 * Report that there are conflicting definitions of name.	*
 * Parameter type is the type that is just now being installed, *
 * and mode is its mode.  g is the old table node with which	*
 * this definition conflicts.					*
 ****************************************************************/

PRIVATE char* irregular_string(int mode)
{
  if(mode & IRREGULAR_MODE_MASK) return "{irregular}";
  else return "";
}

/*------------------------------------------------------------*/

PRIVATE void 
report_definition_conflict(char *name, TYPE *type, int mode,
			   GLOBAL_TABLE_NODE *g)
{
  int l;
# ifndef SMALL_STACK
    char message[MAX_OVERLAP_MESSAGE_LENGTH + 1];
# else
    char *message = (char *) BAREMALLOC(MAX_OVERLAP_MESSAGE_LENGTH + 1);
# endif

  char* ext_current_file_name = strdup(current_pack_params->file_name);
  char* ext_pack_file_name    = strdup(package_descr[g->packnum].file_name);

  force_external(ext_current_file_name);
  force_external(ext_pack_file_name);
  if(2*strlen(name) + strlen(current_pack_params->name) + 
     strlen(ext_current_file_name) + 87 > MAX_OVERLAP_MESSAGE_LENGTH) {
    sprintf(message, "astr: Conflicing definitions\n");
  }
  else {
    sprintf(message, 
	    "astr: Conflicting definitions of %s\n"
	    "  package %s (file %s)\n     has %s%s: ",
	    name, 
	    current_pack_params->name, 
	    ext_current_file_name, 
	    irregular_string(mode),
	    name);
    l = strlen(message);
    sprint_ty(message + l, 
	      (MAX_OVERLAP_MESSAGE_LENGTH - 31 -
	       MAX_PACKAGE_NAME_LENGTH - MAX_FILE_NAME_LENGTH -
	       - MAX_NAME_LENGTH) - l,
	      type);
    l = strlen(message);
    if(strlen(package_descr[g->packnum].name) + 
       strlen(ext_pack_file_name) + strlen(name) + 42 
       <= MAX_OVERLAP_MESSAGE_LENGTH) {
      sprintf(message + l, 
	      "\n  package %s (file %s)\n     has %s%s: ",
	      package_descr[g->packnum].name, 
	      ext_pack_file_name, 
	      irregular_string(g->mode),
	      name);
      l = strlen(message);
      sprint_ty(message + l, MAX_OVERLAP_MESSAGE_LENGTH - l - 1, g->V);
      strcat(message, "\n");
    }
  }

# ifdef MSWIN
    if(strlen(message) + 9 <= MAX_OVERLAP_MESSAGE_LENGTH) {
      strcat(message, "Continue?");
    }
# endif

  FREE(ext_current_file_name);
  FREE(ext_pack_file_name);

  possibly_abort_dialog(message);

# ifdef SMALL_STACK
     FREE(message);
#  endif

}


/****************************************************************
 *			OVERLAP_TEST				*
 ****************************************************************
 * overlap_test(name_index, type, mode) checks whether a new	*
 * definiton of outer_bindings[name_index]: type, with given    *
 * mode, is in conflict with any former definitions.		*
 ****************************************************************/

PRIVATE void overlap_test(LONG name_index, TYPE *type, int mode)
{
  GLOBAL_TABLE_NODE *g;
  Boolean conflict_found = FALSE;
  int     underrides     = mode & UNDERRIDES_MODE_MASK;
  int     overrides      = mode & OVERRIDES_MODE_MASK;
  int     primitive      = mode & PRIMITIVE_MODE_MASK;
  int     is_irregular   = mode & IRREGULAR_MODE_MASK;

  /*-----------------------------------*
   * Test against each previous entry. *
   *-----------------------------------*/
 
  for(g = outer_bindings[name_index].poly_table; 
      g != NULL && !conflict_found; 
      g = g->next) {

    int g_mode = g->mode;
    Boolean different_irregular = 
            is_irregular != (g_mode & IRREGULAR_MODE_MASK);
    Boolean type_overlap_ok =
              underrides || 
	      (g_mode & UNDERRIDES_MODE_MASK) ||
	      (overrides && (g_mode & DEFAULT_MODE_MASK)) ||
	      (primitive && (g_mode & PRIMITIVE_MODE_MASK));
		
    if(different_irregular || !type_overlap_ok) {

      /*-------------------------------------*
       * Get the types for the overlap test  *
       * (For irregular definitions, we must *
       * replace the codomain by `x.)	     *
       * Then do the overlap computation.    *
       *-------------------------------------*/

      int disj;
      TYPE* new_defn_cover_type =
	     is_irregular 
		  ? function_t(find_u(type)->TY1, var_t(NULL))
		  : type;
      TYPE* old_defn_cover_type =
              (g_mode & IRREGULAR_MODE_MASK) 
		? function_t(find_u(g->V)->TY1, var_t(NULL))
		: g->V;
      bump_type(new_defn_cover_type);
      bump_type(old_defn_cover_type);
      disj = disjoint(new_defn_cover_type, old_defn_cover_type);
      drop_type(new_defn_cover_type);
      drop_type(old_defn_cover_type);

      /*--------------------------------------------------------*
       * If the definitions have overlapping types, then	*
       * complain.						*
       *--------------------------------------------------------*/

      if(!disj) {
#       ifdef DEBUG
	  if(trace) {
	    trace_i(317, nonnull(outer_bindings[name_index].name), 
		    mode, g_mode);
	  }
#       endif
	conflict_found = TRUE;
	report_definition_conflict(outer_bindings[name_index].name, 
				   type, mode, g);
      }

    } /* if(different_irregular...) */

  } /* end for(g = ...) */
}


/****************************************************************
 *			INSERT_GLOBAL				*
 ****************************************************************
 * Make a poly-table entry at index name_index.  The parts are	*
 *								*
 *    type		The (polymorphic) type of this id.	*
 *								*
 *    type_instrs	A sequence of instructions whose 	*
 *			evaluation creates 'type'.		*
 *								*
 *    packnum		The index of the package that makes	*
 *			this definition.			*
 *								*
 *    offset		The offset in the code for package	*
 *			packnum where the code to compute this	*
 *			id starts.				*
 *								*
 *    mode		The declaration mode (low order byte	*
 *			only) for this definition.		*
 *								*
 *    dummy		True if this is a dummy entry --  one	*
 *			that cannot be used to get a definition.*
 *								*
 * If do_overlap_tests is true, the do tests with other		*
 * definitions.  (This is used to suppress overlap tests for	*
 * quick loads, and while reading standard.ast.)		*
 ****************************************************************/

void insert_global(LONG name_index, CODE_PTR type_instrs, TYPE *type, 
		   int packnum, LONG offset, int mode, Boolean dummy)
{
  GLOBAL_TABLE_NODE *g, **q;
  LONG underrides;

  if(name_index < 0 || name_index >= next_ent_num) die(46);

# ifdef DEBUG
    if(trace || trace_global_eval || trace_env_descr) {
      trace_i(257, outer_bindings[name_index].name, name_index, packnum, 
	      offset, mode);
    }
# endif

  underrides = mode & UNDERRIDES_MODE_MASK;

  /*----------------------------------------------------*
   * Check for conflicts with existing declarations.  	*
   *----------------------------------------------------*/

  if(do_overlap_tests) {
    overlap_test(name_index, type, mode);
  } 

  /*------------------------------------------------------*
   * Normally insert at the front of the list, but insert *
   * underrides at the end. 				  *
   *------------------------------------------------------*/

  q = &(outer_bindings[name_index].poly_table);
  if(underrides) {
    while(*q != NULL) q = &((**q).next);
  }
  g              = allocate_global_table();
  bump_type(g->V = type_tb(type));
  g->mode        = (UBYTE) mode;
  g->packnum     = packnum;
  g->offset      = offset;
  g->next        = *q;
  g->ty_instrs   = type_instrs;
  g->dummy	 = dummy;
  g->start	 = NULL;
  *q             = g;

  /*---------------------------------------------------------*
   * For a copy dcl, indicate to package.c:read_to_end where *
   * to put a link for starting search.			     *
   *---------------------------------------------------------*/

  set_start_loc = (mode & COPY_MODE_MASK) ? &(g->start) : NULL;

# ifdef DEBUG
    if(trace || trace_global_eval) {
      GLOBAL_TABLE_NODE *p;
      trace_i(258, outer_bindings[name_index].name);
      trace_ty(type); tracenl();
      trace_i(259, outer_bindings[name_index].name);
      for(p = outer_bindings[name_index].poly_table; p != NULL; p = p->next) {
	trace_ty(p->V);
	tracenl();
      }
    }
# endif
}


/************************************************************
 *			POLY_LOOKUP			    *
 ************************************************************
 * Try to find an entry in the poly table starting at start *
 * that has a class that contains type t.  Return it if     *
 * found, or NULL if not found.				    *
 ************************************************************/

GLOBAL_TABLE_NODE* poly_lookup(GLOBAL_TABLE_NODE *start, TYPE *t)
{
  GLOBAL_TABLE_NODE* g = start;

# ifdef DEBUG
    if(trace_global_eval && g == NULL) trace_i(333);
# endif

  /*----------------------------------------------------------*
   * Try to unify t with each type in the list of polymorphic *
   * types for this id.  Note that we can use basic_unify_u   *
   * (unification without the occur check) since t cannot     *
   * contain any unbound variables.                           *
   *							      *
   * When checking an irregular function, only unify the      *
   * domains of the functions.				      *
   *----------------------------------------------------------*/

  clear_new_binding_list_u();
  while(g != NULL) {
#   ifdef DEBUG
      if(trace_global_eval) {
	trace_i(335, nonnull(package_descr[g->packnum].name));
	trace_ty(g->V);
	tracenl();
      }
#   endif

    if(g->dummy) {
#     ifdef DEBUG
        if(trace_global_eval) trace_i(334);
#     endif
      g = g->next;
      continue;
    }

    if((g->mode & IRREGULAR_MODE_MASK) == 0) {
      if(BASIC_UNIFY(t, g->V, TRUE, NULL)) break;
    }
    else {
      if(BASIC_UNIFY(t->TY1, g->V->TY1, TRUE, NULL)) break;
    }

#   ifdef DEBUG
      if(trace_global_eval) trace_i(110);
#   endif

    undo_bindings_u(NIL);
    g = g->next;
  }

  undo_bindings_u(NIL);
  return g;
}


/****************************************************************
 *			SETUP_GLOBALS				*
 ****************************************************************
 * If f = NULL, processes instructions, starting at *cc, 	*
 * for building a global environment.  *cc is set to the	*
 * address just after the global environment building		*
 * instructions.  An environment holding the definitions is	*
 * returned.  Its reference count is 1.				*
 *								*
 * If f != NULL, a similar thing is done, but the input comes	*
 * from file f.							*
 ****************************************************************/

ENVIRONMENT* setup_globals(CODE_PTR *cc, FILE *f)
{
  int n, num_globals, inst, old_use_the_act_bindings;
  LONG m;
  struct envcell *env_open;
  ENTITY e, *a;
  ENVIRONMENT *env;
  TYPE *t, *tt;

  n = (f != NULL) ? fgetuc(f) : *((*cc)++);
  if(n == 0) {

    /*------------------------------------------*
     * There are no globals to get when n is 0. *
     * Read the STOP_G_I instruction. 		*
     *------------------------------------------*/

    n = (f != NULL) ? fgetuc(f) : *((*cc)++);
    if(n != STOP_G_I) die(47);
    return NULL;
  }

# ifdef DEBUG
    if(trace_global_eval) {
      if(n < 0 || n > 3) {
	trace_i(260, n,
               *cc, toint((*cc)[0]), toint((*cc)[1]), toint((*cc)[2]), 
	       toint((*cc)[3]), toint((*cc)[4]));
      }
    }
# endif

  /*--------------------------------*
   * Allocate the environment node. *
   *--------------------------------*/

# ifdef DEBUG
    allocated_global_envs++;
# endif

  env		 = allocate_env(n);  /* Ref from allocate_env. */
  env->kind	 = GLOBAL_ENV;
  env->link	 = NULL;
  env->num_link_entries = 0;
  env_open       = env->cells;
  num_globals    = 0;

  /*------------------------------------------------*
   * Read the instructions that set up the globals. *
   *------------------------------------------------*/

  old_use_the_act_bindings = use_the_act_bindings;
  use_the_act_bindings = 0;
  for(;;) {
    inst = (f == NULL) ? inst = *((*cc)++) : fgetuc(f);

#   ifdef DEBUG
      if(trace_global_eval) {
	trace_i(261, inst, *cc - 1);
      }
#   endif

    switch(inst) {
      /*-----------------------------------------*/
      TYPE_INSTRUCTIONS
	type_instruction_i(inst, cc, f, env, 0);
	break;

      /*-----------------------------------------*/
      case GET_GLOBAL_I:
	m = (f == NULL) ? next_int_m(cc) : index_at_label(toint(get_int_m(f)));

#       ifdef DEBUG
	  if(trace_global_eval) {
	    trace_i(262, num_globals, outer_bindings[m].name);
	    trace_ty(top_type_stk());
	    tracenl();
	  }
#       endif

	num_globals++;
        t  = pop_type_stk();                    /* ref from type stack */
        tt = type_tb(t);
        tt->freeze = 1;      			/* Preserve forever */
	a    = allocate_entity(2);
	a[1] = ENTP(TYPE_TAG, tt);
        env_open->val = ENTP(GLOBAL_INDIRECT_TAG, a);
        if(tt->copy == 0 && mono_lookup(tt, m, &e)) {
	  a[0] = e;
	}
        else {
	  a[0] = ENTG(m);
	}
	env_open++;
        drop_type(t);
	break;

      /*-----------------------------------------*/
      case TYPE_ONLY_I:

#       ifdef DEBUG
         if(trace_global_eval) {
	   if(f == NULL) trace_i(263, *cc, num_globals);
	   else          trace_i(264, num_globals);
	 }
#       endif

	num_globals++;
        t  = pop_type_stk();                    /* ref from type stack */
        tt = type_tb(t);
        tt->freeze = 1;       			/* Preserve forever */
        (env_open++)->val = ENTP(TYPE_TAG, tt); 
        drop_type(t);
	break;

      /*-----------------------------------------*/
      case STOP_G_I:
	env->most_entries = num_globals;
        env->cells[num_globals-1].refs = 1;

#       ifdef DEBUG
          if(trace_global_eval) trace_i(265);
#       endif

        use_the_act_bindings = old_use_the_act_bindings;
	return env;

      /*-----------------------------------------*/
      default:
	die(48, inst);

    } /* end switch */
  } /* end for */
}


/****************************************************************
 *			REALLOCATE_OUTER_BINDINGS		*
 ****************************************************************
 * Double the size of outer_bindings.				*
 ****************************************************************/

void reallocate_outer_bindings()
{
  SIZE_T new_size;

  new_size = 2*outer_bindings_size;
  outer_bindings = (GLOBAL_HEADER_PTR) reallocate
                   ((char *) outer_bindings,
		    outer_bindings_size * sizeof(struct global_header),
		    new_size * sizeof(struct global_header), TRUE);
  outer_bindings_size = new_size;
}


/****************************************************************
 *			INIT_GLOBALS				*
 ****************************************************************/

void init_globals()
{
  next_ent_num      = 0;
  entity_name_table = create_hash2(9);
  outer_bindings_size    = OUTER_BINDINGS_SIZE_INIT;
  outer_bindings = (GLOBAL_HEADER_PTR) 
	      alloc(outer_bindings_size * sizeof(struct global_header));
}
