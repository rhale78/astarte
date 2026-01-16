/*****************************************************************
 * File:    parser/parseerr.c
 * Purpose: Functions for detecting and handling errors during 
 *	    parsing.
 * Author:  Karl Abrahamson
 *****************************************************************/

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
 * This file contains functions for error handling in the parser.  The  *
 * functions defined here are generally checking for errors of various	*
 * kinds.  File error/error.c contains functions that actually report	*
 * errors.								*
 *									*
 * Also included here are functions for doing a panic at a really bad	*
 * error.  (Yes, we do the horrible panic-mode recovery.)		*
 ************************************************************************/

#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include <ctype.h>
#include "../misc/misc.h"
#include "../classes/classes.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../lexer/lexer.h"
#include "../error/error.h"
#include "../exprs/expr.h"
#include "../tables/tables.h"
#include "../utils/strops.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../alloc/allocate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
extern int yyleng;
extern char yytext[];

PRIVATE void dcl_panic1_l(void);

/*****************************************************************
 *			PUBLIC VARIABLES			 *
 *****************************************************************/

/*****************************************************************
 *			suppress_panic				 *
 *****************************************************************
 * suppress_panic is set to 1 to suppress panics when the end of *
 * file has been read.						 *
 *****************************************************************/

Boolean suppress_panic = 0;


/*****************************************************************
 *			syntax_form				 *
 *			alt_syntax_form				 *
 *****************************************************************
 * syntax_form[tok - FIRST_RESERVED_WORD_TOK] is a string that	 *
 * shows the syntax of the construct that begins with token tok, *
 * or is NULL if there is none.  Here, tok should be between	 *
 * FIRST_RESERVED_WORD_TOK and LAST_RESERVED_WORD_TOK.		 *
 *								 *
 * alt_syntax_form[tok - FIRST_ALT_RESERVED_WORD_TOK] is a	 *
 * string that shows the syntax of the construct that begins	 *
 * with token tok, where tok is one of the alternate tokens 	 *
 * defined in parser/tokmod.h.  It is NULL if there is no form   *
 * to show.  Here, tok should be between	 		 *
 * FIRST_ALT_RESERVED_WORD_TOK and LAST_ALT_RESERVED_WORD_TOK.   *
 *								 *
 * Variables syntax_form and alt_syntax_form are set to NULL	 *
 * initially, and are read in from a file when required.	 *
 *****************************************************************/

char** syntax_form = NULL;
char** alt_syntax_form = NULL;

/****************************************************************
 *			READ_SYNTAX_FORMS			*
 ****************************************************************
 * read_syntax_forms() reads array syntax_form from its file,   *
 * if it has not already been read.				*
 ****************************************************************/

#define NUM_SYNTAX_FORMS 70

void read_syntax_forms(void)
{
  if(syntax_form == NULL) {
    char* s = 
      (char*) BAREMALLOC(strlen(MESSAGE_DIR) + strlen(SYNTAX_FORM_FILE) + 1);
    sprintf(s, "%s%s", MESSAGE_DIR, SYNTAX_FORM_FILE);
    init_err_msgs(&syntax_form, s, NUM_SYNTAX_FORMS);
    FREE(s);
  }
}


/****************************************************************
 *			READ_ALT_SYNTAX_FORMS			*
 ****************************************************************
 * read_alt_syntax_forms() reads array alt_syntax_form from its *
 * file, if it has not already been read.			*
 ****************************************************************/

#define NUM_ALT_SYNTAX_FORMS 15

void read_alt_syntax_forms(void)
{
  if(alt_syntax_form == NULL) {
    char* s = 
      (char*) BAREMALLOC(strlen(MESSAGE_DIR) + 
			 strlen(ALT_SYNTAX_FORM_FILE) + 1);
    sprintf(s, "%s%s", MESSAGE_DIR, ALT_SYNTAX_FORM_FILE);
    init_err_msgs(&alt_syntax_form, s, NUM_ALT_SYNTAX_FORMS);
    FREE(s);
  }
}


/****************************************************************
 *			CHECK_BRACES				*
 ****************************************************************
 * Check that kind1 and kind2 are the same kind of brace.	*
 * Complain if they are not.					*
 ****************************************************************/

void check_braces(int kind1, int kind2)
{
  if(kind1 != kind2) {
    syntax_error(BRACE_MISMATCH_ERR, 0);
  }
}


