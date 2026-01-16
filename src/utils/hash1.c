/**********************************************************************
 * File:    utils/hash1.c
 * Purpose: implement hash tables, single key.
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
 * This file implements open hashing, with only a key at each	        *
 * cell.  Such a table has type HASH1_TABLE.  Keys have type HASH_KEY.  *
 *									*
 * HASH_KEY must have a field called num that can never be 0 or -1 in a *
 * correct key.   Key 0 indicates an empty entry.  Key -1 indicates	*
 * a deleted entry.  To delete, store key of -1.			*
 *									*
 * The basic operations are locate_hash1, for doing a straight lookup,  *
 * and insert_loc_hash1, for doing a lookup/insertion.  When you	*
 * run insert_loc_hash1, you get back a pointer to the cell holding	*
 * the desired key, or to an empty cell if the key is not present.  If	*
 * the cell is empty, it is your responsibility to install the desired	*
 * key in the cell.							*
 ************************************************************************/


#include <memory.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/hash.h"
#include "../tables/tables.h"


/******************************************************************
 *			PRIVATE VARIABLES			  *
 ******************************************************************/

/************************************************************************
 *			empty_hash1_cell				*
 ************************************************************************
 * locate_hash1 needs to return a pointer to an empty cell when asked 	*
 * to locate something in an empty table.  empty_hash1_cell is that   	*
 * cell. 								*
 ************************************************************************/

PRIVATE HASH1_CELL empty_hash1_cell;

/************************************************************************
 *			deleted_hash1_cell				*
 ************************************************************************
 * When locate_hash1 is searching, it skips over deleted cells.  	*
 * insert_loc_hash1 needs to know whether any deleted cells were        *
 * skipped, and if so where the first one is.  deleted_hash1_cell	*
 * is used to communicate that information from locate_hash1 to		*
 * insert_loc_hash1.							*
 ************************************************************************/

PRIVATE HASH1_CELLPTR deleted_hash1_cell;


/****************************************************************
 *			CREATE_HASH1				*
 ****************************************************************
 * Return an empty hash table with space for hash_size[N] 	*
 * entries.							*
 ****************************************************************/

HASH1_TABLE* create_hash1(int n)
{
  HASH1_CELLPTR c;
  HASH1_TABLE *t;
  SIZE_T i,size;

  size = hash_size[n];
  if(size == 0) die(0);

  t = allocate_hash1(n);
  for (c = t->cells, i = 0; i < size; i++,c++) c->key.num = 0;
  return t;
}


/****************************************************************
 *			COPY_HASH1				*
 ****************************************************************
 * Return a copy of hash table T.				*
 ****************************************************************/

HASH1_TABLE* copy_hash1(HASH1_TABLE* t)
{
  if(t == NULL) return NULL;
  else {
    int    sizeIndex    = t->size;
    SIZE_T size         = hash_size[sizeIndex];
    HASH1_TABLE* result = allocate_hash1(sizeIndex);

    longmemcpy(result, t, 
	       sizeof(HASH1_TABLE) + (size - 1) * sizeof(HASH1_CELL));
    return result;
  }
}


/****************************************************************
 *			REHASH1					*
 ****************************************************************
 * Move table T to a larger block, and return the new block.    *
 ****************************************************************/

PRIVATE HASH1_TABLE *
rehash1(HASH1_TABLE *t, Boolean (*eqf)(HASH_KEY, HASH_KEY))
{
  HASH1_TABLE *new_table;
  HASH1_CELLPTR p, q;
  HASH_KEY key;
  int sizeIndex;
  SIZE_T size, i;
  
  /*--------------------------*
   * Get the new table space. *
   *--------------------------*/

  sizeIndex = t->size;
  size      = hash_size[sizeIndex];
  new_table = create_hash1(sizeIndex + 1);

  /*----------------------------*
   * Copy t into its new space. *
   *----------------------------*/

  for (i = 0, p = t->cells; i < size; i++, p++) {
    key = p->key;
    if(key.num != 0 && key.num != -1) {
      q  = insert_loc_hash1(&new_table, key, p->hash_val, eqf);
      *q = *p;
    }
  }

  /*-----------------------------------*
   * Free t, and return the new table. *
   *-----------------------------------*/

  free_hash1(t);
  return new_table;
}


/****************************************************************
 *			LOCATE_HASH1				*
 ****************************************************************
 * Return a pointer to the cell in table T holding given KEY.  	*
 * HV is hash(KEY), and EQF is the equality tester.  If KEY is  *
 * not in table T, then the location that KEY should occupy is  *
 * returned.							*
 *								*
 * NOTES:							*
 *  (1) If T is NULL, then the address of empty_hash2_cell	*
 *      is returned.  It should not be used for anything.	*
 *								*
 *  (2) Deleted cells are skipped.  If any deleted cells were   *
 *	encountered, deleted_hash1_cell is set to point to the  *
 *	first one.  If none were encountered, deleted_hash1_cell*
 *	is set to NULL.						*
 ****************************************************************/

