/****************************************************************
 * File:    tables/strtbl.c
 * Purpose: Implement string table routines
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
 * Strings are interned into a hash table, both to avoid having many    *
 * copies of them and to make it possible to compare strings for	*
 * equality using ==.  This file implements the string table.		*
 *									*
 * The string table also includes some additional information telling	*
 * where a given identifier has been declared, if at all.		*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/hash.h"
#include "../tables/tables.h"
#include "../generate/generate.h"
#include "../evaluate/instruc.h"
#include "../error/error.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/************************************************************************
 *			PRIVATE VARIABLES				*
 ************************************************************************/

/**********************************************************************
 *			identifier_table			      *
 *			string_const_table			      *
 **********************************************************************
 * identifier_table and string_const_table are tables for holding     *
 * strings, and have the following purposes.			      *
 *								      *
 * 1. The key is a canonical pointer for a string.  When a string is  *
 *    looked up, the canonical pointer is returned, so that strings   *
 *    that in the same table can be compared for      	      	      *
 *    equality using ==. 					      *
 *								      *
 * 2. Each string has two associated integers, which are	      *
 *    typically the labels where that string was declared in the      *
 *    generated code.  The integer is NO_VALUE if no integer is	      *
 *    stored with the string.  The two integers are stored as an      *
 *    LPAIR with fields label1 and label2.			      *
 *								      *
 *    For string_const_table, label1 is the label for STRING_DCL_I    *
 *    and label2 is the label for REAL_DCL_I or INT_DCL_I. 	      *
 *								      *
 *    For identifier_table, label1 is the label for NAME_DCL_I and    *
 *    label2 is the label for ID_DCL_I or GENUS_DCL_I, or one of the  *
 *    other instructions for creating an identifier.		      *
 *								      *
 * 3. The compiler uses identifier_table to identify reserved words.  *
 *    A reserved word W is found in the table as %W, upper case.      *
 *    For example, there is an entry for "%Let".  It is important     *
 *    not to enter other strings that start with '%' into 	      *
 *    identifier_table, since that will throw off checking for        *
 *    reserved words.						      *
 **********************************************************************/

PRIVATE HASH2_TABLE* identifier_table   = NULL;
PRIVATE HASH2_TABLE* string_const_table = NULL;

/**********************************************************************
 *			last_identifier_looked_up		      *
 *			last_identifier_hash_val		      *
 *			last_string_const_looked_up		      *
 *			last_string_const_hash_val		      *
 **********************************************************************
 * It is common for the compiler to look up the same string several   *
 * times in a row.  We cache each lookup, so as to avoid having to    *
 * go to the table.  last_identifier_looked_up holds the pointer in   *
 * identifier_table that was returned at the last lookup in 	      *
 * identifier_table, or is NULL if none is available. 		      *
 * last_identifier_hash_val is the hash value of		      *
 * last_identifier_looked_up.  Variables last_string_const_looked_up  *
 * and last_string_const_hash_val are similar, for 		      *
 * string_const_table.						      *
 **********************************************************************/

PRIVATE char* last_identifier_looked_up   = NULL;
PRIVATE LONG  last_identifier_hash_val    = 0;
PRIVATE char* last_string_const_looked_up = NULL;
PRIVATE LONG  last_string_const_hash_val  = 0;


/****************************************************************
 *			STR_TB_CHECK				*
 *			ID_TB_CHECK				*
 *			STRING_CONST_TB_CHECK			*
 ****************************************************************
 * str_tb_check(s,t) returns the address of string s in         *
 * one of the string tables, if s is there, or NULL if s is not *
 * in that table.  The table used is				*
 *   t = 0   identifier_table					*
 *   t = 1   string_const_table					*
 *								*
 * tbl must be &identifier_table or &string_const_table.	*
 *								*
 * id_tb_check(s) is str_tb_check(s, 0).			*
 * string_const_tb_check(s) is str_tb_check(s, 1).		*
 ****************************************************************/

