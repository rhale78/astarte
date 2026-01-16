/**************************************************************
 * File:    classes/printty.c
 * Purpose: Print types
 * Author:  Karl Abrahamson
 **************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/****************************************************************
 * The functions here are for printing types in relatively	*
 * pretty form.  File debug/dprintty.c contains functions for	*
 * printing TYPE structures in detailed form, for debugging.	*
 ****************************************************************/

#include <string.h>
#include <ctype.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/lists.h"
#include "../unify/unify.h"
#include "../unify/constraint.h"
#include "../classes/classes.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/cnstrlst.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../error/error.h"
#include "../parser/tokens.h"
#include "../show/gprint.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE void collect_constraints(TYPE *V, PRINT_TYPE_CONTROL *c);
PRIVATE void print_constraints(PRINT_TYPE_CONTROL *c, int n);

/*----------------------------------------------------------------*
 * Printing of variables is controlled by a PRINT_TYPE_CONTROL    *
 * structure.  The fields are as follows.			  *
 *								  *
 * ty_b		A table binding the address of a variable to	  *
 *		a character used to display that variable.  If	  *
 *		variable V is bound to character 'x', then V is	  *
 *		printed as `a, or G`x where G is the domain of V. *
 *								  *
 * constraints	  is a list of pair types (V,T), where such a 	  *
 *		  pair indicates a constraint V >= T (when the    *
 *		  tag is TYPE_L) or V >>= T (when the tag is	  *
 *                TYPE1_L).  During printing, the constraints	  *
 *		  that are encountered are accumulated in this	  *
 *		  variable.  At the end of printing, the	  *
 *		  constraints are shown.			  *
 *								  *
 * next_qual_num is the next number to use for a variables.       *
 *		 When this is 1, the next variable will be shown  *
 *		 as `a, when this is 2, it will be shown as `b,	  *
 *		 etc.						  *
 *								  *
 *		 if qual_num is negative, then its absolute value *
 *		 is used to get the string, but the string will	  *
 *		 be preceded by a 'z'.  This is used to give	  *
 *		 different names to variables that others in a 	  *
 *		 given context.					  *
 *								  *
 * chars_printed The number of characters that have been printed  *
 *               using this control.  This is only kept up to     *
 *		 date in the interpreter.			  *
 *----------------------------------------------------------------*/

/******************************************************************
 *			show_hermit_functions			  *
 ******************************************************************
 * When show_hermit_functions is nonzero, functions types that    *
 * can be unified with () are shown using =>.			  *
 ******************************************************************/

int show_hermit_functions = 0;


/******************************************************************
 *			BEGIN_PRINTING_TYPES			  *
 *			BEGIN_PRINTING_TYPES_UNMARKED		  *
 ******************************************************************
 * Call begin_printing_types(F,C) to begin printing possibly      *
 * several types, using the same names for variables in each type.*
 * C is the structure that is to be used to control printing of   *
 * variables, and F is the file or string on which to print.	  *
 * Structure C is initialized here.                               *
 * 								  *
 * Begin_printing_types_unmarked(F,C) is similar, but it requests *
 * that marks not be shown.  NOTE: Currently, marks are never	  *
 * shown anyway.						  *
 ******************************************************************/

void begin_printing_types(FOS *f, PRINT_TYPE_CONTROL *c)
{
  c->f		    = f;
  c->ty_b           = NULL;
  c->constraints    = NIL;
  c->next_qual_num  = 1;
  c->chars_printed  = 0;
  c->print_marks    = 1;
}

/*--------------------------------------*/

void begin_printing_types_unmarked(FOS *f, PRINT_TYPE_CONTROL *c)
{
  begin_printing_types(f,c);
  c->print_marks = 0;
}

/******************************************************************
 *			BEGIN_PRINTING_ALT_TYPES		  *
 *			BEGIN_PRINTING_ALT_TYPES_UNMARKED	  *
 ******************************************************************
 * Same as begin_printing_types, but it calls for alternate names *
 * (starting with z) to be used for variables.			  *
 ******************************************************************/

void begin_printing_alt_types(FOS *f, PRINT_TYPE_CONTROL *c)
{
  c->f		    = f;
  c->ty_b           = NULL;
  c->constraints    = NIL;
  c->next_qual_num  = -1;
  c->chars_printed  = 0;
  c->print_marks    = 1;
}

/*-----------------------------------*/

void begin_printing_alt_types_unmarked(FOS *f, PRINT_TYPE_CONTROL *c)
{
  begin_printing_alt_types(f,c);
  c->print_marks = 0;
}


/******************************************************************
 *			END_PRINTING_TYPES			  *
 ******************************************************************
 * Call end_printing_types(C) to end printing (possibly) several  *
 * types, using the same names for variables in each type.  C     *
 * is the structure that has been used to control printing of     *
 * variables.							  *
 ******************************************************************/