/****************************************************************
 *		CHECK_FOR_BAD_UNTIL_P				*
 ****************************************************************
 * Check for an exitcase or loopcase case in a choose expr.	*
 ****************************************************************/

void check_for_bad_loop_case_p(int line)
{
  CHOOSE_INFO* inf = top_choose_info(choose_st);
  if(inf->choose_kind != LOOP_TOK) {
    syntax_error(BAD_UNTIL_ERR, line);
  }
}


/****************************************************************
 *		CHECK_FOR_CASE_AFTER_ELSE_P			*
 ****************************************************************
 * Check whether the current context indicates that a further 	*
 * case should not be allowed. 					*
 ****************************************************************/

void check_for_case_after_else_p(int line)
{ int ck = top_i(case_kind_st);
  if(ck == ELSE_ATT || ck == UNTIL_ELSE_ATT ||
     ck == WHILE_ELSE_ATT) {
    syntax_error(CASE_AFTER_ELSE_ERR, line);
  }
}


/****************************************************************
 *			CHECK_ELSE_P				*
 ****************************************************************
 * Check that an else is permitted in the current context.	*
 ****************************************************************/

void check_else_p(void)
{
  CHOOSE_INFO* inf = top_choose_info(choose_st);
  int ck = top_i(case_kind_st);

  if(ck == ELSE_ATT || ck == UNTIL_ELSE_ATT
     || ck == WHILE_ELSE_ATT) {
    syntax_error(CASE_AFTER_ELSE_ERR, 0);
  }
}


/****************************************************************
 *			CHECK_EMPTY_BLOCK_P			*
 ****************************************************************
 * This function is called on the attribute of a begin word     *
 * that has a null body.  For example, if the parser sees	*
 * 								*
 *   Let %Let							*
 *								*
 * then it calls check_empty_block_p(attr) where attr is the    *
 * attribute of Let.  This kind of thing is only allowed when   *
 * the construct begin comes from a semicolon, so that what we  *
 * really have is something like				*
 *								*
 *  Let								*
 *    x = 2;							*
 *  %Let							*
 *								*
 * In that case, attr.tok will be negative.  Complain if 	*
 * attr.tok is positive.					*
 ****************************************************************/

void check_empty_block_p(struct lstr* attr)
{
  if(attr->tok > 0) {
    syntax_error(EMPTY_BLOCK_ERR, attr->line);
  }
}


/****************************************************************
 *			CHECK_END_P				*
 ****************************************************************
 * Check that beginner and ender match.   tok is the token for	*
 * beginner.							*
 ****************************************************************/

void check_end_p(struct lstr *beginner, int tok, struct lstr *ender)
{
  char *name, *ename;

  /*------------------------------------------------------------*
   * ender->line = -1 is a wild card, generated at an error by  *
   * end_word_l.  Just accept it without any questions. 	*
   *------------------------------------------------------------*/

  if(ender->line == -1) return;

  name  = beginner->name;
  ename = name_tail(ender->name);

  /*----------------------------------------------------------------*
   * If the end marker is a period,  check that line numbers match, *
   * and that this is not ending the package. 			    *
   *----------------------------------------------------------------*/

# if defined(DEBUG) && defined(YYDEBUG)
    if(trace_lexical || yydebug) {
      trace_t(457, nonnull(ename), nonnull(name));
    }
# endif

  if(ename == NULL_S){
    if(beginner->line != ender->line) {
      syntax_error2(PERIOD_ERR, (tok <= LAST_RESERVED_WORD_TOK) ? name+1 : name, 
		    (char *)(LONG)(beginner->line), ender->line);
#     ifdef DEBUG
        if(trace) {
	  trace_t(458, toint(beginner->line), toint(ender->line));
	}
#     endif
    }
    if(lcequal(name, "package")) {
      syntax_error(END_ON_PERIOD_ERR, 0);
    }
  }

  /*------------------------------------*
   * Otherwise, check that names match. *
   *------------------------------------*/

  else {
    if(!lcequal(name, ename)) {
      if(tok == PROC_ID_TOK) syntax_error2(BAD_END_ERR, ename, name, 0);
      else syntax_error2(BAD_END_ERR, ename, name + 1, 0);
#     ifdef DEBUG
        if(trace) {
          trace_t(459, nonnull(name), nonnull(ename));
        }
#     endif
    }
  }
}


