/*****************************************************************
 * File:    alloc/allocors.c
 * Purpose: Free space allocators and freers for specific types.
 *          With the exceptions of roles and modes, the types 
 *          handled here are not reference counted.
 *          Other reference counted types are handled 
 *          in separate files.
 * Author:  Karl Abrahamson
 ******************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

#include <memory.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/hash.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../lexer/modes.h"
#include "../classes/classes.h"
#include "../error/error.h"
#ifdef MACHINE
# include "../machstrc/machstrc.h"
#endif
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/********************************************************
 *			ALLOCATE_STR			*
 ********************************************************
 * Allocate a string of N bytes.                        *
 ********************************************************/

char* allocate_str(SIZE_T n)
{
  return (char *) alloc(n);
}


/*==============================================================*
 *			HASH TABLES				*
 *==============================================================*/

/*******************************************************************
 *			free_hash1s				   *
 *			free_hash2s				   *
 *******************************************************************
 * Arrays FREE_HASH1S and FREE_HASH2S hold pointers to free-space  *
 * lists for hash tables.  FREE_HASH1S[n] points to a chain of     *
 * free memory blocks, where the blocks are of a size appropriate  *
 * for hash1-tables with hash_size[n] cells in them.  FREE_HASH2S  *
 * is similar, but for hash2-tables.                               *
 *******************************************************************/

PRIVATE HASH1_TABLE* free_hash1s[HASH_SIZE_SIZE];
PRIVATE HASH2_TABLE* free_hash2s[HASH_SIZE_SIZE];

/********************************************************
 *			ALLOCATE_HASH1			*
 ********************************************************
 * Return a new hash1 table with hash_size[n] cells.    *
 * The size field is set to n, and the load to 0.	*
 ********************************************************/

HASH1_TABLE* allocate_hash1(int n)
{
  SIZE_T size;
  HASH1_TABLE *result;

  if(n >= HASH_SIZE_SIZE) die(0);

# ifdef DEBUG
    allocated_hash1s++;
# endif

  if(free_hash1s[n] != NULL) {
    result         = free_hash1s[n];
    free_hash1s[n] = result->cells[0].key.hash1_table;
  }
  else {
    size   = hash_size[n];
    result = (HASH1_TABLE *) alloc((SIZE_T)(sizeof(HASH1_TABLE) + 
				   (size - 1) * sizeof(HASH1_CELL)));
  }
  result->size = n;
  result->load = 0;
  return result;
}


/********************************************************
 *			FREE_HASH1			*
 ********************************************************
 * Return hash table T to the free space list.          *
 ********************************************************/

void free_hash1(HASH1_TABLE *t)
{
# ifndef GCTEST
    int n;

    if(t == NULL) return;
    n                           = t->size;
    t->cells[0].key.hash1_table = free_hash1s[n];
    free_hash1s[n]              = t;

#   ifdef DEBUG
      allocated_hash1s--;
#   endif
# endif
}


/********************************************************
 *			ALLOCATE_HASH2			*
 ********************************************************
 * Return a new hash2 table with hash_size[N] cells.    *
 * The size field is set to N, and the load to 0.	*
 ********************************************************/

HASH2_TABLE* allocate_hash2(int n)
{
  SIZE_T size;
  HASH2_TABLE *result;

  if(n >= HASH_SIZE_SIZE) die(0);

# ifdef DEBUG
    allocated_hash2s++;
# endif

  if(free_hash2s[n] != NULL) {
    result         = free_hash2s[n];
    free_hash2s[n] = result->cells[0].key.hash2_table;
  }
  else {
    size   = hash_size[n];
    result = (HASH2_TABLE *) alloc((SIZE_T)(sizeof(HASH2_TABLE) + 
				   (size - 1) * sizeof(HASH2_CELL)));
  }
  result->size = n;
  result->load = 0;
  return result;
}