void end_printing_types(PRINT_TYPE_CONTROL *c)
{
  print_constraints(c,-1);
  free_hash2(c->ty_b);
  c->ty_b = NULL; 
}


/******************************************************************
 *			FPRINT_TY				  *
 *			FPRINT_TY_UNMARKED			  *
 *			FPRINT_TY_WITHOUT_CONSTRAINTS		  *
 *			FPRINT_TY_WITHOUT_CONSTRAINTS_UNMARKED	  *
 ******************************************************************
 * fprint_ty(F, T) prints type T on file F, in short format.  So, *
 * for example, (`a,Natural) is printed as (`a,Natural).	  *
 *								  *
 * For the interpreter, F can be a window number.		  *
 *								  *
 * The return value is 0 for the compiler and the number of       *
 * characters printed for the interpreter.			  *
 *								  *
 * The other versions select whether constraints should be shown, *
 * and whether marks should be shown.  fprint_ty prints both      *
 * constraints and marks.  NOTE: Currently, marks are never	  *
 * shown anyway.	      					  *
 ******************************************************************/

PRIVATE int 
fprint_ty_help(FILE *f, TYPE *t, Boolean show_the_constraints,
	       Boolean print_marks)
{
  PRINT_TYPE_CONTROL c;

# ifdef MACHINE
    FILE_OR_STR ff;
    ff.offset = -1;
    ff.u.file = f;
    begin_printing_types(&ff, &c);
# else
    begin_printing_types(f, &c);
# endif
  c.print_marks = print_marks;

  if(show_the_constraints) print_ty1_with_constraints(t, &c);
  else {
    print_ty1_without_constraints(t, &c);
    SET_LIST(c.constraints, NIL);
  }
  end_printing_types(&c);
  return c.chars_printed;
}

/*--------------------------------------------------------------*/

int fprint_ty(FILE *f, TYPE *t) 
{
  return fprint_ty_help(f, t, TRUE, TRUE);
}

/*--------------------------------------------------------------*/

int fprint_ty_unmarked(FILE *f, TYPE *t) 
{
  return fprint_ty_help(f, t, TRUE, FALSE);
}

/*--------------------------------------------------------------*/

int fprint_ty_without_constraints(FILE *f, TYPE *t)
{
  return fprint_ty_help(f, t, FALSE, TRUE);
}

/*--------------------------------------------------------------*/

int fprint_ty_without_constraints_unmarked(FILE *f, TYPE *t)
{
  return fprint_ty_help(f, t, FALSE, FALSE);
}


/******************************************************************
 *		FPRINT_TY_WITH_CONSTRAINTS_INDENTED		  *
 ******************************************************************
 * Print type T on file F, in short format, with each constraint  *
 * on a separate line, indented N spaces.  This is only defined   *
 * for the translator.						  *
 *								  *
 * The return value is 0.  If this becomes defined for the	  *
 * interpreter, then the return value should be the number of     *
 * characters printed.						  *
 ******************************************************************/

#ifdef TRANSLATOR
int fprint_ty_with_constraints_indented(FILE *f, TYPE *t, int n)
{
  PRINT_TYPE_CONTROL c;

  begin_printing_types(f, &c);
  print_ty1_with_constraints_indented(t, &c, n);
  end_printing_types(&c);
  return c.chars_printed;
}
#endif


/********************************************************************
 *			SPRINT_TY				    *
 ********************************************************************
 * sprint_ty(S, LEN, T) prints type T on string buffer S, which has *
 * length LEN, in short format.					    *
 *								    *
 * This function is currently only used by the interpreter.	    *
 ********************************************************************/

#ifdef MACHINE
void sprint_ty(char *s, int len, TYPE *t)
{
  PRINT_TYPE_CONTROL c;
  FILE_OR_STR fors;

  fors.u.str = s;
  fors.offset = 0;
  fors.len = len-1;  /* Leave room for null at end of string */
  *s = '\0';
  begin_printing_types(&fors, &c);
  print_ty1_with_constraints(t, &c);
  end_printing_types(&c);
}
#endif


/*****************************************************************
 *			QUAL_NUM_TO_QUAL_STRING			 *
 *****************************************************************
 * Convert qualifier number N to a string, and put the string    *
 * into buffer S.  The mapping is as follows.			 *
 *								 *
 *    n          s						 *
 *   ---       ----						 *
 *    1         "a"						 *
 *    2		"b"						 *
 *    ...	...						 *
 *    25	"y"						 *
 *    26	"ab"						 *
 *    27	"bb"						 *
 *    28	"cb"						 *
 *    ...	...						 *
 *								 *
 * n should be a positive integer.				 *
 *****************************************************************/

PRIVATE void 
qual_num_to_qual_string(int n, char *s)
{
  n--;
  if(n == 0) strcpy(s, "a");
  else {
    char* p = s;
    while(n != 0) {
      *(p++) = 'a' + (n % 25);
      n = n/25;
    }
    *p = 0;
  }
}


