/****************************************************************
 * File:    clstbl/classtbl.c
 * Purpose: Table manager for genus, species, etc. ids.
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
 * This file contains functions that manage the table of genus,		*
 * species, community and family identifiers.  This table also stores	*
 * the hierarchy relating those ids, with the exception of meets, 	*
 * which are stored in meettbl.c.					*
 *									*
 * Additionally, a table giving information about classes is stored.	*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
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
#include "../clstbl/meettbl.h"
#include "../clstbl/typehash.h"
#include "../evaluate/instruc.h"
#include "../generate/generate.h"
#include "../lexer/modes.h"
#include "../unify/unify.h"
#ifdef MACHINE
# include "../intrprtr/intrprtr.h"
# include "../error/m_error.h"
#endif
#ifdef TRANSLATOR
# include "../error/error.h"
# include "../dcls/dcls.h"
# include "../exprs/expr.h"
#endif
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 * 			VARIABLES				*
 ****************************************************************/

/****************************************************************
 * 			class_id_table				*
 ****************************************************************
 * class_id_table associates information with each identifier   *
 * that names a species, family, genus, or community. 		*
 * The key is the identifier (a string).  The    		*
 * value has type CLASS_TABLE_CELL*, and points to a table      *
 * entry with the following fields.				*
 *								*
 *  code	This tells the kind of thing that the table     *
 *   		entry describes.  This corresponds to a token	*
 *		of the parser, but it is kept small since this	*
 *		field is only one byte.  Possible values are as *
 *		follows. See tables.h for definitions.		*
 *								*
 *		GENUS_ID_CODE	   a genus			*
 *		COMM_ID_CODE	   a community			*
 *	        TYPE_ID_CODE	   a species			*
 *		FAM_ID_CODE	   a family			*
 *		PAIR_CODE	   The entry for "pair", 	*
 *				   corresponding to species of  *
 *				   the form (A,B).		*
 *		FUN_CODE	   The entry for "function",	*
 *				   corresponding to species of  *
 *				   the form (A -> B).		*
 *								*
 *  extensible	0   for a nonextensible genus or community	*
 *		    (for example, REAL);			*
 *		1   for a special genus or community that can   *
 *		    get new members, but cannot explicitly be   *
 *		    extended by a program (for example RANKED);	*
 *		2   for a typical genus or community that can	*
 *		    be extended explicitly.			*
 *								*
 *  expected	0 if this identifier has been defined,		*
 *		1 if this identifier has been expected in the 	*
 *		  current package, but not defined,		*
 *		2 if this identifier has been expected in a	*
 * 		  package that was imported by the current 	*
 * 		  package.					*
 *								*
 *								*
 *  is_changed  TRUE if this genus or community has had its     *
 *		definition changed in the current extension,    *
 *		by having new members added.  This is used to   *
 *		see what needs to be checked at the end of	*
 *		an extension.					*
 *								*
 *  closed	TRUE if this genus or community was present 	*
 *		when the hierarchy was closed at the end	*
 *		of an extension.  Intersections between closed	*
 *		genera/communities cannot be altered.		*
 *								*
 *  nonempty	0 for an empty genus or community, 1 for a	*
 *		nonempty genus or community.  (An empty one has *
 *		no members.)					*
 *								*
 *  opaque	1 for an opaque family or community, 0		*
 *		otherwise.					*
 *								*
 *  partial	Normally, a newly defined species or family	*
 *		has a member for every member of its		*
 *		representation species (or species plural).	*
 *		A partial species or family only has members	*
 *		that correspond to a proper subset of its	*
 *		representation species.	 The constructor for	*
 *		a partial species or family is dangerous 	*
 *		because it does not apply to every member of	*
 *		its domain, and must be dealt with carefully.	*
 *								*
 *  num		The index of this item in array ctcs.  So	*
 *		ctcs[x->num] = x.				*
 *								*
 *  var_num	The index in vctcs of this item.  So		*
 *		vctcs[x->var_num] = x.  The var_num field is 	* 
 *		only nonnegative for genera and communities.	*
 *		It is -1 for other kinds of things.		*
 *								*
 *  std_num	The standard number of a standard thing.	*
 *		This is used for interpreter instructions that  *
 *		create standard species, genera, etc.		*
 *		If this table entry is for something that is 	*
 *		not standard, then the std_num field holds 0.	*
 *								*
 *  mem_num	The index in ctcs of a member of this genus     *
 *              or community, if this table entry is for a 	*
 *		genus or community.  This field is only		*
 *		meaningful when the nonempty field holds 1.	*
 *								*
 *  ancestors	A bit-vector representing the set of the	*
 *		var_num values of ancestors of this genus,	*
 *		community, family or species.			*
 *								*
 *  name	The name of this thing.				*
 *								*
 *  ty		A type expression for this thing.  If this table*
 * 		entry is for a species or family, then ty is	*
 *		that species or family.  If this table entry	*
 *		is for a genus or community, the ty is a	*
 *		variable that ranges over this genus or		*
 *		community.					*
 *								*
 *  package	The name of the package where this thing was 	*
 *		defined.					*
 *								*
 *  constructors A list of the names of the constructors	*
 *		 for a species or family.			*
 *								*
 *  CTC_DEFAULT	If this table entry is for a genus or community,*
 *		then the CTC_DEFAULT field holds the default for*
 *		this genus or community, used in type inference *
 *		and in the interpreter.				*
 *								*
 *  CTC_DEFAULT_TBL This field is only used in the interpreter  *
 *		    to determine how to do defaults.		*
 * 		    It is a table associating with each package *
 *		    number a list [a1,t1,a2,t2,...], where each *
 *		    ai is a byte code address and each ti is    *
 *		    a default type.  The byte code addresses    *
 *		    are in nonascending order.  Each default    *
 *		    type ti is applicable to code at all	*
 *		    addresses that are smaller than ai in the	*
 *		    given package.  The last one that is	*
 *		    applicable is taken as the default.		*
 *								*
 *  dangerous	1 if the default for this genus or community is *
 *		considered dangerous, so that any default using *
 *		it should be reported.				*
 * 								*
 *  CTC_REP_TYPE If this cell is for a species T, then there are*
 *               two possible values for this field.  If T was  *
 *               declared to have a single representation type, *
 *               as in						*
 *								*
 *			Species T = S.				*
 *								*
 *		 then the CTC_REP_TYPE field holds S.  If T has *
 *		 multiple parts, as in				*
 *								*
 *			Species T = S1 | S2.			*
 *								*
 *		 then the CTC_REP_TYPE field holds NULL.	*
 *								*
 *		 If this cell is for a family F, then there are *
 *		 two possible values of the CTC_REP_TYPE field. *
 *		 First, if F is declared with a single part, as *
 *		 in 						*
 *			Species F(A) = S.			*
 *								*
 *		 then the CTC_REP_TYPE field holds pair type	*
 *		 (A,S).	 If F has several parts in its 		*
 *		 definition, as in				*
 *								*
 *			Species F(A) = S1 | S2.			*
 *								*
 *		 then the CTC_REP_TYPE field holds pair type	*
 *		 (A, NULL).					*
 ****************************************************************/

PRIVATE HASH2_TABLE* class_id_table = NULL;

/****************************************************************
 * 			ctcs					*
 * 			vctcs					*
 *			next_class_num				*
 *			next_cg_num				*
 *			ctcs_size				*
 *			vctcs_size				*
 ****************************************************************
 * These arrays are used to get a pointer to a class-table cell *
 * from the num or var_num value.  They are typically used	*
 * for scanning through all species, etc.			*
 * 								*
 * ctcs_size is the physical size of ctcs, and vctcs_size is	*
 * the physical size of vctcs.  These arrays can be reallocated *
 * when needed.							*
 *								*
 * next_class_num is the actual size of ctcs, and next_cg_num	*
 * is the actual size of vctcs, in terms of used entries.	*
 ****************************************************************/

CLASS_TABLE_CELL 	**ctcs, **vctcs;
int			next_class_num = 0;
int			next_cg_num    = 0;
PRIVATE int              ctcs_size, vctcs_size;

/****************************************************************
 * 			Pair_ctc				*
 * 			Function_ctc				*
 ****************************************************************
 * These hold pointers to the class-table entries for some	*
 * special kinds of species constructors. 			*
 *								*
 *   Pair_ctc		The entry for "pair"			*
 *   Function_ctc	The entry for "function"		*
 ****************************************************************/

CLASS_TABLE_CELL	*Function_ctc, *Pair_ctc;

/****************************************************************
 * 			fam_codes				*
 *			concrete_codes				*
 ****************************************************************
 * fam_codes is a set of the code values FAM_ID_CODE, 		*
 * COMM_ID_CODE, PAIR_CODE and FUN_CODE.  These are things that *
 * require a link label when beneath a genus in the 		*
 * hierarchy.							*
 *								*
 * concrete_codes is a set of the codes FAM_ID_CODE, PAIR_CODE, *
 * FUN_CODE and TYPE_ID_CODE.  These are things that must be    *
 * leaves in the hierarchy.					*
  ****************************************************************/

intset fam_codes, concrete_codes;

/****************************************************************
 * 			altered_hierarchy			*
 ****************************************************************
 * altered_hierarchy is set TRUE when the hierarchy is altered  *
 * inside an externsion.  That way, at the end of the 		*
 * extension, we know whether it is necessary to rebuild	*
 * the hierarchy.						*
 ****************************************************************/

