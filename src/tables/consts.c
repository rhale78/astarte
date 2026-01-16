/**********************************************************************
 * File:    tables/consts.c
 * Purpose: Constant and string tables for interpreter.
 * Author:  Karl Abrahamson
 **********************************************************************/

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
 * This file implements string and constant tables for the interpreter. *
 * It also manages converting C-strings to entities.			*
 ************************************************************************/

#define STRHASH_C
#include <string.h>
#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../machdata/entity.h"
#include "../gc/gc.h"
#include "../alloc/allocate.h"
#include "../tables/tables.h"
#include "../rts/rts.h"
#include "../evaluate/instruc.h"
#include "../evaluate/evaluate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#include "../show/prtent.h"
#endif

PRIVATE void reallocate_constants(void);

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			id_names				*
 *			next_name_index				*
 *			id_names_size				*
 ****************************************************************
 * Array id_names holds identifiers that have been declared     *
 * using NAME_DCL_I instructions.  id_names_size is its		*
 * physical size and next_name_index is the next index to fill.	*
 ****************************************************************/

char**         id_names        = NULL;
PRIVATE int    id_names_size   = 0;
PRIVATE int    next_name_index = 0;

/****************************************************************
 *			constants				*
 *			next_const				*
 *			constants_size				*
 ****************************************************************
 * constants points to an array of entities that are the	*
 * constants declared in programs.  constants_size is the 	*
 * current physical size of that array -- the array is 		*
 * reallocated when it needs to grow.  next_const is the 	*
 * actual number of constants that are stored in array 		*
 * constants.							*
 *								*
 * XREF: 							*
 *   constants and next_const are used in gc.c for marking the	*
 *   constants.							*
 *								*
 *   constants and next_const are used in m_debug.c for 	*
 *   showing the constants.					*
 *****************************************************************/

PRIVATE LONG    constants_size = CONSTANTS_SIZE_INIT;
LONG 	        next_const     = 0;
ENTITY HUGEPTR  constants      = NULL;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			string_table				*
 ****************************************************************
 * The constant strings are stored in string_table as the keys. *
 * The keys are char* values, not ENTITYs.			*
 *								*
 * The associated value is the index in constants where this	*
 * string can be found (as an ENTITY), or is -1 if this string	*
 * is not in constants.						*
 ****************************************************************/

PRIVATE HASH2_TABLE* string_table = NULL;


/****************************************************************
 *			MAKE_STR, MAKE_STRN			*
 ****************************************************************
 * make_str(s) returns string s as an entity.  make_strn(s,n)   *
 * returns the string of length n starting at address s, as	*
 * an entity.							*
 ****************************************************************/

ENTITY make_str(charptr s)
{
  if(s == NULL) return nil;
  return make_strn(s, strlen((char *)s));
}


/*---------------------------------------------------------------*/

ENTITY make_strn(charptr s, LONG n)
{
  charptr p;
  CHUNKPTR chunk;

  if(n == 0) return nil;

  chunk = allocate_binary(n);
  p     = STRING_BUFF(chunk);
  longmemcpy(p, s, n);
  return ENTP(STRING_TAG, chunk);
}


/****************************************************************
 *			MAKE_CSTR				*
 ****************************************************************
 * Return an entity with tag CSTR_TAG that holds string s.      *
 ****************************************************************/

ENTITY make_cstr(char *s)
{
  return ENTP(CSTR_TAG, stat_str_tb(s));
}


/****************************************************************
 *			MAKE_PERM_STR				*
 ****************************************************************
 * Return a copy of s, allocated in the heap.       		*
 * For SMALL_ENTITIES option, the result address is aligned	*
 * at a true long word boundary.				*
 ****************************************************************/

char* make_perm_str(char *s)
{
  int n    = strlen(s);
# ifdef SMALL_ENTITIES
    char* ss = bd_alloc(n+1, TRUE_LONG_ALIGN);
# else
    char* ss = alloc(n+1);
# endif
  strcpy(ss,s);
  return ss;
}


/****************************************************************
 *			STAT_STR_TB				*
 *			STAT_STR_TB1				*
 *			STAT_ID_TB				*
 ****************************************************************
 * Here for use with routines shared by translator, and for	*
 * keeping a string table.					*
 *								*
 * stat_str_tb(s) is a copy of s in the heap.  It is also       *
 * in the string hash table, so that strings are not		*
 * duplicated.							*
 *								*
 * For SMALL_ENTITIES option, the resulting string is at an	*
 * address that is at a true long word boundary.		*
 ****************************************************************/

char* stat_str_tb1(char *s, LONG hash)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  char *result;

  if(s == NULL) return NULL;

  u.str = s;
  h     = insert_loc_hash2(&string_table, u, hash, equalstr);

  if(h->key.str != NULL_S) return h->key.str;
  else {
    h->key.str = result = make_perm_str(s);
    h->val.num = -1;
    return result;
  }
}

/*-----------------------------------------------------*/

char* stat_str_tb(char *s) 
{
  return stat_str_tb1(s, strhash(s));
}

/*-----------------------------------------------------*/

char* stat_id_tb(char *s) 
{
  return stat_str_tb1(s, strhash(s));
}


/****************************************************************
 *			ID_TB0					*
 *			STRING_CONST_TB0			*
 *			ID_TB10					*
 *			STR_TB1					*
 ****************************************************************
 * Here for use with routines shared by translator.		*
 ****************************************************************/