/*****************************************************************
 *			FPRINT_QUALIFIER			 *
 *****************************************************************
 * Print `s or ``s or `1s or `2s, etc, on controller C, as the 	 *
 * qualifier for type variable T.  Number N tells how to print	 *
 * the qualifier.  Its absolute  value is the number to use in   *
 * qual_num_to_qual_string to get the qualifier string, and its  *
 * sign tells whether an alternate form should be used.		 *
 *****************************************************************/

PRIVATE void fprint_qualifier(PRINT_TYPE_CONTROL *c, int n, TYPE *t)
{
  char qual_str[10];
  char *norestrictor, *indicator, *z;
  TYPE_TAG_TYPE t_kind = TKIND(t);

  Boolean is_wrapped = IS_WRAP_VAR_T(t_kind);
  Boolean is_primary = IS_PRIMARY_VAR_T(t_kind);

  qual_num_to_qual_string(absval(n), qual_str);
  norestrictor = (t->norestrict) ? "~" : "";
  indicator    = is_wrapped ? "`" : is_primary ? "*" : "";
  if(n > 0) {
    if(t->PREFER_SECONDARY) qual_str[0] = toupper(qual_str[0]);
    z = "";
  }
  else z = (t->PREFER_SECONDARY) ? "Z" : "z";

  FPRINTF(c->f, "`%s%s%s%s", indicator, norestrictor, z, qual_str);

# ifdef MACHINE
    c->chars_printed += 1 + strlen(indicator) + strlen(norestrictor)
			+ strlen(z) + strlen(qual_str);
# endif
}


/****************************************************************
 * 			PRINT_VARIABLE				*
 ****************************************************************
 * Print variable T according to controller C, entering it into *
 * the table of controller C if necessary.  			*
 *								*
 * Return FALSE if this is a previously encountered variable,   *
 * and TRUE if this is the first time this variable has been    *
 * encountered in this print.					*
 *								*
 * This function will print pseudo-variables, when TRANSLATOR   *
 * is defined.  For pseudo-variables, it returns FALSE.		*
 ****************************************************************/

PRIVATE Boolean print_variable(TYPE *t, PRINT_TYPE_CONTROL *c)
{
  HASH_KEY u;
  HASH2_CELLPTR h;
  Boolean result;

  FOS* f = c->f;

  /*--------------------------------------------------------*
   * Handle pseudo-type variables, which are created by the *
   * choose-matching completeness checker.                  *
   *--------------------------------------------------------*/

# ifdef TRANSLATOR
    if(t->special) {
      FPRINTF(f, "`#%d", t->TOFFSET);
      result = FALSE;
    }
    else 
# endif

  /*---------------------------*
   * Handle normal variables.  *
   *---------------------------*/

    {

     /*---------------------------------------------------------*
      * Print the genus or community name, but don't print ANY. *
      *---------------------------------------------------------*/

     if(t->ctc != NULL) {
       char* name = name_tail(t->ctc->name);
       FPRINTF(f, "%s", name);
#      ifdef MACHINE
         c->chars_printed += strlen(name);
#      endif
     }

     /*-------------------------------------------------------------*
      * Get the qualifier, either out of the table if this variable *
      * has been encountered before or as an as-yet unused          *
      * qualifier. Print the qualifier.                             *
      *-------------------------------------------------------------*/
    
     u.num = tolong(t);
     h     = insert_loc_hash2(&(c->ty_b), u, inthash(u.num), eq);
     if(h->key.num != 0) {
       result = FALSE;
       fprint_qualifier(c, toint(h->val.num), t);
     }
     else {
       int qual_num = c->next_qual_num;
       result = TRUE;
       h->key.num = tolong(t);
       h->val.num = qual_num;
       fprint_qualifier(c, qual_num, t);
       if(qual_num > 0) c->next_qual_num++;
       else c->next_qual_num--;
     }

     /*-----------------------------------------------------------------*
      * When tracing, show the address where the variable is stored,	*
      * plus a mark if the variable is marked nosplit.			*
      *-----------------------------------------------------------------*/

#    ifdef DEBUG
       if(trace) {
	 if(t->nosplit) FPRINTF(f, "('%p)", t);
	 else FPRINTF(f, "(%p)", t);
       }
#    endif

   }
   return result;
}


/****************************************************************
 * 			ADD_ALL_CONSTRAINTS			*
 ****************************************************************
 * Add the constraints that apply to variables in type T to     *
 * the constraint list of controller C.				*
 ****************************************************************/

PRIVATE void add_all_constraints(TYPE *t, PRINT_TYPE_CONTROL *c)
{
  TYPE_TAG_TYPE t_kind;

 tail_recur:
  t = find_u(t);
  if(t == NULL) return;

  t_kind = TKIND(t);
  if(IS_STRUCTURED_T(t_kind)) {
    add_all_constraints(t->TY1, c);
    t = t->TY2;
    goto tail_recur;
  } 
  else if(IS_VAR_T(t_kind)) {
    collect_constraints(t, c);
  }
}