Boolean	altered_hierarchy = FALSE;

/****************************************************************
 * 			report_extends				*
 ****************************************************************
 * report_extends should be set TRUE to cause each creation of  *
 * extension of a genus or community to be reported in the	*
 * compiler listing, and to FALSE to cause such reports to be	*
 * suppressed.							*
 ****************************************************************/

Boolean	report_extends = TRUE;

/****************************************************************
 * 		building_standard_types				*
 ****************************************************************
 * building_standard_types is true when constructing the	*
 * standard species, families, genera and communities.  It 	*
 * affects processing in some ways.				*
 ****************************************************************/

Boolean building_standard_types = FALSE;

/****************************************************************
 * 		ignore_class_table_full				*
 ****************************************************************
 * If ignore_class_table_full is true, then any attempt to add  *
 * a genus, community, species or family that overfills the     *
 * class table will be ignored.  When this variable is false,   *
 * such an attempt causes the program to abort.			*
 ****************************************************************/

Boolean ignore_class_table_full = FALSE;

/****************************************************************
 * 			gen_code				*
 ****************************************************************
 * This file is shared by the interpreter and the compiler.     *
 * gen_code is provided here as a substitute for the compiler's *
 * gen_code.  It is always false.				*
 ****************************************************************/

#ifdef MACHINE
Boolean gen_code = FALSE;
#endif

/******************************************************************
 * 			last_get_ctc_name			  *
 * 			last_get_ctc_ctc			  *
 ******************************************************************
 * last_get_ctc_name is the most recently looked up name, or NULL *
 * if there is none.  						  *
 *								  *
 * last_get_ctc_ctc is the result returned for last_get_ctc_name. *
 *								  *
 * These variables are used to speed up consecutive searches      *
 * for the same id in get_ctc_tm, get_ctc_with_hash_tm and	  *
 * get_new_ctc_tm.						  *
 ******************************************************************/

PRIVATE char*             last_get_ctc_name = NULL;
PRIVATE CLASS_TABLE_CELL* last_get_ctc_ctc;


/****************************************************************
 *		        CLASS_NAME				*
 ****************************************************************
 * Return the name of token tok, for error-reporting purposes.	*
 * tok must be one of TYPE_ID_TOK, FAM_ID_TOK, GENUS_ID_TOK,	*
 * COMM_ID_TOK.							*
 ****************************************************************/

char* class_name(int tok)
{
  if(tok == TYPE_ID_TOK) return "species";
  if(tok == FAM_ID_TOK) return "family";
  if(tok == GENUS_ID_TOK) return "genus";
  if(tok == COMM_ID_TOK) return "community";
  return "??";
}


/****************************************************************
 *			CLEAR_CLASS_TABLE_MEMORY		*
 ****************************************************************
 * Forget things that are being remembered to speed up lookups. *
 * This should be called when entering a new context.  See	*
 * clear_table_memory.						*
 ****************************************************************/

void clear_class_table_memory(void)
{
  last_get_ctc_name = NULL;
}


/****************************************************************
 *			INIT_CLASS_TBL_TM			*
 ****************************************************************
 * This should be called at startup.				*
 ****************************************************************/

void init_class_tbl_tm()
{
  fam_codes		= (1L << FAM_ID_CODE)  | 
			  (1L << COMM_ID_CODE) |
			  (1L << PAIR_CODE)    |
			  (1L << FUN_CODE);
  concrete_codes	= (1L << FAM_ID_CODE)  |
			  (1L << PAIR_CODE)    |
			  (1L << FUN_CODE)     |
			  (1L << TYPE_ID_CODE);

  /*-------------------------------------------------*
   * Build the tables and the arrays ctcs and vctcs. *
   *-------------------------------------------------*/

  class_id_table	= create_hash2(4);
  init_intersect_table();
  ctcs_size		= INIT_CTCS_SIZE;
  ctcs 			= (CLASS_TABLE_CELL **) 
			  alloc(ctcs_size * sizeof(CLASS_TABLE_CELL *));
  vctcs_size 		= INIT_VCTCS_SIZE;
  vctcs 		= (CLASS_TABLE_CELL **)
			  alloc(vctcs_size * sizeof(CLASS_TABLE_CELL *));

  /*--------------------------*
   * Set up ctcs[0], for ANY. *
   *--------------------------*/

  add_class_tm(stat_id_tb(std_type_id[ANYG_TYPE_ID]), GENUS_ID_TOK, 
	       1, FALSE, FALSE);
  SET_TYPE(ctcs[0]->ty, var_t(NULL));
}


/****************************************************************
 *			CTC_NUM					*
 ****************************************************************
 * Return the num value of ctc (0 if ctc is NULL).		*
 ****************************************************************/

int ctc_num(CLASS_TABLE_CELL *ctc)
{
  return (ctc == NULL) ? 0 : ctc->num;
} 


/*===============================================================
 *			ANCESTOR TESTS, ETC.
 *==============================================================*/

/****************************************************************
 *			 ANCESTOR_TM				*
 ****************************************************************
 * Return true if a > b in the hierarchy, or if a is ANY.  	*
 * a and b are given by pointers to their class table entries.  *
 * A null pointer is interpreted as a pointer to the entry for  *
 * genus ANY.							*
 ****************************************************************/

#define AC1 LOG_LONG_BITS
#define AC2 (LONG_BITS - 1)

Boolean ancestor_tm(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b)

{ 
  int m;
  Boolean result;

  /*-----------------------------------------*
   * If a is ANY, then a > b automatically.  *
   * (Note that we are supposed to return    *
   * true even when b is ANY in this case.   *
   *-----------------------------------------*/

  if(a == NULL || a->num == 0) return TRUE;

  /*-----------------------------------------------------------------*
   * Otherwise, if b is ANY, then it cannot be the case that a >  b. *
   *-----------------------------------------------------------------*/

  if(b == NULL) return FALSE;

  /*--------------------------------------------------------*
   * If a is not a genus or community, then a > b is false. *
   *--------------------------------------------------------*/

  m = a->var_num;
  if(m == -1) {result = FALSE; goto out;}

  /*----------------------------------------------------------------*
   * Otherwise, check ancestry by looking at b->ancestors.  This is *
   * a bit vector representing a set of the var_nums (indices in    *
   * vctcs) of the ancestors of b. 				    *
   *----------------------------------------------------------------*/

  result = ((b->ancestors[m >> AC1]) & (1L << (m & AC2))) != 0;

 out:

# ifdef NEVER
#   ifdef DEBUG
      if(trace_classtbl > 2) {
         trace_s(43, a->name, b->name, toint(result));
       }
#   endif
# endif

  return result;
}


/****************************************************************
 *			 SET_ANCESTOR_TM			*
 ****************************************************************
 * Add n to bit-set v.  					*
 *								*
 * If a and b are pointers to class table cells, then you can	*
 * set a to be an ancestor of b using				*
 *								*
 *   set_ancestor_tm(b->ancestors, a->var_num).			*
 ****************************************************************/

PRIVATE void set_ancestor_tm(ANCESTOR_TYPE *v, int n)
{
  v[n >> AC1] |= (1L << (n & AC2));
}


/****************************************************************
 *			COPY_ANCESTORS_TM			*
 ****************************************************************
 * a is a new descendant of b.					*
 *								*
 * Put all of the ancestors of b (including b) into the		*
 * ancestor list of all of the descendants of a (including a).  *
 *								*
 * labels is the label pair on the link from a to b.  It is	*
 * only used for consistency checking, and is only used in the  *
 * translator.  If labels.label1 = -1, then no checking is 	*
 * done.							*
 ****************************************************************/

void copy_ancestors_tm(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b,
		       LPAIR labels)
{
  int i, j, a_num, b_varnum, a_code;
  ANCESTOR_TYPE *b_anc;

# ifdef DEBUG
    if(trace_classtbl) {
      trace_s(62, a->name, toint(a->num), b->name, toint(b->num));
      print_lpair(labels);
      tracenl();
    }
# endif

  b_anc     = b->ancestors;
  b_varnum  = b->var_num;
  a_num     = a->num;
  a_code    = a->code;

  /*--------------------------------------------------------------*
   * Put a itself beneath b when there is no link label involved. *
   *--------------------------------------------------------------*/

  if(a_code == TYPE_ID_CODE || a_code == GENUS_ID_CODE) {
    set_ancestor_tm(a->ancestors, b_varnum);
  }

  /*----------------------------------------------------------------------*
   * Try each genus, species, etc. to see if its ancestors need updating. *
   *----------------------------------------------------------------------*/

  for(i = 1; i < next_class_num; i++) {
    CLASS_TABLE_CELL* ctci = ctcs[i];
    if(i == a_num || ancestor_tm(a, ctci)) {

      /*----------------------------------------------------------------*
       * Check for violation of closure.  We cannot make a change	*
       * that contradicts something that was previously true.  If	*
       * X and Y both existed before, and X was not beneath Y, then	*
       * we cannot now decide that X is beneath Y, or previous		*
       * inferences will be incorrect.  Here, we are about to claim     *
       * that ctci is beneath b, and hence beneath everything above b.	*
       * Check each of those.						*
       *----------------------------------------------------------------*/

      if(ctci->closed) {
	for(j = 1; j < next_cg_num; j++) {
	  CLASS_TABLE_CELL* vctcj = vctcs[j];
          if((b_varnum == j || ancestor_tm(vctcj, b)) 
	     && !ancestor_tm(vctcj, ctci) && vctcj->closed) {
	    semantic_error2(CLOSURE_ERR, display_name(ctci->name), 
			    display_name(vctcj->name), 0);
          } 
        }
      }

      /*-----------------------------------------------------------*
       * Check that the link labels are correct, if there are any. *
       *-----------------------------------------------------------*/

      if(labels.label1 >= 0 &&
	 (a_code == FAM_ID_CODE || a_code == COMM_ID_CODE || 
	  a_code == PAIR_CODE) && b->code == GENUS_ID_CODE) {
        LPAIR correct_labs = get_link_label_tm(ctc_num(b), a_num);
        if(labels.label1 != correct_labs.label1 || 
	   labels.label2 != correct_labs.label2) {
	  semantic_error2(BAD_LINK_LABEL_ERR, display_name(a->name), 
			  display_name(b->name), 0);
        }
      }

      /*-----------------------------*
       * Install the ancestors of i. *
       *-----------------------------*/

      for(j = 0; j < ANCESTORS_SIZE; j++) {
        ctci->ancestors[j] |= b_anc[j];
      }
      set_ancestor_tm(ctci->ancestors, b_varnum);

    } /* end if(i == a_num...) */

  } /* end for(i = ...) */
}


