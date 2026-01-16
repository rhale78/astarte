/****************************************************************
 * File:    clstbl/m_dflt.c
 * Purpose: Functions for managing table of defaults for interpreter
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
 * This file contain functions that manage defaults for the class table *
 * in the interpreter.							*
 ************************************************************************/

#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../intrprtr/intrprtr.h"
#include "../classes/classes.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/m_dflt.h"
#include "../clstbl/typehash.h"
#include "../error/error.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/********************************************************
 *		   ALREADY_PRESENT_DEFAULT		*
 ********************************************************
 * Return true if a default pair [addr,dflt] is present *
 * in list L, and false otherwise.			*
 *							*
 * List L has alternating addr, dflt values in it. 	*
 * That is, it looks like [addr1,dflt1,addr2,dflt2,...].*
 ********************************************************/

PRIVATE Boolean 
already_present_default(char *addr, TYPE *dflt, LIST *L)
{
  LIST *p;
  for(p = L; p != NIL; p = p->tail->tail) {
    if(p->head.str == addr && p->tail->head.type == dflt) {
      return TRUE;
    }
  }
  return FALSE;
}


/********************************************************
 *		   INSTALL_RUNTIME_DEFAULT_TM		*
 ********************************************************
 * Install dflt as a default for the genus or 		*
 * community described by ctc.  packg is the package    *
 * info structure for the package that is doing this    *
 * default.						*
 ********************************************************/

void install_runtime_default_tm(CLASS_TABLE_CELL* ctc, TYPE *dflt, 
				struct pack_params *packg)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  char *addr;

  /*--------------------------------------------------------------------*
   * ctc->CTC_DEFAULT_TBL is a hash table associating, with each	*
   * package number, a list of the form [addr1,dflt1,addr2,dflt2,...],	*
   * giving defaults for this genus or community along with the		*
   * code addresses where they were defined in the byte code.		*
   *--------------------------------------------------------------------*/

  u.num = packg->num;
  h = insert_loc_hash2(&(ctc->CTC_DEFAULT_TBL), u, inthash(u.num), eq);
  if(h->key.num == 0) {
    h->key.num  = u.num;
    h->val.list = NIL;
  }

  addr = (char *) (packg->start + packg->current);
  dflt = type_tb(dflt);
  if(!already_present_default(addr, dflt, h->val.list)) {
    SET_LIST(h->val.list,
	     str_cons(addr, type_cons(dflt, h->val.list)));
  }
}


/********************************************************
 *			GET_RUNTIME_DEFAULT		*
 ********************************************************
 * Return the appropriate run-time default for the      *
 * genus or community described by table entry ctc,     *
 * at location pc in the byte code.			*
 ********************************************************/

TYPE* get_runtime_default_tm(CLASS_TABLE_CELL* ctc, UBYTE *pc)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  struct package_descr *pd;
  LIST *p;

  /*--------------------------------------------------------*
   * The ctc->CTC_DEFAULT_TBL has precedence, if it exists. *
   * Otherwise, use ctc->CTC_DEFAULT.			    *
   *--------------------------------------------------------*/

  if(ctc->CTC_DEFAULT_TBL == NULL) return ctc->CTC_DEFAULT;

  pd = get_pd_entry(pc);
  u.num = pd->num;
  h = locate_hash2(ctc->CTC_DEFAULT_TBL, u, inthash(u.num), eq);
  if(h->key.num == 0) return ctc->CTC_DEFAULT;

  /*------------------------------------------------------*
   * Only take an entry if it was made prior to pc in the *
   * byte code.						  *
   *------------------------------------------------------*/

  for(p = h->val.list; p != NIL; p = p->tail->tail) {
    if(p->head.str <= (char *) pc) {
      return p->tail->head.type;
    }
  }

  return ctc->CTC_DEFAULT;
}


/************************************************************************
 *			DYNAMIC_DEFAULT					*
 ************************************************************************
 * Return the appropriate default for variable V.			*
 ************************************************************************/

TYPE* dynamic_default(TYPE *V)
{
  TYPE_TAG_TYPE V_kind = TKIND(V);
  if(IS_WRAP_VAR_T(V_kind) || V->PREFER_SECONDARY) {
    return wrap_tf(V->ctc);
  }
  else {
    TYPE* dflt = get_runtime_default_tm(V->ctc, the_act.program_ctr);
    if(dflt == NULL) return wrap_tf(V->ctc);
    else return dflt;
  }
}