/****************************************************************
 * 			COLLECT_CONSTRAINTS			*
 ****************************************************************
 * Add the constraints on variable V to control C.  Also add    *
 * any constraints that are found inside the constraints.	*
 ****************************************************************/

PRIVATE void collect_constraints(TYPE *V, PRINT_TYPE_CONTROL *c)
{
  TYPE_LIST *p;
  TYPE* this_constraint;

  for(p = lower_bound_list(V); p != NIL; p = p->tail) {
    if(LKIND(p) != TYPE2_L) {
      bump_type(this_constraint = pair_t(V, p->head.type));

      if(!type_mem(this_constraint, c->constraints, LKIND(p))) {
        HEAD_TYPE hd;
        hd.type = this_constraint;
        SET_LIST(c->constraints, 
	         general_cons(hd, c->constraints, LKIND(p)));
        add_all_constraints(p->head.type, c);
      }
      drop_type(this_constraint);
    }
  }
}


/****************************************************************
 * 			PRINT_CONSTRAINTS			*
 ****************************************************************
 * Print constraints t >= x or t >>= x for each pair (t,x) in	*
 * the constraint list of C.					*
 *								*
 * After the constraints are printed, C's constraint list is 	*
 * set to NIL to prevent printing them again.			*
 *								*
 * For the translator:						*
 *  If N < 0, then print them all without line breaks.		*
 *  If N >= 0, then print each constraint on a separate line,	*
 *  indented N spaces.						*
 *								*
 * For the interpreter, N is ignored.				*
 ****************************************************************/

PRIVATE void print_constraints(PRINT_TYPE_CONTROL *c, int n)
{
  TYPE_LIST *p;
  Boolean need_comma = FALSE;
  FOS* f = c->f;

  if(c->constraints != NIL) {
#   ifdef TRANSLATOR
      if(n >= 0) {fnl(f); findent(f,n);}
      else FPRINTF(f, " ");
#   else
      FPRINTF(f, " ");
#   endif
    FPRINTF(f, "constraint(");
#   ifdef MACHINE
      c->chars_printed += 11;
#   endif
    SET_LIST(c->constraints, reverse_list(c->constraints));
    for(p = c->constraints; p != NIL; p = p->tail) {
      if(LKIND(p) != TYPE2_L) {
        TYPE* pr  = p->head.type;
        char *rel;
#       ifdef MACHINE
          int rel_len;
#       endif

        if(LKIND(p) == TYPE_L) {
	  rel     = " >= ";
#         ifdef MACHINE
	    rel_len = 4;
#         endif
	}
	else {
	  rel     = " >>= ";
#         ifdef MACHINE
	    rel_len = 5;
#         endif
	}
	
	if(need_comma) {
	  FPRINTF(f, ", ");
#         ifdef MACHINE
            c->chars_printed += 2;
#         endif
	}
        print_variable(pr->TY1, c);
        FPRINTF(f, rel);
#       ifdef MACHINE
          c->chars_printed += rel_len;
#       endif
        print_rt1_without_constraints(pr->TY2, NULL, c);
	need_comma = TRUE;
      }
    }
    FPRINTF(f, ")");
#   ifdef MACHINE
      c->chars_printed++;
#   endif
    SET_LIST(c->constraints, NIL);
  }
}


/****************************************************************
 *			PRINT_FUNCTION				*
 ****************************************************************
 * Print function type T with role R using controller C.	*
 ****************************************************************/

PRIVATE void print_function(TYPE *t, ROLE *r, PRINT_TYPE_CONTROL *c)
{
  ROLE* left_role  = NULL;
  ROLE* right_role = NULL;
  FOS*  f = c->f;

# ifdef TRANSLATOR
    if(r != NULL && RKIND(r) == FUN_ROLE_KIND) {
      left_role = r->role1;
      right_role = r->role2;
    }
# endif

  FPRINTF(f, "(");
  print_rt1_without_constraints(t->TY1, left_role, c);

# ifdef TRANSLATOR
#   ifdef DEBUG
      if(trace || show_hermit_functions) {
        FPRINTF(f, t->hermit_f ? " => " : " -> ");
      }
      else {
        FPRINTF(f, " -> ");
      }
#   else
      if(show_hermit_functions && t->hermit_f) {
	  FPRINTF(f, " => ");
      }
      else {
        FPRINTF(f, " -> ");
      }
#   endif
# else
    FPRINTF(f, " -> ");
# endif

  print_rt1_without_constraints(t->TY2, right_role, c);
  FPRINTF(f, ")");
# ifdef MACHINE
    c->chars_printed += 6;
# endif
}