HASH1_CELLPTR locate_hash1(HASH1_TABLE *t, HASH_KEY key, LONG hv,
 			   Boolean (*eqf)(HASH_KEY, HASH_KEY))
{
  deleted_hash1_cell = NULL;

  /*------------------------------------------------------------------*
   * Handle empty tables by returning empty_hash1_cell, forced empty. *
   *------------------------------------------------------------------*/

  if(t == NULL) {
  return_empty:
    empty_hash1_cell.key.num = 0;
    return &empty_hash1_cell;
  }

  /*--------------------------------------------------------------------*
   * Tables with just one cell need to be handled in a special way.	*
   * This is because the main algorithm assumes that there is at least	*
   * one empty cell in a table.  A table with just one cell can have    *
   * its sole entry occupied.						*
   *--------------------------------------------------------------------*/

  if(t->size == 0) {
    HASH1_CELLPTR c = t->cells;
    HASH_KEY c_key = c->key;
    if(c_key.num == 0) return c;
    if(c_key.num == -1) deleted_hash1_cell = c;
    else if(hv == c->hash_val && (*eqf)(c_key, key)) return c;
    goto return_empty;
  }

  /*------------------------------------------------*
   * Handle tables with more than one cell.         *
   * h2 is a second hash value, for double hashing. *
   *------------------------------------------------*/

  {register HASH1_CELLPTR c;
   register SIZE_T i;
   HASH_KEY c_key;
   HASH1_CELLPTR h     = t->cells;
   SIZE_T        size  = hash_size[t->size];
   SIZE_T        h2    = SECOND_HASH(hv, size);

   i = toSizet(hv % size);
   c = h + i;
   c_key = c->key;

   while(c_key.num != 0) {

     /*------------------------------------------------------------*
      * If this is a deleted cell, record it for insert_loc_hash1. *
      *------------------------------------------------------------*/

     if(c_key.num == -1) {
       if(deleted_hash1_cell == NULL) deleted_hash1_cell = c;
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


/****************************************************************
 *			INSERT_LOC_HASH1			*
 ****************************************************************
 * Same as locate_hash1, but if KEY is not present, then the 	*
 * load is incremented, and the table is reallocated if		*
 * necessary, in anticipation of an insertion.  It is  		*
 * up to the caller to perform the insertion.  Since the table  *
 * can change location after rehashing, T is a variable, and 	*
 * can be changed.						*
 ****************************************************************/

HASH1_CELLPTR insert_loc_hash1(HASH1_TABLE **t, HASH_KEY key, LONG hv,
  			       Boolean (*eqf)(HASH_KEY,HASH_KEY))
{
  HASH1_CELLPTR cell;
  HASH1_TABLE *star_t, *tt;
  SIZE_T load, size;

  /*----------------------------------------------------------------------*
   * If an insertion would overload the table, move it to a larger block. *
   *----------------------------------------------------------------------*/

  star_t = *t;
  if(star_t == NULL) star_t = *t = create_hash1(0);
  load   = star_t->load;
  size   = hash_size[star_t->size];

  if(load > 0 && LFB1 * tolong(load+1) > LFA1 * tolong(size)) {
    tt = *t = rehash1(star_t, eqf);
  }
  else tt = star_t;

  /*------------------------------*
   * Find the cell and return it. *
   *------------------------------*/

  cell = locate_hash1(tt,key,hv,eqf);

  if(cell->key.num == 0) {
    if(deleted_hash1_cell != NULL) {
      cell = deleted_hash1_cell;
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
 *			INSERT_STR_HASH1			*
 ****************************************************************
 * Insert id_tb0(S) into table TBL.				*
 ****************************************************************/

void insert_str_hash1(HASH1_TABLE **tbl, CONST char *s)
{
  HASH1_CELLPTR h;
  HASH_KEY u;

  LONG hash  = strhash(s);
  u.str      = id_tb10(s, hash);
  h          = insert_loc_hash1(tbl, u, hash, eq);
  h->key.str = u.str;
}


/****************************************************************
 *			DELETE_STR_HASH1			*
 ****************************************************************
 * Delete S from table TBL.					*
 ****************************************************************/

void delete_str_hash1(HASH1_TABLE *tbl, CONST char *s)
{
  HASH1_CELLPTR h;
  HASH_KEY u;

  LONG hash  = strhash(s);
  u.str      = s;
  h          = locate_hash1(tbl, u, hash, equalstr);
  if(h->key.str != NULL) h->key.num = -1;
}


/****************************************************************
 *			STR_MEM_HASH1				*
 ****************************************************************
 * Say whether S is a member of table TBL.			*
 ****************************************************************/

Boolean str_mem_hash1(HASH1_TABLE *tbl, CONST char *s)
{
  HASH1_CELLPTR h;
  HASH_KEY u;

  LONG hash  = strhash(s);
  u.str      = s;
  h          = locate_hash1(tbl, u, hash, equalstr);
  return h->key.num != 0;
}


/****************************************************************
 *			NUM_BINDINGS_HASH1			*
 ****************************************************************
 * Return the number of entries in table T.			*
 ****************************************************************/

/*---------------------*
 * Not currently used. *
 *---------------------*/

#ifdef NEVER
LONG num_bindings_hash1(HASH1_TABLE *t)
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
 *			SCAN_HASH1				*
 ****************************************************************
 * Apply function f to each nonempty cell in table T.		*
 ****************************************************************/

void scan_hash1(HASH1_TABLE *t, void (*f)(HASH1_CELLPTR ))
{
  SIZE_T i, s;
  HASH1_CELLPTR h;

  if(t == NULL) return;
  s = hash_size[t->size];
  for(i = 0, h = t->cells; i < s; i++, h++) {
    if(h->key.num != 0 && h->key.num != -1) f(h);
  }
}
