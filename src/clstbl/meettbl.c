/****************************************************************
 * File:    clstbl/meettbl.c
 * Purpose: Table manager for meets and joins of genera 
 *	    and communties.
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
 * The genus/community hierarchy is a lattice, and it is necessary to   *
 * calculate meets (intersections) and joins (almost unions) in this	*
 * lattice.								*
 *									*
 * This file manages the tables of genus and community meets and joins. *
 * It also has functions that check the consistency of the lattice      *
 * (to ensure that it is indeed a lattice).				*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/meettbl.h"
#include "../error/error.h"
#include "../generate/generate.h"
#include "../evaluate/instruc.h"
#include "../parser/parser.h"
#include "../parser/tokens.h"
#include "../unify/unify.h"
#include "../standard/stdtypes.h"
#ifdef MACHINE
# include "../intrprtr/intrprtr.h"
# include "../show/gprint.h"
#endif
#ifdef TRANSLATOR
# include "../dcls/dcls.h"
#endif
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 * 			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			intersection_table			*
 ****************************************************************
 * intersection_table tells meets of genera and communities.  	*
 * The meets are necessarily intersections when the genera are	*
 * viewed as sets of species.					*
 *								*
 * The key is an LPAIR representing the pair			*
 * (ctc_num(A), ctc_num(B)), where A and B are the two things	*
 * whose intersction is being given.  The ctc_num values in the *
 * LPAIR are always stored in ascending order, so we don't need *
 * to worry about (A,B) or (B,A) being stored.			*
 *								*
 * The value in the table is an integer, indicating the		*
 * ctc_num of the intersection.					*
 *								*
 * Only nonempty intersections are stored in the table.  Also,  *
 * only intersections between incomparable values in the	*
 * lattice are stored.  					*
 ****************************************************************/

PRIVATE HASH2_TABLE* intersection_table = NULL;

/****************************************************************
 *			join_table				*
 ****************************************************************
 * join_table tells joins of genera and communities.		*
 *								*
 * The key is an LPAIR representing the pair 			*
 * (ctc_num(A), ctc_num(B)), where A and B are the two things 	*
 * whose join is being given.  The ctc_num values in the LPAIR	*
 * are always stored in ascending order, so we don't need to	*
 * worry about (A,B) or (B,A) being stored.			*
 *								*
 * The value in the table is an integer, indicating the		*
 * ctc_num of the join of A and B.				*
 *								*
 * Only joins that are not ANY are stored.  Also, only joins	*
 * of incomparable items are stored.				*
 *								*
 * Note: The join of two genera A and B is not necessarily	*
 * a genus whose extension is the union of the extensions of	*
 * A and B.  The join can contain species that are in neither	*
 * A nor B.  This is different from meets, where the meet is	*
 * exactly the intersection of the extensions.			*
 ****************************************************************/

PRIVATE HASH2_TABLE* join_table = NULL;

/****************************************************************
 *			ahead_meets				*
 ****************************************************************
 * ahead_meets is a chain that indicate what meet declarations  *
 * are waiting to be done.  When declaration			*
 *								*
 *	Meet{ahead} A & B = C(L1,L2)				*
 *								*
 * is issued, where A and B do not both yet exist, then an	*
 * entry is added to the front of this chain holding the names  *
 * A, B, C, L1 and L2.						*
 *								*
 * After an entry in this chain has been processed (because A	*
 * and B have both been defined), the A field is set 		*
 * to NULL, to prevent reprocessing.				*
 ****************************************************************/

PRIVATE AHEAD_MEET_CHAIN* ahead_meets = NULL;


/*===============================================================
 *			HANDLING AHEAD MEETS
 *==============================================================*/

/****************************************************************
 *			    ADD_AHEAD_MEET_TM			*
 ****************************************************************
 * Do declaration Meet{ahead} a & b = c(l1,l2), where l1 and l2 *
 * are NULL if not present.  c is "PAIR@" for a pair.		*
 * If l1 is (char *)(-1), then no label checking is done.	*
 ****************************************************************/

void add_ahead_meet_tm(char *a, char *b, char *c, char *l1, char *l2)
{
  AHEAD_MEET_CHAIN* node = 
    (AHEAD_MEET_CHAIN *) alloc_small(sizeof(AHEAD_MEET_CHAIN));
  node->a    = name_tail(a);
  node->b    = name_tail(b);
  node->c    = name_tail(c);
  node->l1   = (l1 == (char *)(-1)) ? l1 : name_tail(l1);
  node->l2   = name_tail(l2);
  node->next = ahead_meets;
  ahead_meets = node;

# ifdef TRANSLATOR
    if(gen_code) {
      generate_meet_dcl_g(NULL, a, NULL, b, NULL, c, AHEAD_MEET_DCL_I);
    }
# endif
}


/****************************************************************
 *			    PROCESS_AHEAD_MEETS_TM		*
 ****************************************************************
 * Perform all ahead meets called for by chain ahead_meets.	*
 * Cancel those that are done, so that they will not be done	*
 * again.							*
 ****************************************************************/