/****************************************************************
 *			PRINT_PAIR				*
 ****************************************************************
 * Print pair type T with role R, using controller C.		*
 ****************************************************************/

PRIVATE void print_pair(TYPE *t, ROLE *r, PRINT_TYPE_CONTROL *c)
{
  ROLE* left_role  = NULL;
  ROLE* right_role = NULL;
  FOS*  f = c->f;

# ifdef TRANSLATOR
    if(r != NULL && RKIND(r) == PAIR_ROLE_KIND) {
      left_role = r->role1;
      right_role = r->role2;
    }
# endif

  FPRINTF(f, "(");
  print_rt1_without_constraints(t->TY1, left_role, c);
  FPRINTF(f, ",");
  print_rt1_without_constraints(t->TY2, right_role, c);
  FPRINTF(f, ")");
# ifdef MACHINE
   c->chars_printed += 3;
# endif
}


/****************************************************************
 *			PRINT_FAM_MEM				*
 ****************************************************************
 * Print family member t using controller C.			*
 ****************************************************************/

PRIVATE void print_fam_mem(TYPE *t, PRINT_TYPE_CONTROL *c)
{      
  TYPE* fam = find_u(t->TY2);
  FOS*  f = c->f;

  /*---------------------------------------------*
   * Print List(T) as [T] and Box(T) as <:T:>. 	 *
   *---------------------------------------------*/

  if(fam != NULL_T && TKIND(fam) == FAM_T) {
    if(fam->ctc == list_fam->ctc) {
      FPRINTF(f, "[");
      print_rt1_without_constraints(t->ty1, NULL, c);
      FPRINTF(f, "]");
#     ifdef MACHINE
        c->chars_printed += 2;
#     endif
      return;
    }
    else if(fam->ctc == box_fam->ctc) {
      FPRINTF(f, "<:");
      print_rt1_without_constraints(t->ty1, NULL, c);
      FPRINTF(f, ":>");
#     ifdef MACHINE
        c->chars_printed += 4;
#     endif
      return;
    }
  }

  /*--------------------------------------------*
   * Handle a family member that is not a list. *
   *--------------------------------------------*/

  print_rt1_without_constraints(t->TY2, NULL, c);
  FPRINTF(f, "(");
  print_rt1_without_constraints(t->TY1, NULL, c);
  FPRINTF(f, ")");
# ifdef MACHINE
    c->chars_printed += 2;
# endif
}


/****************************************************************
 *			PRINT_TYPE_ID				*
 ****************************************************************
 * Print type, family, genus or community identifier T on       *
 * controller C.  If t has a NULL ctc field, print ANY.		*
 *								*
 * Handle the case where T is a pseudo-type, created by the	*
 * choose-matching exhaustiveness tester.			*
 ****************************************************************/

PRIVATE void print_type_id(PRINT_TYPE_CONTROL *c, TYPE *t)
{
  if(t->ctc == NULL) {

    /*---------------------------------------------------------*
     * Check for a pseudo-type, created by the choose-matching *
     * exhaustiveness checker.                                 *
     *---------------------------------------------------------*/

#   ifdef TRANSLATOR
      if(t->special) fprintf(c->f, "#%d", t->TOFFSET);
      else
#   endif

      {FPRINTF(c->f, "ANY");
#      ifdef MACHINE
         c->chars_printed += 3;
#      endif
      }
  }

  else {
    char* name = nonnull(name_tail(t->ctc->name));
    FPRINTF(c->f, "%s", name);
#   ifdef MACHINE
      c->chars_printed += strlen(name);
#   endif
  }
}


/****************************************************************
 *			PRINT_RT1_WITHOUT_CONSTRAINTS		*
 ****************************************************************
 * print_rt1_without_constraints(T, R, CTL) prints type T with	*
 * role R, in short format, with print controller CTL.		*
 * Additionally, collect any constraints that are encountered   *
 * in the constraint list of controller CTL.			*
 *								*
 * Structure CTL should be initialized before calling		*
 * print_rt1_without_constraints.  See begin_printing_types.	*
 *								*
 * Note: For the interpreter, roles are not known.  Printing    *
 * of roles is suppressed.					*
 *								*
 * Note: print_rt1_without_constraints will print pseudo-types	*
 * created by the choose-matching completeness checker.		*
 *								*
 * Note: The prmark field of t is used to detect loops.  It     *
 * should not be used for any other purpose.			*
 ****************************************************************/