/********************************************************
 *			FREE_HASH2			*
 ********************************************************
 * Return table T to the free space.                    *
 ********************************************************/

void free_hash2(HASH2_TABLE *t)
{
# ifndef GCTEST
    int n;

    if(t == NULL) return;
    n                           = t->size;
    t->cells[0].key.hash2_table = free_hash2s[n];
    free_hash2s[n]              = t;

#   ifdef DEBUG
      allocated_hash2s--;
#   endif

# endif
}


/********************************************************
 *			INIT_HASH_ALLOC			*
 ********************************************************
 * Set up free_hash1s and free_hash2s.  This must be	*
 * called at startup time.		                *
 ********************************************************/

void init_hash_alloc(void)
{
  int i;

  for(i = 0; i < HASH_SIZE_SIZE; i++) {
    free_hash1s[i] = NULL;
    free_hash2s[i] = NULL;
  }
}


/*==============================================================*
 *			CLASS UNION CELLS 			*
 *==============================================================*/

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE CLASS_UNION_CELL* free_cucs = NULL;    

/********************************************************
 *			 ALLOCATE_CUC			*
 ********************************************************
 * Return a new CLASS_UNION_CELL node, zeroed out.      *
 ********************************************************/

CLASS_UNION_CELL* allocate_cuc(void)
{
  CLASS_UNION_CELL* cuc = free_cucs;
  if(cuc != NULL) free_cucs = cuc->u.next;
  else {
    cuc = (CLASS_UNION_CELL *) alloc_small(sizeof(CLASS_UNION_CELL));
  }
  memset(cuc, 0, sizeof(CLASS_UNION_CELL));

# ifdef DEBUG
    allocated_cucs++;
# endif

  return cuc;
}


/********************************************************
 *			 FREE_CUC			*
 ********************************************************
 * Free CLASS_UNION_CELL node CUC, dropping parts, but  *
 * not freeing the next in the chain.			*
 ********************************************************/

void free_cuc(CLASS_UNION_CELL *cuc)
{

  /*-----------------*
   * Recursive drops *
   *-----------------*/

  int tok = cuc->tok;
  if(tok == TYPE_ID_TOK) {
    drop_type(cuc->CUC_TYPE);
#   ifdef TRANSLATOR
      drop_role(cuc->CUC_ROLE);
#   endif
  }
  else if(tok == TYPE_LIST_TOK) {
    drop_list(cuc->CUC_TYPES);
#   ifdef TRANSLATOR
      drop_list(cuc->CUC_ROLES);
#   endif
  }

# ifdef TRANSLATOR
    if(tok >= CASE_ATT && tok <= WHILE_ELSE_ATT) {
      drop_expr(cuc->CUC_EXPR);
    }
# endif

  /*-----------------*
   * Free this node. *
   *-----------------*/
 
# ifndef GCTEST
    cuc->u.next = free_cucs;
    free_cucs = cuc;
#   ifdef DEBUG
      allocated_cucs--;
#   endif
# else
    cuc->special = -1;
# endif
}


/********************************************************
 *			 FREE_CUCS_FUN			*
 ********************************************************
 * Free the chain of CLASS_UNION_CELL nodes that starts *
 * at CUC (linked through the next pointers).           *
 ********************************************************/

void free_cucs_fun(CLASS_UNION_CELL *cuc)
{
  CLASS_UNION_CELL *p, *q;

  for(p = cuc; p != NULL; p = q) {
    q = p->u.next;
    free_cuc(p);
  }
}


/*==============================================================*
 *			CLASS TABLE CELLS 			*
 *==============================================================*/

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE CLASS_TABLE_CELL* free_ctcs = NULL;

/********************************************************
 *			 ALLOCATE_CTC			*
 ********************************************************
 * Return a new CLASS_TABLE_CELL node, zeroed out.      *
 ********************************************************/

