/**********************************************************************
 * File:    machdata/wrap.c
 * Purpose: Handling of wrapped entities
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
 * This file provides support for managing wrapped entities.		*
 * The entities can either be wrapped with a type (tag WRAP_TAG)        *
 * or with a nonnegative integer (tag WRAP_TAG or QWRAP0_TAG,...        *
 * QWRAP3_TAG.								*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/rdwrt.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../tables/tables.h"
#include "../clstbl/typehash.h"
#include "../show/getinfo.h"
#include "../show/prtent.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/********************************************************
 *			WRAP    			*
 ********************************************************
 * Return the result of tagging e with type t.          *
 *							*
 * In order to do this, type t must not have any 	*
 * unbound variables in it.  Any unbound variables	*
 * are defaulted.					*
 *							*
 * A wrapped value is not wrapped again.  It is returned*
 * unchanged.						*
 ********************************************************/

ENTITY wrap(ENTITY e, TYPE *t)
{
  /*------------------------------------------------------------*
   * If e is already wrapped, then don't rewrap.		*
   *------------------------------------------------------------*/

  if(TAG(e) == WRAP_TAG) {
    ENTITY hd = *ENTVAL(e);
    if(TAG(hd) == TYPE_TAG) return e;
  }

  /*-------------------------*
   * Otherwise, do the wrap. *
   *-------------------------*/

  {ENTITY* w = allocate_entity(2);
   w[0] = ENTP(TYPE_TAG, force_ground(t)); 
   w[1] = e;
   return ENTP(WRAP_TAG, w);
  }
}


/********************************************************
 *			DOMAIN_WRAP    			*
 ********************************************************
 * Return the result of tagging e with type t->TY1.     *
 ********************************************************/

ENTITY domain_wrap(ENTITY e, TYPE *t)
{
  return wrap(e, FIND_U_NONNULL(t)->TY1);
}


/********************************************************
 *			QWRAP    			*
 ********************************************************
 * Return the result of tagging e with tag discr.       *
 ********************************************************/

ENTITY qwrap(int discr, ENTITY e)
{
  ENTITY *w;

  /*--------------------------------------*
   * For small tags, use QWRAP0_TAG, etc. *
   *--------------------------------------*/

  if(discr <= 3) {
    w  = allocate_entity(1);
    *w = e;
    return  ENTP(QWRAP0_TAG + discr, w);
  }

  /*-------------------------------------------*
   * For larger tags, use the general WRAP_TAG *
   *-------------------------------------------*/

  else { /* discr > 3 */
    w    = allocate_entity(2);
    w[0] = ENTU(discr);
    w[1] = e;
    return ENTP(WRAP_TAG, w);
  }
}


/*********************************************************
 *			QUNWRAP_I			 *
 *********************************************************
 * Unwrap *a, putting the discriminator in discr and the *
 * value that is wrapped in *a.  In the event of 	 *
 * time-out, put the promise for the computation of *a	 *
 * in *a.     	 					 *
 *********************************************************/

void qunwrap_i(int *discr, ENTITY *a, LONG *time_bound)
{
  ENTITY e, *w;
  int tag;
  REGPTR_TYPE ptrmark = reg1_ptrparam(&a);

  /*---------------------------------------------------------------------*
   * Evaluate *a.  If evaluation fails, give up.			 *
   *									 *
   * Note: e is not registered because there are no ops after definition *
   * of e that can allocate memory. 					 *
   *---------------------------------------------------------------------*/

  SET_EVAL(e, *a, time_bound);  /* e = eval(*a, time_bound); */

  if(failure >= 0) {
    *discr = 0;
    *a     = e;
    unregptr(ptrmark);
    return;
  }

  tag = TAG(e);

  /*------------------------------------------------------------------*
   * Handle () wrapped with a numeric tag.  It will be in a NOREF_TAG *
   * entity. 							      *
   *------------------------------------------------------------------*/

  if(tag == NOREF_TAG) {
    *discr = toint(VAL(e));
    *a     = hermit;
  }

  /*-------------------------*
   * Handle QWRAP0_TAG, etc. *
   *-------------------------*/

  else if(QWRAP0_TAG <= tag && tag <= QWRAP3_TAG) {
    *discr = tag - QWRAP0_TAG;
    *a     = *ENTVAL(e);
  }

  /*------------------*
   * Handle WRAP_TAG. *
   *------------------*/

  else {
    w      = ENTVAL(e);
    if(TAG(w[0]) != NOREF_TAG) die(76);
    *discr = toint(NRVAL(w[0]));
    *a     = w[1];
  }
  unregptr(ptrmark);
}


/********************************************************
 *			POSITION_STDF			*
 ********************************************************
 * Return qwrap_tag(a), as an entity.			*
 ********************************************************/

ENTITY position_stdf(ENTITY a)
{
  return ENTI(qwrap_tag(a));
}


/********************************************************
 *			QWRAP_TAG			*
 ********************************************************
 * Return the tag wrapped with a.			*
 ********************************************************/

int qwrap_tag(ENTITY a)
{
  ENTITY av;

  /*-----------------------------------------------*
   * Check for a NOREF_TAG value, which is the     *
   * qwrap-tag itself, with () as data.		   *
   *-----------------------------------------------*/

  int tag = TAG(a);
  if(tag == NOREF_TAG) return toint(VAL(a));

  /*---------------------------------------*
   * Handle QWRAP0_TAG through QWRAP3_TAG. *
   *---------------------------------------*/

  if(QWRAP0_TAG <= tag && tag <= QWRAP3_TAG) return tag - QWRAP0_TAG;

  /*---------------------------------------*
   * Handle WRAP_TAG by looking at the     *
   * cell pointed to for the tag.	   *
   *---------------------------------------*/

  if(tag != WRAP_TAG) die(76);

  av = *ENTVAL(a);
  if(TAG(av) != NOREF_TAG) die(76);

  return toint(VAL(av));
}


/********************************************************
 *			QWRAP_VAL			*
 ********************************************************
 * Return the value wrapped with a.			*
 ********************************************************/

ENTITY qwrap_val(ENTITY a)
{
  int tag = TAG(a);

  /*-----------------------------------------------*
   * Check for a NOREF_TAG value, which is the     *
   * qwrap-tag itself, with () as data.		   *
   *-----------------------------------------------*/

  if(tag == NOREF_TAG) return hermit;

  /*---------------------------------------*
   * Handle QWRAP0_TAG through QWRAP3_TAG. *
   *---------------------------------------*/

  if(QWRAP0_TAG <= tag && tag <= QWRAP3_TAG) return *ENTVAL(a);

  /*---------------------------------------*
   * Handle WRAP_TAG by looking at the     *
   * second member of the pair pointed to. *
   *---------------------------------------*/

  if(tag != WRAP_TAG) die(76);
  return ENTVAL(a)[1];
}