void print_rt1_without_constraints(TYPE *t, ROLE *r, PRINT_TYPE_CONTROL *ctl)
{
  Boolean marked;
  FOS* f = ctl->f;

  /*-------------------------------*
   * Skip through bound variables. *
   *-------------------------------*/

  t = find_mark_u(t, &marked);

  /*------------------------*
   * Print the role labels. *
   *------------------------*/

# ifdef TRANSLATOR
    if(r != NULL && r->namelist != NULL) {
      print_namelist(f, r->namelist);
      FPRINTF(f, "~>");
    }
# endif

  /*----------------------------------------*
   * Show the mark, but only in debug mode. *
   *----------------------------------------*/

# ifdef DEBUG
#   ifdef TRANSLATOR
      if(trace && marked) FPRINTF(f, "{");
#   endif
# endif

  /*--------------------------*
   * Print a NULL type as `?. *
   *--------------------------*/

  if(t == NULL_T){
    FPRINTF(f, "`?");
#   ifdef MACHINE
      ctl->chars_printed += 2;
#   endif
    goto out;
  }

  /*----------------------------------------------------------------*
   * There can be a loop in the type due to unification, before the *
   * occur check.  If a loop is detected, just print (loop).        *
   *----------------------------------------------------------------*/

  if(t->prmark != 0) {
    FPRINTF(f, "(loop)");
#   ifdef MACHINE
      ctl->chars_printed += 6;
#   endif
    goto out;
  }

  t->prmark = 1;
  switch(TKIND(t)) {
    /*-----------------------------------------------*/
    case BAD_T:
      FPRINTF(f, "BAD");
#     ifdef MACHINE
        ctl->chars_printed += 3;
#     endif
      break;

    /*-----------------------------------------------*/
    case FAM_MEM_T:
      print_fam_mem(t, ctl);
      break;

    /*-----------------------------------------------*/
    case PAIR_T:
      print_pair(t, r, ctl);
      break;

    /*-----------------------------------------------*/
    case FUNCTION_T:
      print_function(t, r, ctl);
      break;

    /*-----------------------------------------------*/
    case TYPE_ID_T:
    case FAM_T:
    case WRAP_TYPE_T:
    case WRAP_FAM_T:
      print_type_id(ctl, t);
      break;

    /*-----------------------------------------------*/
    case FICTITIOUS_TYPE_T:
    case FICTITIOUS_FAM_T:
      print_type_id(ctl, t);
#     ifdef TRANSLATOR
        FPRINTF(f, "-Member%ld", t->TNUM);
#     else
        {char str[20];
	 sprintf(str, "%ld", t->TNUM);
	 FPRINTF(f, "-Member%s", str);
	 ctl->chars_printed += strlen(str) + 7;
	}
#     endif
      break;

    /*-----------------------------------------------*/
    case FICTITIOUS_WRAP_TYPE_T:
      print_type_id(ctl, t);
#     ifdef TRANSLATOR
        FPRINTF(f, "-SUBGENUS%ld", t->TNUM);
#     else
        {char str[20];
	 sprintf(str, "%ld", t->TNUM);
	 FPRINTF(f, "-SUBGENUS%s", str);
	 ctl->chars_printed += strlen(str) + 9;
	}
#     endif
      break;

    /*-----------------------------------------------*/
    case FICTITIOUS_WRAP_FAM_T:
      print_type_id(ctl, t);
#     ifdef TRANSLATOR
        FPRINTF(f, "-SUBCOMM%ld", t->TNUM);
#     else
        {char str[20];
	 sprintf(str, "%ld", t->TNUM);
	 FPRINTF(f, "-SUBCOMM%s", str);
	 ctl->chars_printed += strlen(str) + 8;
	}
#     endif
      break;

    /*-----------------------------------------------*/
    case FAM_VAR_T:
    case TYPE_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case PRIMARY_FAM_VAR_T:
    case WRAP_TYPE_VAR_T:
    case WRAP_FAM_VAR_T:
      if(print_variable(t, ctl)) {
        collect_constraints(t, ctl);
      }
      break;

    /*-----------------------------------------------*/
    default: 
      {
#      ifdef TRANSLATOR
#        ifdef DEBUG
           long_print_ty(t, 1);
#        else
	   FPRINTF(f, "(unknown kind %d)", TKIND(t));
#        endif
#      else
         FPRINTF(f, "???");
	 ctl->chars_printed += 3;
#      endif
     }
  } /* end switch */

  t->prmark = 0;

 out:
# ifdef DEBUG
#   ifdef TRANSLATOR
      if(trace && marked) FPRINTF(f, "}");
#   endif
# endif
}


/****************************************************************
 *			PRINT_RT1_WITH_CONSTRAINTS		*
 ****************************************************************
 * Similar to print_rt1_without_constraints, but also prints	*
 * the constraints.						*
 ****************************************************************/

#ifdef TRANSLATOR

void print_rt1_with_constraints(TYPE *t, ROLE *r, PRINT_TYPE_CONTROL *ctl)
{
  print_rt1_without_constraints(t, r, ctl);
  print_constraints(ctl, -1);
}

#endif


/****************************************************************
 *		  PRINT_RT1_WITH_CONSTRAINTS_INDENTED		*
 ****************************************************************
 * Similar to print_rt1_with_constraints, but prints each	*
 * constraint on a separate line, indented N spaces.		*
 ****************************************************************/