/****************************************************************
 *	CHECK_FOR_DUPLICATE_IDS_IN_PATTERN_FORMAL		*
 ****************************************************************
 * This function checks whether expression formal has at most	*
 * one occurrence of each pattern variable or local id.		*
 * It should only be use when formal is the left-hand side of	*
 * a pattern or expand declaration such as			*
 *								*
 *    Pattern formal => translation.				*
 ****************************************************************/

/*----------------------------------------------------------------------*
 * dup_patform_table associates a count of number of occurrences with   *
 * each local id that occurs in the expression' formal'.		*
 *									*
 * dup_patform_patfun_name is the name of the pattern function that is	*
 * being defined.  							*
 *									*
 * formal_line is the line number of expression formal, for error	*
 * reporting.								*
 *----------------------------------------------------------------------*/

PRIVATE HASH2_TABLE* dup_patform_table;
PRIVATE char*        dup_patform_patfun_name;
PRIVATE int	    formal_line;

/*----------------------------------------------------------------------*
 * check_dup_patform_help1(e) adds one to the count associated with	*
 * identifier *e, provided e is an identifier.				*
 *----------------------------------------------------------------------*/

PRIVATE Boolean check_dup_patform_help1(EXPR **e)
{
  EXPR* star_e = *e;
  EXPR_TAG_TYPE e_kind = EKIND(star_e);
  if(e_kind == PAT_VAR_E || e_kind == LOCAL_ID_E || e_kind == IDENTIFIER_E) {
    char* e_name = star_e->STR;

    /*-----------------------------------------------------------*
     * Only update the count if e is a local id or non-anonymous *
     * pattern variable.  If the name of e is the same as that	 *
     * of the pattern function being defined, then e is		 *
     * considered global.					 *
     *-----------------------------------------------------------*/

    if(!is_visible_global_tm(e_name, current_package_name, TRUE) && 
       !is_anon_pat_var(star_e) &&
       dup_patform_patfun_name != e_name) {
      HASH_KEY u;
      HASH2_CELLPTR h;
      u.str = e_name;
      h = insert_loc_hash2(&dup_patform_table, u, strhash(u.str), eq);
      if(h->key.num == 0) {
	h->key.str = e_name;
	h->val.num = 1;
      }
      else {
	h->val.num++;
      }
    }
    return 1;  /* Do not scan child of pattern var. */
  }
  return 0;  /* Scan children. */
}

/*-------------------------------------------------------*
 * check_dup_patform_help2(h) reports an error if cell h *
 * contains an identifier with an occurrence count that	 *
 * is >1.						 *
 *-------------------------------------------------------*/

PRIVATE void check_dup_patform_help2(HASH2_CELLPTR h)
{
  if(h->val.num > 1) {
    semantic_error1(DUP_IN_PATRULE_ERR, h->key.str, formal_line);
  }
}

/*----------------------------------------------------*/

void check_for_duplicate_ids_in_pattern_formal(EXPR *formal)
{
  if(formal == NULL) return;
  else {
    EXPR* ss_formal = skip_sames(formal);
    formal_line = formal->LINE_NUM;
    if(EKIND(ss_formal) == APPLY_E) {
      EXPR *patfun = get_applied_fun(ss_formal, FALSE);    
      if(is_id_p(patfun)) {
	dup_patform_patfun_name = patfun->STR;
	dup_patform_table = NULL;
	scan_expr(&(ss_formal), check_dup_patform_help1, FALSE);
	scan_hash2(dup_patform_table, check_dup_patform_help2);
	free_hash2(dup_patform_table);
      }
    }
  }
}


/****************************************************************
 *			   DCL_PANIC_L				*
 ****************************************************************
 * Panic to the next declaration.  Print error messages 	*
 * indicating that a panic is taking place.			*
 ****************************************************************/

