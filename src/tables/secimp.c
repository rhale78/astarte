/****************************************************************
 * File:    tables/secimp.c
 * Purpose: Handle second and later imports of packages.
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

/************************************************************************
 * The functions in this file handle imports of packages that have	*
 * already been imported.  They scan tables to simulate an import	*
 * of the package again, without rereading the package.  This is	*
 * essential, since rereading the package will cause definitions	*
 * to conflict.  Here, we accomplish the intent of a second import, 	*
 * without getting the conflicts.					*
 ************************************************************************/

#include <string.h>
#include <memory.h>
#include "../misc/misc.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/dflttbl.h"
#include "../clstbl/abbrev.h"
#include "../dcls/dcls.h"
#include "../exprs/expr.h"
#include "../exprs/prtexpr.h"
#include "../classes/classes.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../unify/unify.h"
#include "../error/error.h"
#include "../lexer/modes.h"
#include "../parser/parser.h"
#include "../parser/tokens.h"
#include "../patmatch/patmatch.h"
#include "../generate/prim.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			doing_second_import			*
 ****************************************************************
 * doing_second_import is nonzero when doing a second import    *
 * of a package.						*
 ****************************************************************/

PRIVATE int doing_second_import = 0;

/****************************************************************
 *			global_expectations			*
 ****************************************************************
 * global_expectations is a hash table associating with each    *
 * identifier a chain holding cells that describe every		*
 * expectation that has been done for that identifier, along 	*
 * with the visibility of the expectation. 			*
 * 								*
 * This is needed when doing a second import of a package.  	*
 * Information about expectations is not present in the frames	*
 * in file_info_st, so is not available from			*
 * import_archive_list.  That information is obtained from	*
 * global_expectations.						*
 ****************************************************************/

PRIVATE HASH2_TABLE* global_expectations = NULL;

/****************************************************************
 *			global_anticipations			*
 ****************************************************************
 * global_anticipations is a hash table associating with each   *
 * identifier a chain holding cells that describe every		*
 * anticipation that has been done for that identifier, along 	*
 * with the visibility of the anticipation. 			*
 * 								*
 * This is needed when doing a second import of a package.  	*
 * Information about anticipations is not present in the frames	*
 * in file_info_st, so is not available from			*
 * import_archive_list.  That information is obtained from	*
 * global_anticipations.					*
 ****************************************************************/

PRIVATE HASH2_TABLE* global_anticipations = NULL;

/****************************************************************
 *			secimp_current_expectation_cell		*
 *			secimp_current_anticipation_cell	*
 ****************************************************************
 * secimp_current_expectation_cell is a pointer to the cell in	*
 * global_expectations that is being processed by 		*
 * second_import_expectations.  Its purpose is as follows.	*
 *								*
 * When doing a second import, expectations are found in 	*
 * global_expectations.  When those expectations, are done,	*
 * expect_global_id_tm will come back to add_global_expectation,*
 * and request that the expectation be added to			*
 * global_expectations.  All that should really be done is that *
 * visibility should be added to the cell that is being 	*
 * processed.  That cell in pointed to by 			*
 * secimp_current_expectation_cell.  				*
 *								*
 * secimp_current_anticipation_cell is similar, but for doing	*
 * anticipations.  It points to a cell in global_anticipations. *
 ****************************************************************/

PRIVATE GLOBAL_EXPECTATION_CELL* secimp_current_expectation_cell  = NULL;
PRIVATE GLOBAL_EXPECTATION_CELL* secimp_current_anticipation_cell = NULL;

/****************************************************************
 *			secimp_imported_package			*
 ****************************************************************
 * secimp_imported_package is used to communicate the package	*
 * name to functions that are called by scan_hash2.		*
 ****************************************************************/

PRIVATE char* secimp_imported_package;

/****************************************************************
 *			NEW_GEC					*
 ****************************************************************
 * Return a new GLOBAL_EXPECTATION_CELL with the given 		*
 * information installed in it.					*
 *								*
 * MODE is a safe pointer.  It only lives as long as this	*
 * function call.						*
 ****************************************************************/