/*==============================================================*
 *		MISC OPERATIONS					*
 *==============================================================*/

/****************************************************************
 *			IS_CLASS_ID				*
 ****************************************************************
 * Return true if s is one of the keys in class_id_table.	*
 * hash must be strhash(s).					*
 ****************************************************************/

#ifdef TRANSLATOR
Boolean is_class_id(char *s, LONG hash)
{
  HASH2_CELLPTR h;
  HASH_KEY u;

  u.str = s;
  h = locate_hash2(class_id_table, u, hash, eq);
  return (h->key.num != 0);
}
#endif


/****************************************************************
 *			DROP_HASH_CTC				*
 ****************************************************************
 * Free h->val.ctc.						*
 ****************************************************************/

#ifdef TRANSLATOR
void drop_hash_ctc(HASH2_CELLPTR h)
{
  CLASS_TABLE_CELL* ctc = h->val.ctc;
  drop_ctc_parts(ctc);
  free_ctc(ctc);
}
#endif

#ifdef NEVER
/****************************************************************
 *			GET_TF_TM				*
 ****************************************************************
 * Returns a type expr for the type or family of s, or NULL if  *
 * s is not defined.  s need not be in the string table.        *
 * CURRENTLY UNUSED.						*
 ****************************************************************/

TYPE* get_tf_tm(char *s)
{
  CLASS_TABLE_CELL* c = get_ctc_tm(s);

  if(c == NULL) return NULL_T;
  else return c->ty;
}
#endif


/****************************************************************
 *			GET_CTC_TM				*
 *			GET_CTC_WITH_HASH_TM			*
 ****************************************************************
 * Returns the class table cell for identifier s, or NULL if 	*
 * there is none.  s need not be in the string table.  Hash is  *
 * strhash(s).							*
 ****************************************************************/

CLASS_TABLE_CELL* get_ctc_with_hash_tm(char *s, LONG hash)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  CLASS_TABLE_CELL *result;

  u.str = id_tb10(s, hash);

  /*------------------------------------------------------------*
   * If we are looking up the same thing as last time, return   *
   * the same value as last time.				*
   *------------------------------------------------------------*/

  if(u.str == last_get_ctc_name) {
    return last_get_ctc_ctc;
  }

# ifdef DEBUG
    if(trace_classtbl > 1) trace_s(47, u.str, u.str);
# endif

  /*--------------------------------*
   * Try looking in class_id_table. *
   *--------------------------------*/

  h = locate_hash2(class_id_table, u, hash, eq);
  if(h->key.num != 0) {
    result = h->val.ctc;
    goto found;
  }

# ifdef DEBUG
    if(trace_classtbl > 1 && h->key.num != 0) trace_s(48);
# endif

  /*----------------------------------------------------------*
   * If not found in class_id_table, try abbreviation tables. *
   * The abbreviations tables are stored with the file-info   *
   * nodes, since they change as imports are started and      *
   * stopped.						      *
   *----------------------------------------------------------*/

# ifdef TRANSLATOR
    result = get_abbrev_tm(u, hash);
    if(result != NULL) goto found;
# endif

  /*-------------------------------------*
   * If we get here, no entry was found. *
   *-------------------------------------*/

# ifdef DEBUG
    if(trace_classtbl > 1) trace_s(49);
# endif
  return NULL;

 found:

  /*-------------------------------------*
   * If we get here, an entry was found. *
   *-------------------------------------*/

# ifdef DEBUG
    if(trace_classtbl > 1) {
      trace_s(50, u.str, MAKE_TOK(result->code));
    }
# endif

  last_get_ctc_name = u.str;
  last_get_ctc_ctc = result;
  return result;
}

/*-------------------------------------------------------------*/

CLASS_TABLE_CELL* get_ctc_tm(char *s)
{
  return get_ctc_with_hash_tm(s, strhash(s));
}


/****************************************************************
 *			GET_NEW_CTC_TM				*
 ****************************************************************
 * Return  a class table cell for identifier s, inserting s 	*
 * into class_id_table if it is not already there.  s need not  *
 * be in the string table.					*
 ****************************************************************/

CLASS_TABLE_CELL* get_new_ctc_tm(char *s)
{
  HASH2_CELLPTR h;
  HASH_KEY u;

  /*-----------------------------------------------*
   * First see if there is already an entry for s. *
   *-----------------------------------------------*/

  LONG hash = strhash(s);
  CLASS_TABLE_CELL* result = get_ctc_with_hash_tm(s, hash);
  if(result != NULL) return result;

  /*------------------------------------------*
   * If there is not an entry, then make one. *
   *------------------------------------------*/

  u.str = id_tb10(s, hash);
  h     = insert_loc_hash2(&class_id_table, u, hash, eq);
  if(h->key.num == 0) {
    h->key.str = u.str;
    result     = h->val.ctc = allocate_ctc();
#   ifdef DEBUG
      if(trace_classtbl > 1) trace_s(51, u.str, u.str);
#   endif
  }
  else {
    result = h->val.ctc;
  }

  last_get_ctc_name = u.str;
  last_get_ctc_ctc = result;
  return result;
}


/****************************************************************
 *			REALLOC_CLASSTBL			*
 ****************************************************************
 * Currently, array *c has *s entries.  Double its size.	*
 * But do not make it larger than max_size.			*
 ****************************************************************/

PRIVATE void
realloc_classtbl(CLASS_TABLE_CELL ***c, int *s, int max_size)
{
  CLASS_TABLE_CELL **new, **old;
  int old_size, new_size;

  old      = *c;
  old_size = *s;
  new_size = old_size + old_size;
  if(new_size > max_size) new_size = max_size;

  new = (CLASS_TABLE_CELL **) 
          reallocate((char *) old, 
		     old_size * sizeof(CLASS_TABLE_CELL *),
		     new_size * sizeof(CLASS_TABLE_CELL *), TRUE);
  *s = new_size;
  *c = new;
}


/*============================================================
 *		CREATING GENERA AND COMMUNITIES AND
 *		EXTENDING GENERA AND COMMUNITIES
 *===========================================================*/

/****************************************************************
 *			ADD_CLASS_TM				*
 ****************************************************************
 * Add a genus or community.  s is the name, tok the token	*
 * (GENUS_ID_TOK or COMM_ID_TOK) telling what kind of thing     *
 * s is.							*
 *								*
 * extensible is the value for the extensible entry.		*
 *								*
 * declare is true if a declaration of the name s should be	*
 * generated in the output file, genf.				*
 *								*
 * opaque is true if this is an opaque community.		*
 *								*
 * If the main context is EXPORT_CX, then write this identifier *
 * into the index file.						*
 *								*
 * This function returns class table cell entry for s on	*
 * success and NULL on failure.  It fails if name s is already  *
 * in use.							*
 ****************************************************************/