CLASS_TABLE_CELL* allocate_ctc(void)
{
  CLASS_TABLE_CELL* ctc = free_ctcs;
  if(ctc != NULL) free_ctcs = (CLASS_TABLE_CELL *) ctc->ty;
  else {
    ctc = (CLASS_TABLE_CELL *) alloc_small(sizeof(CLASS_TABLE_CELL));
  }
  memset((char *) ctc, 0, sizeof(CLASS_TABLE_CELL));
  return ctc;
}


/********************************************************
 * 			FREE_CTC			*
 ********************************************************
 * Free CLASS_TABLE_CELL node C.                        *
 ********************************************************/

void free_ctc(CLASS_TABLE_CELL *c)
{
# ifndef GCTEST
    c->ty     = (TYPE *) free_ctcs;
    free_ctcs = c;
# endif
}


/********************************************************
 * 			DROP_CTC_PARTS			*
 ********************************************************
 * Drop the ref-counted fields in ctc.                  *
 ********************************************************/

#ifdef TRANSLATOR
void drop_ctc_parts(CLASS_TABLE_CELL *ctc)
{
  drop_list(ctc->constructors);
  drop_type(ctc->ty);
  drop_role(ctc->role);
  drop_type(ctc->u.rep_type);
}
#endif

#ifdef TRANSLATOR

/*==============================================================*
 *			LEXER/PARSER				*
 ===============================================================*/

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE IMPORT_STACK* free_import_stacks = NULL;  

/********************************************************
 * 			ALLOCATE_IMPORT_STACK           *
 ********************************************************
 * Return a new IMPORT_STACK cell, zeroed out.          *
 ********************************************************/

IMPORT_STACK* allocate_import_stack(void)
{
  IMPORT_STACK* p = free_import_stacks;
  if(p != NULL) free_import_stacks = free_import_stacks->next;
  else p = (IMPORT_STACK *) alloc(sizeof(IMPORT_STACK));
  memset((char *) p, 0, sizeof(IMPORT_STACK));
  return p;
}


/********************************************************
 * 			FREE_IMPORT_STACK		*
 ********************************************************
 * Free IMPORT_STACK node p, dropping the references to *
 * things that this node refers to.                     *
 ********************************************************/

void free_import_stack(IMPORT_STACK *p)
{
  /*------------------------*
   * Drop reference counts. *
   *------------------------*/

  drop_type(p->global_real_const_assumption);
  drop_type(p->real_const_assumption);
  drop_type(p->global_nat_const_assumption);
  drop_type(p->nat_const_assumption);
  drop_list(p->no_tro_backout);
  drop_list(p->import_ids);
  drop_list(p->private_packages);
  drop_list(p->public_packages);
  drop_list(p->shadow_nums);
  drop_list(p->shadow_stack);

# ifndef GCTEST
# ifdef NEVER
    free_hash1(p->unary_op_table);
    free_hash2(p->op_table);
    free_hash2(p->ahead_descr_table);
    free_hash1(p->no_tro_table);
    free_hash2(p->global_patfun_assume_table); 
    free_hash2(p->patfun_assume_table);
    free_hash2(p->import_dir_table);
    yy_delete_buffer(p->lexbuf);

    scan_and_clear_hash2(p->default_table, drop_hash_type);
    free_hash2(p->default_table);

    scan_and_clear_hash2(p->other_local_expect_table, drop_hash_list);
    free_hash2(p->other_local_expect_table);

    scan_and_clear_hash2(p->local_expect_table, drop_hash_list);
    free_hash2(p->local_expect_table);

    scan_and_clear_hash2(p->global_abbrev_id_table, drop_hash_type);
    free_hash2(p->global_abbrev_id_table);

    scan_and_clear_hash2(p->abbrev_id_table, drop_hash_type);
    free_hash2(p->abbrev_id_table);

    scan_and_clear_hash2(p->global_assume_role_table, drop_hash_role);
    free_hash2(p->global_assume_role_table);

    scan_and_clear_hash2(p->assume_role_table, drop_hash_role);
    free_hash2(p->assume_role_table);

    scan_and_clear_hash2(p->global_assume_table, drop_hash_type);
    free_hash2(p->global_assume_table);

    scan_and_clear_hash2(p->assume_table, drop_hash_type);
    free_hash2(p->assume_table);
# endif
    p->next = free_import_stacks;
    free_import_stacks = p;
# endif
}
    