PRIVATE char* str_tb_check(char *s, Boolean t)
{
  char **last_string_looked_up;
  LONG *last_string_hash_val;
  HASH2_TABLE **string_table;
  HASH2_CELLPTR h;
  HASH_KEY u;
  register LONG hash;

  if(s == NULL) return NULL;

  if(t) {
    last_string_looked_up = &last_string_const_looked_up;
    last_string_hash_val  = &last_string_const_hash_val;
    string_table          = &string_const_table;
  }
  else {
    last_string_looked_up = &last_identifier_looked_up;
    last_string_hash_val  = &last_identifier_hash_val;    
    string_table	  = &identifier_table;
  } 

  u.str = s;
  hash  = strhash(s);
  if(hash == *last_string_hash_val && strcmp(s, *last_string_looked_up) == 0) {
    return *last_string_looked_up;
  }

  /*------------------------------------------------------------*
   * If s is not the same as the last time, then look s up in   *
   * string_table.						*
   *------------------------------------------------------------*/

  *last_string_hash_val = hash;
  h = locate_hash2(*string_table, u, hash, equalstr);
  return (*last_string_looked_up = h->key.str);
}

/*------------------------------------------------------------------*/

char* id_tb_check(char *s)
{
  if(s == last_identifier_looked_up) return s;
  return str_tb_check(s, 0);
}

/*------------------------------------------------------------------*/

char* string_const_tb_check(char *s)
{
  if(s == last_string_const_looked_up) return s;
  return str_tb_check(s, 1);
}

  
/****************************************************************
 *			STR_TB1					*
 ****************************************************************
 * str_tb1 looks in a table determined by t, where the table is *
 *   t = 0   identifier_table					*
 *   t = 1   string_const_table					*
 *								*
 * If a string with the same characters as s is in the string 	*
 * table, return that string (pointer).  Otherwise, create an   *
 * entry for s, allocating new memory to hold the string, and   *
 * return the new pointer.					*
 *								*
 * kind is either 0 or an instruction (ID_DCL_I, SPECIES_DCL_I, *
 * FAMILY_DCL_I, GENUS_DCL_I, COMMUNITY_DCL_I, etc.,		*
 * STRING_DCL_I or NAME_DCL_I).					*
 *								*
 * If kind != 0, then the following is generated in the code	*
 * file.							*
 *								*
 *		LABEL_DCL_I n					*
 *		kind	    s					*
 *								*
 * where n is the next available global label.  n is stored	*
 * with string s in the table.  See the table descriptions	*
 * above.							*
 *								*
 * The hash parameter of str_tb1 must be strhash(s).		*
 *								*
 * The following are macros.					*
 *								*
 *  id_tb and id_tb1 use the identifier table.  id_tb does not  *
 *  need to be told strhash(s).					*
 *								*
 *  string_const_tb and string_const_tb1 use the string constant*
 *  table.							*
 ****************************************************************/

