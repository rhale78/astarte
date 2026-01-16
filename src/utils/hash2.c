/**********************************************************************
 * File:    utils/hash2.c
 * Purpose: implement hash tables
 * Author:  Karl Abrahamson
 **********************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/************************************************************************
 * This file implements open hashing, with a key and a value in each	*
 * cell.  Such a table has type HASH2_TABLE.  Keys have type HASH_KEY.	*
 * Associated values have type HASH2_VAL.				*
 *									*
 * HASH_KEY must have a field called num that can never be 0 or -1 in a *
 * correct key.   Key 0 indicates an empty entry.  Key -1 indicates	*
 * a deleted entry.  To delete, store key of -1.			*
 *									*
 * The basic operations are locate_hash2, for doing a straight lookup,  *
 * and insert_loc_hash2, for doing a lookup/insertion.  When you	*
 * run insert_loc_hash2, you get back a pointer to the cell holding	*
 * the desired key, or to an empty cell if the key is not present.  If	*
 * the cell is empty, it is your responsibility to install the desired	*
 * key and value in the cell.						*
 ************************************************************************/


#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/********************************************************************
 *			VARIABLES				    *
 ********************************************************************/

/************************************************************************
 *			hash_size					*
 ************************************************************************
 * The entries in hash_size are the sizes of hash table blocks that 	*
 * are used.  They should be increasing, and each entry should be   	*
 * prime. The first entry in hash_size must be 1, and the last entry	*
 * must be 0 to flag the end of the array.			    	*
 *								    	*
 * Define HASH_SIZE_SIZE in hash.h to be the number of entries in   	*
 * hash_size below, not counting the 0 at the end.  		    	*
 ************************************************************************/

SIZE_T FARR hash_size[] =
 {      1UL,
        3UL,
        7UL,
       17UL,
       31UL,
       61UL,
       127UL,
       251UL,
       509UL,
      1021UL,
      2039UL,
      4093UL,
      8191UL,
     16381UL,
     32749UL,
     65521UL,
    131072UL,
    262139UL,
    524287UL,
   1048573UL,
   2097143UL,
         0UL
}; 

/************************************************************************
 *			empty_hash2_cell				*
 ************************************************************************
 * locate_hash2 needs to return a pointer to an empty cell when asked 	*
 * to locate something in an empty table.  EMPTY_HASH2_CELL is that   	*
 * cell. 								*
 ************************************************************************/

PRIVATE HASH2_CELL empty_hash2_cell;

/************************************************************************
 *			deleted_hash2_cell				*
 ************************************************************************
 * When locate_hash2 is searching, it skips over deleted cells.  	*
 * insert_loc_hash2 needs to know whether any deleted cells were        *
 * skipped, and if so where the first one is.  DELETED_HASH2_CELL	*
 * is used to communicate that information from locate_hash2 to		*
 * insert_loc_hash2.							*
 ************************************************************************/

PRIVATE HASH2_CELLPTR deleted_hash2_cell;


/*******************************************************************
 *			CREATE_HASH2				   *
 *******************************************************************
 * Return an empty hash table with space for hash_size[N] entries. *
 *******************************************************************/


HASH2_TABLE* create_hash2(int n)
{
  HASH2_TABLE *t;
  SIZE_T i, size;

  size = hash_size[n];
  if(size == 0) die(0);

  t = allocate_hash2(n);
  for (i = 0; i < size; i++) {
    register HASH2_CELLPTR h = t->cells;
    h[i].key.num = 0;
  }
  return t;
}


/****************************************************************
 *			COPY_HASH2				*
 ****************************************************************
 * Return a copy of hash table T.				*
 ****************************************************************/

HASH2_TABLE* copy_hash2(HASH2_TABLE* t)
{
  if(t == NULL) return NULL;
  else {
    int    sizeIndex    = t->size;
    SIZE_T size         = hash_size[sizeIndex];
    HASH2_TABLE* result = allocate_hash2(sizeIndex);

    longmemcpy(result, t, 
	       sizeof(HASH2_TABLE) + (size - 1) * sizeof(HASH2_CELL));
    return result;
  }
}


/****************************************************************
 *			REHASH2					*
 ****************************************************************
 * Move table T to a larger block, and return the new block.	*
 ****************************************************************/