PRIVATE GLOBAL_EXPECTATION_CELL* 
new_gec(char *id, char *package_name, MODE_TYPE *mode, int line,
	TYPE *t, ROLE *r, STR_LIST *who_sees)
{
  GLOBAL_EXPECTATION_CELL *gec;
  gec = (GLOBAL_EXPECTATION_CELL *)
        alloc_small(sizeof(GLOBAL_EXPECTATION_CELL));
  gec->id           = id;
  gec->package_name = package_name;
  gec->mode         = copy_mode(mode);
  gec->line         = line;
  bump_type(gec->type = t);
  bump_role(gec->role = r);
  bump_list(gec->visible_in = who_sees);
  return gec;
}


/****************************************************************
 *			ALREADY_HAVE_GEC			*
 ****************************************************************
 * Return true if chain L contains a cell with information	*
 * name, t, r, who_sees, mode, package_name.			*
 *								*
 * MODE is a safe pointer.  It only lives as long as this	*
 * function call.						*
 ****************************************************************/

PRIVATE Boolean 
already_have_gec(GLOBAL_EXPECTATION_CELL *L, char *name, TYPE *t, ROLE *r,
		 STR_LIST *who_sees, MODE_TYPE *mode, char *package_name)
{
  GLOBAL_EXPECTATION_CELL *p;
  for(p = L; p != NULL; p = p->next) {
    if(p->id == name && 
       p->package_name == package_name && 
       mode_equal(p->mode, mode) &&
       overlap_u(p->type, t) == EQUAL_OV &&
       role_equal(p->role, r) &&
       str_list_subset(who_sees, p->visible_in)) {
      return TRUE;
    }
  }
  return FALSE;
}


/****************************************************************
 *			INSTALL_SECIMP_EXP			*
 ****************************************************************
 * Add expectation name:ty with additional information given	*
 * into the cell for identifier name in table *tbl, if it	*
 * is not already there.					*
 *								*
 * MODE is a safe pointer.  It only lives as long as this	*
 * function call.						*
 ****************************************************************/


PRIVATE void 
install_secimp_exp(HASH2_TABLE **tbl, char *name, TYPE *t, ROLE *r,
		   LIST *who_sees, MODE_TYPE *mode,
		   char *package_name, int line)
{
  HASH2_CELLPTR h;
  HASH_KEY u;

  /*----------------------------------------------------------*
   * Look up this expectation, and install NULL if none have	*
   * been made for name.					*
   *----------------------------------------------------------*/

  u.str = name;
  h = insert_loc_hash2(tbl, u, strhash(u.str), eq);
  if(h->key.num == 0) {
    h->key.str = u.str;
    h->val.global_expectation = NULL;
  }

  /*-----------------------------------------------------*
   * Install the expectation if it is not already there. *
   *-----------------------------------------------------*/

  if(!already_have_gec(h->val.global_expectation, name, t, r, who_sees, 
		       mode, package_name)) {

    /*---------------------*
     * Build the new cell. *
     *---------------------*/
    
    GLOBAL_EXPECTATION_CELL *gec;
    gec = new_gec(name, package_name, mode, line, t, r, who_sees);

    /*------------------------------------------------*
     * Install the new cell at the end of the chain.  *
     * (This makes second imports be done in the same *
     * order as they were originally.)		      *
     *------------------------------------------------*/

    gec->next = NULL;
    if(h->val.global_expectation == NULL) {
      h->val.global_expectation = gec;
    }
    else {
      GLOBAL_EXPECTATION_CELL *p;
      for(p = h->val.global_expectation; p->next != NULL; p = p->next) {}
      p->next = gec;
    }
  }
}


/****************************************************************
 *			ADD_GLOBAL_EXPECTATION			*
 ****************************************************************
 * The package called package_name is making an expectation	*
 * name:t with role r and mode MODE.  who_sees tells which	*
 * packages can see this expectation.  Enter the expectation	*
 * into global_expectations.					*
 *								*
 * If this expectation is coming from a second import, all we   *
 * do is add visibility to secimp_current_expectation_cell, 	*
 * since that will be the cell that corresponds to this		*
 * expectation.							*
 *								*
 * MODE is a safe pointer.  It only lives as long as this	*
 * function call.						*
 ****************************************************************/

void add_global_expectation(char *name, TYPE *t, ROLE *r,
			    LIST *who_sees, MODE_TYPE *mode,
			    char *package_name, int line)
{
  if(doing_second_import) {
    SET_LIST(secimp_current_expectation_cell->visible_in,
             str_list_union(secimp_current_expectation_cell->visible_in, 
			    who_sees));
  }
  else {
    install_secimp_exp(&global_expectations, name, t, r, who_sees,
		       mode, package_name, line);
  }
}