/*==============================================================*
 * 		GLOBAL ID TABLE	(TRANSLATOR)			*
 *==============================================================*/

/********************************************************
 *			FREE SPACE LISTS		*
 ********************************************************/

PRIVATE EXPECTATION*    free_expectations   = NULL; 
PRIVATE DESCRIP_CHAIN*  free_descrip_chains = NULL; 

/********************************************************
 *			ALLOCATE_GIC			*
 ********************************************************
 * Return a new GLOBAL_ID_CELL node, zeroed out.        *
 ********************************************************/

GLOBAL_ID_CELL* allocate_gic(void)
{
  GLOBAL_ID_CELL* result = 
	(GLOBAL_ID_CELL *) alloc_small(sizeof(GLOBAL_ID_CELL));
  memset((char *) result, 0, sizeof(GLOBAL_ID_CELL));
  return result;
}


/********************************************************
 *			ALLOCATE_EXPECTATION		*
 ********************************************************
 * Return a new EXPECTATION node.			*
 ********************************************************/

EXPECTATION* allocate_expectation(void)
{
  EXPECTATION* result = free_expectations;
  if(result == NULL) {
    result = (EXPECTATION *) alloc_small(sizeof(EXPECTATION));
  }
  else free_expectations = free_expectations->next;
  return result;
}


/********************************************************
 *			FREE_EXPECTATION		*
 ********************************************************
 * Free the EXPECTATION chain beginning at EXP.		*
 ********************************************************/

void free_expectation(EXPECTATION *exp)
{
# ifndef GCTEST
    EXPECTATION* p = exp;
    while(p != NULL) {
      EXPECTATION* q = p->next; 
      drop_type(p->type);
      drop_list(p->visible_in);

      p->next           = free_expectations;
      free_expectations = p;
      p                 = q;
    }
# endif
}


/********************************************************
 *		ALLOCATE_PATFUN_EXPECTATION		*
 ********************************************************
 * Return a new PATFUN_EXPECTATION node.		*
 ********************************************************/

PATFUN_EXPECTATION* allocate_patfun_expectation(void)
{
  return (PATFUN_EXPECTATION *) alloc_small(sizeof(PATFUN_EXPECTATION));
}


/********************************************************
 *			ALLOCATE_EXPAND_INFO		*
 ********************************************************
 * Return a new EXPAND_INFO node, zeroed out.		*
 ********************************************************/

EXPAND_INFO* allocate_expand_info(void)
{
  EXPAND_INFO* result = (EXPAND_INFO *) alloc_small(sizeof(EXPAND_INFO));
  memset(result, 0, sizeof(EXPAND_INFO));
  return result;
}


/********************************************************
 *			ALLOCATE_PART			*
 ********************************************************
 * Return a new PART node, zeroed out.               	*
 ********************************************************/

PART* allocate_part(void)
{
  PART* p = (PART *) alloc_small(sizeof(PART));
  memset(p, 0, sizeof(PART));
  return p;
}


/********************************************************
 *			ALLOCATE_ROLE_CHAIN		*
 ********************************************************
 * Return a new ROLE_CHAIN node, zeroed out.            *
 ********************************************************/

ROLE_CHAIN* allocate_role_chain(void)
{
  ROLE_CHAIN* p = (ROLE_CHAIN *) alloc_small(sizeof(ROLE_CHAIN));
  memset(p, 0, sizeof(ROLE_CHAIN));
  return p;
}