#ifdef TRANSLATOR

void print_rt1_with_constraints_indented
      (TYPE *t, ROLE *r, PRINT_TYPE_CONTROL *ctl, int n)
{
  print_rt1_without_constraints(t, r, ctl);
  print_constraints(ctl, n);
}

#endif


/******************************************************************
 *			PRINT_RT				  *
 ******************************************************************
 * print_rt(F,T,R) prints type T with role R, in short format, on *
 * file F.							  *
 ******************************************************************/

#ifdef TRANSLATOR

void print_rt(FILE *f, TYPE *t, ROLE *r)
{
  PRINT_TYPE_CONTROL c;

  begin_printing_types(f, &c);
  print_rt1_without_constraints(t, r, &c);
  end_printing_types(&c);
}

#endif


/******************************************************************
 *		PRINT_RT_WITH_CONSTRAINTS_INDENTED		  *
 ******************************************************************
 * print_rt(F,T,R,N) prints type T with role R, in short format,  *
 * on file F, with each constraint on a separate line, indented   *
 * N spaces.							  *
 ******************************************************************/

#ifdef TRANSLATOR

void print_rt_with_constraints_indented(FILE *f, TYPE *t, ROLE *r, int n)
{
  PRINT_TYPE_CONTROL c;

  begin_printing_types(f, &c);
  print_rt1_without_constraints(t, r, &c);
  print_constraints(&c, n);
  end_printing_types(&c);
}

#endif


/****************************************************************
 *			PRINT_TY1_WITHOUT_CONSTRAINTS		*
 ****************************************************************
 * Prints type T with null role, without showing constraints,	*
 * on controller CTL.						*
 ****************************************************************/

void print_ty1_without_constraints(TYPE *t, PRINT_TYPE_CONTROL *ctl)
{
  print_rt1_without_constraints(t, NULL, ctl);
}


/****************************************************************
 *			PRINT_TY1_WITH_CONSTRAINTS		*
 ****************************************************************
 * Prints type T with null role, followed by its constraints.	*
 ****************************************************************/

void print_ty1_with_constraints(TYPE *t, PRINT_TYPE_CONTROL *ctl)
{
  print_rt1_without_constraints(t, NULL, ctl);
  print_constraints(ctl, -1);
}


/****************************************************************
 *		PRINT_TY1_WITH_CONSTRAINTS_INDENTED		*
 ****************************************************************
 * Prints type T with null role, followed by its constraints.	*
 * Each constraint is printed on a separate line, indented N	*
 * spaces.							*
 ****************************************************************/

void print_ty1_with_constraints_indented(TYPE *t, PRINT_TYPE_CONTROL *ctl, 
					int n)
{
  print_rt1_without_constraints(t, NULL, ctl);
  print_constraints(ctl, n);
}


/****************************************************************
 *			   PRINT_NAMELIST			*
 ****************************************************************
 * Print role namelist L on file F.				*
 *								*
 * print_role_name(f, fmt, name) prints name, with printf format*
 * fmt, preceded by a comma if need_comma is true.		*
 *								*
 * print_namelist(f,l) prints role namelist l, with <> around	*
 * it and commas separating names.				*
 ****************************************************************/

#ifdef TRANSLATOR

PRIVATE Boolean need_comma;  /* Used to control printing of commas in 	*
                              * role name lists 			*/

/*---------------------------------------------------------*/

PRIVATE void print_role_name(FILE *f, char *fmt, char *name)
{
  if(need_comma) fprintf(f, ",");
  need_comma = TRUE;
  fprintf(f, fmt, name);
}

/*---------------------------------------------------------*/

void print_namelist(FILE *f, STR_LIST *l)
{
  LIST *p;
  fprintf(f, "<");
  need_comma = FALSE;
  for(p = l; p != NIL; p = p->tail) {
    char* name_with_mode = p->head.str;
    int   mode           = *name_with_mode;
    char* name           = name_with_mode + 1;

    if(mode & KILL_ROLE_MODE) {
      print_role_name(f, "kill__%s", name);
    }
    else if(mode & ROLE_ROLE_MODE) {
      print_role_name(f, "%s", name);
    }

    if(mode & CHECK_ROLE_MODE) {
      print_role_name(f, "check__%s", name);
    }

    if(mode & NO_ROLE_MODE) {
      print_role_name(f, "no__%s", name);
    }
  }
  fprintf(f, ">");
}

#endif

/****************************************************************
 *			   FPRINT_ROLE				*
 ****************************************************************
 * Print role R on file F.					*
 ****************************************************************/

#ifdef TRANSLATOR

