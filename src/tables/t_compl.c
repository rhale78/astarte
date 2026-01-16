/************************************************************************
 * File:    clstbl/t_compl.c
 * Purpose: Check for completeness of expectations
 * Author:  Karl Abrahamson
 ************************************************************************/

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
 * This file is used by the compiler to check whether all expectations  *
 * have been met.  It uses functions provided by file complete.c to     *
 * compare the types of the existing definitions to the types that	*
 * were expected.  This file goes through the global id table, so it	*
 * makes direct use of the data stored in file tables/globtbl.c.	*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../parser/tokens.h"
#include "../classes/classes.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../unify/unify.h"
#include "../error/error.h"
#include "../standard/stdtypes.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/******************************************************************
 *			printed_missing_expect_header		  *
 ******************************************************************
 * printed_missing_expect_header is true after the compiler has   *
 * printed "Missing expectations".  It is looked at in compiler.c *
 * to decide whether to indicate, at the end of the package, that *
 * there were missing expectations.				  *
 ******************************************************************/

Boolean printed_missing_expect_header = FALSE;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/******************************************************************
 *			checking_anticipate_table		  *
 ******************************************************************
 * check_expect_cell is used for both expect_id_table and	  *
 * anticipate_id_table.  But those tables have different 	  *
 * kinds of values associated with them.  See globtbl.c.  	  *
 * If checking_anticipate_table is TRUE, then check_expect_cell   *
 * treats its parameter as a cell in anticipate_id_table.  	  *
 * Otherwise, it treats its parameter as a cell in		  *
 * expect_id_table.						  *
 ******************************************************************/

PRIVATE Boolean checking_anticipate_table;


/******************************************************************
 *			reported_unsatisfied			  *
 ******************************************************************
 * reported_unsatisfied is a list telling what has already been   *
 * reported concerning unsatisfied expectations.  The list	  *
 * consists of alternating names and types, with a name x followed*
 * by the type t such that x:t has been reported as not being     *
 * defined.							  *
 * 								  *
 * This list is used to avoid redundant reports.		  *
 ******************************************************************/

PRIVATE LIST* reported_unsatisfied = NIL;


/****************************************************************
 *			ALREADY_REPORTED_UNSAT			*
 ****************************************************************
 * Return true if id:t has already been reported as not defined.*
 ****************************************************************/

PRIVATE Boolean already_reported_unsat(char *id, TYPE *t)
{
  LIST *p;

  for(p = reported_unsatisfied; p != NIL; p = p->tail->tail) {
    if(p->head.str == id && !disjoint(p->tail->head.type, t)) {
      return TRUE;
    }
  }
  return FALSE;
}


/****************************************************************
 *			CHECK_EXPECT_CELL			*
 ****************************************************************
 * Check that identifier id has definitions for all types in    *
 * list l, where (id, l) is the pair in cell c.			*
 ****************************************************************/