void process_ahead_meets_tm(void)
{
  AHEAD_MEET_CHAIN *p;

  for(p = ahead_meets; p != NULL; p = p->next) {

    /*--------------------------------------------------------*
     * p->a == NULL indicates that node p has been cancelled. *
     *--------------------------------------------------------*/

    if(p->a != NULL) {

      /*------------------------------------------------------*
       * If both a and b exist, then do the meet declaration. *
       *------------------------------------------------------*/

      CLASS_TABLE_CELL* a_ctc = get_ctc_tm(p->a);
      CLASS_TABLE_CELL* b_ctc = get_ctc_tm(p->b);
      if(a_ctc != NULL && b_ctc != NULL) {

#       ifdef DEBUG
          if(trace_classtbl) {
	    trace_s(106, p->a, p->b, nonnull(p->c), 
		    nonnull(p->l1), nonnull(p->l2));
	  }
#       endif

        /*-------------------------------------*
	 * Add this intersection, if possible. *
         *-------------------------------------*/

	add_intersection_from_strings_tm(a_ctc, b_ctc, p->c, p->l1, p->l2);

	/*-------------------------*
         * Cancel this ahead meet. *
         *-------------------------*/

	p->a = NULL;
      }
    }
  }
}


/*===============================================================
 *			GETTING LINK LABELS
 *==============================================================*/

/****************************************************************
 *			  GET_LINK_LABEL_TM			*
 ****************************************************************
 * Return the link label or labels from hi to lo, where lo the  *
 * index in ctcs of a family or community or pair node.		*
 * The link label is returned as a pair of ctc-indices.  For a  *
 * family member, only the first member of the pair is relevant.*
 ****************************************************************/

LPAIR get_link_label_tm(int hi, int lo)
{
  if(ctcs[hi]->code == COMM_ID_CODE) return NOCHECK_LP;
  if(ctcs[lo]->opaque) return NULL_LP;
  else if(lo == Pair_ctc->num) return make_lpair_t(hi,hi);
  else return make_lpair_t(hi,0); 
}


/*===============================================================
 *	ACCESSING AND MODIFYING THE INTERSECTION TABLE
 *==============================================================*/

/****************************************************************
 *			    CHECK_INTERSECTION_TM		*
 ****************************************************************
 * Check whether declaration meet A & B = C(labels) makes any	*
 * sense.  If the declaration does not make sense, print an	*
 * error message.						*
 *								*
 * If labels.label1 < 0, then no consistency check is done on   *
 * labels.							*
 *								*
 * Return TRUE if the intersection is sensible, FALSE if not.	*
 ****************************************************************/

#ifdef TRANSLATOR

PRIVATE Boolean 
check_intersection_tm(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b,
		      CLASS_TABLE_CELL *c, LPAIR labels)
{
  char* a_name = a->name;
  char* b_name = b->name;
  int   a_code = a->code;
  int   b_code = b->code;
  int   c_code = c->code;

  /*------------------------------------------------------------*
   * Both a and b must be genera or communities.		*
   *------------------------------------------------------------*/

  if(a_code != GENUS_ID_CODE && a_code != COMM_ID_CODE) {
    semantic_error1(NO_CG_ERR, display_name(a->name), 0);
    return FALSE;
  }

  if(b_code != GENUS_ID_CODE && b_code != COMM_ID_CODE) {
    semantic_error1(NO_CG_ERR, display_name(b->name), 0);
    return FALSE;
  }

  /*---------------------------------*
   * c must be beneath both a and b. *
   *---------------------------------*/

  if(c != a && !ancestor_tm(a,c)) {
    semantic_error2(BAD_MEET_ERR2, display_name(c->name), 
		    display_name(a->name), 0);
    return FALSE;
  }

  if(c != b && !ancestor_tm(b,c)) {
    semantic_error2(BAD_MEET_ERR2, display_name(c->name), 
		    display_name(b->name), 0);
    return FALSE;
  }

  /*------------------------------------------------------------*
   * If either of a or b is a community, the intersection must  *
   * be a community or family.  Moreover, the intersection must *
   * be of the same kind (transparent or opaque) as the 	*
   * community among a or b.					*
   *------------------------------------------------------------*/

  if(a_code == COMM_ID_CODE || b_code == COMM_ID_CODE) {
    if(c_code != FAM_ID_CODE && c_code != COMM_ID_CODE) {
      semantic_error(BAD_MEET_ERR3, 0);
      return FALSE;
    }
    if((a_code == COMM_ID_CODE && a->opaque != c->opaque) ||
       (b_code == COMM_ID_CODE && b->opaque != c->opaque)) {
      semantic_error(BAD_MEET_ERR4, 0);
      return FALSE;
    }
  }

  /*--------------------------------------------------------------*
   * Check for violation of closure. If we have already concluded *
   * that this intersection is empty, then complain about that.   *
   * If we already have an intersection that is different from	  *
   * the one being defined here, then complain about that.	  *
   *--------------------------------------------------------------*/

  if(a->closed && b->closed) {
    int val;

#   ifdef DEBUG
      if(trace) trace_s(87, a_name, b_name);
#   endif

    val = get_intersection_tm(a,b);
    if(val == 0) {
      semantic_error2(INTERSECT_CLOSURE_ERR, display_name(a->name), 
		      display_name(b->name), 0);    
      return FALSE;
    }

    if(val != ctc_num(c)) {
      LPAIR lp = get_link_label_tm(val, val);
      dup_intersect_err(a->name, b->name, c, labels, val, lp);
    }
    return FALSE;
  }

  /*--------------------------------------------------------------------*
   * Check for correct link labels, in the case where the intersection	*
   * is a family or pair or community. If a and b are both genera, 	*
   * then c must be opaque, since otherwise the intersection is really	*
   * empty.								*
   *--------------------------------------------------------------------*/

  if((a_code == GENUS_ID_CODE || b_code == GENUS_ID_CODE) &&
     (c_code == FAM_ID_CODE || c_code == COMM_ID_CODE || 
      c_code == PAIR_CODE)) {
    LPAIR lp;
    int gnum;

    if(a_code == GENUS_ID_CODE && b_code == GENUS_ID_CODE && !c->opaque) {
      semantic_error2(FAM_INTERSECT_ERR, display_name(a_name), 
		      display_name(b_name), 0);
      return FALSE;
    }

    if(labels.label1 >= 0) {
      gnum = ctc_num((a_code == GENUS_ID_CODE) ? a : b);
      lp   = get_link_label_tm(gnum, ctc_num(c));
      if(labels.label1 != lp.label1 || labels.label2 != lp.label2) {
        dup_intersect_err(a_name, b_name, c, labels, ctc_num(c), lp);
        return FALSE;
      }
    }
  }

  return TRUE;
}
#endif


