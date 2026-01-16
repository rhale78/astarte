/**********************************************************************
 * File:    rts/tostring.c
 * Purpose: Misc converters to string
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

/********************************************************
 * The following functions require fully evaluated 	*
 * arguments.  They convert their arguments to strings  *
 * that describe them.					*
 *							*
 * Function init_object_names must be called at         *
 * interpreter initialization. 				*
 ********************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
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
#include "../show/getinfo.h"
#include "../show/prtent.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE ENTITY bool_name[2], comparison_name[3];
PRIVATE ENTITY fileMode_name[3];
PRIVATE ENTITY boxflavor_name[2];
PRIVATE ENTITY copyflavor_name[3];
PRIVATE ENTITY hermit_name;

/********************************************************
 *			INIT_OBJECT_NAMES		*
 ********************************************************
 * Set up the names of standard entities.		*
 ********************************************************/

void init_object_names(void)
{
  hermit_name  			   = make_cstr("()");
  bool_name[0] 			   = make_cstr(std_id[FALSE_ID]);
  bool_name[1] 			   = make_cstr(std_id[TRUE_ID]);
  comparison_name[0] 		   = make_cstr(std_id[EQUAL_ID]);
  comparison_name[1] 		   = make_cstr(std_id[GREATER_ID]);
  comparison_name[2] 		   = make_cstr(std_id[LESS_ID]);
  boxflavor_name[0]		   = make_cstr(std_id[NONSHARED_ID]);
  boxflavor_name[1]		   = make_cstr(std_id[SHARED_ID]);
  copyflavor_name[0]		   = boxflavor_name[0];
  copyflavor_name[1]		   = boxflavor_name[1];
  copyflavor_name[2]		   = make_cstr(std_id[SAME_ID]);
  fileMode_name[VOLATILE_INDEX_FM] = make_cstr(std_id[VOLATILEMODE_ID]);
  fileMode_name[APPEND_INDEX_FM]   = make_cstr(std_id[APPENDMODE_ID]);
  fileMode_name[BINARY_INDEX_FM]   = make_cstr(std_id[BINARYMODE_ID]);
}


/********************************************************
 *			BOOL_TO_STRING			*
 ********************************************************
 * Return the name of Boolean entity a.			*
 ********************************************************/

ENTITY bool_to_string(ENTITY a)
{
  return bool_name[toint(VAL(a))];
}


/********************************************************
 *			COMPARISON_TO_STRING		*
 ********************************************************
 * Return the name of a, which is one of the values	*
 * of species Comparison.				*
 ********************************************************/

ENTITY comparison_to_string(ENTITY a)
{
  return comparison_name[toint(VAL(a))];
}

/********************************************************
 *			BOXFLAVOR_TO_STRING		*
 ********************************************************
 * Return the name of a, which is one of the values	*
 * of species BoxFlavor.				*
 ********************************************************/

ENTITY boxflavor_to_string(ENTITY a)
{
  return boxflavor_name[toint(VAL(a))];
}

/********************************************************
 *			COPYFLAVOR_TO_STRING		*
 ********************************************************
 * Return the name of a, which is one of the values	*
 * of species CopyFlavor.				*
 ********************************************************/

ENTITY copyflavor_to_string(ENTITY a)
{
  return copyflavor_name[toint(VAL(a))];
}

/********************************************************
 *			FILEMODE_TO_STRING		*
 ********************************************************
 * Return the name of a, which is one of the values	*
 * of species FileMode.					*
 ********************************************************/

ENTITY fileMode_to_string(ENTITY a)
{
  return fileMode_name[toint(VAL(a))];
}


/********************************************************
 *			HERMIT_TO_STRING		*
 ********************************************************
 * Return string "()".					*
 ********************************************************/

ENTITY hermit_to_string(ENTITY herm_unused)
{
  return hermit_name;
}


/********************************************************
 *			FILEMODE_TO_STRING		*
 ********************************************************
 * Return the name of species a.			*
 ********************************************************/

ENTITY species_dollar_stdf(ENTITY a)
{
  TYPE *t;
  ENTITY result;

# ifndef SMALL_STACK
    char temp[MAX_TYPE_STR_LEN];
# else
    char* temp = (char *) BAREMALLOC(MAX_TYPE_STR_LEN);
# endif

  if(TAG(a) != TYPE_TAG) die(191);

  t = TYPEVAL(a);
  set_in_binding_list();
  bump_type(t = freeze_type(t));
  get_out_binding_list();
  sprint_ty(temp, MAX_TYPE_STR_LEN, t);
  drop_type(t);	 
  result = make_str(temp);

# ifdef SMALL_STACK
    FREE(temp);
# endif

  return result;
}


/********************************************************
 *			EXCEPTION_TO_STRING		*
 ********************************************************
 * Return $(a), where a is an exception.		*
 ********************************************************/

ENTITY exception_to_string(ENTITY a)
{
  int n;
  TYPE *t, *tt;
  char *name, *p;
  ENTITY arg, dollar, dollar_result, result;
  REG_TYPE mark = reg3(&arg, &dollar, &dollar_result);

  n = qwrap_tag(a);
  if(n < 0 || n >= next_exception) die(77);

  /*--------------------------------------------------------------------*
   * An exception name might have the form package/realname.  Skip over *
   * "package/" for printing. 						*
   *--------------------------------------------------------------------*/

  name = exception_data[n].name;
  p    = strchr(name, '/');
  if(p != NULL) name = p+1;
  
  /*---------------------------------------------------------------*
   * If this exception has no associated value, just get the name. *
   *---------------------------------------------------------------*/

  if(exception_data[n].type_instrs == NULL) {
  simple:
    failure           = -1; 
    failure_as_entity = NOTHING;
    result            = make_cstr(name);
    unreg(mark);
    return result;
  }

  /*------------------------*
   * Get the argument type. *
   *------------------------*/

  eval_type_instrs(exception_data[n].type_instrs, NULL);
  tt = pop_type_stk();  /* ref from type_stack */
  t  = force_ground(tt);
  drop_type(tt);

  /*-----------------------------------------------------*
   * If the argument type is (), treat like no argument. *
   *-----------------------------------------------------*/

  if(is_hermit_type(t)) goto simple;

  /*-------------------------------------------*
   * Convert the argument to a string using $. *
   *-------------------------------------------*/

  arg = qwrap_val(a);
  if(in_show && TAG(arg) != APPEND_TAG && is_lazy(arg)) {
    char str[MAX_LAZY_NAME_LENGTH];
    dollar_result = make_str(lazy_name(str, arg));
  } 
  else {
    LONG time = PRINT_ENT_TIME;
    bump_type(t = function_t(t, string_type));
    dollar    = get_and_eval_global(DOLLAR_SYM, t, &time);
    drop_type(t);
    time = PRINT_ENT_TIME;
    if(failure >= 0) goto simple;
    dollar_result = run_fun(dollar, arg, the_act.state_a, the_act.trap_vec_a, 
			    &time);
    if(failure >= 0) goto simple;
  }

  /*----------------------------------*
   * Build the exception description. *
   *----------------------------------*/

  result = quick_append(make_cstr(name), 
		      ast_pair(ENTCH('('), 
			       tree_append(dollar_result,
					    make_cstr(")"))));
  unreg(mark);
  return result;
}