/****************************************************************
 *			SECOND_IMPORT_EXPECTATIONS		*
 ****************************************************************
 * Issue all expectations that were made while reading package  *
 * imported_package, but restrict attention to those indicated	*
 * by identifier list new_imported_ids.				*
 *								*
 * Helper second_import_expectations_in_cell does the work, by	*
 * by doing all expectations in the chain that is the val field *
 * of a hash2-table cell.					*
 ****************************************************************/

PRIVATE void 
second_import_expectations_in_cell(HASH2_CELL *h)
{
  GLOBAL_EXPECTATION_CELL *gec;
  STR_LIST* newimp = new_import_ids;

  for(gec = h->val.global_expectation; gec != NULL; gec = gec->next) {

    /*------------------------------------------------------------------*
     * Check whether this expectation is visible to imported_package	*
     * and whether the id that it expects is one of the ids that 	*
     * is of interest to the importing package.	 Note that		*
     * gec->visible_in is a positive list of package names, so we just  *
     * do a membership test.						*
     *------------------------------------------------------------------*/

    if(str_memq(secimp_imported_package, gec->visible_in) &&
       (newimp == (LIST *)1 || str_member(gec->id, newimp))) {

      /*----------------------*
       * Do this expectation. *
       *----------------------*/

      STR_LIST *this_id_visible_in;
      EXPECT_LIST *this_id_exps;
      bump_list(this_id_visible_in   = get_visible_in(gec->mode, gec->id));
      current_line_number            = gec->line;
      secimp_current_expectation_cell = gec;
      bump_list(this_id_exps = 
		expect_global_id_tm(gec->id, gec->type, gec->role,
				    this_id_visible_in,
				    0, 0, 0, TRUE, gec->mode,
				    gec->package_name, gec->line, NULL));
      secimp_current_expectation_cell = NULL;
      drop_list(this_id_exps);
      drop_list(this_id_visible_in);
    }
  } /* end for(gec = ...) */
}

/*-------------------------------------------------------------*/

PRIVATE void second_import_expectations(char *imported_package)
{
  secimp_imported_package = imported_package;
  scan_hash2(global_expectations, second_import_expectations_in_cell);
}


/****************************************************************
 *			ADD_GLOBAL_ANTICIPATION			*
 ****************************************************************
 * Place anticipation id:ty (with mode MODE, visible to 	*
 * packages in list who_sees) into global_anticipations.	*
 * This anticipation is being made by the package called	*
 * package_name, at line line.
 *								*
 * If this anticipation is from a second-import, just add	*
 * visibility to cell secimp_current_anticipation_cell.		*
 *								*
 * MODE is a safe pointer.  It only lives as long as this	*
 * function call.						*
 ****************************************************************/

void add_global_anticipation(char *id, TYPE *ty, LIST* who_sees,
			     MODE_TYPE *mode, char *package_name, 
			     int line)
{
  /*--------------------------------------------------------*
   * Handle anticipations from the standard package, which  *
   * everybody knows about anyway.                          *
   *--------------------------------------------------------*/

  if(who_sees == NULL) return;

  /*------------------------------------------------------------*
   * For a second import, just add visibility, don't add a new	*
   * anticipation.						*
   *------------------------------------------------------------*/

  if(doing_second_import) {
    SET_LIST(secimp_current_anticipation_cell->visible_in,
             str_list_union(secimp_current_anticipation_cell->visible_in, 
			    who_sees));
  }

  else {
    install_secimp_exp(&global_anticipations, id, ty, NULL, who_sees,
		       mode, package_name, line);
  }
}


/****************************************************************
 *			SECOND_IMPORT_ANTICIPATIONS		*
 ****************************************************************
 * Issue all anticipations that were made while reading package *
 * imported_package, but restrict attention to those indicated	*
 * by new_imported_ids.						*
 ****************************************************************/