/****************************************************************
 *			    ADD_INTERSECTION_TM			*
 ****************************************************************
 * Declare that a & b = c(labels).  labels is only used for     *
 * error checking, and is only used by the translator.		*
 ****************************************************************/

void add_intersection_tm(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b,
			 CLASS_TABLE_CELL *c, LPAIR labels)
{
# ifdef DEBUG
    if(trace_classtbl) {
      trace_s(86, a->name, b->name);
      if(c == NULL) fprintf(TRACE_FILE,"NULL ");
      else fprintf(TRACE_FILE, "%s ", c->name);
      print_lpair(labels);
    }
# endif

  /*----------------------------------------------*
   * Check whether this intersection makes sense. *
   *----------------------------------------------*/

# ifdef TRANSLATOR
    if(!check_intersection_tm(a, b, c, labels)) return;
# endif

  /*---------------------------------*
   **** Install the intersection. ****
   *---------------------------------*/

  /*----------------------------------------------------*
   * If c is the same as a, then just put a beneath b.  *
   * If c is the same as b, then just put b beneath a.  *
   *----------------------------------------------------*/

  if     (c == a) {
    copy_ancestors_tm(a, b, labels);
#   ifdef TRANSLATOR
      if(gen_code) {
	generate_relate_dcl_g(b, a);
      }
#   endif
  }
  else if(c == b) {
    copy_ancestors_tm(b, a, labels);
#   ifdef TRANSLATOR
      if(gen_code) {
        generate_relate_dcl_g(a, b);
      }
#   endif
  }

  /*-----------------------------------------------------*
   * If c is different from a and b, then we need a true *
   * intersect table entry. 				 *
   *-----------------------------------------------------*/

  else {
    HASH_KEY u;
    HASH2_CELLPTR h;

    u.lpair = make_ordered_lpair_t(ctc_num(a), ctc_num(b));
    h       = insert_loc_hash2(&intersection_table, u, lphash(u.lpair), eq);
    h->key.lpair = u.lpair;
    h->val.num   = ctc_num(c);

#   ifdef TRANSLATOR
      if(gen_code) {
        generate_meet_dcl_g(a, NULL, b, NULL, c, NULL, MEET_DCL_I);
      }
#   endif
  }

  /*--------------------------------------*
   * Report what was done in the listing. *
   *--------------------------------------*/

# ifdef TRANSLATOR
    report_dcl_aux_p(a->name, MEET_E, 0, NULL, NULL, b->name, c, labels);
# endif
}
    

/****************************************************************
 *		      GET_MEET_DCL_RHS				*
 ****************************************************************
 * A meet declaration has right-hand side c(l1,l2).  Set *c_ctc *
 * to the class table entry for c, and *l1_num, *l2_num to the  *
 * class table numbers of l1 and l2.  Note that l1 and l2 might *
 * be NULL to indicate that they are not present.		*
 *								*
 * If there is an error because one of c, l1 and l2 do not	*
 * exist, complain and return FALSE.  The complaint is for the  *
 * meet of a and b. 						*
 *								*
 * If all is well, return TRUE.					*
 ****************************************************************/