PRIVATE HASH2_TABLE *
rehash2(HASH2_TABLE* t, Boolean (*eqf)(HASH_KEY,HASH_KEY))
{
  HASH2_TABLE *new_table;
  HASH2_CELLPTR p, q;
  int sizeIndex;
  SIZE_T i, size;
  
  /*--------------------------*
   * Get the new table space. *
   *--------------------------*/

  sizeIndex = t->size;
  size      = hash_size[sizeIndex];
  new_table = create_hash2(sizeIndex + 1);

  /*----------------------------*
   * Copy t into its new space. *
   *----------------------------*/

  for (i = 0, p = t->cells; i < size; i++, p++) {
    register HASH_KEY key = p->key;
    if(key.num != 0 && key.num != -1) {
      q = insert_loc_hash2(&new_table, key, p->hash_val, eqf);
      *q = *p;
    }
  }

  /*-----------------------------------*
   * Free t, and return the new table. *
   *-----------------------------------*/

  free_hash2(t);
  return new_table;
}


/****************************************************************
 *			LOCATE_HASH2				*
 ****************************************************************
 * Return a pointer to the cell in table T holding given KEY.  	*
 * HV is hash(KEY), and EQF is the equality tester.  If KEY is  *
 * not in table T, then the location that KEY should occupy is  *
 * returned.							*
 *								*
 * NOTES:							*
 *								*
 *  (1) If T is NULL, then the address of empty_hash2_cell	*
 *      is returned.  It should not be used for anything.	*
 *								*
 *  (2) Deleted cells are skipped.  If any deleted cells were   *
 *	encountered, deleted_hash2_cell is set to point to the  *
 *	first one.  If none were encountered, deleted_hash2_cell*
 *	is set to NULL.						*
 ****************************************************************/

HASH2_CELLPTR locate_hash2(HASH2_TABLE *t, HASH_KEY key, LONG hv,
			   Boolean (*eqf)(HASH_KEY, HASH_KEY))
{
  deleted_hash2_cell = NULL;

  /*------------------------------------------------------------------*
   * Handle empty tables by returning empty_hash2_cell, forced empty. *
   *------------------------------------------------------------------*/

  if(t == NULL) {
  return_empty:
    empty_hash2_cell.key.num = 0;
    return &empty_hash2_cell;
  }

  /*--------------------------------------------------------------------*
   * Tables with just one cell need to be handled in a special way.	*
   * This is because the main algorithm assumes that there is at least	*
   * one empty cell in a table.  A table with just one cell can have    *
   * its sole entry occupied.						*
   *--------------------------------------------------------------------*/

  if(t->size == 0) {
    HASH2_CELLPTR c = t->cells;
    HASH_KEY c_key = c->key;
    if(c_key.num == 0) return c;
    if(c_key.num == -1) deleted_hash2_cell = c;
    else if(hv == c->hash_val && (*eqf)(c_key, key)) return c;
    goto return_empty;
  }

  /*------------------------------------------------*
   * Handle tables with more than one cell.         *
   * h2 is a second hash value, for double hashing. *
   *------------------------------------------------*/

  {register SIZE_T i;
   register HASH2_CELLPTR c;
   SIZE_T size  = hash_size[t->size];
   SIZE_T h2    = SECOND_HASH(hv,size);
   HASH2_CELLPTR h = t->cells;
   HASH_KEY c_key;

   i = toSizet(hv % size);
   c = h + i;
   c_key = c->key;

   while(c_key.num != 0) {

     /*------------------------------------------------------------*
      * If this is a deleted cell, record it for insert_loc_hash2. *
      *------------------------------------------------------------*/

     if(c_key.num == -1) {
       if(deleted_hash2_cell == NULL) deleted_hash2_cell = c;
     }

     /*----------------------------------------*
      * If the key is found, return this cell. *
      *----------------------------------------*/

     else if(hv == c->hash_val && (*eqf)(c_key, key)) return c;

     /*------------------------------------------------------------*
      * Move to the next cell, wrapping around if at end of table. *
      *------------------------------------------------------------*/

     c += h2;
     i += h2;
     if(i >= size) {
       i -= size;
       c -= size;
     }
     c_key = c->key;

   } /* end while(c_key.num != 0) */

   return c;
  }
}


/******************************************************************
 *			INSERT_LOC_HASH2			  *
 ******************************************************************
 * Same as locate_hash2, but if KEY is not present, then the load *
 * is incremented and the table is possibly reallocated in	  *
 * anticipation of an insertion.  Since the table can change	  *
 * location after rehashing, T is a variable, and can be changed. *
 *								  *
 * It is up to the caller to perform the insertion, by installing *
 * informatin in the key and value fields when the key is 0.	  *
 ******************************************************************/

