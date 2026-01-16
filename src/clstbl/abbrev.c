/****************************************************************
 * File:    clstbl/abbrev.c
 * Purpose: Table manager for species, etc. abbreviations, as
 *          created by Abbrev declarations.
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
 * Abbreviations created by Abbrev declarations are placed in   *
 * tables that are stored in the file-info frames that are      *
 * managed in fileinfo.c.  That way, each package gets its own  *
 * abbreviation table, making it easy to back out of local      *
 * abbreviations when a package is finished.  			*
 *								*
 * Abbreviations can be local to a package or global (exported).*
 * There are actually two abbreviation tables in each frame, 	*
 * one holding local (and global) abbreviations made in that	*
 * package, and one holding only global abbreviations.  That	*
 * is to make it easy to archive the global abbreviations for	*
 * use during subsequent imports of the same package.		*
 *								*
 * Each abbreviation table is a hash table keyed by the name    *
 * of the abbreviation and holding a pointer to a 		*
 * CLASS_TABLE_CELL cell that describes the abbreviation.	*
 * The contents of each such CLASS_TABLE_CELL are   		*
 * similar to the entries in class_id_table, managed by		*
 * classtbl.c.  Each entry contains a code that tells what kind *
 * of thing the entry describes.  Codes for abbreviations are	*
 * as follows.							*
 *								*
 *  UNKNOWN_CLASS_ID_CODE   An unknown id.  (An abbreviation	*
 *			    that has been undone.)		*
 *								*
 *  TYPE_ABBREV_CODE        An abbreviation for a 		*
 *			    (polymorphic) species expr.  	*
 *								*
 *  FAM_ABBREV_CODE	    An abbreviation for a family 	*
 *								*
 *  FAM_VAR_ABBREV_CODE     An abbreviation for a  		*
 *			    family variable			*
 *								*
 *  GENUS_ABBREV_CODE       An abbreviation for a genus.	*
 *								*
 *  COMM_ABBREV_CODE        An abbreviation for a 		*
 *			    community				*
 *								*
 * The fields that are stored in the table entry are as		*
 * follows.							*
 *								*
 *  name	The name of the thing that this abbreviates,    *
 *		in the case where that is appropriate.  For	*
 *		example, if this id abbreviates a genus, then	*
 *		name is the name of that genus.			*
 *								*
 *  ty		If this is a species or variable		*
 *		abbreviation, then ty is what is abbreviated.	*
 *								*
 *  role	The main role of a species abbreviation.	*
 *								*
 *  package	The name of the package where this abbreviation *
 *		was defined.					*
 *								*
 ****************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/abbrev.h"
#include "../error/error.h"
#include "../evaluate/instruc.h"
#include "../generate/generate.h"
#include "../parser/parser.h"
#include "../unify/unify.h"
#ifdef TRANSLATOR
# include "../dcls/dcls.h"
# include "../exprs/expr.h"
#endif
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			    UNABBREV_TM				*
 ****************************************************************
 * Remove abbreviation u from hash table tbl.  Parameter hash   *
 * must be the hash value of u.					*
 *								*
 * This is a helper for abbrev_tm.				*
 ****************************************************************/

PRIVATE void unabbrev_tm(HASH2_TABLE **tbl, HASH_KEY u, LONG hash)
{
  HASH2_CELLPTR h;

  h = locate_hash2(*tbl, u, hash, eq);
  if(h->key.num != 0) {
    h->val.ctc->code = UNKNOWN_CLASS_ID_CODE;
    SET_TYPE(h->val.ctc->ty, NULL_T);
  }
}


/****************************************************************
 *			    INSTALL_ABBREV_TM			*
 ****************************************************************
 * Install abbreviation u.str = t (with role r) into table *tbl,*
 * where t has kind tok.  tok must be the token equivalent of   *
 * one of the CODE values described in the large comment above. *
 * For example, it might be TYPE_ABBREV_TOK.			*
 *								*
 * Hash must be strhash(u.str).					*
 *								*
 * If tok is QUESTION_MARK_TOK, then we are asked to remove	*
 * abbreviation u.str.						*
 *								*
 * This is a helper for abbrev_tm.				*
 ****************************************************************/