PRIVATE Boolean 
get_meet_dcl_rhs(char *a, char *b, char *c, char *l1, char *l2,
		 CLASS_TABLE_CELL **c_ctc, 
		 LPAIR *labels)
{
  CLASS_TABLE_CELL *l1_ctc, *l2_ctc;
  Boolean ok = TRUE;

  *labels = NULL_LP;
  *c_ctc  = get_ctc_tm(c);

  if(l1 == (char *)(-1)) {
    *labels = NOCHECK_LP;
  }
  else {
    if(l1 != NULL) {
      l1_ctc = get_ctc_tm(l1);
      if(l1_ctc == NULL) ok = FALSE;
      else labels->label1 = l1_ctc->num;
    }
    if(l2 != NULL) {
      l2_ctc = get_ctc_tm(l2);
      if(l2_ctc == NULL) ok = FALSE;
      else labels->label2 = l2_ctc->num;
    }
  }

  if(!ok || *c_ctc == NULL) {
#   ifdef TRANSLATOR
      semantic_error3(BAD_AHEAD_MEET_ERR, a, b, c, 0);
#   else
      die(180, a, b, c);
#   endif
    return FALSE;
  }
  else {
    return TRUE;
  }
}


/****************************************************************
 *		    ADD_INTERSECTION_FROM_STRINGS_TM		*
 ****************************************************************
 * Perform declaration Meet A & B = C(labels) where A and B are *
 * given by the class table entries a_ctc and b_ctc, C is given *
 * by its name c, and the labels are given by names l1 and l2   *
 * (NULL if not present).	       				*
 ****************************************************************/

void add_intersection_from_strings_tm(CLASS_TABLE_CELL *a_ctc,
				      CLASS_TABLE_CELL *b_ctc,
				      char *c, char *l1, char *l2)
{
  LPAIR labels;
  CLASS_TABLE_CELL *c_ctc;

  if(get_meet_dcl_rhs(a_ctc->name, b_ctc->name, c, l1, l2, &c_ctc, &labels)) {
    add_intersection_tm(a_ctc, b_ctc, c_ctc, labels);
  }
}


/****************************************************************
 *			    GET_INTERSECTION_TM			*
 ****************************************************************
 * Returns the ctc_num of intersection of a and b from 		*
 * interesect_tbl, or 0 if the intersection is empty.		*
 *								*
 * IMPORTANT NOTE:						*
 * This function should only be used after you have already	*
 * tested whether a is beneath b, or whether b is beneath a.	*
 * It does not test for those conditions.			*
 ****************************************************************/

int get_intersection_tm(CLASS_TABLE_CELL* a, CLASS_TABLE_CELL* b)
{
  HASH_KEY u;
  HASH2_CELLPTR h;

  u.lpair = make_ordered_lpair_t(ctc_num(a), ctc_num(b));
  h       = locate_hash2(intersection_table, u, lphash(u.lpair), eq);

  if(h->key.num != 0) {
    return h->val.num;
  }
  else return 0;
}


/****************************************************************
 *		    FULL_GET_INTERSECTION_TM			*
 ****************************************************************
 * Returns the ctc_num of the intersection of a and b, or -1 	*
 * if the intersection is empty.				*
 *								*
 * Unlike get_intersection_tm, full_get_intersection_tm		*
 * allows the possibility that one of a and b is beneath the	*
 * other.							*
 ****************************************************************/

int full_get_intersection_tm(CLASS_TABLE_CELL* a, CLASS_TABLE_CELL* b)
{
  int result;

# ifdef DEBUG
    if(trace_classtbl > 2) {
      trace_s(88);
      trace_s(89, a->name, ctc_num(a), b->name, ctc_num(b));
    }
# endif
    
  /*-------------------------------------------------------*
   * If a and b are the same, then the intersection is a.  *
   * If b is NULL, then we say that the intersection is a. *
   *-------------------------------------------------------*/

  if(a == b || b == NULL) result = ctc_num(a); 

  /*-----------------------------------------------*
   * If a is NULL, say that the interscetion is b. *
   *-----------------------------------------------*/

  else if(a == NULL) result = ctc_num(b);

  /*----------------------------------------------------------*
   * Check whether a is beneath b or b is beneath a.  If a is *
   * beneath b, then the intersection is a.  If b is beneath  *
   * a, then the intersection is a.			      *
   *----------------------------------------------------------*/

  else if(ancestor_tm(a, b)) result = ctc_num(b);
  else if(ancestor_tm(b, a)) result = ctc_num(a);

  /*--------------------------------------------------------------------*
   * If either of a or b is a species or family, then the only way	*
   * to get a nonempty intersection is for one to be beneath the	*
   * other, or for both to be equal.  Since both of those cases		*
   * have been dealt with above, return -1 now for this case.		*
   *--------------------------------------------------------------------*/

  else {
    int b_code = b->code;
    int a_code = a->code;

    if(a_code == TYPE_ID_CODE || a_code == FAM_ID_CODE || 
       b_code == TYPE_ID_CODE || b_code == FAM_ID_CODE) {
      result = -1;
    }

    /*-------------------------------------------------------*
     * Here, we encounter the case where get_intersection_tm *
     * does the job.					     *
     *-------------------------------------------------------*/

    else {
#     ifdef DEBUG
        if(trace_classtbl > 2) {
           trace_s(88);
	   trace_s(93);
        }
#     endif
      result = get_intersection_tm(a, b);
      if(result == 0) result = -1;
    }
  }

# ifdef DEBUG
    if(trace_classtbl > 2) {
       trace_s(86 , nonnull(a->name), nonnull(b->name));
       if(result == 0) fprintf(TRACE_FILE, "empty\n");
       else fprintf(TRACE_FILE, "%s\n", nonnull(ctcs[result]->name));
     }
# endif

  return result;
}