PRIVATE void check_expect_cell(HASH2_CELLPTR c)
{
  char *id;
  LONG hash_val;
  TYPE_LIST *expect_list, *a;
  TYPE_LIST *main_source_list;
  LIST *main_missing_list, *import_missing_list;
  GLOBAL_ID_CELL *gic;

  /*-----------------------------*
   * Put id in the string table. *
   *-----------------------------*/

  hash_val = strhash(c->key.str);
  id       = id_tb10(c->key.str, hash_val);

# ifdef DEBUG
    if(trace_complete) {
      trace_t(111, id, checking_anticipate_table);
    }
# endif

# ifdef NEVER
    /*------------------------------------------------------------------*
     * At one point it seemed like a good idea to skip completeness	*
     * tests for assumed pattern functions.  That is not done now.	*
     *------------------------------------------------------------------*/

    if(is_assumed_patfun_tm(id)) return;
# endif

  /*------------------------------------------------------------*
   * Get the expectation list, containing types to check 	*
   * for completness. 						*
   *------------------------------------------------------------*/

  expect_list = c->val.list;

# ifdef DEBUG
    if(trace_complete > 1) {
      trace_t(113);
      print_type_list_separate_lines(expect_list, " ");
      tracenl();
    }
# endif

  /*------------------------------------------------------*
   * Get the gic (table entry) for this id.  If there is  *
   * none, give up. 					  *
   *------------------------------------------------------*/

  gic = get_gic_hash_tm(id, hash_val);
  if(gic == NULL) {
#   ifdef DEBUG
      if(trace) trace_t(115, id);
#   endif
    return;
  }

  /*--------------------------------------------------------*
   * Get the list of types for which this id was declared   *
   * missing. Get both the list that includes 		    *
   * missing_tables[IMPORTED_MISSING_TABLE] and the list    *
   * that does not include it. 				    *
   *							    *
   * Lists main_missing_list and imported_missing_list can  *
   * have two kinds of node.  Those with tag TYPE_L are     *
   * types, while those of type NAME_TYPE_L are pointers to *
   * NAME_TYPE nodes.					    *
   *--------------------------------------------------------*/

  {INT_LIST *select_main, *select_imports;
   bump_list(select_main = 
     int_cons(STRONG_MISSING_TABLE, int_cons(BASIC_MISSING_TABLE, NIL)));
   bump_list(select_imports =
     int_cons(IMPORTED_MISSING_TABLE, NIL));
   bump_list(main_missing_list = 
     types_declared_missing_tm(id, select_main, hash_val));
   bump_list(import_missing_list =
     types_declared_missing_tm(id, select_imports, hash_val));
   drop_list(select_main);
   drop_list(select_imports);
  }

# ifdef DEBUG
     if(trace_complete) {
       trace_t(116);
       print_type_list_separate_lines(main_missing_list, " ");
       tracenl();
       trace_t(108);
       print_type_list_separate_lines(import_missing_list, " ");
       tracenl();
     }
# endif

  /*--------------------------------------------------------------------*
   * Get the list of part types that contribute to expectations for 	*
   * this id.  Each irregular part with type A -> B has A -> `x put     *
   * into this list, since it is presumed to cover its entire codomain. *
   *									*
   * Note: If this part was placed by an expectation in the export	*
   * part of the main package, then it should not be considered to	*
   * be a contribution, so it is skipped.  (No parts are created for    *
   * expectations in the implementation part of the main package.)	*
   *--------------------------------------------------------------------*/

  {ENTPART *p;
   main_source_list = NIL;

#  ifdef DEBUG
     if(trace_complete) {
       trace_t(117, id, main_package_name, main_package_name);
       print_entpart_chain(gic->entparts, 1);
     }
#   endif

   for(p = gic->entparts; p != NULL; p = p->next) {
     TYPE *tt;

     if(p->from_expect && p->package_name == main_package_name) continue;

     bump_type(tt = get_part_cover_type(p));
     SET_LIST(main_source_list, type_cons(copy_type(tt,0), main_source_list));
     drop_type(tt);
   }
  }

  /*--------------------------------------------------*
   * Put the main missing list onto the source lists. *
   *--------------------------------------------------*/

  {TYPE_LIST *q;

   for(q = main_missing_list; q != NULL; q = q->tail) {
     TYPE *tt;
     tt = (LKIND(q) == TYPE_L) ? q->head.type : q->head.name_type->type;
     SET_LIST(main_source_list, 
	      type_cons(copy_type(tt,0), main_source_list));
   }
  }

# ifdef DEBUG
    if(trace_complete > 1) {
      trace_t(118);
      print_type_list_separate_lines(main_source_list, " ");
      tracenl();
    }
# endif

  /*-------------------------------------------------------------*
   * Delete types that are subsumed by other types in each list. *
   * This speeds up the tests in some cases.			 *
   *-------------------------------------------------------------*/

  SET_LIST(main_source_list, 
	   reduce_type_list_with(NIL, main_source_list));

# ifdef DEBUG
    if(trace_complete) {
      trace_t(119);
      print_type_list_separate_lines(main_source_list, " ");
      tracenl();
    }
# endif

  /*-----------------------------------------------------------*
   * Check each expectation against the parts that contribute. *
   *-----------------------------------------------------------*/

  for(a = expect_list; a != NIL; a = a->tail) {

    TYPE_LIST *this_source_list;
    TYPE *expect_type;
    char *expect_package;

    /*------------------------------------------------------------------*
     * The tag of list cell a is one of the following.			*
     *									*
     *   TYPE_L		a holds a type.  This occurs when we 		*
     *			are scanning expect_id_table.			*
     *									*
     *   EXPECT_TABLE_L a holds an EXPECT_TABLE cell.  This occurs when	*
     *			scanning anticipate_id_table.			*
     *------------------------------------------------------------------*/

    if(checking_anticipate_table) {
      expect_type    = a->head.expect_table->type;
      expect_package = a->head.expect_table->package_name;
    }
    else {
      expect_type    = a->head.type;
      expect_package = NULL;
    }
    bump_type(expect_type);
    replace_null_vars(&expect_type);

#   ifdef DEBUG
      if(trace_complete) {
	trace_t(120, nonnull(expect_package), id);
        trace_ty(expect_type);
	tracenl();
      }
#   endif

    /*----------------------------------------------------------*
     * Augment the source list with the imported missing types  *
     * that were declared in other packages than the package    *
     * that made this expectation.				*
     *								*
     * This is only done wheh checking anticipations, since	*
     * imported missing types are assumed to apply only to them.*
     *----------------------------------------------------------*/

    bump_list(this_source_list = main_source_list);
    if(checking_anticipate_table) {
      LIST *q;
      Boolean added_something = FALSE;

      for(q = import_missing_list; q != NULL; q = q->tail) {
        if(LKIND(q) == NAME_TYPE_L && 
	   q->head.name_type->name != expect_package) {
	  SET_LIST(this_source_list, 
		   type_cons(copy_type(q->head.name_type->type, 0), 
			     this_source_list));
	  added_something = TRUE;
	}
      }      
      if(added_something) {
	SET_LIST(this_source_list, 
		 reduce_type_list_with(NIL, this_source_list));
      }
    }

#   ifdef DEBUG
      if(trace_complete) {
        trace_t(112);
        print_type_list_separate_lines(this_source_list, " ");
        tracenl();
      }
#   endif

    /*--------------------------------------------------*
     * Compute the missing types for this expectation.	*
     * If there were any missing types, report one.	*
     *							*
     * Note: If we are checking the anticipate table	*
     * and the result from missing_type contains a 	*
     * variable that ranges over an empty genus or	*
     * community, then we should say that there are	*
     * no missing types.				*
     *--------------------------------------------------*/

    {TYPE *mt;
     bump_type(mt = missing_type(this_source_list, expect_type));
     if(mt != NULL && checking_anticipate_table) {
       SET_TYPE(mt, member_of_class(mt));
     }

     if(mt != NULL) {
       if(!missing_type_use_fictitious_types) {
	 SET_TYPE(mt, bind_for_no_fictitious(mt));
       }
       if(!already_reported_unsat(id, mt)) {
	 if(!printed_missing_expect_header) {
	   printed_missing_expect_header = TRUE;
	   err_print(UNSAT_EXPECTATIONS_ERR);
	 }
	 err_print(SPACE2_STR_ERR, display_name(id));
	 err_print_ty(expect_type);
	 err_nl();
	 err_print(EG_MISSING_ERR);
	 err_print_ty(mt); 
	 err_print_str(")");
	 err_nl();
	 err_nl();
	 SET_LIST(reported_unsatisfied, 
		  str_cons(id, type_cons(mt, reported_unsatisfied)));
       }
       drop_type(mt);
     } /* end if(mt != NULL) */
    }

    drop_type(expect_type);
    drop_list(this_source_list);

  } /* end for(a = expect_list...) */

  drop_list(main_source_list);
  drop_list(main_missing_list);
  drop_list(import_missing_list);
}