char* str_tb1(char *s, int kind, LONG hash, Boolean t)
{
  char **last_string_looked_up;
  LONG *last_string_hash_val;
  char *a;
  HASH2_TABLE **string_table;
  HASH2_CELLPTR h;
  HASH_KEY u;

  if(s == NULL) return NULL;

  if(t) {
    last_string_looked_up = &last_string_const_looked_up;
    last_string_hash_val  = &last_string_const_hash_val;
    string_table          = &string_const_table;
  }
  else {
    last_string_looked_up = &last_identifier_looked_up;
    last_string_hash_val  = &last_identifier_hash_val;    
    string_table	  = &identifier_table;
  } 

  /*-----------------------------------------------------------*
   * Check whether s is the same as the last string looked up. *
   * But only do this when kind = 0, since the associated      *
   * value is not cached.				       *
   *-----------------------------------------------------------*/

  if(kind == 0 && hash == *last_string_hash_val &&
     (s == *last_string_looked_up || 
      strcmp(s, *last_string_looked_up) == 0)) {
    return *last_string_looked_up;
  }
  *last_string_hash_val = hash;

  /*------------------------------------------------------------*
   * If s is different from the last string looked up, then 	*
   * look s up in string_table.  If found, return the value     *
   * found in the table.					*
   *------------------------------------------------------------*/

  u.str = s;
  h = insert_loc_hash2(string_table, u, hash, equalstr);
  if(h->key.str != NULL_S) {
    if((kind == STRING_DCL_I || kind == NAME_DCL_I) && 
	h->val.lpair.label1 == NO_VALUE) {
      h->val.lpair.label1 = generate_str_dcl_g(kind, s, 0);
    }
    else if(kind != 0 && h->val.lpair.label2 == NO_VALUE) {
      h->val.lpair.label2 = generate_str_dcl_g(kind, s, 0);
    }
    return (*last_string_looked_up = h->key.str);
  }

  /*------------------------------------*
   * If s is not in the table, add it.	*
   *------------------------------------*/

  a = allocate_str(strlen(s) + 1);
  strcpy(a,s);
  h->key.str = a;
  if(kind == STRING_DCL_I || kind == NAME_DCL_I) {
    h->val.lpair.label2 = NO_VALUE;
    h->val.lpair.label1 = generate_str_dcl_g(kind, a, 0);
  }
  else if(kind != 0) {
    h->val.lpair.label1 = NO_VALUE;
    h->val.lpair.label2 = generate_str_dcl_g(kind, a, 0);
  }

  return (*last_string_looked_up = a);
}


/****************************************************************
 *			STR_TB10				*
 *			ID_TB0, ID_TB10				*
 *			STRING_CONST_TB0, STRING_CONST_TB10	*
 ****************************************************************
 * id_tb0(x) and id_tb10(x) are the same as id_tb(x,0) and	*
 * id_tb1(x,0), respectively.					*
 *								*
 * string_const_tb0(x) and string_const_tb10(x) are the same as *
 * string_const_tb(x,0) and string_const_tb1(x,0), respectively.*
 *								*
 * These functions are used often, so we avoid the extra 	*
 * function call by inlining str_tb10.				*
 ****************************************************************/

PRIVATE char* str_tb10(char *s, LONG hash, Boolean t)
{
  char **last_string_looked_up;
  LONG *last_string_hash_val;
  char *a;
  HASH2_TABLE **string_table;
  HASH2_CELLPTR h;
  HASH_KEY u;

  if(s == NULL) return NULL;

  if(t) {
    last_string_looked_up = &last_string_const_looked_up;
    last_string_hash_val  = &last_string_const_hash_val;
    string_table          = &string_const_table;
  }
  else {
    last_string_looked_up = &last_identifier_looked_up;
    last_string_hash_val  = &last_identifier_hash_val;    
    string_table	  = &identifier_table;
  } 

  if(hash == *last_string_hash_val &&
     strcmp(s, *last_string_looked_up) == 0) {
    return *last_string_looked_up;
  }
  *last_string_hash_val = hash;

  u.str = s;
  h = insert_loc_hash2(string_table, u, hash, equalstr);
  if(h->key.str != NULL_S) {
    return (*last_string_looked_up = h->key.str);
  }

  a = allocate_str(strlen(s) + 1);
  strcpy(a,s);
  h->key.str = a;
  h->val.num = NO_VALUE;

  return (*last_string_looked_up = a);
}

/*-----------------------------------------------------------*/

char* id_tb0(char *s) 
{
  if(s == last_identifier_looked_up) return s;
  return str_tb10(s, strhash(s), 0);
}

/*-----------------------------------------------------------*/

char* id_tb10(char *s, LONG hash) 
{
  if(s == last_identifier_looked_up) return s;
  return str_tb10(s, hash, 0);
}

/*-----------------------------------------------------------*/

char* string_const_tb0(char *s) 
{
  if(s == last_string_const_looked_up) return s;
  return str_tb10(s, strhash(s), 1);
}

/*-----------------------------------------------------------*/

char* string_const_tb10(char *s, LONG hash) 
{
  if(s == last_string_const_looked_up) return s;
  return str_tb10(s, hash, 1);
}