CLASS_TABLE_CELL* 
add_class_tm(char *s, int tok, int extensible, Boolean declare, Boolean opaque)
{
  CLASS_TABLE_CELL *c;

# ifdef DEBUG
    if(trace_classtbl) {
      if(tok == GENUS_ID_TOK) trace_s(52, s);
      else trace_s(53, s);
    }
# endif

  /*------------------------*
   * Build the table entry. *
   *------------------------*/

  c 		= get_new_ctc_tm(s);
  c->is_changed = 1;

# ifdef TRANSLATOR
    /*-------------------------------------*
     * Check whether this id is available. *
     *-------------------------------------*/

    if(c->code != 0 && c->code != UNKNOWN_CLASS_ID_CODE) return NULL;
# endif

  /*------------------------------------------------------------*
   * Install information into the table entry.
   *------------------------------------------------------------*/

  c->code       = MAKE_CODE(tok);
  c->name 	= id_tb0(s);
  c->extensible = extensible;
  c->num	= next_class_num;
  c->var_num    = next_cg_num;
  c->opaque     = opaque;
  c->CTC_DEFAULT_TBL = NULL;
  c->CTC_DEFAULT = NULL;
  bmp_type(c->ty = var_t(c));

# ifdef DEBUG
    if(trace_classtbl) {
      trace_s(54, s, toint(c->num));
    }
# endif

  /*------------------------------------------------------*
   * Record the table entry in ctcs and vctcs. Reallocate *
   * those arrays if necessary.				  *
   *------------------------------------------------------*/

  if(next_class_num >= ctcs_size) {
    if(ctcs_size == MAX_NUM_SPECIES) goto ran_out_of_space;   /* bottom */
    realloc_classtbl(&ctcs, &ctcs_size, MAX_NUM_SPECIES);
  }
  ctcs[next_class_num] = c;
  if(next_cg_num >= vctcs_size) {
    if(vctcs_size == MAX_NUM_VARIETIES) goto ran_out_of_space;  /* bottom */
    realloc_classtbl(&vctcs, &vctcs_size, MAX_NUM_VARIETIES);
  }
  vctcs[next_cg_num] = c;
  next_class_num++;
  next_cg_num++;

# ifdef TRANSLATOR
    /*----------------------*
     * Report the addition. *
     *----------------------*/

    if(report_extends) {
      report_dcl_p(c->name, (tok == GENUS_ID_TOK ? GENUS_E : COMM_E),
	           NULL, NULL, NULL);
    }

    /*--------------------------------------------------*
     * Write the id into index_file, if in export part. *
     *--------------------------------------------------*/

    if(doing_export() && index_file != NULL) {
      fprintf(index_file, "%c%s\n", (tok == GENUS_ID_TOK) ? 'g' : 'c', s);
    }

    /*----------------------------------------------*
     * Generate a string declaration, if requested. *
     *----------------------------------------------*/

    if(declare && gen_code) {
      if(tok == GENUS_ID_TOK) id_tb(s, NEW_GENUS_DCL_I);
      else id_tb(s, opaque ? NEW_OPAQUE_COMMUNITY_DCL_I 
		           : NEW_TRANSPARENT_COMMUNITY_DCL_I);
    }
# endif

  /*------------------------------------------------------*
   * If this is a new community, put it beneath OPAQUE or *
   * TRANSPARENT.  Do not do this when building standard  *
   * genera and communities.				  *
   *------------------------------------------------------*/

  if(!building_standard_types && tok == COMM_ID_TOK) {
    CLASS_TABLE_CELL* top_ctc = opaque ? OPAQUE_ctc : TRANSPARENT_ctc;
    extend1ctc_tm(c, NULL, top_ctc, declare && gen_code, NULL);
  }

  return c;

ran_out_of_space:
  if(!ignore_class_table_full) die(23);
  return NULL;
}


/****************************************************************
 *		DO_EXPECTATION_FROM_ANTICIPATION		*
 ****************************************************************
 * Type or family adding_to_extensible is being added to genus  *
 * or community class_being_extended.				*
 *								*
 * Issue an expectation from each anticipation in cell h.	*
 * The key of h is an identifier, and the value is a list of	*
 * EXPECT_TABLE nodes, each holding an anticipation and		*
 * its visibility.						*
 *								*
 * List no_expect_from_anticipation holds identifiers whose     *
 * expectation should be suppressed.				*
 *								*
 * adding_to_extensible is the type being added, and 		*
 * class_being_extended is the genus or community that is	*
 * being extended.						*
 ****************************************************************/

#ifdef TRANSLATOR

PRIVATE TYPE*             adding_to_extensible;
PRIVATE CLASS_TABLE_CELL* class_being_extended;
PRIVATE STR_LIST*         no_expect_from_anticipation;

PRIVATE void do_expectation_from_anticipation(HASH2_CELLPTR h)
{
  TYPE_LIST *p;
  TYPE *t, *v, *adding;
  char *name;
  LIST *mark;
  TYPE_TAG_TYPE kind;

# ifdef DEBUG
    if(trace_defs > 1) trace_s(55);
# endif

  /*-------------------------------------------------------------------*
   * Get the id to expect, and check if its expectation is suppressed. *
   *-------------------------------------------------------------------*/

  name = h->key.str;
  if(str_member(name, no_expect_from_anticipation)) return;

  /*---------------------------------------------------------------*
   * Perform an expectation for each cell in h->val.list. But only *
   * do the anticipations that are visible to the current package. *
   * Anticipations made in the standard package will have a 	   *
   * visibility list of NIL.  They should be considered visible to *
   * all packages.						   *
   *---------------------------------------------------------------*/

  for(p = h->val.list; p != NIL; p = p->tail) {
    STR_LIST* who_sees = p->head.expect_table->visible_in;
    if(who_sees == NIL || str_memq(current_package_name, who_sees)) {

      bump_type(t = p->head.expect_table->type);

      /*----------------------------------------------------------*
       *** Try to make an expectation from anticipation name:t. ***
       *----------------------------------------------------------*/

      /*---------------------------------------------------------*
       * Get the extensible variable that occurs in t, if there  *
       * is just one, and put it in v.  (v will be NULL if there *
       * is no unique extensible variable in t.)		 *
       *---------------------------------------------------------*/

      v = extensible_var_in(t, class_being_extended);

#     ifdef DEBUG
        if(trace_defs > 1) {
	  trace_t(192, nonnull(name));
          trace_ty(t);
	  trace_t(193);
	  trace_ty(v);
	  tracenl();
        }
#     endif

      if(v != NULL && v != (TYPE *) 1) {

	/*--------------------------------------------------------------*
         * If adding a family to a community, it is possible that v is 	*
	 * a genus that contains the community.  In that case, get	*
	 * the type to unify with v. 					*
	 *--------------------------------------------------------------*/

	adding = adding_to_extensible;
	kind   = TKIND(find_u(adding));
	if(v->ctc->code == GENUS_ID_CODE && 
	   (kind == FAM_T || kind == FAM_VAR_T || 
	    kind == PRIMARY_FAM_VAR_T || kind == WRAP_FAM_VAR_T)) {
	  LPAIR lp;
	  lp = get_link_label_tm(v->ctc->num, find_u(adding)->ctc->num);
	  adding = fam_mem_t(adding, ctcs[lp.label1]->ty);
	}
	bump_type(adding);

	bump_list(mark = finger_new_binding_list());
	if(unify_u(&adding, &v, TRUE)) {
	  TYPE *cpyt;

	  /*----------------------------------------------------------*
	   * Restrict the domain of F(T) to the declared domain of F. *
	   *----------------------------------------------------------*/

	  if(restrict_families_t(t)) {
	    bump_type(cpyt = copy_type(t, 1));

	    /*----------------------------------------------------*
	     * Must undo bindings, to restore the global id table *
	     * before trying to do the expectation. 		  *
	     *----------------------------------------------------*/

	    undo_bindings_u(mark);

	    /*------------------------------------------------------------*
	     * Do the expectation, and report it. This expectation should *
	     * be deferred, since expectations are generated from 	  *
	     * anticipations inside extend declarations. 		  *
	     *------------------------------------------------------------*/

#	    ifdef DEBUG
	      if(trace_defs) {
		trace_t(499, name);
		trace_ty(cpyt);
		tracenl();
	      }
#	    endif

	    defer_expect_ent_id_p(name, cpyt, NULL, EXPECT_ATT, 0, 
				  current_line_number);
	    drop_type(cpyt);
	  } /* end if(restricted...) */
	} /* end if(unify...) */

	undo_bindings_u(mark);
	drop_list(mark);
	drop_type(adding);

      } /* end if(v != NULL...) */

      drop_type(t);

    } /* end if(who_sees == NULL...) */

  } /* end for(p = ...) */
}

#endif


/****************************************************************
 *			RECORD_MEMBER_FOR_NONEMPTY		*
 ****************************************************************
 * record_member_for_nonempty(ctci, ctcj) records that ctcj is 	*
 * a member of ctci, marking the nonempty flag of ctci if 	*
 * appropriate.  If record_member_for_nonempty sets the 	*
 * nonempty flag, it returns true.  Otherwise it returns false.	*
 ****************************************************************/

#ifdef TRANSLATOR
PRIVATE Boolean 
record_member_for_nonempty(CLASS_TABLE_CELL *ctci, 
			   CLASS_TABLE_CELL *ctcj)
{
  LPAIR lbl;
  int codei, codej;

  lbl = get_link_label_tm(ctc_num(ctci), ctc_num(ctcj));
  codei = ctci->code;
  codej = ctcj->code;
  switch(codej) {
    case TYPE_ID_CODE:
      goto mark_nonempty;

    case FAM_ID_CODE:
      if(codei == COMM_ID_CODE) goto mark_nonempty;
      if(ctcs[lbl.label1]->nonempty) goto mark_nonempty;
      return FALSE;

    case PAIR_CODE:
    case FUN_CODE:
      if(ctcs[lbl.label1]->nonempty && ctcs[lbl.label2]->nonempty) {
        goto mark_nonempty;
      }
      return FALSE;

    case GENUS_ID_CODE:
      if(ctcj->nonempty) goto mark_nonempty;
      return FALSE;

    case COMM_ID_CODE:
      if(!ctcj->nonempty) return FALSE;
      if(codei == COMM_ID_CODE) goto mark_nonempty;
      if(ctcs[lbl.label1]->nonempty) goto mark_nonempty;
      return FALSE;
  }

mark_nonempty:

# ifdef DEBUG
    if(trace_classtbl) {
      fprintf(TRACE_FILE, "Marking ");
      trace_ty( ctci->ty);
      fprintf(TRACE_FILE, " nonempty\n");
    }
# endif

  ctci->nonempty = 1;
  ctci->mem_num = ctc_num(ctcj);
  return TRUE;
}
#endif