HASH2_CELLPTR insert_loc_hash2(HASH2_TABLE **t, HASH_KEY key, LONG hv,
			       Boolean (*eqf)(HASH_KEY, HASH_KEY))
{
  HASH2_CELLPTR cell;
  HASH2_TABLE *star_t, *tt;
  SIZE_T load, size;

  /*----------------------------------------------------------------------*
   * If an insertion would overload the table, move it to a larger block. *
   *----------------------------------------------------------------------*/

  star_t = *t;
  if(star_t == NULL) star_t = *t = create_hash2(0);
  load   = star_t->load;
  size   = hash_size[star_t->size];

  if(load > 0 && LFB2 * tolong(load+1) > LFA2 * tolong(size)){
    tt = *t = rehash2(star_t, eqf);
  }
  else tt = star_t;

  /*-------------------------------------------------------------*
   * Find the cell and return it.  If the cell is empty, and a   *
   * deleted cell was skipped, return the address of the deleted *
   * cell.							 *
   *-------------------------------------------------------------*/

  cell = locate_hash2(tt, key, hv, eqf);

  if(cell->key.num == 0) {
    if(deleted_hash2_cell != NULL) {
      cell = deleted_hash2_cell;
      cell->key.num = 0;
    }
    else {
      tt->load++;
    }
    cell->hash_val = hv;
  }
  return cell;
}
      

/****************************************************************
 *			CLEAR_DATA_HASH2			*
 ****************************************************************
 * Set the data field of every record in table T to D.		*
 ****************************************************************/

void clear_data_hash2(HASH2_TABLE *t, HASH2_VAL d)
{
  SIZE_T i, size;

  size = hash_size[t->size];
  for(i = 0; i < size; i++) {
    register HASH2_CELLPTR h = t->cells;
    if(h[i].key.num != 0) {
      h[i].val = d;
    }
  }
}


/****************************************************************
 *			KEY_LIST				*
 ****************************************************************
 * Return a list of the keys that occur in table TBL.		*
 ****************************************************************/
 
LIST* key_list(HASH2_TABLE *tbl)
{
  LONG i, size;
  HASH2_CELLPTR cells;
  LIST* result;

  if(tbl == NULL) return NIL;

  cells  = tbl->cells;
  size   = hash_size[tbl->size];
  result = NIL;
  for(i = 0; i < size; i++) {
    if(cells[i].key.num != 0) {
      HEAD_TYPE hd;
      hd.hash_key = cells[i].key;
      result = general_cons(hd, result, HASH_KEY_L);
    }
  }
  return result;
}


/****************************************************************
 *			NUM_BINDINGS_HASH2			*
 ****************************************************************
 * Return the number of bindings in table T. 			*
 ****************************************************************/

/*---------------------*
 * Not currently used. *
 *---------------------*/

#ifdef NEVER
LONG num_bindings_hash2(HASH2_TABLE *t)
{
  SIZE_T i, s;
  HASH2_CELLPTR h;
  LONG result = 0;

  if(t == NULL) return 0;

  s = hash_size[t->size];
  for(i = 0, h = t->cells; i < s; i++, h++) {
    if(h->key.num != 0 && h->key.num != -1) result++;
  }
  return result;
}
#endif


/****************************************************************
 *			SCAN_HASH2				*
 ****************************************************************
 * Apply function F to each nonempty cell in table T.		*
 ****************************************************************/

void scan_hash2(HASH2_TABLE *t, void (*f)(HASH2_CELLPTR))
{
  SIZE_T i, s;
  HASH2_CELLPTR h;

  if(t == NULL) return;
  s = hash_size[t->size];
  for(i = 0, h = t->cells; i < s; i++, h++) {
    if(h->key.num != 0 && h->key.num != -1) f(h);
  }
}


/****************************************************************
 *			SCAN_AND_CLEAR_HASH2			*
 ****************************************************************
 * Call F at each nonempty cell in table *TBL, then free *TBL,	*
 * and set *TBL to NULL.					*
 ****************************************************************/

void scan_and_clear_hash2(HASH2_TABLE **tbl, void (*f)(HASH2_CELLPTR))
{
  scan_hash2(*tbl, f);
  free_hash2(*tbl);
  *tbl = NULL;
}


/****************************************************************
 *			SORT_INT_HASH2				*
 ****************************************************************
 * Sort a hash table with integer keys, putting the empty 	*
 * cells at the end.						*
 *								*
 * IMPORTANT: THIS IS A HIGHLY DESTRUCTIVE OPERATION. IT RUINS	*
 * THE HASH TABLE. ONLY USE THIS ON A TABLE THAT IS ABOUT TO	*
 * BE DESTROYED.						*
 ****************************************************************/

PRIVATE int cmp_int_hash2(const HASH2_CELLPTR a, const HASH2_CELLPTR b)
{
  LONG aval, bval;

  aval = a->key.num;
  if(aval == -1) aval = a->key.num = 0;
  bval = b->key.num;
  if(bval == -1) bval = b->key.num = 0;

  if(aval == 0) {
    if(bval == 0) return 0;
    else return 1;
  }

  else if(bval == 0) return -1;

  else {
    register LONG diff = aval - bval;
    if(diff > 0) return 1;
    if(diff < 0) return -1;
    return 0;
  }
}

