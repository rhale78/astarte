/***************************************************************
 * File:    tables/assume.c
 * Purpose: Handler for assume tables.
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

/****************************************************************
 * The assume tables hold information from assume declarations. *
 * The tables are stored with each package, in the file-info	*
 * record managed by fileinfo.c.  The functions here manage	*
 * that information.						*
 *								*
 * There are six hash tables in each frame to handle assumes.	*
 * They come in groups of two, a local table and a global table.*
 * The global table holds assumptions that are exported, and    *
 * the local table holds assumptions that are either exported   *
 * or are local to the package.					*
 * The reason for having two tables is to make			*
 * it easy to record exported information for subsequent	*
 * imports of the same package.					*
 *								*
 * The three pairs of tables are as follows.			*
 *								*
 *   assume_table, global_assume_table:				*
 *	- the key is an identifier (a string) and the value	*
 *	  is a type for that id.				*
 *								*
 *   assume_role_table, global_assume_role_table		*
 *	- the key is an identifier and the value is a role.	*
 *        (This is used to get the role of the type.)		*
 *								*
 *   patfun_assume_table, global_patfun_assume_table		*
 *	- These tables associate value 1 with each identifier	*
 *	  that has been assumed to be a pattern function, using *
 *	    Assume id: pattern.					*
 *								*
 * Identifiers are stored under their basic names, not under    *
 * modified names.  For example, identifier p:x is stored in    *
 * the assume table under x.					*
 *								*
 * In addition to these hash table, each file-info frame holds	*
 * four types,							*
 *    nat_const_assumption 	    - local assumption for 	*
 *				      natural number constants	*
 *    global_nat_const_assumption   - global assumption for 	*
 *				      natural number constants.	*
 *    real_const_assumption 	    - local assumption for 	*
 *				      real number constants	*
 *    global_real_const_assumption  - global assumption for 	*
 *				      real number constants.	*
 ****************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../unify/unify.h"
#include "../classes/classes.h"
#include "../standard/stdfuns.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../exprs/expr.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../dcls/dcls.h"
#include "../parser/parser.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../generate/prim.h"
#ifdef SMALL_STACK
#  ifdef USE_MALLOC_H
#    include <malloc.h>
#  else
#    include <stdlib.h>
#  endif
# include "../alloc/allocate.h"
#endif
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			    GEN_ASSUME_TM			*
 *			    PUT_VAL_TM				*
 ****************************************************************
 * Gen_assume_tm installs an assumption id:v. 			*
 * kind tells which table the assumption should be		*
 * installed in:						*
 *								*
 *   1 for assume_table (v is a type)				*
 *   2 for assume_role_table (v is a role)			*
 *   3 for patfun_assume_table (v is a number).			*
 *								*
 * 'clear' is true if the assumption should be removed rather	*
 * than added.  						*
 *								*
 * 'global' is true if the assumption should be			*
 * made in all tables that can see this assumption.		*
 *								*
 * put_val_tm is a helper that does a similar job on a single	*
 * hash table.							*
 ****************************************************************/

PRIVATE void put_val_tm(HASH2_TABLE **tbl, HASH_KEY u, LONG hash, 
		       HASH2_VAL v, int val_kind)
{
  HASH2_CELLPTR h;

  h = insert_loc_hash2(tbl, u, hash, eq);
  if(h->key.num == 0) {
    h->key.str = u.str;
    h->val.num = 0;
  }
  if(val_kind == 1) SET_TYPE(h->val.type, v.type);
  else if(val_kind == 2) SET_ROLE(h->val.role, v.role);
  else h->val.num = v.num;
}

/*----------------------------------------------------------------*/

PRIVATE void gen_assume_tm(char *id, HASH2_VAL v, int kind, 
			  Boolean clear, Boolean global)
{
  HASH_KEY u;
  HASH2_TABLE** tbl     = NULL;
  HASH2_TABLE** globtbl = NULL;
  IMPORT_STACK *p;
  LONG hash;
  int val_kind;

  if(!local_error_occurred) {
    id    = name_tail(id);
    hash  = strhash(id);
    u.str = id_tb10(id, hash);

    /*----------------------------------------------------------*
     * Install the assumption in the local and global table(s). *
     *----------------------------------------------------------*/

    p = file_info_st;
    do {
      switch(kind) {
        case 1:
	  tbl     = &(p->assume_table);
          globtbl = &(p->global_assume_table);
          break;

        case 2:
          tbl     = &(p->assume_role_table);
          globtbl = &(p->global_assume_role_table);
          break;

        case 3:
	  tbl     = &(p->patfun_assume_table);
	  globtbl = &(p->global_patfun_assume_table);
	  break;
      }
      val_kind = clear ? 0 : kind;

      put_val_tm(tbl, u, hash, v, val_kind);
      if(global) put_val_tm(globtbl, u, hash, v, val_kind);
      p = p->next;
    } while(global && p != NULL);
  }
}


/****************************************************************
 *			    ASSUME_TM				*
 ****************************************************************
 * Create an assumption id: t.  The assumption is exported if	*
 * global is true.						*
 *								*
 * This function should only be used for an assume declaration, *
 * not for an assume expression.				*
 ****************************************************************/