/****************************************************************
 *			EXTEND_TM				*
 ****************************************************************
 * Adds members of list l to the genus or community I		*
 * whose class_table data entry is c, if possible.  		*
 *								*
 * Each member of list l is a CLASS_UNION_CELL describing what  *
 * is to be added.  See the description of classUnion in 	*
 * parser.y for a description of the members of l.		*
 *								*
 * ex is TRUE if this is a genuine extension (from a 		*
 * declaration of the form Genus I => T %Genus, for example), 	*
 * and is FALSE if this is part of the original list for I.	*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

#ifdef TRANSLATOR

void extend_tm(LIST *l, CLASS_TABLE_CELL *c, Boolean ex, MODE_TYPE *mode)
{
  LIST *p;

  bump_list(l);

  /*---------------*
   * Error checks. *
   *---------------*/

  if(c == NULL) {
#   ifdef TRANSLATOR
      semantic_error(UNK_ID_ERR, 0);
#   endif
    goto out;
  }

  if(ex && c->extensible != 2) {
#   ifdef TRANSLATOR
      semantic_error1(NOT_EXTENS_ERR, display_name(c->name), 0);
#   endif
    goto out;
  }

  /*------------------------------------*
   * Add the ancestors and descendants. *
   *------------------------------------*/

  for(p = l; p != NIL; p = p->tail) {
    extend1_tm(p->head.cuc, c, TRUE, mode);
  }

out:
  drop_list(l);
}

#endif


/****************************************************************
 *			CHECK_EXTEND_CONSISTENCY		*
 ****************************************************************
 * check_extend_consistency returns TRUE if adding mem to the 	*
 * genus or community whose table entry is container is		*
 * sensible, and FALSE if not.  If not reasonable, an 		*
 * error messages is printed.  mem_ctc is the class table entry *
 * for mem.							*
 ****************************************************************/
 
PRIVATE Boolean 
check_extend_consistency(CLASS_UNION_CELL *mem, 
			 CLASS_TABLE_CELL *mem_ctc, 
			 CLASS_TABLE_CELL *container)
{
  int mem_tok       = mem->tok;
  int container_tok = MAKE_TOK(container->code);

  /*-----------------------------------------------*
   * A with clause is not allowed in an extension. *
   *-----------------------------------------------*/

# ifdef TRANSLATOR
    if(mem->withs != NIL) {
      semantic_error(WITH_IN_CLASS_DCL_ERR, 0);
      return FALSE;
    }
# endif

  /*------------------------------------------------*
   * "Curried" parts are not allowed in extensions. *
   *------------------------------------------------*/

# ifdef TRANSLATOR
    if(mem_tok == TYPE_LIST_TOK) {
      syntax_error(CURRIED_EXTEND_ERR, 0);
      return FALSE;
    }
# endif

  /*---------------------------------------------------------*
   * Only a family or community can be added to a community. *
   *---------------------------------------------------------*/

  if(container_tok == COMM_ID_TOK && 
     mem_tok != FAM_ID_TOK && 
     mem_tok != COMM_ID_TOK) {
#   ifdef TRANSLATOR
      bad_extens_err(mem, container, "Adding non-family to community.");
#   endif
    return FALSE;
  }

  /*---------------------------------------------------------*
   * When adding a family or community to a community,	     *
   * opacity must match.				     *
   *---------------------------------------------------------*/

  if(container_tok == COMM_ID_TOK &&
     mem_ctc->opaque != container->opaque) {
#   ifdef TRANSLATOR
      bad_extens_err(mem, container, "One is transparent, the other is not.");
#   endif
    return FALSE;
  }

  /*--------------------------------------------------------*
   * A family or community cannot be added to a genus.      *
   *--------------------------------------------------------*/

  if(container_tok == GENUS_ID_TOK &&
          (mem_tok == FAM_ID_TOK || mem_tok == COMM_ID_TOK)) {
#   ifdef TRANSLATOR
      bad_extens_err(mem, container, "Adding family to genus");
#   endif
    return FALSE;
  }

  /*---------------------------------------*
   * Functions cannot be beneath anything. *
   *---------------------------------------*/

  if(mem->CUC_TYPE != NULL && TKIND(mem->CUC_TYPE) == FUNCTION_T) {
#   ifdef TRANSLATOR
      semantic_error(FUN_GEN_MEM_ERR, 0);
#   endif
    return FALSE;
  }

  /*---------------------------------------------------------*
   * It is not allowed to add an old thing to an old thing,  *
   * unless it was already there.  			     *
   *---------------------------------------------------------*/

  if(mem_ctc->closed && container->closed) {
    Boolean same          = (mem_tok == container_tok && 
			     mem->name == container->name);
    Boolean already_there = ancestor_tm(container, get_ctc_tm(mem->name));

    if(!same && !already_there) {
#     ifdef TRANSLATOR
        semantic_error2(OLD_EXTENSION_ERR, display_name(container->name), 
		        display_name(mem_ctc->name), 0);
#     endif
      return FALSE;
    }
  }

  return TRUE;
}


/****************************************************************
 *			GET_EXTEND_CTC				*
 ****************************************************************
 * get_extend_ctc returns the class table entry for the thing	*
 * described by cell mem, which is to be added to some genus	*
 * or community container.  The return value is NULL if no	*
 * table entry can be found.  In that case, an error message	*
 * is printed.							*
 *								*
 * When mem holds a wrapped species or family, its token is 	*
 * modified to GENUS_ID_TOK or COMM_ID_TOK, so that it will	*
 * be treated like a genus or community.			*
 *								*
 * label is set to the label information in mem.		*
 *								*
 * Global adding_to_extensible is also set to the ty entry of   *
 * the result table cell, or to the type being added, 		*
 * when the result is not NULL.					*
 ****************************************************************/

PRIVATE CLASS_TABLE_CELL* 
get_extend_ctc(CLASS_UNION_CELL *mem, CLASS_TABLE_CELL *container, 
	       LPAIR *label)
{
  CLASS_TABLE_CELL* result;

  int mem_tok = mem->tok;
  label->label1 = label->label2 = 0;    /* default */

  /*------------------------------------------------------------*
   * If mem is not a TYPE_ID_TOK, then its name indicates what	*
   * is being added.						*
   *------------------------------------------------------------*/

  if(mem_tok != TYPE_ID_TOK) {
    result = get_ctc_tm(mem->name);
    if(result != NULL) {
#     ifdef TRANSLATOR
        bump_type(adding_to_extensible = copy_type(result->ty, 0));
#     endif
    }
  }

  /*--------------------------------------------------*
   * When adding a type, we have several options.     *
   *--------------------------------------------------*/

  else { /* mem_tok == TYPE_ID_TOK */
    TYPE_TAG_TYPE kind;
    TYPE* ty = find_u(mem->CUC_TYPE);

#   ifdef TRANSLATOR
      bump_type(adding_to_extensible = copy_type(ty, 0));
#   endif
    replace_null_vars(&ty);

    kind = TKIND(ty);
    switch(kind) {

      case FAM_MEM_T:

	/*------------------------------------------------------*
	 * Adding a family to a genus.  Get the family and the  *
         * link label.						*
	 *------------------------------------------------------*/

#       ifdef TRANSLATOR
	  if(!type_id_or_var(ty->TY1)) {
	    bad_cu_mem_err(ty, mem->line);
	    return NULL;
	  }
          else 
#       endif
        {
	 result = ty->TY2->ctc;
	 label->label1 = ctc_num(ty->TY1->ctc);
	}
	break;

      case FUNCTION_T:
      case PAIR_T:

	/*--------------------------------------------------------*
	 * Adding a pair or function to a genus.  Get the pair or *
	 * function ctc, and the link labels. 			  *
	 *--------------------------------------------------------*/

#       ifdef TRANSLATOR
	  if(!good_class_union_pair(ty)) {
	    bad_cu_mem_err(ty, mem->line);
	    return NULL;
	  }
#       endif

	/*-----------------------------------------------*
	 * Generate a declaration for PAIR@ or FUNCTION@ *
	 *-----------------------------------------------*/

	if(kind == PAIR_T) {
#         ifdef TRANSLATOR
 	    id_tb(std_type_id[PAIR_TYPE_ID], FAMILY_DCL_I);
#         endif
	  result = Pair_ctc;
	}
	else {
	  id_tb(std_type_id[FUNCTION_TYPE_ID], FAMILY_DCL_I);
	  result = Function_ctc;
	}
	label->label1 = ctc_num(ty->TY1->ctc);
	label->label2 = ctc_num(ty->TY2->ctc);
	break;

      case TYPE_ID_T:
      case FAM_T:

        /*----------------------------------------------------------*
         * We might have an unknown thing, indicated by a	    *
         * constructor and type ().  That should be caught.	    *
         *----------------------------------------------------------*/

        result = ty->ctc;
#       ifdef TRANSLATOR
	  if(result->num == Hermit_num && mem->name != NULL) {
	    syntax_error1(UNKNOWN_ID_ERR, display_name(mem->name), 0);
	  }
#       endif
	break;

      case WRAP_TYPE_T:
      case WRAP_FAM_T:
   
        /*------------------------------------------------------*
	 * Treat this like a genus or community. Modify the	*
         * token in cell mem, and install the name.		*
         *------------------------------------------------------*/

        mem->tok  = (kind == WRAP_TYPE_T) ? GENUS_ID_TOK : COMM_ID_TOK;
        result    = mem->CUC_TYPE->ctc;
        mem->name = result->name;
        break;

      default: 
	result =  NULL;

    } /* end switch */
  } /* end else (mem_tok == TYPE_ID_TOK) */

  if(result == NULL) {
#   ifdef TRANSLATOR
      bad_extens_err(mem, container, "I don't recognize what is to be added");
      SET_TYPE(adding_to_extensible, NULL);
#   endif
  }

  return result;
}