/****************************************************************
 *		   INTERSECTION_CTC_TM				*
 ****************************************************************
 * Return a pointer to the class table cell of the intersection *
 * of A and B, or NULL if their intersection is null.           *
 ****************************************************************/

CLASS_TABLE_CELL* 
intersection_ctc_tm(CLASS_TABLE_CELL *A, CLASS_TABLE_CELL *B)
{
  int i = full_get_intersection_tm(A, B);
  if(i < 0) return NULL;
  else return ctcs[i];
}


/*===============================================================
 *		ACCESSING AND MODIFYING THE JOIN TABLE
 *==============================================================*/

/****************************************************************
 *			COMPUTE_JOIN 			        *
 ****************************************************************
 * Find the join of A and B, and return its index in ctcs.	*
 * This is the smallest thing that contains both A and B.	*
 * 								*
 * It is required that A and B are incomparable.		*
 ****************************************************************/

PRIVATE int 
compute_join(CLASS_TABLE_CELL *A, CLASS_TABLE_CELL *B) 
{
  int i, result;
  CLASS_TABLE_CELL *vctci;

  result = 0;
  for(i = 1; i < next_cg_num; i++) {
    vctci = vctcs[i];
    if(ancestor_tm(vctci, A) && ancestor_tm(vctci, B)) {
      if(result == 0 || ancestor_tm(vctcs[result], vctci)) {
        result = i;
      }
    }
  }

# ifdef DEBUG
    if(trace_classtbl > 2) {
      trace_s(15, A->name, B->name, vctcs[result]->name);
    }
# endif

  return vctcs[result]->num;
}


/****************************************************************
 *			INSTALL_JOIN 			        *
 ****************************************************************
 * Install c as the join of a and b.  				*
 ****************************************************************/

PRIVATE void 
install_join(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b, CLASS_TABLE_CELL *c)
{
  HASH_KEY u;
  HASH2_CELLPTR h;

  u.lpair = make_ordered_lpair_t(ctc_num(a), ctc_num(b));
  h       = insert_loc_hash2(&join_table, u, lphash(u.lpair), eq);
  h->key.lpair = u.lpair;
  h->val.num   = ctc_num(c);
}


/****************************************************************
 *			GET_JOIN_FROM_TABLE		        *
 ****************************************************************
 * Return the join of a and b.	For this function, it is 	*
 * required that a and b are incomparable.  See full_get_join	*
 * for a more flexible function.				*
 *								*
 * The join is obtained from table tbl.				*
 *								*
 * The return value if the index in ctcs of the join (0 if the	*
 * join is ANY).						*
 ****************************************************************/

PRIVATE int 
get_join_from_table(HASH2_TABLE *tbl, CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b)
{
  HASH_KEY u;
  HASH2_CELLPTR h;
 
  u.lpair = make_ordered_lpair_t(ctc_num(a), ctc_num(b));
  h       = locate_hash2(tbl, u, lphash(u.lpair), eq);
  if(h->key.num == 0) return 0;
  else return h->val.num;
}


/****************************************************************
 *			INCONSISTENT_JOIN_ERR			*
 ****************************************************************
 * Report that inconsistent joins have been declared.		*
 * Join a & b = old_join was previously reported, but join	*
 * a & b = new_join is now declared.				*
 *								*
 * This function is not currently used.  We allow joins to 	*
 * change.							*
 ****************************************************************/

#ifdef NEVER
PRIVATE void 
inconsistent_join_err(CLASS_TABLE_CELL *a, 
		      CLASS_TABLE_CELL *b,
		      CLASS_TABLE_CELL *old_join, 
		      CLASS_TABLE_CELL *new_join)
{
# ifdef TRANSLATOR
    err_head(0, current_line_number);
    err_print(INCONSISTENT_JOIN_ERR, display_name(a->name), 
	      display_name(b->name), 
	      display_name(old_join->name), 
	      display_name(new_join->name));
# else
   gprintf(STDERR, "Inconsistent joins\n (%s,%s) joins to %s and %s.\n",
	   a->name, b->name, old_join->name, new_join->name);
# endif
}
#endif