/****************************************************************
 *			CHECK_EXPECTS				*
 ****************************************************************
 * Check that all expectations and anticipations are satisfied.	*
 ****************************************************************/

void check_expects(void)
{
# ifdef DEBUG
#   ifdef NEVER
      trace_unify = FALSE;
#   endif

    if(trace_complete > 1) trace_missing_type += trace_complete - 1;
# endif

  printed_missing_expect_header = FALSE;

  /*----------------------*
   * Check anticipations. *
   *----------------------*/

# ifdef DEBUG
    if(trace_complete) trace_t(538);
# endif

  missing_type_use_fictitious_types = FALSE;
  checking_anticipate_table = TRUE;
  scan_hash2(anticipate_id_table, check_expect_cell);

  /*----------------------*
   * Check expectations.  *
   *----------------------*/

# ifdef DEBUG
    if(trace_complete) trace_t(539);
# endif

  missing_type_use_fictitious_types = TRUE;
  checking_anticipate_table = FALSE;
  scan_hash2(expect_id_table, check_expect_cell);

# ifdef DEBUG
    if(trace_complete > 1) trace_missing_type -= trace_complete - 1;
# endif
}


/****************************************************************
 *			CHECK_TF_EXPECTS	       		*
 ****************************************************************
 * Check that type and family expectations are satisfied.	*
 ****************************************************************/

void check_tf_expects()
{
  int i;
  CLASS_TABLE_CELL *c;

  for(i = 0; i < next_class_num; i++) {
    c = ctcs[i];
    if(c->expected == 1) {
      err_print(MISSING_EXPECTED_TF_ERR, quick_display_name(c->name));
      error_occurred = TRUE;
    }
  }
}