/******************************************************************
 *			EXTEND1_TM				  *
 ******************************************************************
 * Add mem as a descendant of container.  mem is given as a 	  *
 * class-union-cell,  as would be created in parsing the 	  *
 * right-hand side of a genus or community declaration.		  *
 * Container is given by a pointer to its class table entry.  	  *
 *								  *
 * Generate a relate dcl if gen is true.			  *
 *								  *
 * Create expectations from anticipations if appropriate.	  *
 * List no_expects is a list of identifiers (as strings) that     *
 * should not be expected.					  *
 *								  *
 * mode		is the mode of the declaration.			  *
 *		It is a safe pointer: it will not be kept	  *
 *		longer than the lifetime of this function call.	  *
 ******************************************************************/

void extend1_tm(CLASS_UNION_CELL *mem, CLASS_TABLE_CELL *container, 
		Boolean gen, MODE_TYPE *mode)
{
  CLASS_TABLE_CELL *main;
  LPAIR label;
  int mem_tok, container_tok;

# ifdef TRANSLATOR
    if(local_error_occurred || mem == NULL || container == NULL) return;
# else
    if(mem == NULL || container == NULL) return;
#endif

  /*-----------------------------------------------------------------*
   * Get the ctc (main) for the thing to be added to container and   *
   * the label of the link from c to main.  For example, if we are   *
   * doing       						     *
   *								     *
   *  Extend G by F(H).						     *
   *								     *
   * then mem is F(H), main is set to the table entry for F, and     *
   * label is set so that label.label1 is the number of the table    *
   * entry for H.						     *
   *			             				     *
   * Also get the type (adding_to_extensible) that should            *
   * be used to replace variables that range over container in       *
   * anticipations, when changing the anticipations to expectations. *
   *-----------------------------------------------------------------*/

  main = get_extend_ctc(mem, container, &label);
  if(main == NULL) return;

  /*-------------------------------------------------------------*
   * Check consistency. We can only add a family or community to *
   * a community.  We cannot add a family or community to a      *
   * genus.  (We will add a family member, holding link label    *
   * info, rather than a raw family.)				 *
   *-------------------------------------------------------------*/

  if(!check_extend_consistency(mem, main, container)) return;

# ifdef TRANSLATOR
    class_being_extended = container;
# endif

  altered_hierarchy     = TRUE;
  container->is_changed = 1;
  mem_tok               = mem->tok;
  container_tok         = MAKE_TOK(container->code);

  /*----------------------------------------------*
   * Check whether there is any extension to do.  *
   *----------------------------------------------*/

  if(!((mem_tok == container_tok && mem->name == container->name) ||

      ((container_tok == COMM_ID_TOK || mem_tok == GENUS_ID_TOK) &&
       ancestor_tm(container, get_ctc_tm(mem->name))) ||

       (mem_tok == TYPE_ID_TOK && 
        (TKIND(mem->CUC_TYPE) == TYPE_ID_T || TKIND(mem->CUC_TYPE) == FAM_T) &&
        ancestor_tm(container, mem->CUC_TYPE->ctc)))) {

    /*-----------------------------------------------*
     * If we get here, then this is a new extension. *
     * Install mem as a descendant of container.     *
     *-----------------------------------------------*/

#   ifdef TRANSLATOR

      LPAIR old_labels, new_labels;
      Boolean old_ancestor;

      /*----------------------------------------------------*
       * Get the initial link labels, for comparison below. *
       *----------------------------------------------------*/

      old_ancestor = ancestor_tm(container, main);
      if(old_ancestor) {
	old_labels = get_link_label_tm(container->num, main->num);
      }
#   endif

#   ifdef DEBUG
      if(trace_classtbl) {
	trace_s(57, container->name, main->name);
	print_lpair(label);
      }
#   endif

    /*---------------------*
     * Copy all ancestors. *
     *---------------------*/

    copy_ancestors_tm(main, container, label);

#   ifdef TRANSLATOR

      /*-------------------------------*
       * Mark nonempty if appropriate. *
       *-------------------------------*/

      if(!(container->nonempty)) {
        record_member_for_nonempty(container, main);
      }

      /*-----------------------------------------*
       * Generate the relate dcl if appropriate. *
       *-----------------------------------------*/

      if(main_context != IMPORT_CX && gen && gen_code) {
	generate_relate_dcl_g(container, main);
      }

      /*-----------------------------------*
       * Report the change, if called for. *
       *-----------------------------------*/

      new_labels = get_link_label_tm(container->num, main->num);
      if(report_extends && 
	 (!old_ancestor || 
	  new_labels.label1 != old_labels.label1 ||
	  new_labels.label2 != old_labels.label2)) {
        report_dcl_aux_p(container->name, EXTEND_E, 0, NULL, NULL, NULL, 
			 main, new_labels);
      }

#   endif
  }

  /*------------------------------------------------------------*
   * Generate expectations from anticipations, if appropriate. 	*
   * (Don't add if suppressed by mode, or it the thing being	*
   * added is an extensible genus or community.) 	 	*
   *								*
   * Note that this should be done even if we are not doing	*
   * an extension, since these expectations are local.		*
   *------------------------------------------------------------*/

# ifdef TRANSLATOR
    if(!has_mode(mode, NO_EXPECT_MODE) && !(main->extensible)) {
      no_expect_from_anticipation = get_mode_noexpects(mode);
      scan_hash2(anticipate_id_table, do_expectation_from_anticipation);
    }
    SET_TYPE(adding_to_extensible, NULL);
# endif

}


/****************************************************************
 *			TRY_EXTEND_TM				*
 ****************************************************************
 * Try to extend upper by lower, each given by a name.  If the  *
 * names do not refer to the correct kind of thing, then	*
 * complain.							*
 *								*
 * Parameter arg tells the argument of lower, when it is a	*
 * family or community.  If lower is not a family or community, *
 * arg should be NULL.						*
 *								*
 * Generate a relate dcl for each extension is gen is true.	*
 *								*
 * mode is the mode of the declaration.				*
 ****************************************************************/

#ifdef TRANSLATOR

void try_extend_tm(char *upper, char *lower, TYPE *arg, Boolean gen,
		   MODE_TYPE *mode)
{
  CLASS_TABLE_CELL* upper_ctc = get_ctc_tm(upper);
  CLASS_TABLE_CELL* lower_ctc = get_ctc_tm(lower);
  if(upper_ctc == NULL) {
    semantic_error1(NO_SUCH_TYPE_ERR, quick_display_name(upper), 0);
  }
  else if(lower_ctc == NULL) {
    semantic_error1(NO_SUCH_TYPE_ERR, quick_display_name(lower), 0);
  }
  else extend1ctc_tm(lower_ctc, arg, upper_ctc, gen, mode);
}
#endif

/****************************************************************
 *			TRY_EXTENDS_TM				*
 ****************************************************************
 * Try to extend each member of list L by lower.  L is a list   *
 * of strings and lower is a string.  Complain if something is  *
 * wrong.							*
 *								*
 * Each name in list L, as well as lower, is converted to its	*
 * internal name as given by new_name.				*
 *								*
 * Parameter arg tells the argument of lower, when it is a	*
 * family or community.  If lower is not a family or community, *
 * arg should be NULL.						*
 *								*
 * Generate a relate dcl for each extension is gen is true.	*
 *								*
 * mode is the mode of the declaration.				*
 ****************************************************************/

#ifdef TRANSLATOR

void try_extends_tm(STR_LIST *L, char *lower, TYPE *arg, Boolean gen,
		    MODE_TYPE *mode)
{
  STR_LIST *p;

  for(p = L; p != NIL; p = p->tail) {
    try_extend_tm(new_name(p->head.str, FALSE), 
		  new_name(lower, FALSE), arg, gen, mode);
  }
}

#endif

/****************************************************************
 *			EXTEND1CTC_TM				*
 ****************************************************************
 * If arg is NULL, add mem as a descendant of container.  	*
 * If arg is not NULL, add mem(arg) as a descendant of		*
 * container.  (In the latter case,  mem must be a family or	*
 * community.)  Generate a relate dcl if gen is true.		*
 ****************************************************************/

void extend1ctc_tm(CLASS_TABLE_CELL *mem, TYPE *arg, 
		   CLASS_TABLE_CELL *container, Boolean gen, 
		   MODE_TYPE *mode)
{
  CLASS_UNION_CELL *cuc;

  cuc          = allocate_cuc();
  cuc->tok     = (arg == NULL) ? MAKE_TOK(mem->code) : TYPE_ID_TOK;
  cuc->name    = mem->name;
  cuc->special = 0;
  bump_type(cuc->CUC_TYPE = 
    (arg == NULL) ? mem->ty : fam_mem_t(mem->ty, arg));
  cuc->CUC_ROLE = NULL;
  cuc->withs   = NIL;
  cuc->line    = 0;
  extend1_tm(cuc, container, gen, mode);
  free_cuc(cuc);
}