void assume_tm(char *id, TYPE *t, Boolean global)
{
  HASH2_VAL v;
  v.type = t;
  gen_assume_tm(id, v, 1, 0, global);
}


/****************************************************************
 *			    ASSUME_ROLE_TM			*
 ****************************************************************
 * Create an assumption id:r, where r is the role.  This must 	*
 * happen just after an associated id:ty assumption.  The 	*
 * assumption is exported if global is true.			*
 *								*
 * This function should only be used for an assume declaration, *
 * not for an assume expression.				*
 ****************************************************************/

void assume_role_tm(char *id, ROLE *r, Boolean global)
{
  HASH2_VAL v;
  v.role = r;
  gen_assume_tm(id, v, 2, 0, global);
}


/****************************************************************
 *			    ASSUMED_RTYPE_TM			*
 ****************************************************************
 * Return the type and role assumed for id, or NULL if there	*
 * is none.							*
 *								*
 * The returned type and role are not copied: they point	*
 * directly to the entry in a table.  They should not be	*
 * changed.							*
 ****************************************************************/

RTYPE assumed_rtype_tm(char *id)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  RTYPE rt;
  LONG hash;

  u.str = name_tail(id);
  hash  = strhash(u.str);
  h     = locate_hash2(current_assume_table, u, hash, eq);

  if(h->key.num != 0) {
    rt.type = h->val.type;
    h = locate_hash2(current_assume_role_table, u, hash, eq);
    rt.role = (h->key.num == 0) ? NULL : h->val.role;
  }
  else {
    rt.type = NULL; 
    rt.role = NULL;
  }
  return rt;
}


/****************************************************************
 *			    PATFUN_ASSUME_TM			*
 ****************************************************************
 * Add id to the pattern function assume table.	 That is, do	*
 * Assume id: pattern %Assume.  The assumption is exported if	*
 * global is true.						*
 ****************************************************************/

void patfun_assume_tm(char *id, Boolean global)
{
  HASH2_VAL v;
  v.num = 1;
  gen_assume_tm(id, v, 3, 0, global);
}


/****************************************************************
 *			    DELETE_PATFUN_ASSUME_TM		*
 ****************************************************************
 * Delete any assumption id:pattern.  Delete from all tables if *
 * global is true.						*
 ****************************************************************/

void delete_patfun_assume_tm(char *id, Boolean global)
{
  HASH2_VAL v;
  v.num = 0;
  gen_assume_tm(id, v, 3, 1, global);
}


/****************************************************************
 *			    IS_ASSUMED_PATFUN_TM		*
 ****************************************************************
 * Return true if id is assumed to be a pattern function.	*
 ****************************************************************/

Boolean is_assumed_patfun_tm(char *id)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  LONG hash;

  id    = name_tail(id);
  hash  = strhash(id);
  u.str = id_tb10(id, hash);

  /*-----------------------------------*
   * First try the local assume table. *
   *-----------------------------------*/

  if(get_local_pattern_assume_tm(u.str)) return TRUE;

  /*--------------------------------------------------------*
   * If there is no local assumption, try the global table. *
   *--------------------------------------------------------*/

  h = locate_hash2(current_patfun_assume_table, u, hash, eq);
  if(h->key.num != 0) {
    return tobool(h->val.num);
  }

  /*------------------------------------------*
   * If there is no assumption, return false. *
   *------------------------------------------*/

  return FALSE;
}


/****************************************************************
 *			DO_CONST_ASSUME_TM			*
 ****************************************************************
 * Perform a constant assumption.  The assumption is for 	*
 * natural number constants if kind is 0, and for real 		*
 * constants if kind is 1.  The assumed type is t.  If global 	*
 * is true, then the assumption is to be made in all tables	*
 * that see this assumption.  It is an error if t is not a	*
 * member of genus c.						*
 *								*
 * This function can be used either for an assume declaration	*
 * or an assume expression.					*
 ****************************************************************/

void do_const_assume_tm(int kind, char *c, TYPE *t, Boolean global)
{
  int ov;
  TYPE *s;

  /*----------------------------------------*
   * Check that t is beneath or equal to c. *
   *----------------------------------------*/

  bump_type(s = type_var_t(c));
  ov = half_overlap_u(t, s);
  if(ov != EQUAL_OR_CONTAINED_IN_OV) {
    semantic_error(BAD_ASSUME_ERR, 0);
    return;
  }

  /*-----------------------------------------------------*
   * For an assume declaration, install the assumption   *
   * in the local and global table(s). 			 *
   *-----------------------------------------------------*/

  if(dcl_context) {
    TYPE **where, **globwhere;
    IMPORT_STACK *p;

    p = file_info_st;
    do {
      if(kind == 0) {
        where     = &(p->nat_const_assumption);
        globwhere = &(p->global_nat_const_assumption);
      }
      else {
        where     = &(p->real_const_assumption);
        globwhere = &(p->global_real_const_assumption);
      }
      set_type(where, t);
      if(global) set_type(globwhere, t);
      p = p->next;
    } while(global && p != NULL);
  }

  /*-------------------------------*
   * Case of an assume expression. *
   *-------------------------------*/

  else {
    push_local_const_assume_tm(kind, t);
  }
}