PRIVATE void second_import_anticipations_in_cell(HASH2_CELLPTR h)
{
  GLOBAL_EXPECTATION_CELL *gec;
  STR_LIST* newimp = new_import_ids;

  for(gec = h->val.global_expectation; gec != NULL; gec = gec->next) {

    /*------------------------------------------------------------------*
     * Check whether this anticipation is visible to imported_package	*
     * and whether the id that it expects is one of the ids that 	*
     * is of interest to the importing package.	 Note that		*
     * gec->visible_in is a positive list of package names, so we just  *
     * do a membership test.						*
     *------------------------------------------------------------------*/

    if(str_memq(secimp_imported_package, gec->visible_in) &&
       (newimp == (LIST *)1 || str_member(gec->id, newimp))) {

      /*-----------------------*
       * Do this anticipation. *
       *-----------------------*/

      STR_LIST *this_id_visible_in;
      bump_list(this_id_visible_in = get_visible_in(gec->mode, gec->id));
      secimp_current_anticipation_cell = gec;
      note_expectation_tm(gec->id, gec->type, this_id_visible_in,
			  ANTICIPATE_ATT, gec->mode,
			  gec->package_name, gec->line);

      secimp_current_anticipation_cell = NULL;
      drop_list(this_id_visible_in);
    }
  } /* end for(gec = ...) */
}

/*-------------------------------------------------------------*/

PRIVATE void second_import_anticipations(char *imported_package)
{
  secimp_imported_package = imported_package;
  scan_hash2(global_anticipations, second_import_anticipations_in_cell);
}


/****************************************************************
 *			SECOND_IMPORT_ENTPARTS			*
 ****************************************************************
 * Do a second import of all parts in the entpart chains of	*
 * the global id table.  Package imported_package is being	*
 * imported.							*
 ****************************************************************/

PRIVATE void do_secimp_entparts(HASH2_CELLPTR h)
{
  ENTPART* p;
  char*           id  = h->key.str;
  GLOBAL_ID_CELL* gic = h->val.gic;

  for(p = gic->entparts; p != NULL; p = p->next) {

    /*----------------------------------------------------------*
     * If this part was placed by package standard, then there 	*
     * is no need to add any visibility.  If it was placed by  	*
     * another package, then we add the current visiblity if   	*
     * this part is visible to secimp_imported_package.		*
     *----------------------------------------------------------*/

    if(p->package_name != standard_package_name &&
       str_memq(secimp_imported_package, p->visible_in)) {

      STR_LIST* this_id_visible_in;
      bump_list(this_id_visible_in = get_visible_in(0, id));
      
      /*--------------------------------------------------------------*
       * Part p is visible to the package that is being imported.     *
       * So make it visible to all of the packages in list	      *
       * this_id_visible_in as well.  If doing so causes this part    *
       * to become visible to the main package, and the main package  *
       * is in its implementation part, then mark this part as being  *
       * in the body, for hidden import testing.		      *
       *--------------------------------------------------------------*/

      if(outer_context == BODY_CX && 
	 !str_memq(main_package_name, p->visible_in) &&
	 str_memq(main_package_name, this_id_visible_in)) {

	p->in_body = 1;
      }
      SET_LIST(p->visible_in, 
	       str_list_union(p->visible_in, this_id_visible_in));

      drop_list(this_id_visible_in);
	
    } /* end if(p->package_name != standard...) */

  } /* end for(p = ...) */
}

/*------------------------------------------------------------*/

PRIVATE void
second_import_entparts(char *imported_package)
{
  secimp_imported_package = imported_package;
  scan_hash2(global_id_table, do_secimp_entparts);
}


/****************************************************************
 *			SECOND_IMPORT_EXPANSIONS		*
 ****************************************************************
 * second_import_expansions does a second import of all		*
 * parts information in the expand_parts of the global id 	*
 * table.  Package imported_package is being imported.		*
 *								*
 * Note that the order of rules in an EXPAND_PART chain matters.*
 * We must be sure to add new rules to the front of the chain,	*
 * unless they are marked underride, in which case we add them  *
 * to the rear of the chain.					*
 ****************************************************************/

PRIVATE void 
install_expand_part(EXPAND_PART *p, EXPAND_PART **front, EXPAND_PART **rear)
{
  p->next = NULL;
  if(*front == NULL) {
    *front = *rear = p;
  }
  else {
    (*rear)->next = p;
  }
}

/*--------------------------------------------------------------*
 * do_secimp_rules(id, rules) updates chain *rules so that rules*
 * that are visible to secimp_imported_package become visible	*
 * to the current packages.  It also moves rules to the front	*
 * when that is necessary.					*
 *--------------------------------------------------------------*/