/*============================================================
 *		ADDING SPECIES AND FAMILIES
 *===========================================================*/


/****************************************************************
 *			CHECK_OLD_TF				*
 ****************************************************************
 * This function is used by both the interpreter and the 	*
 * compiler, but has slightly different behaviors for the two.  *
 *								*
 * For the compiler:						*
 *								*
 * We are either defining or expecting species or family id.    *
 *								*
 * If arg is NULL, then id is a species.  If arg is not NULL,	*
 * then id is a family, and arg is the argument in the		*
 * definition or expectation.					*
 *								*
 * In the case where id is a family, opaque is true if the	*
 * family is marked opaque.					*
 *								*
 * If exp is TRUE, then this is an expectation.			*
 * If exp is FALSE, then this is a definition.  		*
 *								*
 * Cell c is the former class table cell for id, or is a new    *
 * cell if this is a new species or family.  If this is a new   *
 * species or family, then c->code is 0 or is 			*
 * UNKNOWN_CLASS_ID_CODE.  					*
 *								*
 * Check that this definition is consistent with what		*
 * information is recorded in cell c.  If not, then complain	*
 * and return bad_type.  If id was formerly expected, and the   *
 * new definition or expectation of id is consistent with the   *
 * old one, then return c->ty.  If this is a new id, then	*
 * return NULL.							*
 *								*
 * For the interpreter:						*
 *								*
 * Always report that a duplicate species or family definition  *
 * has been encountered, and return NULL.			*
 ****************************************************************/

PRIVATE TYPE* 
check_old_tf(char *id, TYPE *arg, int tok, CLASS_TABLE_CELL *c, 
	     Boolean exp, Boolean opaque, Boolean partial)
{
  if(c->code == 0 || c->code == UNKNOWN_CLASS_ID_CODE) return NULL;

  else {
#   ifdef TRANSLATOR
      int c_exp;

      /*--------------------------------------------------------*
       * If c was due to a definition and we are doing another  *
       * definition, then there is trouble.  			*
       *--------------------------------------------------------*/

      c_exp = c->expected;
      if(!c_exp && !exp) {
        semantic_error1(ID_DEFINED_ERR, display_name(id), 0);
        return bad_type;
      }

      /*------------------------------------------------------*
       * If this definition or expectation is for a different *
       * kind of thing than c describes, there is trouble.    *
       *------------------------------------------------------*/

      if(MAKE_TOK(c->code) != tok) {
	semantic_error1(EXPECTATION_ERR, display_name(id), 0);
	err_print(EXPECTED_BUT_SEE_ERR,
		  class_name(MAKE_TOK(c->code)), class_name(tok));
        return bad_type;
      }

      /*--------------------------------------------------*
       * Check that the domain and opacity of a family is *
       * consistent. 					  *
       *--------------------------------------------------*/

      if(arg != NULL) {
	int ov = half_overlap_u(c->CTC_REP_TYPE->TY1, arg);
	if(ov != EQUAL_OR_CONTAINED_IN_OV) {
	  semantic_error(FAM_DOMAIN_MISMATCH_ERR, 0);
	}
        if(c->opaque != opaque) {
	  semantic_error1(FAM_OPAQUE_MISMATCH_ERR, display_name(id), 0);
	}
      }

      /*------------------------------------------------* 
       * Check that partial modes are consistent.	*
       *------------------------------------------------*/

      if(c->partial != partial) {
        semantic_error1(PARTIAL_MISMATCH_ERR, display_name(id), 0);
      }

      /*---------------------------------------------------------*
       * If this is a definition, then note that the expectation *
       * has been met, and just return the ty entry that was	 *
       * already here.			                         *
       *---------------------------------------------------------*/

      if(!exp) c->expected = 0;

      /*-----------------------------------------------------------*
       * If this is an expectation, and we are importing a package *
       * then indicate that an expectation has been done in an     *
       * import.                                                   *
       *-----------------------------------------------------------*/

      else if(main_context == IMPORT_CX || main_context == INIT_CX) {
	c->expected = 2;
      }

      return c->ty;

    /*------------------------------------------------------------*
     * The interpreter never does species or family expectations. *
     * So if we are compiling the interpreter, this must be a	  *
     * duplicate definition.			  		  *
     *------------------------------------------------------------*/

#   else
      package_die(172, id);
      return NULL_T;
#   endif
  }
}


/****************************************************************
 *			INSTALL_TF_TM				*
 ****************************************************************
 * Install species or family 'name', whose class_id_table entry *
 * is given by c.  						*
 *								*
 * gen_name is true if a type or family declaration should be	*
 * generated in the genf file for 'name'.			*
 *								*
 * tok is TYPE_ID_TOK or FAM_ID_TOK, telling whether 'name' is  *
 * a species or family.						*
 *								*
 * arg is null for a type, the domain for a family.		*
 *								*
 * If this is a family, then opaque is 1 if this is an opaque   *
 * family, and 0 if not.					*
 *								*
 * If the main context is EXPORT_CX, then write this identifier *
 * into the index file.						*
 *								*
 * The returned value is the species or family that was just    *
 * created, or NULL if creation failed.				*
 ****************************************************************/

PRIVATE TYPE *
install_tf_tm(CLASS_TABLE_CELL *c, char *name, int tok, 
	      TYPE *arg, Boolean gen_name, Boolean opaque)
{
  int kind;
  TYPE *ty;
# ifdef TRANSLATOR
    int gen_instr;
# endif

  /*------------------------*
   * Build the table entry. *
   *------------------------*/

  if(next_class_num >= ctcs_size) {
    if(ctcs_size == MAX_NUM_SPECIES) {
      if(ignore_class_table_full) return NULL;
      else die(23);
    }
    realloc_classtbl(&ctcs, &ctcs_size, MAX_NUM_SPECIES);
  }
  ctcs[next_class_num] = c;
  c->code              = MAKE_CODE(tok);
  c->name              = id_tb0(name);
  c->constructors      = NIL;
  c->num               = next_class_num++;
  c->var_num           = -1;
# ifdef TRANSLATOR
    c->package         = current_package_name;
# endif
  if(arg != NULL) {
    bmp_type(c->CTC_REP_TYPE = pair_t(copy_type(arg, 0), NULL));
    c->opaque = opaque;
  }
  else c->CTC_REP_TYPE = NULL;

# ifdef DEBUG
    if(trace_classtbl) {
      trace_s(54, name, toint(c->num));
    }
# endif

  /*-----------------------------------------------*
   * Get the kind and generate a name declaration. *
   *-----------------------------------------------*/

  if(tok == TYPE_ID_TOK) {
    kind = TYPE_ID_T;
#   ifdef TRANSLATOR
      gen_instr = NEW_SPECIES_DCL_I;
#   endif
  }
  else {
    kind = FAM_T;
#   ifdef TRANSLATOR
      gen_instr = opaque ? NEW_OPAQUE_FAMILY_DCL_I 
			 : NEW_TRANSPARENT_FAMILY_DCL_I;
#   endif
  }
# ifdef TRANSLATOR
    if(gen_name && gen_code) id_tb(name, gen_instr);
# endif

  /*------------------------*
   * Fill in the type of c. *
   *------------------------*/

  bmp_type(c->ty = ty = new_type(kind, NULL_T));
  ty->ctc = c;
# ifdef MACHINE
    SET_TYPE(ty, type_tb(ty));
# endif

  /*------------------------------------------------------------*
   * If this is a family, put it beneath OPAQUE or TRANSPARENT.	*
   * But don't do this when building standard species and	*
   * families, since that is already done for them.		*
   *------------------------------------------------------------*/

  if(!building_standard_types && arg != NULL) {
    CLASS_TABLE_CELL* top_ctc = opaque ? OPAQUE_ctc : TRANSPARENT_ctc;
    extend1ctc_tm(c, NULL, top_ctc, gen_name && gen_code, NULL);  
  }

# ifdef TRANSLATOR

    /*--------------------------------------------------*
     * Write the id into index_file, if in export part. *
     *--------------------------------------------------*/

    if(doing_export() && index_file != NULL) {
      fprintf(index_file, "%c%s\n", (arg == NULL) ? 's' : 'f', name);
    }
# endif

  return ty; 
}


/****************************************************************
 *			ADD_TF_TM				*
 ****************************************************************
 * Add species or family 'name', with token 'tok' (TYPE_ID_TOK  *
 * or FAM_ID_TOK).  If gen_name is true, generate a declaration *
 * of 'name' into the output code.  Return a type node for	*
 * species or family 'name'.					*
 *								*
 * If 'name' is a family, then arg is the argument of name its 	*
 * definition.  For example, if family F defined by	        *
 *								*
 *   Species F(A) = B.						*
 *								*
 * then arg is A.  If 'name' is a species, then arg is NULL.	*
 *								*
 * If this is a family, then opaque is 1 if this is an opaque   *
 * family, and 0 if not.					*
 *								*
 * Parameter partial is true if this is a partial species or	*
 * family, to be marked by setting the partial field in the 	*
 * table.							*
 *								*
 * The returned pointer points to the ty entry in the table, so *
 * it is not necessary for the caller to drop the reference 	*
 * count.							*
 ****************************************************************/