/****************************************************************
 *			INSTALL_JOINS 			        *
 ****************************************************************
 * Install joins of all pairs of species, families, genera and 	*
 * communities.	 						*
 *								*
 * Also check that all are consistent with previous values, if  *
 * any previous values existed.					*
 ****************************************************************/

void install_joins(void)
{
  int i, j, join_ij;
  CLASS_TABLE_CELL *ctci, *ctcj;
  HASH2_TABLE* old_join_table;

  old_join_table = join_table;
  join_table = NULL;
  for(i = 0; i < next_class_num; i++) {
    ctci = ctcs[i];
    for(j = 0; j < next_class_num; j++) {
      ctcj = ctcs[j];
      if(i != j && !ancestor_tm(ctci, ctcj) && !ancestor_tm(ctcj, ctci)) {
	join_ij = compute_join(ctci, ctcj);

	/*--------------------------------------------------------*
	 * The following complains if the joins have changed.  We *
	 * allow the joins to change now.			  *
	 *--------------------------------------------------------*/

#       ifdef NEVER
	if(ctci->closed && ctcj->closed) {
	  int old_join_ij = get_join_from_table(old_join_table, ctci, ctcj);
	  if(join_ij != old_join_ij) {
	    inconsistent_join_err(ctci, ctcj, 
				  ctcs[old_join_ij], ctcs[join_ij]);
	  }
	}
#       endif
	
	if(join_ij != 0) install_join(ctci, ctcj, ctcs[join_ij]);
      }
    }
  }

  free_hash2(old_join_table);
}


/****************************************************************
 *			GET_JOIN 			        *
 ****************************************************************
 * Return the join of a and b.	For this function, it is 	*
 * required that a and b are incomparable.  See full_get_join	*
 * for a more flexible function.				*
 *								*
 * The return value if the index in ctcs of the join (0 if the	*
 * join is ANY).						*
 ****************************************************************/

int get_join(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b)
{
  int result = get_join_from_table(join_table, a, b);

# ifdef DEBUG
    if(trace_classtbl > 2) {
      trace_s(16, a->name, b->name, ctcs[result]->name);
    }
# endif
 
  return result;
}

/****************************************************************
 *			FULL_GET_JOIN 			        *
 ****************************************************************
 * Return the join of a and b.					*
 *								*
 * The return value is the index in ctcs of the join (0 if the	*
 * join is ANY).						*
 ****************************************************************/

int full_get_join(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b)
{
  if     (a == NULL || b == NULL) return 0;
  else if(a == b)                 return ctc_num(a);
  else if(ancestor_tm(a, b))      return ctc_num(a);
  else if(ancestor_tm(b, a))      return ctc_num(b);
  else                            return get_join(a,b);
}


/*===============================================================
 *			CHECKING INTERSECTIONS
 *==============================================================*/

/****************************************************************
 *			 EMPTY_COMPLAIN			        *
 ****************************************************************
 * Complain that a /\ (b /\ c) is empty but (a /\ b) /\ c is    *
 * ctcs[k]. x and y are the two things that have been found to  *
 * have an empty intersection in a /\ (b /\ c).  They might be  *
 * b and c, or they might be a and (b /\ c).			*
 *								*
 * There is an important exception.  Do not report an error	*
 * when x and y are genera and k is the index of a transparent	*
 * family or community, since then there is really no problem. 	*
 *								*
 * For the translator, if err_flags.reported_non_assoc_error is *
 * true, then suppress the report, to avoid multiple reports.	*
 ****************************************************************/

PRIVATE void 
empty_complain(CLASS_TABLE_CELL *a,
	       CLASS_TABLE_CELL *b,
	       CLASS_TABLE_CELL *c,
	       CLASS_TABLE_CELL *x,
	       CLASS_TABLE_CELL *y,
	       int k)
{
  char *a_name, *b_name, *c_name;
  CLASS_TABLE_CELL *ab_c_ctc;

  /*----------------------------------------------------*
   * Check for the special case where k is the index of *
   * a transparent family, and a and b are genera.	*
   *----------------------------------------------------*/

  if(x->code == GENUS_ID_CODE && y->code == GENUS_ID_CODE) {
    CLASS_TABLE_CELL* ctck = ctcs[k];
    if(ctck == Pair_ctc ||
       ((ctck->code == FAM_ID_CODE || ctck->code == COMM_ID_CODE)
	&& !ctck->opaque)) {
      return;
    }
  }

# ifdef TRANSLATOR
    if(err_flags.reported_non_assoc_error) return;
    err_flags.reported_non_assoc_error = TRUE;
# endif

  a_name = display_name(a->name);
  b_name = display_name(b->name);
  c_name = display_name(c->name);
  ab_c_ctc  = ctcs[k];
    
# ifdef TRANSLATOR
    err_head(0, current_line_number);
    err_print(NON_ASSOC_ERR, a_name, b_name, c_name, "(empty)");
    err_print(MEET2_ERR, a_name, b_name, c_name, 
	      display_name(ab_c_ctc->name));
# else
    package_die(173, a_name, b_name, c_name, "(empty)",
		a_name, b_name, c_name, ab_c_ctc->name);
# endif

}