PRIVATE void do_secimp_rules(char *id, EXPAND_PART **rules)
{
  EXPAND_PART *rule, *p;
  Boolean can_add_visibility, saw_a_visible_rule;
  STR_LIST *this_id_visible_in;

  if(*rules == NULL) return;

  /*--------------------------------------------------------------------*
   * Sometimes, the objective can be achieved by adding visibility 	*
   * to existing cells.  However, doing that to some cells and		*
   * not others will tend to change the order of the definitions,	*
   * and that will not do.  So we check here whether all 		*
   * definitions can be achieved by adding visibility.  If so		*
   * then we will add visibility afterwards.				*
   *--------------------------------------------------------------------*/

  can_add_visibility = TRUE;
  saw_a_visible_rule = FALSE;
  for(rule = *rules; rule != NULL && can_add_visibility; rule = rule->next) {

    /*----------------------------------------------------------*
     * If this part was placed by package standard, then there 	*
     * is no need to add any visibility.  Skip.  Also, if this  *
     * part is not visible to the secimp_imported_package,	*
     * then it is irrelevant, so skip it.			*
     *----------------------------------------------------------*/

    if(rule->package_name == standard_package_name ||
       !str_memq(secimp_imported_package, rule->visible_in)) continue;

    saw_a_visible_rule = TRUE;

    /*------------------------------------------------------------------*
     * First, consider the case where we are not dealing with		*
     * an underriding rule.  Adding visibility to this rule would	*
     * only cause problems if there is a prior rule that is not		*
     * visible to secimp_imported_package, and whose type overlaps	*
     * the type of this rule.  (Then, that prior rule is not 		*
     * participating in this update, and, under some circumstances, that*
     * prior rule might be taken where this one should be instead.)	*
     *									*
     * Next, consider the case where we are dealing with an underriding *
     * rule.  Adding visibility to this rule can only cause trouble if  *
     * there is a subsequent rule in the chain that is not visible to	*
     * secimp_imported_package (so it is not participating in this	*
     * update) and whose type has a nonempty intersection with this	*
     * rule's type.  (Then adding visibility to this rule might cause	*
     * this rule to be taken rather than the other one.  Since this	*
     * rule underrides, that is not right.				*
     *------------------------------------------------------------------*/

    p = (rule->mode & UNDERRIDES_MODE_MASK) == 0 ? *rules : rule->next;
    while(p != rule && p != NULL &&
	  (visible_part(secimp_imported_package, p)
	   || disjoint(p->ty, rule->ty))) {
      p = p->next;
    }
    if(p != rule && p != NULL) can_add_visibility = FALSE;

  } /* end for(rules = ...) */

  /*------------------------------------------------------------*
   * If no rules were visible to secimp_imported_package, then	*
   * there is nothing to do.  Otherwise, we need to modify 	*
   * the parts.  Find out where the new parts will be visible.	*
   *------------------------------------------------------------*/

  if(!saw_a_visible_rule) return;
  bump_list(this_id_visible_in = get_visible_in(0, id));

  /*---------------------------------------*
   * If we can solve the problem by adding *
   * visibility, then do so. 		   *
   *---------------------------------------*/

  if(can_add_visibility) {
    for(rule = *rules; rule != NULL; rule = rule->next) {
      if(visible_part(secimp_imported_package, rule)) {
        SET_LIST(rule->visible_in, 
		 str_list_union(rule->visible_in, this_id_visible_in));
      }
    }
  }

  /*----------------------------------------------------------------*
   * If we cannot add visibility, then install each rule into a	    *
   * one of two chains, one for normal rules and one for underriding*
   * rules.  At the end, add the normal chain to the front, and     *
   * the underriding chain to the rear of *rules.		    *
   *----------------------------------------------------------------*/

  else {
    EXPAND_PART* normal_front    = NULL;
    EXPAND_PART* normal_rear     = NULL;
    EXPAND_PART* underride_front = NULL;
    EXPAND_PART* underride_rear  = NULL;
    EXPAND_PART* last_rule       = NULL;
    for(rule = *rules; rule != NULL; rule = rule->next) {
      last_rule = rule;
      if(visible_part(secimp_imported_package, rule)) {
	p  = allocate_expand_part();
	*p = *rule;
	bump_type(p->ty);
	bump_expr(p->u.rule);
	bump_list(p->visible_in = this_id_visible_in);
	if(p->mode & UNDERRIDES_MODE) {
	  install_expand_part(p, &normal_front, &normal_rear);
	}
        else {
	  install_expand_part(p, &underride_front, &underride_rear);
	}
      }
    } /* end for(rule = ...) */

    if(normal_front != NULL) {
      normal_rear->next = *rules;
      *rules = normal_front;
    }
    if(underride_front != NULL) {
      last_rule->next = underride_front;
    }
  } /* else (cannot add visibility) */

  drop_list(this_id_visible_in);
}