void fprint_role(FILE *f, ROLE *r)
{
  if(r == NULL) fprintf(f, "-");
  else {
    if(r->namelist != NIL) print_namelist(f, r->namelist);
    else fprintf(f, "-");
    if(r->role1 != NULL || r->role2 != NULL) {
      fprintf(f, "(");
      fprint_role(f,r->role1);
      if(r->kind == PAIR_ROLE_KIND) fprintf(f, ",");
      else fprintf(f, "->");
      fprint_role(f, r->role2);
      fprintf(f, ")");
    }
  }
}

#endif

/****************************************************************
 *			PRINT_LINK_LABELS			*
 ****************************************************************
 * Print labels after a link name, in parentheses, if there are	*
 * any labels.  Print on file F.  CTC is the class table entry	*
 * for the thing at the bottom of the link whose labels are	*
 * being shown.  Show the link labels appropriately for going	*
 * down to it.							*
 ****************************************************************/

#ifdef TRANSLATOR

void print_link_labels(FILE *f, int label1, int label2, CLASS_TABLE_CELL *ctc)
{
  char *name;
  Boolean display;
  int code;

  /*------------------------------------------------*
   * Decide whether anything needs to be displayed. *
   *------------------------------------------------*/

  display = FALSE;
  code    = ctc->code;
  if(code == FAM_ID_CODE || code == COMM_ID_CODE || 
     code == PAIR_CODE || code == FUN_CODE) {
    display = TRUE;
  }

  /*-----------------*
   * Do the display. *
   *-----------------*/

  if(display) {
    fprintf(f,"(");
    if(label1 >= 0) {
      name = display_name(ctcs[label1]->name);
      fprintf(f,"%s", name);
    }
    else fprintf(f,std_type_id[ANYG_TYPE_ID]);
    if(label2 != 0 || code == FUN_CODE || code == PAIR_CODE) {
      fprintf(f,(code != FUN_CODE) ? "," : "->");
      name = (label2 <= 0) ? std_type_id[ANYG_TYPE_ID] 
	                   : display_name(ctcs[label2]->name);
      fprintf(f,"%s", name);
    }
    fprintf(f,")");
  }
}

#endif

/****************************************************************
 *			PRINT_LABELED_TYPE			*
 ****************************************************************
 * Print the type of CTC, with label LP, on file F.		*
 * If LP is NOCHECK_LP, then don't print the label.		*
 ****************************************************************/

#ifdef TRANSLATOR

void print_labeled_type(FILE *f, CLASS_TABLE_CELL *ctc, LPAIR lp)
{
  int code = ctc->code;
  if(code != PAIR_CODE && code != FUN_CODE) {
    fprintf(f, "%s", display_name(ctc->name));
  }
  if(lp.label1 >= 0) {
    print_link_labels(f, lp.label1, lp.label2, ctc);
  }
}

#endif

/****************************************************************
 *			   FPRINT_TAG_TYPE			*
 ****************************************************************
 * Type T is a pseudo-type, created by the choose-matching	*
 * completeness checker.					*
 *								*
 * Print T as an indication of constructors that are used,	*
 * for pattern match choice completeness checker.  Type MIRROR	*
 * is a genuine type, used for obtaining information about the	*
 * type being printed.  It should be the type that T is derived	*
 * from, or the type of the pattern that T is derived from.	*
 * If PRINT_FOR_DEBUG is true, then show T in a form suitable   *
 * for a debug print.						*
 ****************************************************************/

#ifdef TRANSLATOR
void fprint_tag_type(FILE *f, TYPE *t, TYPE *mirror, Boolean print_for_debug)
{
  t      = find_u(t);
  mirror = find_u(mirror);
  switch(TKIND(t)) {
    /*-----------------------------------------------*/
    case PAIR_T:
      {int mirror_kind = TKIND(mirror);
       if(mirror_kind == PAIR_T) fprintf(f, "(");
       fprint_tag_type(f, t->TY1, mirror->TY1, print_for_debug);
       fprintf(f, (mirror_kind == PAIR_T) ? "," : "  ");
       fprint_tag_type(f, t->TY2, mirror->TY2, print_for_debug);
       if(mirror_kind == PAIR_T) fprintf(f, ")");
       break;
      }

    /*-----------------------------------------------*/
    case FAM_MEM_T:
      fprint_tag_type(f, t->TY2, mirror->TY2, print_for_debug);
      break;

    /*-----------------------------------------------*/
    case TYPE_ID_T:
    case FAM_T:
      {char *constr_name;
       if(t->ctc != NULL) constr_name = "?";
       else {
	 constr_name = tag_num_to_tag_name(mirror, t->TOFFSET);
	 if(constr_name == NULL) constr_name = "?";
       }
       fprintf(f, "%s", display_name(constr_name));
       break;
      }

    /*-----------------------------------------------*/
    case TYPE_VAR_T:
    case FAM_VAR_T:
      fprintf(f, print_for_debug ? "`%p" : "?", t);
      break;

    /*-----------------------------------------------*/
    default:
      fprintf(f, "?");
  }      
}
#endif