Boolean dcl_panic_l(void)
{
  Boolean open;
  int tok, line, col;
  char *name;

  if(current_shadow_stack != NIL) {

    /*--------------------------------------------------*
     * Find the token that begins the construct 	*
     * in which the panic occurs.  It is used		*
     * for error reporting.				*
     *							*
     * Cases are as follows.				*
     *							*
     *   1. If the most recent token is an end (%W) or	*
     *      period, then the end has come prematurely.  *
     *      Set tok to the token that was popped by 	*
     *	    this end or period.				*
     *							*
     *   2. If the most recent token is a begin token, 	*
     *      then it has caused the error, but it is	*
     *	    really the previous top of the stack that	*
     *	    should be reported.  So back up one in the	*
     *      shadow stack.				*
     *							*
     *   3. Otherwise, just get the top of the shadow	*
     *      stack as the begin of the embedding context.*
     *--------------------------------------------------*/

    /*---------*
     * Case 1. *
     *---------*/

    if((last_tok == END_TOK || last_tok == PERIOD_TOK) &&
       last_popped_tok != 0) {
      tok  = last_popped_tok;
      line = last_popped_tok_line;
      name = is_reserved_word_tok(tok) ? rw_name(tok) : NULL;
    }

    /*---------*
     * Case 2. *
     *---------*/

    else if(is_begin_tok(last_tok) && current_shadow_stack->tail != NIL) {
      shadow_top(current_shadow_stack->tail, &tok, &name, &open);
      shadow_top_nums(current_shadow_nums->tail, &line, &col);
    }

    /*---------*
     * Case 3. *
     *---------*/

    else {
      shadow_top(current_shadow_stack, &tok, &name, &open);
      shadow_top_nums(current_shadow_nums, &line, &col);
    }

    /*----------------------------------------------------------*
     * Indicate that we are starting a panic, and also print 	*
     * a synopsis of the syntax for tok if one is available.	*
     *----------------------------------------------------------*/

    if(tok != PACKAGE_TOK) {
      if(tok > LAST_RESERVED_WORD_TOK) name = lparen_from_rparen(tok);
      if(name != NULL && name[0] == '%') name++;
      syntax_error2(MOST_RECENT_BEGIN_ERR, nonnull(name),
		    (char *) (LONG) line, 0);
      show_syntax(tok);
    }
  }

  if(stop_at_first_err) clean_up_and_exit(1);

  if(!suppress_panic) {
    syntax_error(PANIC_ERR, 0);
    panic_mode = TRUE;
    dcl_panic1_l();
    return 1;
  }
  return 0;
}


/****************************************************************
 *			   DCL_PANIC1_L				*
 ****************************************************************
 * Panic to the start of the next declaration.  		*
 ****************************************************************/

PRIVATE void dcl_panic1_l(void)
{
  Boolean open;
  int tok;
  char *name;

# ifdef DEBUG
    if(trace) trace_t(460);
# endif

  /*-------------------------------------------------------------------*
   * Pop the shadow stack back to an appropriate token for end of dcl. *
   *-------------------------------------------------------------------*/

  while(current_shadow_stack != NIL &&
        (shadow_top(current_shadow_stack, &tok, &name, &open), 
	   tok != PACKAGE_TOK && tok != EXTENSION_TOK && tok != TEAM_TOK)) {

#   ifdef DEBUG
      if(trace) {
        trace_t(461);
	if(current_shadow_stack->kind == INT_L) {
	  fprintf(TRACE_FILE, "%d", tok);
	}
	else {
	  fprintf(TRACE_FILE, "%s", name);
	}	 
        trace_t(462);
      }
#   endif

    pop(&current_shadow_stack);
    pop(&current_shadow_nums);
  }

  /*---------------------------------------*
   * Throw away tokens until begin of dcl. *
   *---------------------------------------*/
    
  for(;;) {
    panic_mode = TRUE;
    tok = yylexx();
    panic_mode = TRUE;

    /*---------------------------*
     * End of file: just return. *
     *---------------------------*/

    if(tok == 0) {

#     ifdef DEBUG
        if(trace) trace_t(463);
#     endif

      return;
    }

    /*-----------*
     * %Package. *
     *-----------*/

    if(tok == END_TOK && lcequal(yylval.ls_at.name, "%package")) {
      seen_package_end = TRUE;
      return;
    }

    /*-------------*
     * %Extension. *
     *-------------*/

    if(tok == END_TOK && lcequal(yylval.ls_at.name, "%extension")) {
      push_back_token(tok, yylval, END_K, NULL);
      panic_mode = FALSE;
      yytext[0] = '\0';
      break;
    }

    /*----------------------------------------*
     * ========= or beginning of declaration. *
     *----------------------------------------*/

    if (tok == LINE_TOK || 
        (is_reserved_word_tok(tok) && token_kind(tok) == DCL_BEGIN_K &&
         isupper(yytext[0]))) {
      push_back_token(tok, yylval, BEGIN_K, NULL);
      yytext[0] = '\0';
      panic_mode = FALSE;
      break;
    }
  }

# ifdef DEBUG
    if(trace) trace_t(464);
# endif

}