TYPE* add_tf_tm(char *name, TYPE *arg, Boolean gen_name, Boolean opaque,
		Boolean partial)
{
  CLASS_TABLE_CELL *c;
  int tok = (arg == NULL) ? TYPE_ID_TOK : FAM_ID_TOK;

# ifdef DEBUG
    if(trace_classtbl) {
      if(tok == TYPE_ID_TOK) {trace_s(58, name);}
      else {trace_s(59, name);}
    }
# endif

  c = get_new_ctc_tm(name);

  /*------------------------------------------------------------------*
   * Check for a cell that has been placed by an earlier expectation. *
   * We need to check consistency between this definition and the     *
   * expectation.						      *
   *------------------------------------------------------------------*/

  {TYPE* result = check_old_tf(name, arg, tok, c, FALSE, opaque, partial);
   if(result != NULL) return result;
  }

  return install_tf_tm(c, name, tok, arg, gen_name, opaque);
}


/****************************************************************
 *			EXPECT_TF_TM				*
 ****************************************************************
 * Expect species or family id.  Arg is null for a species   	*
 * and is the domain type for a family.  Return the species or	*
 * family created, or NULL if none could be created.		*
 *								*
 * If this is a family, then opaque is 1 if this is an opaque   *
 * family, and 0 if not.					*
 *								*
 * Parameter partial is true if this is a partial species or	*
 * family, to be marked by setting the partial field in the 	*
 * table.							*
 ****************************************************************/

#ifdef TRANSLATOR

TYPE* expect_tf_tm(char *id, TYPE *arg, Boolean opaque, Boolean partial) 
{
  CLASS_TABLE_CELL *c;
  int tok = (arg == NULL) ? TYPE_ID_TOK : FAM_ID_TOK;

# ifdef DEBUG
    if(trace_classtbl) {
      if(arg == NULL) {trace_s(60, id);}
      else {trace_s(61, id);}
    }
# endif

  /*---------------------------------------------*
   * Check if id is already in use for entities. *
   *---------------------------------------------*/

  if(check_for_ent_id(id, 0)) return NULL;

  /*-----------------------*
   * Get a new ctc for id. *
   *-----------------------*/

  c = get_new_ctc_tm(id);

  /*-------------------------------------------------------------*
   * If there was a previous expectation, check for consistency. *
   *-------------------------------------------------------------*/

  {TYPE* result = check_old_tf(id, arg, tok, c, TRUE, opaque, partial);
   if(result != NULL) return result;
  }

  /*---------------------------*
   * Install the expected type *
   *---------------------------*/

  c->expected = (main_context == IMPORT_CX || main_context == INIT_CX) ? 2 : 1;
  return install_tf_tm(c, id, tok, arg, TRUE, opaque);
}

#endif


/*===============================================================
 *		CHECKING AND PATCHING THE HIERARCHY
 *==============================================================*/

/****************************************************************
 *			 PATCH_HIERARCHY			*
 ****************************************************************
 * Perform patches of the genus/community hierarchy.  The	*
 * following are done.						*
 *								*
 * If A <= B and A <= C, where A, B and C are all different,    *
 * then we ensure that A <= B & C (the intersection of B and C) *
 * if possible.							*
 ****************************************************************/

PRIVATE void patch_hierarchy(void)
{
  int i, j, k, meet_jk_num;
  CLASS_TABLE_CELL *ctci, *ctcj, *ctck, *meet_jk;

  for(i = 1; i < next_class_num; i++) {
    ctci = ctcs[i];
    for(j = 1; j < next_cg_num; j++) {
      ctcj = vctcs[j];
      if(ctci != ctcj && ancestor_tm(ctcj, ctci)) {
        for(k = 1; k < next_cg_num; k++) {
	  ctck = vctcs[k];
	  if(ctck != ctci && ctck != ctcj && ancestor_tm(ctck, ctci)) {
	    meet_jk_num = full_get_intersection_tm(ctcj, ctck);
	    if(meet_jk_num != -1) {
	      meet_jk = ctcs[meet_jk_num];
	      if(meet_jk != ctcj && meet_jk != ctck && meet_jk != ctci
	         && !ancestor_tm(meet_jk, ctci) && meet_jk->extensible > 1) {

#               ifdef DEBUG
		  if(trace_classtbl) {
		    trace_s(101,
			    nonnull(ctci->name), nonnull(ctcj->name),
			    nonnull(ctck->name), nonnull(meet_jk->name));
		  }
#               endif

	        copy_ancestors_tm(ctci, meet_jk, NOCHECK_LP);

#               ifdef TRANSLATOR
		  if(report_extends) {
		    report_dcl_aux_p(meet_jk->name, EXTEND_E, 0, NULL, NULL,
				     NULL, ctci, NULL_LP);
		  }
#               endif
	      }
	    }
	  }
	}
      }
    }
  }
}


/****************************************************************
 *			 CHECK_FOR_CYCLES_TM			*
 ****************************************************************
 * Check that there are no cycles in the genus/community	*
 * hierarchy.  Print a message if an cycle is found.		*
 ****************************************************************/

PRIVATE void check_for_cycles_tm(void)
{
  int i;

# ifdef DEBUG
    if(trace_classtbl) trace_s(66);
# endif

  /*---------------------------------------------------------------*
   * There is a cycle if something is a proper ancestor of itself. *
   *---------------------------------------------------------------*/

  for(i = 1; i < next_class_num; i++) {
    if(ancestor_tm(ctcs[i], ctcs[i])) {
#     ifdef DEBUG
        if(trace) trace_s(67, i);
#     endif
      semantic_error1(CYCLE_ERR, display_name(ctcs[i]->name), 0);
    }
  }
}


/****************************************************************
 *			FINISH_CLOSURE				*
 ****************************************************************
 * Mark all entries in the class table as closed and not	*
 * recently changed.						*
 ****************************************************************/

PRIVATE void finish_closure(void)
{
  int i;
  for(i = 1; i < next_class_num; i++) {
    ctcs[i]->closed = TRUE;
    ctcs[i]->is_changed = FALSE;
  }
  altered_hierarchy = FALSE;
}

/****************************************************************
 *			CLOSE_CLASSES_P				*
 ****************************************************************
 * Check that the hierarchy is ok, and build a join table.  	*
 * This is called at the end of an extension.			*
 *								*
 * XREF: 							*
 *  Called by parser.y at the end of an extension.		*
 * 								*
 *  Called by error/t_compl.c to extend the extensible classes  *
 *  for completeness check.					*
 *								*
 *  Called by classes/type.c to close the standard table.	*
 *								*
 *  Called by intrprtr/package.c(read_package) at the end of    *
 *  an extension.						*
 ****************************************************************/

void close_classes_p(void)
{
# ifdef DEBUG
    if(trace_classtbl) trace_s(44);
# endif

  if(altered_hierarchy) {

    /*--------------------------------------------------------*
     * Check for a cycle and for a non-associative hierarchy. *
     * Then build the join table.			      *
     *--------------------------------------------------------*/

    patch_hierarchy();
    check_for_cycles_tm();
    check_associativity_tm();
    install_joins();
  }

  finish_closure();

# ifdef TRANSLATOR
#   ifdef DEBUG
      if(trace_classtbl > 1) {
        print_type_table();
	show_meet_table();
	show_join_table();
      }
#   endif
# endif

}

#ifdef DEBUG
/****************************************************************
 *			PRINT_CTC				*
 ****************************************************************
 * Print class table cell c on TRACE_FILE.			*
 ****************************************************************/

PRIVATE void print_ctc(CLASS_TABLE_CELL *c)
{
  int j;

  trace_s(8, c->name, c->num, c->var_num,
          MAKE_TOK(toint(c->code)),
          toint(c->extensible), toint(c->expected), toint(c->opaque));
  fprintf(TRACE_FILE, "type = "); 
  trace_ty(c->ty); tracenl();
  fprintf(TRACE_FILE, "ancestors: ");
  for(j = 0; j < ANCESTORS_SIZE; j++) {
    fprintf(TRACE_FILE, " %lx", c->ancestors[j]);
  }
  tracenl();
}


/******************************************************************
 *			PRINT_TYPE_TABLE			  *
 ******************************************************************
 * print_type_table() prints the contents of table class_id_table. *
 ******************************************************************/

PRIVATE void print_type_cell(HASH2_CELLPTR h)
{
  print_ctc(h->val.ctc);
  tracenl();
}

/*---------------------------------------------------------*/

void print_type_table()
{
  trace_s(9);
  scan_hash2(class_id_table, print_type_cell);
}


/****************************************************************
 *			PRINT_LPAIR				*
 ****************************************************************
 * Print LPAIR lp on TRACE_FILE, in the form label:(l1,l2).     *
 ****************************************************************/

void print_lpair(LPAIR lp)
{
  fprintf(TRACE_FILE, "label:(");
  if(lp.label1 <= 0) fprintf(TRACE_FILE, "%d,", lp.label1);
  else fprintf(TRACE_FILE, "%s,", ctcs[lp.label1]->name);
  if(lp.label2 <= 0) fprintf(TRACE_FILE, "%d)\n", lp.label2);
  else fprintf(TRACE_FILE, "%s)\n", ctcs[lp.label2]->name);
}


#endif