char* id_tb0(char *s) {return stat_str_tb(s);}
char* string_const_tb0(char *s) {return stat_str_tb(s);}
char* id_tb10(char *s, LONG hash) {return stat_str_tb1(s, hash);}

char* str_tb1(char *s, int k_unused, LONG hash, Boolean t_unused) {
  return stat_str_tb1(s, hash);
}


/*****************************************************************
 *			NAME_TB				 	 *
 *****************************************************************
 * Enter string s into the id_names array and return its index.  *
 * If it is already there, just return the index.		 *
 *****************************************************************/

int name_tb(char *s)
{
  int i;

  for(i = 0; i < next_name_index; i++) {
    if(strcmp(id_names[i], s) == 0) return i;
  }

  if(next_name_index >= id_names_size) {
    if(id_names_size == 0) {
      id_names = (char**) alloc(4*sizeof(char*));
      id_names_size = 4;
    }
    else {
      LONG new_size = 2*id_names_size;
      id_names = (char**) reallocate((char *) id_names,
				     id_names_size * sizeof(char*),
				     new_size * sizeof(char*), TRUE);
      id_names_size = new_size;
    }
  }

  id_names[next_name_index] = stat_str_tb(s);
  return next_name_index++;
}
  

/*****************************************************************
 *			CONST_TB				 *
 *****************************************************************
 * Enter integer or rational constant s into the constant table, *
 * and return its index in constants.  kind is NAT_CON if s is   *
 * an integer constant, and RAT_CON if s is a rational constant. *
 *****************************************************************/

LONG const_tb(char *s, int kind)
{
  ENTITY e, c;
  LONG i;
  LONG t = LONG_MAX;

  if(next_const >= constants_size) reallocate_constants();

  e = (kind == NAT_CON) ?  ast_str_to_int(s) : ast_str_to_rat(s,TRUE);

  /*------------------------------------------------------*
   * If e is already in the table, then return its index. *
   *------------------------------------------------------*/

  for(i = 0; i < next_const; i++) {
    c = constants[i];
    if(TAG(c) == TAG(e) && IS_TRUE(ast_equal(c, e, &t))) {

#     ifdef DEBUG
        if(trace) {
	  trace_i(216, s, i);
	  trace_print_entity(e);
	  tracenl();
	}
#     endif

      return i;
    }
  }

  /*-----------------------------------------------------------------*
   * If we get here, then e needs to be added to the constant table. *
   *-----------------------------------------------------------------*/

# ifdef DEBUG
    if(trace) {
      trace_i(217, s, next_const);
      trace_print_entity(e);
      tracenl();
    }
# endif

  constants[next_const] = e;

  return next_const++;
}


/****************************************************************
 *			INTERN_STRING_STDF			*
 ****************************************************************
 * Return a CSTR_TAG string that is equal to s.			*
 * If s is a very long string, we will not intern it.		*
 ****************************************************************/

ENTITY intern_string_stdf(ENTITY s)
{
  if(TAG(s) == CSTR_TAG) return s;
  else {
    return make_lazy_prim(INTERN_TMO, s, 
			  make_lazy_prim(LENGTH_TMO, s, zero));
  }
}


/****************************************************************
 *			STRING_TB				*
 ****************************************************************
 * Enter string s into the constant table (as an entity), and   *
 * return its index in constants.				*
 ****************************************************************/

LONG string_tb(char *s)
{
  HASH2_CELLPTR c;
  HASH_KEY u;
  char *key;
  LONG h, val;

  /*-------------------------------*
   * Locate s in the string table. *
   *-------------------------------*/

  u.str = s;
  h     = strhash(s);
  c     = insert_loc_hash2(&string_table, u, h, equalstr);
  key   = c->key.str;
  val   = c->val.num;

  /*-------------------------------------------------------------*
   * If s is already there, and has a number, return its number. *
   *-------------------------------------------------------------*/

  if(key != NULL && val >= 0) return val;

  /*---------------------------------------------*
   * If s is not yet in the table, put it there. *
   *---------------------------------------------*/

  if(key == NULL) {
     c->key.str = make_perm_str(s);
  }

  /*-----------------------------------------------------------------*
   * Assign s an index, and put a record in constants at that index. *
   *-----------------------------------------------------------------*/

  if(next_const >= constants_size) reallocate_constants();
  constants[next_const] = ENTP(CSTR_TAG, c->key.str);
  return c->val.num = next_const++;
}


/****************************************************************
 *			REALLOCATE_CONSTANTS			*
 ****************************************************************
 * Double the size of the constants array.			*
 ****************************************************************/

PRIVATE void reallocate_constants(void)
{
  LONG new_size = 2*constants_size;

  constants = (ENTITY HUGEPTR) reallocate((char *) constants,
				    constants_size * sizeof(ENTITY),
				    new_size * sizeof(ENTITY), TRUE);
  constants_size = new_size;
}


/****************************************************************
 *			INIT_STR_TB				*
 ****************************************************************
 * Initialize the string and constants tables, but only if 	*
 * such initialization has not already been done.		*
 ****************************************************************/

void init_str_tb()
{
  if(string_table == NULL) string_table = create_hash2(9);

  if(constants == NULL) {
    next_const = 0;
    constants_size = CONSTANTS_SIZE_INIT;
    constants = (ENTITY HUGEPTR) alloc(constants_size*sizeof(ENTITY));
  }
}