/****************************************************************
 *			STAT_ID_TB				*
 ****************************************************************
 * stat_id_tb(s) is similar to id_tb(s,0), but the memory      *
 * pointed to by s is static, and pointer s itself can be put   *
 * into the table, so that no new memory needs to be allocated  *
 * if s is not yet in the table. 				*
 ****************************************************************/

char* stat_id_tb(char *s)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  register LONG hash;

  /*----------------------------------------------------------*
   * We don't perform full cache test, since it will not help *
   * for this function.					      *
   *----------------------------------------------------------*/

  if(last_identifier_looked_up == s || s == NULL) return s;

  u.str = s;
  last_identifier_hash_val = hash = strhash(s);
  h = insert_loc_hash2(&identifier_table, u, hash, equalstr);
  if(h->key.str != NULL_S) return (last_identifier_looked_up = h->key.str);
  h->key.str = s;
  h->val.num = NO_VALUE;
  return (last_identifier_looked_up = s);
}


/******************************************************************
 *			ID_LOC					  *
 *			STRING_CONST_LOC			  *
 ******************************************************************
 * Return the value associated with string s in the string table. *
 * where the string table is given as in the other functions 	  *
 * above.							  *
 *								  *
 * If k != 0, and no value is currently associated with s, then	  *
 * generate a declaration with instruction k, and	  	  *
 * associate that with s.					  *
 ******************************************************************/

int str_loc(char *s, int k, Boolean t)
{
  HASH2_TABLE **string_table;
  HASH2_CELLPTR h;
  char *a;
  HASH_KEY u;
  int result;

  /*------------------------------------------------------------*
   * This function does not participate in caching, since we 	*
   * do not cache the value associated with a string. 		*
   *------------------------------------------------------------*/

  /*------------------------------------------------------------*
   * First, look to see if this string is already in the table. *
   *------------------------------------------------------------*/

  string_table = t ? &string_const_table : &identifier_table;
  u.str = s;
  h = insert_loc_hash2(string_table, u, strhash(s), equalstr);
  if(h->key.str != NULL_S) {
    if(k == STRING_DCL_I || k == NAME_DCL_I) {
      if(h->val.lpair.label1 == NO_VALUE) {
        h->val.lpair.label1 = generate_str_dcl_g(k, s, 0);
      }
      return h->val.lpair.label1;
    }
    else {
      if(h->val.lpair.label2 == NO_VALUE) {
        if(k == 0) {
	  semantic_error(CANNOT_DCL_STR_ERR, 0);
	  die(45);
        }
        else h->val.lpair.label2 = generate_str_dcl_g(k, s, 0);
      }
      return h->val.lpair.label2;
    }
  }

  /*----------------------------------------------------*
   * Here, the string is not yet in the table.  Add it. *
   *----------------------------------------------------*/

  a = allocate_str(strlen(s) + 1);
  strcpy(a,s);
  h->key.str = a;
  result = generate_str_dcl_g(k, a, 0);
  if(k == STRING_DCL_I || k == NAME_DCL_I) {
    h->val.lpair.label1 = result;
  }
  else {
    h->val.lpair.label2 = result;
  }
  return result;
}


/******************************************************************
 *			CLEAR_STRING_TABLE_DATA			  *
 ******************************************************************
 * Set all data entries in the string tables to NO_VALUE.	  *
 ******************************************************************/

void clear_string_table_data(void)
{
  HASH2_VAL u;
  u.num = NO_VALUE;
  clear_data_hash2(identifier_table, u);
  clear_data_hash2(string_const_table, u);
}


/******************************************************************
 *			INIT_STR_TBL				  *
 ******************************************************************
 * Initialize the string tables, but only if they		  *
 * have not already been initialized.				  *
 ******************************************************************/

void init_str_tbl(void)
{
  if(identifier_table == NULL) identifier_table = create_hash2(9);
  if(string_const_table == NULL) string_const_table = create_hash2(4);
}