/****************************************************************
 *			NON_ASSOC_ERR				*
 ****************************************************************
 * The class hierarchy is not associative.  Report the		*
 * inconsistency as a /\ (b /\ c) = a_bc, but 			*
 * (a /\ b) /\ c = ab_c.  					*
 *								*
 * For the translator, if err_flags.reported_non_assoc_error is *
 * true, then suppress the report, to avoid multiple reports.	*
 ****************************************************************/

PRIVATE void 
non_assoc_err(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b, 
	      CLASS_TABLE_CELL *c, int a_bc, int ab_c)
{
  char *a_name, *b_name, *c_name;
  CLASS_TABLE_CELL *a_bc_ctc, *ab_c_ctc;

# ifdef TRANSLATOR
    if(err_flags.reported_non_assoc_error) return;
    err_flags.reported_non_assoc_error = TRUE;
# endif

  a_name = display_name(a->name);
  b_name = display_name(b->name);
  c_name = display_name(c->name);
  a_bc_ctc  = ctcs[a_bc];
  ab_c_ctc  = ctcs[ab_c];
    
# ifdef TRANSLATOR
    err_head(0, current_line_number);
    err_print(NON_ASSOC_ERR, a_name, b_name, c_name, 
	      display_name(a_bc_ctc->name));
    err_print(MEET2_ERR, a_name, b_name, c_name, 
	      display_name(ab_c_ctc->name));
# else
    package_die(173, a_name, b_name, c_name, a_bc_ctc->name,
		a_name, b_name, c_name, ab_c_ctc->name);
# endif
}


/****************************************************************
 *			 CHECK_ASSOC_CASE			*
 ****************************************************************
 * Check that a /\ (b /\ c) = (a /\ b) /\ c.  Report an error   *
 * if not.							*
 *								*
 * ab is the intersection of a and b.				*
 *								*
 * Tables reported_errs and reported_emp tell errors that have  *
 * already been reported, so that they will not be reported 	*
 * again.							*
 ****************************************************************/

PRIVATE void
check_assoc_case(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b, 
		 CLASS_TABLE_CELL *c, int ab)
{
  /*--------------------*
   * ab   = a  /\ b	*
   * bc   = b  /\ c	*
   * ab_c = ab /\ c	*
   * a_bc = a  /\ bc	*
   *--------------------*/

  int a_bc, ab_c, bc;
   
  ab_c = full_get_intersection_tm(ctcs[ab], c);
  bc   = full_get_intersection_tm(b,c);

  /*-------------------------------------------------------------*
   * If b /\ c is empty and (a /\ b) /\ c is not empty, then we  *
   * have a conflict, since a /\ (b /\ c) will be empty.         *
   * But do not report this when b and c are genera and 	 *
   * (a /\ b) /\ c is a transparent family or community, since   *
   * then the intersection really might be empty.		 *
   *-------------------------------------------------------------*/

  if(bc == -1) {
    if(ab_c != -1) {
      empty_complain(a, b, c, b, c, ab_c);
    }
    return;
  }

  a_bc = full_get_intersection_tm(a, ctcs[bc]);

  if(a_bc == -1) {

    /*----------------------------------------------------------*
     * If a /\ (b /\ c) and (a /\ b) /\ c are both empty, then  *
     * this case looks fine.					*
     *----------------------------------------------------------*/

    if(ab_c == -1) return;

    /*-----------------------------------------------------------*
     * If a /\ (b /\ c) is empty but (a /\ b) /\ c is not empty, *
     * then complain.						 *
     *-----------------------------------------------------------*/

    else {
      empty_complain(a, b, c, a, ctcs[bc], ab_c);
      return;
    }
  }

  /*------------------------------------------------------------*
   * If a /\ (b /\ c) is not empty but (a /\ b) /\ c is empty,  *
   * then complain.						*
   *------------------------------------------------------------*/

  else if(ab_c == -1) {
    empty_complain(c, b, a, ctcs[ab], c, a_bc);
    return;
  }

  /*------------------------------------------------------------*
   * If both a /\ (b /\ c) and (a /\ b) /\ c are nonempty, then *
   * we must check that they are compatible.			*
   *------------------------------------------------------------*/

  else {
    if(ab_c != a_bc) {
      non_assoc_err(a, b, c, a_bc, ab_c);
    }
    return;
  }
}


/****************************************************************
 *			 CHECK_ASSOCIATIVITY_TM			*
 ****************************************************************
 * Check that the hierarchy is associative.
 *								*
 * Tables reported_errs and reported_emp tell errors that have  *
 * already been reported, so that they will not be reported 	*
 * again.							*
 ****************************************************************/