PRIVATE void install_abbrev_tm(HASH2_TABLE **tbl, HASH_KEY u, int tok, 
			      TYPE *t, ROLE *r, LONG hash)
{
  HASH2_CELLPTR h;
  CLASS_TABLE_CELL *c;

# ifdef DEBUG
    if(trace_classtbl > 1) trace_s(64, u.str, tok, tbl);
# endif

  /*------------------------------------------------------------*
   * Check for unabbrev. If tok is QUESTION_MARK_TOK, then we   *
   * are doing Abbrev u.str = ?.				*
   *------------------------------------------------------------*/

  if(tok == QUESTION_MARK_TOK) {
    unabbrev_tm(tbl, u, hash);
    return;
  }

  /*--------------------------------------------------------*
   * Locate the position for the abbreviation installation. *
   *--------------------------------------------------------*/

  h = insert_loc_hash2(tbl, u, hash, eq);
  if(h->key.str == NULL) {
    h->key.str = u.str;
    h->val.ctc = allocate_ctc();
  }
  c = h->val.ctc;
      
  /*---------------------------*
   * Install the abbreviation. *
   *---------------------------*/

  c->code = MAKE_CODE(tok);
  c->name = u.str;
  bump_type(t);
  replace_null_vars(&t);
  c->ty = t;                /* inherits ref from t. */
  if(tok == TYPE_ABBREV_TOK || tok == FAM_MACRO_TOK) {
    bump_role(c->role = r);
  }
}


/****************************************************************
 *			    ABBREV_TM				*
 ****************************************************************
 * Install abbreviation id, of kind tok, with value t and role  *
 * r.  tok is the token equivalent of one of the CODE values    *
 * described in the large comment above.			*
 *								*
 * If tok is QUESTION_MARK_TOK, then remove abbreviation t	*
 * instead.							*
 *								*
 * The abbreviation is global if global is TRUE.  Otherwise, it *
 * is only installed in the table of the current package's 	*
 * frame, and it will disappear as soon as the current package  *
 * finishes.							*
 ****************************************************************/

void abbrev_tm(char *id, int tok, TYPE *t, ROLE *r, Boolean global)
{
  HASH_KEY u;
  LONG hash;

  if(local_error_occurred) return;

  u.str = new_name(id,FALSE);

# ifdef DEBUG
    if(trace_classtbl) {
      trace_s(65, tok, u.str, u.str);
      trace_ty(t); tracenl();
    }
# endif

  /*-------------------------------------------------------------*
   * Check to see whether id is already in use to name entities. *
   * If it is, we can't redefine it.				 *
   *-------------------------------------------------------------*/

  if(check_for_ent_id(u.str, 0)) return;

  /*---------------------------------------------------------*
   * See if id is already being used to name a species, etc. *
   *---------------------------------------------------------*/

  hash = strhash(u.str);
  if(is_class_id(u.str, hash)) {
    semantic_error1(ID_DEFINED_ERR, display_name(u.str), 0);
    return;
  }

  /*-------------------------------------------------------------*
   * Do the abbreviation.  If this is a local abbreviation, then *
   * install it only in the current file-info frame.  If this is *
   * a global abbreviation, then install it in all of the 	 *
   * file-info frames in the current stack of frames.  We 	 *
   * install in both the local and global abbrev tables, so that *
   * we only need to do lookups in the local tables.		 *
   *-------------------------------------------------------------*/

  {IMPORT_STACK* p = current_or_standard_frame();
   do {
     install_abbrev_tm(&(p->abbrev_id_table), u, tok, t, r, hash);
     if(global) {
       install_abbrev_tm(&(p->global_abbrev_id_table), u, tok, t, r, hash);
     }
     p = p->next;
   } while(global && p != NULL);
  }

  /*----------------------------------------------------------------*
   * If this abbreviation is global and is not an unabbrev, and     *
   * main_context is EXPORT_CX, then write this id into index_file. *
   * (This is the .asx file, for help.)				    *
   *----------------------------------------------------------------*/

  if(   global
     && tok != QUESTION_MARK_TOK 
     && doing_export()
     && index_file != NULL) {
    fprintf(index_file, "%c%s\n", 'A', id);
  }
}


/****************************************************************
 *			    GET_ABBREV_TM			*
 ****************************************************************
 * Return the class table cell for abbreviation u, whose hash	*
 * value is hash.  If there is no such abbreviation, return	*
 * NULL.							*
 ****************************************************************/

CLASS_TABLE_CELL* get_abbrev_tm(HASH_KEY u, LONG hash)
{
  HASH2_TABLE *tbl;
  HASH2_CELLPTR h;

  /*----------------------------*
   * First try the local table. *
   *----------------------------*/

  h = locate_hash2(current_abbrev_id_table, u, hash, eq);
  if(h->key.num != 0
     && h->val.ctc->code != UNKNOWN_CLASS_ID_CODE) return h->val.ctc;

  /*--------------------------------------------------------------*
   * If the local table has nothing to say, try the global table. *
   *--------------------------------------------------------------*/

  tbl = (current_package_name == standard_package_name)
	     ? standard_import_frame->abbrev_id_table
	     : standard_import_frame->global_abbrev_id_table;
  h = locate_hash2(tbl, u, hash, eq);
  if(h->key.num != 0) return h->val.ctc;
  else return NULL;
}