/********************************************************
 *		ALLOCATE_DESCRIP_CHAIN			*
 ********************************************************
 * Return a new DESCRIP_CHAIN node.	  		*
 ********************************************************/

DESCRIP_CHAIN* allocate_descrip_chain(void)
{
  DESCRIP_CHAIN* result = free_descrip_chains;
  if(result == NULL) {
    result = (DESCRIP_CHAIN *) alloc_small(sizeof(DESCRIP_CHAIN));
  }
  else free_descrip_chains = free_descrip_chains->next;

# ifdef DEBUG
    allocated_descr_chains++;
# endif

  return result;
}


/********************************************************
 *			FREE_DESCRIP_CHAIN		*
 ********************************************************
 * Free DC and the chain that follows it.		*
 ********************************************************/

void free_descrip_chain(DESCRIP_CHAIN *dc)
{
  while(dc != NULL) {
    DESCRIP_CHAIN* next = dc->next;
    drop_type(dc->type);
    drop_mode(dc->mode);
    drop_list(dc->visible_in);

#   ifndef GCTEST
      dc->next          = free_descrip_chains;
      free_descrip_chains = dc;
#     ifdef DEBUG
        allocated_descr_chains--;
#     endif
#   endif
    dc = next;
  }
}  


/*==============================================================*
 * 		REPORT_RECORDS	(TRANSLATOR)			*
 *==============================================================*/

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

REPORT_RECORD* free_report_records = NULL;

/****************************************************************
 *			ALLOCATE_REPORT_RECORD			*
 ****************************************************************
 * Return a new REPORT_RECORD node                              *
 ****************************************************************/

REPORT_RECORD* allocate_report_record(void)
{
  REPORT_RECORD* t = free_report_records;
  if(t != NULL) free_report_records = t->next;
  else t = (REPORT_RECORD *) alloc_small(sizeof(REPORT_RECORD));

# ifdef DEBUG
    allocated_report_records++;
# endif

  return t;
}


/****************************************************************
 *			FREE_REPORT_RECORD			*
 ****************************************************************
 * Free report record node T and the entire chain accessible    *
 * from it.		i	                                *
 ****************************************************************/

void free_report_record(REPORT_RECORD *t)
{
  REPORT_RECORD *r, *q;

  r = t; 
  while(r != NULL) {
    drop_type(r->type);
    drop_role(r->role);
    drop_mode(r->mode);

    q = r->next;
    r->next = free_report_records;
    free_report_records = r;
#   ifdef DEBUG
      allocated_report_records--;
#   endif
    r = q;
  }
}


/*==============================================================*
 * 			ROLES	(TRANSLATOR)			*
 *==============================================================*/

/****************************************************************
 * Roles are managed by reference counts.  Roles are allocated	*
 * with a ref count of 0.  Use					*
 *   bump_role(e)   to increment ref count of e			*
 *   drop_role(e)   to decrement ref count of e, and possibly 	*
 *		    free,					*
 *   SET_ROLE(x,e)  to set role variable x to e, when x already *
 *		    has a value.				*
 ****************************************************************/


/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE ROLE* free_roles = NULL;

/********************************************************
 *			ALLOCATE_ROLE			*
 ********************************************************
 * Return a new role node with ref count 0.             *
 ********************************************************/

ROLE* allocate_role(void)
{
  ROLE* r = free_roles;
  if(r != NULL) free_roles = r->role1;
  else r = (ROLE*) alloc_small(sizeof(ROLE));
  r->ref_cnt = 0;

# ifdef DEBUG
    allocated_roles++;
# endif

  return r;
}


/****************************************************************
 *			BUMP_ROLE				*
 ****************************************************************
 * Increment the reference count of R.  R can be NULL.          *
 ****************************************************************/

void bump_role(ROLE *r)
{
  if(r != NULL) r->ref_cnt++;
}


/****************************************************************
 *			DROP_ROLE				*
 ****************************************************************
 * Decrement the reference count of R, possibly freeing.        *
 * Do recursive drops on parts if freed.  r can be NULL.	*
 ****************************************************************/