/*--------------------------------------------------------------*
 * do_secimp_pat_expectations second-imports the pattern 	*
 * expectations in chain patexp, importing package		*
 * secimp_imported_package.  id is the name of the id that	*
 * this chain is for.						*
 *--------------------------------------------------------------*/

PRIVATE void do_secimp_pat_expectations(char *id, PATFUN_EXPECTATION *patexp)
{
  PATFUN_EXPECTATION* p;

  for(p = patexp; p != NULL; p = p->next) {

    /*----------------------------------------------------------*
     * If this expectation was placed by package standard, then *
     * is no need to add any visibility.  If it was placed by  	*
     * another package, then we add the current visiblity if   	*
     * this part is visible to secimp_imported_package.		*
     *----------------------------------------------------------*/

    if(p->package_name != standard_package_name &&
       str_memq(secimp_imported_package, p->visible_in)) {

      STR_LIST* this_id_visible_in;
      bump_list(this_id_visible_in = get_visible_in(0, id));
      SET_LIST(p->visible_in, 
	       str_list_union(p->visible_in, this_id_visible_in));
      drop_list(this_id_visible_in);

    } /* end if(p->package_name != standard...) */

  } /* end for(p = ...) */
}


/*------------------------------------------------------------*/

PRIVATE void do_secimp_expansions(HASH2_CELLPTR h)
{
  char*           id   = h->key.str;
  GLOBAL_ID_CELL* gic  = h->val.gic;
  EXPAND_INFO*    info = gic->expand_info;

  if(info != NULL) {
    do_secimp_pat_expectations(id, info->patfun_expectations);
    do_secimp_rules(id, &(info->patfun_rules));
    do_secimp_rules(id, &(info->expand_rules));
  }
}

/*------------------------------------------------------------*/

PRIVATE void
second_import_expansions(char *imported_package)
{
  secimp_imported_package = imported_package;
  scan_hash2(global_id_table, do_secimp_expansions);
}

/****************************************************************
 *			DO_SECIMP... FUNCTIONS			*
 ****************************************************************
 * These functions are used to perform second imports from	*
 * the file info tables.  They are passed to scan_hash1 and	*
 * scan_hash2 to scan the tables.				*
 ****************************************************************/

PRIVATE void do_secimp_import_dir(HASH2_CELLPTR h)
{
  def_import_dir(h->key.str, h->val.str);
}

/*-------------------------------------------------------------*/

PRIVATE void do_secimp_assume(HASH2_CELLPTR h)
{
  assume_tm(h->key.str, h->val.type, 1);
}

/*-------------------------------------------------------------*/

PRIVATE void do_secimp_role_assume(HASH2_CELLPTR h)
{
  assume_role_tm(h->key.str, h->val.role, 1);
}

/*-------------------------------------------------------------*/

PRIVATE void do_secimp_patfun_assume(HASH2_CELLPTR h)
{
  if(h->val.num) patfun_assume_tm(h->key.str, 1);
  else delete_patfun_assume_tm(h->key.str, 1);
}

/*-------------------------------------------------------------*/

PRIVATE void do_secimp_abbrev(HASH2_CELLPTR h)
{
  register CLASS_TABLE_CELL *ctc = h->val.ctc;
  abbrev_tm(h->key.str, MAKE_TOK(ctc->code), ctc->ty, ctc->role, 1);
}

/*-------------------------------------------------------------*/

PRIVATE void do_secimp_op_dcl(HASH2_CELLPTR h)
{
  MODE_TYPE* mode = NULL;
  Boolean open = FALSE;
  int tok = toint(h->val.num);
  if(tok < 0) {
    open = TRUE;
    tok = -tok;
  }
  if(tok > 1000) {
    tok -= 1000;
    mode = simple_mode(OVERRIDES_MODE);         /* ref cnt is 1. */
  }
  operator_tm(h->key.str, tok, open, mode);
  drop_mode(mode);
}