void check_associativity_tm(void)
{
  int an, bn, cn;
  Boolean b_is_concrete;
  CLASS_TABLE_CELL *a, *b, *c;
  int ab;

# ifdef DEBUG
    if(trace_classtbl) trace_s(76);
# endif

  /*-------------------------------------------------------------*
   * Check each genus or community a that has been changed in    *
   * the current extension.					 *
   *-------------------------------------------------------------*/

  for(an = 1; an < next_cg_num; an++) {
    a = vctcs[an];
    if(a->is_changed) {

      /*------------------------------------------------*
       * For each a, try each b and c to see if 	*
       * (a /\ b) /\ c = a /\ (b /\ c)			*
       *------------------------------------------------*/

      for(bn = 1; bn < next_class_num; bn++) {
	b = ctcs[bn];
	b_is_concrete = MEMBER(b->code, concrete_codes);
	if(a != b) {
	  ab = full_get_intersection_tm(a,b);
	  if(ab != -1) {
	    for(cn = 1; cn < next_class_num; cn++) {
	      c = ctcs[cn];

	      /*----------------------------------------------------*
	       * If b and c are both species, then there is no need *
	       * to do any test.  If b != c, then (a /\ b) /\ c and *
	       * a /\ (b /\ c) are both empty.  If b == c, then	    *
	       * both must be either empty or b.		    *
	       * The same reasoning goes if both are families, or   *
	       * if one is a family and the other is a species.	    *
	       *----------------------------------------------------*/

	      if(!b_is_concrete || !MEMBER(c->code, concrete_codes)) {

#	        ifdef DEBUG
		  if(trace_classtbl > 2) {
		    trace_s(77, a->name, b->name, c->name, 
			    ctcs[ab]->name);
		  }
#	        endif

	        /*-----------------------------------------------*
		 * If c is the same as a or b, then there cannot *
		 * be any problem.  So only check if c != b and  *
		 * c != a.					 *
		 *-----------------------------------------------*/

	        if(c != b && c != a) {
		  check_assoc_case(a, b, c, ab);
		  if(!b->is_changed) {
		    check_assoc_case(b, a, c, ab);
		  }
		}

#               ifdef DEBUG
		  if(trace_classtbl > 2) {
		    trace_s(78, a->name, b->name, c->name, 
			    ctcs[ab]->name);
		  }
#	        endif

	      } /* end if(!b_is_concrete...) */

	    } /* end for(cn = ...) */

	  } /* end if(ab != -1) */

	} /* end if(a != b) */

      } /* end for(bn = ...) */

    } /* end if(a->is_changed) */

  } /* end for(an = ...) */
}

/*===============================================================
 *			INITIALIZATION
 *==============================================================*/

/****************************************************************
 *			INIT_INTERSECT_TABLE			*
 ****************************************************************
 * Initialize intersect_table and join_table.  			*
 ****************************************************************/

void init_intersect_table(void)
{
  intersection_table = create_hash2(3);
  join_table 	     = create_hash2(3);
}


/*===============================================================
 *			DEBUGGING SUPPORT
 *==============================================================*/

#ifdef DEBUG
/****************************************************************
 *			PRINT_INTERSECT_TABLE			*
 ****************************************************************
 * Print the intersection table on TRACE_FILE.			*
 ****************************************************************/

PRIVATE void 
print_intersect_cell(HASH2_CELLPTR h)
{
  LPAIR key;
  int lt;
  CLASS_TABLE_CELL *a, *b;

  lt  = h->val.num;
  key = h->key.lpair;
  a   = ctcs[key.label1];
  b   = ctcs[key.label2];
  trace_s(10, nonnull(a->name), nonnull(b->name), nonnull(ctcs[lt]->name));
}

/*-----------------------------------------------------------*/

void print_intersect_table()
{
  trace_s(11);
  scan_hash2(intersection_table, print_intersect_cell);
}

/****************************************************************
 *			SHOW_JOIN_TABLE			        *
 ****************************************************************
 * Print the entire join table, including joins that are not    *
 * directly stored in the table.				*
 ****************************************************************/

void show_join_table(void)
{
  int i,j, k;
 
  trace_s(102);
  for(i = 0; i < next_class_num; i++) {
    for(j = 0; j < next_class_num; j++) {
      k = full_get_join(ctcs[i], ctcs[j]);
      trace_s(103, nonnull(ctcs[i]->name), nonnull(ctcs[j]->name), 
	      nonnull(ctcs[k]->name));

    }
  }
}


/****************************************************************
 *			SHOW_MEET_TABLE			        *
 ****************************************************************
 * Print the entire meet table, including meets that are not    *
 * stored in the table.						*
 ****************************************************************/

void show_meet_table(void)
{
  int i,j, k;
  char *meet;
 
  trace_s(99);
  for(i = 0; i < next_class_num; i++) {
    for(j = 0; j < next_class_num; j++) {
      if(i == 0 && j == 0) meet = "ANY";
      else {
        k = full_get_intersection_tm(ctcs[i], ctcs[j]);
        meet = (k == -1) ? "empty" : nonnull(ctcs[k]->name);
      }
      trace_s(100, nonnull(ctcs[i]->name), nonnull(ctcs[j]->name), meet);
    }
  }
}


#endif