void drop_role(ROLE *r)
{
  if(r != NULL) {
#   ifdef GCTEST
      rkindf(r);
#   endif
    r->ref_cnt--;

    if(r->ref_cnt == 0) {

      /*------------------*
       * Recursive drops. *
       *------------------*/

      drop_role(r->role2);
      drop_role(r->role1);
      drop_mode(r->mode);

      /*---------*
       * Free r. *
       *---------*/

#     ifdef GCTEST
        r->ref_cnt = -100;
#     else
        r->role1   = free_roles;
        free_roles = r;
#       ifdef DEBUG
          allocated_roles--;
#       endif
#     endif
    }
  }
}  


/****************************************************************
 *			SET_ROLE				*
 ****************************************************************
 * Set X = R, managing ref counts.                              *
 ****************************************************************/

void set_role(ROLE **x, ROLE *r)
{
  bump_role(r);
  drop_role(*x);
  *x = r;
}


/****************************************************************
 *			BUMP_RTYPE				*
 ****************************************************************
 * Bump a role and a type.                                      *
 ****************************************************************/

void bump_rtype(RTYPE r)
{
  bump_type(r.type);
  bump_role(r.role);
}


/****************************************************************
 *			DROP_RTYPE				*
 ****************************************************************
 * Drop a role and a type                                       *
 ****************************************************************/

void drop_rtype(RTYPE r)
{
  drop_type(r.type);
  drop_role(r.role);
}


/*==============================================================*
 * 		DEFFERED DECLARATIONS	(TRANSLATOR)		*
 *==============================================================*/

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

DEFERRED_DCL_TYPE* free_deferred_dcls = NULL;  


/****************************************************************
 *			ALLOCATE_DEFERRED_DCL			*
 ****************************************************************
 * Return a new DEFERRED_DCL_TYPE node.                         *
 ****************************************************************/

DEFERRED_DCL_TYPE* allocate_deferred_dcl(void)
{
  DEFERRED_DCL_TYPE* t = free_deferred_dcls;

  if(t != NULL) free_deferred_dcls = t->next;
  else t = (DEFERRED_DCL_TYPE *) alloc_small(sizeof(DEFERRED_DCL_TYPE));
  return t;
}


/****************************************************************
 *			FREE_DEFERRED_DCL			*
 ****************************************************************
 * Free DEFERRED_DCL_TYPE node T, and all nodes in the chain    *
 * that starts at T.  Drop references.                          *
 ****************************************************************/

void free_deferred_dcl(DEFERRED_DCL_TYPE *t)
{
  DEFERRED_DCL_TYPE *r, *q;

  r = t; 
  while(r != NULL) {
    switch(r->tag) {
      case ISSUE_DCL_DEFER:
        drop_expr(r->fields.issue_dcl_fields.ex);
	drop_mode(r->fields.issue_dcl_fields.mode);
	break;

      case ISSUE_MISSING_DEFER:
        drop_type(r->fields.expect_dcl_fields.type);
	break;

      case EXPECT_DCL_DEFER:
	drop_type(r->fields.expect_dcl_fields.type);
	drop_role(r->fields.expect_dcl_fields.role);
	drop_mode(r->fields.expect_dcl_fields.mode);
	break;

      case ATTACH_PROP_DEFER:
	drop_type(r->fields.attach_prop_fields.type);
	break;

      default: {}
    }

    q                  = r->next;
    r->next            = free_deferred_dcls;
    free_deferred_dcls = r;
    r                  = q;
  }
}


/*==============================================================*
 * 			MODES	(TRANSLATOR)			*
 *==============================================================*/

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE MODE_TYPE* free_modes = NULL;  

/********************************************************
 *			ALLOCATE_MODE			*
 ********************************************************
 * Return a new mode node with ref count 1, but zeroed  *
 * out.					                *
 ********************************************************/