/*-------------------------------------------------------------*/

PRIVATE void do_secimp_unary_op_dcl(HASH1_CELLPTR h)
{
  operator_tm(h->key.str, UNARY_OP, FALSE, 0);
}

/*-------------------------------------------------------------*/

PRIVATE void do_secimp_default(HASH2_CELLPTR h)
{
  MODE_TYPE *mode = NULL;

  if(h->val.type->TOFFSET) {
    mode = simple_mode(DANGEROUS_MODE);
  }
  default_tm(ctcs[toint(h->key.num) - 1], h->val.type, mode);
  drop_mode(mode);
}

/****************************************************************
 *			SECOND_IMPORT_TM			*
 ****************************************************************
 * Simulate an import of package imported_package.		*
 * Global list new_import_ids tells which ids			*
 * are being imported, or is (LIST *) 1 to import all ids.	*
 *								*
 * MODE is the mode of the import declaration.  It is a safe    *
 * pointer: it does not live longer than this function call.	*
 ****************************************************************/

void second_import_tm(char *imported_package, char *file_name,
		      MODE_TYPE *mode_unused)
{
# ifdef DEBUG
    if(trace_defs || trace_importation) {
      trace_t(384, imported_package);
    }
# endif

  /*--------------------------------------------------------------------*
   * Put this package into the imported_package list of all packages    *
   * that are listening.						*
   *--------------------------------------------------------------------*/

  note_imported(imported_package);

  /*----------------------------------------------------------------------*
   * Indicate that we are doing a second import.  Expectations are	  *
   * recorded during the first import, but not in subsequent		  *
   * imports.								  *
   *----------------------------------------------------------------------*/

  doing_second_import++;

  /*----------------------------------------------------------------------*
   * Push a new frame on the import stack to simulate begin of an import. *
   *----------------------------------------------------------------------*/

  push_file_info_frame(file_name, NULL);
  file_info_st->package_name =
    file_info_st->private_packages->head.str =
    file_info_st->public_packages->head.str = imported_package;
  main_context = IMPORT_CX;

  /*------------------------------------------------------*
   * Simulate the import of expectations, definitions and *
   * pattern/expand rules. 				  *
   *------------------------------------------------------*/

  second_import_expectations(imported_package);
  second_import_anticipations(imported_package);
  second_import_entparts(imported_package);
  second_import_expansions(imported_package);

  /*------------------------------------------------------------*
   * Find the archived import frame, and simulate its exported	*
   * declarations. 						*
   *------------------------------------------------------------*/

  {IMPORT_STACK* p = import_archive_list;
   while(p != NULL && p->package_name != imported_package) p = p->next;
   if(p == NULL) {
     die(139, file_name);
   }

   /*--------------------------------*
    * Simulate assume, etc. imports. *
    *--------------------------------*/

   scan_hash2(p->import_dir_table, do_secimp_import_dir);
   scan_hash2(p->global_assume_table, do_secimp_assume);
   scan_hash2(p->global_assume_role_table, do_secimp_role_assume);
   scan_hash2(p->global_patfun_assume_table, do_secimp_patfun_assume);
   scan_hash2(p->global_abbrev_id_table, do_secimp_abbrev);
   scan_hash2(p->op_table, do_secimp_op_dcl);
   scan_hash1(p->unary_op_table, do_secimp_unary_op_dcl);
   scan_hash2(p->default_table, do_secimp_default);
   if(p->global_nat_const_assumption != NULL) {
     do_const_assume_tm(0, NULL, p->global_nat_const_assumption, 1);
   }
   if(p->global_real_const_assumption != NULL) {
     do_const_assume_tm(1, NULL, p->global_real_const_assumption, 1);
   }
  }

  /*----------------------------------------------*
   * Restore file_info_st and exit second import. *
   *----------------------------------------------*/

  {IMPORT_STACK* fs = file_info_st;
   file_info_st = file_info_st->next;
   free_import_stack(fs);
  }

  doing_second_import--;

# ifdef DEBUG
    if(trace_defs || trace_importation) {
      trace_t(385, imported_package);
    }
# endif
}