/*--------------------------------------------------------*/

typedef int (*QSORT_CMP_TYPE)(const void*, const void*);

void sort_int_hash2(HASH2_TABLE *tbl)
{
  qsort((char *)(tbl->cells), (int)hash_size[tbl->size],
	sizeof(HASH2_CELL), (QSORT_CMP_TYPE)cmp_int_hash2);
}


/****************************************************************
 *			SORT_INT_VAL_HASH2			*
 ****************************************************************
 * Sort a hash table by its integer values, putting empty 	*
 * cells at the end.  The sort is into DESCENDING order.	*
 *								*
 * IMPORTANT: THIS IS A HIGHLY DESTRUCTIVE OPERATION. IT RUINS	*
 * THE HASH TABLE. ONLY USE THIS ON A TABLE THAT IS ABOUT TO	*
 * BE DESTROYED.						*
 ****************************************************************/

PRIVATE int cmp_int_val_hash2(const HASH2_CELLPTR a, const HASH2_CELLPTR b)
{
  LONG akey, bkey;

  akey = a->key.num;
  if(akey == -1) akey = a->key.num = 0;
  bkey = b->key.num;
  if(bkey == -1) bkey = b->key.num = 0;

  if(akey == 0) {
    if(bkey == 0) return 0;
    else return 1;
  }
  else if(bkey == 0) return -1;
  else {
    register LONG aval = a->val.num;
    register LONG bval = b->val.num;
    register LONG diff = aval - bval;
    if(diff > 0) return -1;
    if(diff < 0) return 1;
    return 0;
  }
}

/*-------------------------------------------------------------------*/

void sort_int_val_hash2(HASH2_TABLE *tbl)
{
  qsort((char *)(tbl->cells), (int)hash_size[tbl->size],
	sizeof(HASH2_CELL), (QSORT_CMP_TYPE)cmp_int_val_hash2);
}


/****************************************************************
 *			SORT_PROFILE_VAL_HASH2			*
 ****************************************************************
 * Sort a hash table by its instructions_executed field of the  *
 * profile_info entry in the val field.				*
 * The sort is into DESCENDING order.				*
 *								*
 * IMPORTANT: THIS IS A HIGHLY DESTRUCTIVE OPERATION. IT RUINS	*
 * THE HASH TABLE. ONLY USE THIS ON A TABLE THAT IS ABOUT TO	*
 * BE DESTROYED.						*
 ****************************************************************/

PRIVATE int 
cmp_profile_val_hash2(const HASH2_CELLPTR a, const HASH2_CELLPTR b)
{
  LONG akey, bkey;

  akey = a->key.num;
  if(akey == -1) akey = a->key.num = 0;
  bkey = b->key.num;
  if(bkey == -1) bkey = b->key.num = 0;

  if(akey == 0) {
    if(bkey == 0) return 0;
    else return 1;
  }
  else if(bkey == 0) return -1;
  else {
    register LONG aval = a->val.profile_info->instructions_executed;
    register LONG bval = b->val.profile_info->instructions_executed;
    register LONG diff = aval - bval;
    if(diff > 0) return -1;
    if(diff < 0) return 1;
    return 0;
  }
}

/*-------------------------------------------------------------------*/

void sort_profile_val_hash2(HASH2_TABLE *tbl)
{
  qsort((char *)(tbl->cells), (int)hash_size[tbl->size],
	sizeof(HASH2_CELL), (QSORT_CMP_TYPE)cmp_profile_val_hash2);
}


/****************************************************************
 *			SORT_STR_HASH2				*
 ****************************************************************
 * Sort a hash table with string keys, putting the empty	*
 * cells at the end.						*
 *								*
 * IMPORTANT: THIS IS A HIGHLY DESTRUCTIVE OPERATION. IT RUINS	*
 * THE HASH TABLE. ONLY USE THIS ON A TABLE THAT IS ABOUT TO	*
 * BE DESTROYED.						*
 ****************************************************************/

PRIVATE int 
cmp_str_hash2(const HASH2_CELLPTR a, const HASH2_CELLPTR b)
{
  char *aval, *bval;

  aval = a->key.str;
  if(a->key.num == -1) aval = a->key.str = NULL;
  bval = b->key.str;
  if(b->key.num == -1) bval = b->key.str = NULL;
  if(aval == NULL) {
    if(bval == NULL) return 0;
    else return 1;
  }
  else if(bval == NULL) return -1;
  else return strcmp(aval, bval);
}

/*-----------------------------------------------------------------*/

void sort_str_hash2(HASH2_TABLE *tbl)
{
  qsort((char *)(tbl->cells), (int) hash_size[tbl->size],
	sizeof(HASH2_CELL), (QSORT_CMP_TYPE)cmp_str_hash2);
}