MODE_TYPE* allocate_mode(void)
{
  MODE_TYPE* r = free_modes;
  if(r != NULL) free_modes = r->u.link;
  else r = (MODE_TYPE*) alloc_small(sizeof(MODE_TYPE));
  memset(r, 0, sizeof(MODE_TYPE));
  r->ref_cnt = 1;
  return r;
}


/****************************************************************
 *			BUMP_MODE				*
 ****************************************************************
 * Increment the reference count of R.  R can be NULL.          *
 ****************************************************************/

void bump_mode(MODE_TYPE *r)
{
  if(r != NULL) r->ref_cnt++;
}


/****************************************************************
 *			DROP_MODE				*
 ****************************************************************
 * Decrement the reference count of R, possibly freeing.        *
 * Do recursive drops on parts if freed.  r can be NULL.	*
 ****************************************************************/

void drop_mode(MODE_TYPE *r)
{
  if(r != NULL) {
#   ifdef GCTEST
      if(r->ref_cnt <= 0) die(184, r->ref_cnt);
#   endif
    r->ref_cnt--;

    if(r->ref_cnt == 0) {

      /*------------------*
       * Recursive drops. *
       *------------------*/

      drop_list(r->noexpects);
      drop_list(r->visibleIn);

      /*--------------------------------------------------------*
       * Free r. Refuse to free this_mode, defopt_mode or	*
       * null_mode, just in case we are really stupid.		*
       *--------------------------------------------------------*/

#     ifdef GCTEST
        r->ref_cnt = -100;
#     else
        if(r != &null_mode && r != &this_mode && 
           r != &defopt_mode && r != &propagate_mode) {
          r->u.link   = free_modes;
          free_modes  = r;
	}
#     endif
    }
  }
}  


/****************************************************************
 *			SET_MODE				*
 ****************************************************************
 * Set X = R, managing ref counts.                              *
 ****************************************************************/

void set_mode(MODE_TYPE **x, MODE_TYPE *r)
{
  bump_mode(r);
  drop_mode(*x);
  *x = r;
}

/*==============================================================*
 * 			CHOOSE_INFO	(TRANSLATOR)		*
 *==============================================================*/

PRIVATE CHOOSE_INFO* free_choose_infos = NULL;

/****************************************************************
 *			ALLOCATE_CHOOSE_INFO			*
 ****************************************************************
 * Return a newly allocated CHOOSE_INFO node, zeroed out.       *
 ****************************************************************/

CHOOSE_INFO* allocate_choose_info(void)
{
  CHOOSE_INFO* r = free_choose_infos;
  if(r != NULL) free_choose_infos = r->next;
  else r = (CHOOSE_INFO*) alloc_small(sizeof(CHOOSE_INFO));
  memset(r, 0, sizeof(CHOOSE_INFO));
  return r;  
}

/****************************************************************
 *			FREE_CHOOSE_INFO			*
 ****************************************************************
 * Free node R.	 Drop references to pointers in *R.		*
 ****************************************************************/

void free_choose_info(CHOOSE_INFO *r)
{
  drop_expr(r->choose_from);
  drop_expr(r->else_exp);
  drop_expr(r->status_var);
  drop_expr(r->loop_ref);
  drop_list(r->working_choose_matching_list);

  r->next = free_choose_infos;
  free_choose_infos = r;
}

#endif

#ifdef MACHINE

/*==============================================================*
 *			GLOBAL_TABLE (MACHINE)			*
 *==============================================================*/

GLOBAL_TABLE_NODE* allocate_global_table(void)
{
 return (struct global_table_node *) 
        alloc_small(sizeof(struct global_table_node));
}


/*==============================================================*
 *			PACKAGES (MACHINE)			*
 ===============================================================*/

CODE_PTR allocate_package(long n)
{
  return (CODE_PTR) alloc((SIZE_T) n);
}

#endif
