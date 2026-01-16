/****************************************************************
 * File:    lexer/lexerr.c
 * Purpose: Error handling support routines for lexer.
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
 * This file contains functions for error handling and recovery in the  *
 * lexer.  Included are							*
 *									*
 *  functions for managing the "shadow stack", which gives partial	*
 *  information about what is in the parser's stack,			*
 *									*
 *  functions for checking whether an end-word or right bracket matches *
 *  the kind that is expected, based on what is on the shadow stack,	*
 *									*
 *  functions for checking indentation,					*
 *									*
 *  functions for checking whether a separator token is a reasonable	*
 *  one in the current context.  When the separator is not ok, a	*
 *  different token is returned instead.				*
 *									*
 *  functions for the lexer to call to check that reserved word case	*
 *  is correct,								*
 *									*
 *  a function to check whether a line ====== is acceptable, and to	*
 *  replace it by another token if needed.				*
 ************************************************************************/

#include <string.h>
#include <stdlib.h>
#include "../misc/misc.h"
#include "../lexer/lexer.h"
#include "../standard/stdids.h"
#include "../parser/parser.h"
#include "../parser/tokens.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../tables/tables.h"
#include "../error/error.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
extern char yytext[];
extern int yyleng;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			suppress_indent_warning			*
 ****************************************************************
 * When suppress_indent_warning is true, indentation warnings   *
 * are not issued.  This is used to suppress incorrect warnings *
 * after procedure id tokens.  This variable is an additional   *
 * suppressor to variable suppress_indentation_errs.		*
 ****************************************************************/

PRIVATE Boolean suppress_indent_warning = FALSE;


/****************************************************************
 * The following functions manage the "shadow stack", which     *
 * shadows the parser's stack.  It keeps track of begin words   *
 * that are waiting for matching end words, and parentheses     *
 * and brackets that are waiting for matching right versions of *
 * themselves.  						*
 *								*
 * current_shadow_stack is a list holding integer tokens and,   *
 * for procedure identifiers, procedure names.  The list tag    *
 * tells which kind a given list member is.  When a left	*
 * bracket is read, its matching right bracket is pushed on 	*
 * the shadow stack.						*
 *								*
 * When an open construct is begun, the negative of the begin   *
 * token is pushed.  For example, open Let causes -LET_TOK to   *
 * be pushed onto current_shadow_stack.				*
 *								*
 * Related stacks store the line and column where begin words   *
 * and left brackets occur.  All of these stacks are stored 	*
 * with the information on the current file.  File information  *
 * is managed by tables/fileinfo.c.				*
 ****************************************************************/


/****************************************************************
 *			    SHADOW_TOP				*
 ****************************************************************
 * Set *t to the token on top of stack st (a substack of 	*
 * current_shadow_stack), and set *name to the name of the 	*
 * token when t is PROC_ID_TOK or a reserved word.		*
 * 								*
 * Set *open to TRUE if the token on top of st is marked as 	*
 * open, and to FALSE if it is not marked open.			*
 ****************************************************************/

void shadow_top(LIST *st, int *t, char **name, Boolean *open)
{
  int k;

  *open = FALSE;      /* default */
  *name = NULL;

  if(st == NIL) {
    *t    = 0; 
    *name = "(none)"; 
  }

  else if(LKIND(st) == INT_L) {
    k = top_i(st);
    if(k < 0) {
      *open = TRUE;
      k     = -k;
    }
    if(k <= LAST_RESERVED_WORD_TOK) {
      *name = rw_name(k);
    }
    *t = k;
  }

  else {
    *t = PROC_ID_TOK;
    *name = top_str(st);
    if(LKIND(st) == STR2_L) *open = TRUE;
  }
}


/****************************************************************
 *			    SHADOW_TOP_NUMS			*
 ****************************************************************
 * Set *line and *col to the line and column numbers associated *
 * with the top of stack st (a substack of current_shadow_nums).*
 ****************************************************************/

void shadow_top_nums(LIST *st, int *line, int *col)
{
  struct two_shorts ts;

  if(st != NIL) {
    ts = st->head.two_shorts;
    *line = ts.line;
    *col  = ts.col;
  }
  else *line = *col = 0;
}
 

/****************************************************************
 *			    PUSH_SHADOW				*
 ****************************************************************
 * Push token n onto shadow stack, and push line and column	*
 * information onto the shadow num stack.			*
 *								*
 * If n is a begin token that is preceded by open, then push    *
 * -n onto the shadow stack.  					*
 ****************************************************************/

void push_shadow(int n)
{
  int tok_to_push;
  if(panic_mode) return;

  tok_to_push = (last_tok == OPEN_TOK) ? -n : n;
  push_int(current_shadow_stack, tok_to_push);
  push_shorts(current_shadow_nums, current_line_number, 
	      yylval.ls_at.column);

# ifdef DEBUG
    if(trace_lexical > 1) {
      trace_t(310, n);
      trace_t(312);
      print_str_list_nl(current_shadow_stack);
      trace_t(311);
      print_str_list_nl(current_shadow_nums);
      tracenl();
    }
# endif

}


/****************************************************************
 *			    PUSH_SHADOW_STR			*
 ****************************************************************
 * Push string s onto shadow stack.  Here, s is the name of a	*
 * procedure.  Normally, string s is pushed with tag STR_L.     *
 * If the token that precedes this procedure id is 'open',	*
 * then tag STR2_L is used instead. 				*
 ****************************************************************/

void push_shadow_str(char *s)
{
  if(panic_mode) return;

# ifdef GCTEST
    if(current_shadow_stack != NIL) LKIND(current_shadow_stack);
# endif

  push_str(current_shadow_stack, s);
  push_shorts(current_shadow_nums, current_line_number, 
	      yylval.ls_at.column);
  if(last_tok == OPEN_TOK) {
    current_shadow_stack->kind = STR2_L;
  }

# ifdef DEBUG
    if(trace_lexical > 1) {
      trace_t(313, s);
      trace_t(312);
      print_str_list_nl(current_shadow_stack);
      trace_t(311);
      print_str_list_nl(current_shadow_nums);
      tracenl();
    }
# endif

}


/****************************************************************
 *			    POP_SHADOW				*
 ****************************************************************
 * Pop shadow stack.						*
 ****************************************************************/

void pop_shadow(void)
{
  if(panic_mode) return;

  if(current_shadow_stack != NIL) {

#   ifdef DEBUG
      int kind;
      kind = LKIND(current_shadow_stack);
      if(trace_lexical > 1) {
        if(kind == INT_L) {
          trace_t(314, top_i(current_shadow_stack));
	}
	else {
	  trace_t(315, top_str(current_shadow_stack));
	}
      }
#   endif

    last_popped_tok = (LKIND(current_shadow_stack) == INT_L) 
                        ? top_i(current_shadow_stack)
                        : 0;
    last_popped_tok_line = current_shadow_nums->head.two_shorts.line;
    pop(&current_shadow_stack);
    pop(&current_shadow_nums);

#   ifdef DEBUG
      if(trace_lexical > 1) {
	trace_t(312);
	print_str_list_nl(current_shadow_stack);
	tracenl();
      }
#   endif

  }
}


/****************************************************************
 *			    POP_PROC_ID				*
 ****************************************************************
 * Pop a proc id from the shadow stack.  It might be on the top,*
 * or next down from the top.					*
 ****************************************************************/

void pop_proc_id()
{
  if(LKIND(current_shadow_stack) == STR_L) pop_shadow();
  else {

#   ifdef DEBUG
      if(trace_lexical > 1) {
        trace_t(315, top_str(current_shadow_stack->tail));
      }
#   endif

    SET_LIST(current_shadow_stack, 
	     int_cons(current_shadow_stack->head.i, 
		      current_shadow_stack->tail->tail));
    SET_LIST(current_shadow_nums, 
	     shorts_cons(current_shadow_nums->head.two_shorts, 
			 current_shadow_nums->tail->tail));

#   ifdef DEBUG
      if(trace_lexical > 1) {
        trace_t(312);
        print_str_list_nl(current_shadow_stack);
        tracenl();
      }
#   endif

  }
}


/****************************************************************
 *			    END_MATCH_L				*
 ****************************************************************
 * top is the token that occurs on the top of the shadow stack. *
 * (When the shadow stack has a procedure id on it, top is 	*
 * PROC_ID_TOK.)						*
 *								*
 * proc is the name of the procedure on top of the shadow stack *
 * when top is PROC_ID_TOK.					*
 *								*
 * kind tells about the next token.  It is			*
 *    0 for an end word, such as %if				*
 *    1 for a period						*
 *    n for token number n, such as a right parenthesis.	*
 *								*
 * Return true if the top of the shadow stack matches the 	*
 * next token.  The name of the next token is taken from        *
 * yytext.							*
 *								*
 * In the special case where top is PROC_ID_TOK, kind is 	*
 * RPAREN_TOK and the most recent read character is a procedure	*
 * identifier, pop the shadow stack before checking for a	*
 * left parenthesis.						*
 ****************************************************************/

Boolean end_match_l(int top, char *proc, int kind)
{
  /*----------------------------------------------------------*
   * Handle brackets.  The shadow stack has the right bracket *
   * on it, so just check whether top == kind.		      *
   *----------------------------------------------------------*/

  if(kind > 1 && top == kind) return TRUE;

  /*---------------------------------------------*
   * Check for the special case mentioned above. *
   *---------------------------------------------*/
  
  if(kind == RPAREN_TOK && top == PROC_ID_TOK && last_tok == PROC_ID_TOK) {
    return labs(top_i(current_shadow_stack->tail)) == RPAREN_TOK;
  }

  /*------------------*
   * Handle a period. *
   *------------------*/

  if(kind == 1) return top <= LAST_RESERVED_WORD_TOK || top == PROC_ID_TOK;

  /*-----------------------------------*
   * Handle an end word such as %loop. *
   *-----------------------------------*/

  if(top <= LAST_RESERVED_WORD_TOK) return lcequal(rw_name(top), yytext);

  /*---------------------*
   * Handle a procedure. *
   *---------------------*/

  if(top == PROC_ID_TOK) return strcmp(yytext+1, proc) == 0;
  return FALSE;
}


/****************************************************************
 *			    END_PROVIDED_L			*
 ****************************************************************
 * Indicate that an end word or right bracket has been provided *
 * by the lexer.  						*
 *								*
 * top tells what token was expected, and name is name of a     *
 * procedure if top is PROC_ID_TOK.				*
 ****************************************************************/

PRIVATE int end_provided_l(int top, char *name, int line)
{
  if(top == PACKAGE_TOK) {
    return RECOVER;
  }

  if(top == PROC_ID_TOK) {
    soft_syntax_error2(PROC_ENDED_ERR, name, (char *) line, 0);
    yylval.ls_at.line = -1;
    yylval.ls_at.name = name;
    recent_begin_from_semicolon = FALSE;
    return END_TOK;
  }

  if(top > LAST_RESERVED_WORD_TOK) {
    soft_syntax_error2(PAREN_PROVIDED_ERR, token_name(top,yylval), 
		       (char *) line, 0);
    return top;
  }

  /*-------------------------------*
   * top <= LAST_RESERVED_WORD_TOK *
   *-------------------------------*/

  if(top != 0) {
    soft_syntax_error2(END_PROVIDED_ERR, name, (char *) line, 0); 
    yylval.ls_at.name = name;
  }

  else syntax_error(SYNTAX_ERR, 0);

  yylval.ls_at.line = -1;
  recent_begin_from_semicolon = FALSE;
  return END_TOK;
}


/****************************************************************
 *			    END_WORD_L				*
 ****************************************************************
 * Get the token for an end word whose lexeme is in yytext.	*
 *								*
 * Kind is							*
 *    0 for an end word, such as %If				*
 *    1 for a period						*
 *    n for token number n, such as a right parenthesis.	*
 *								*
 * Return							*
 *    0     if this word should be ignored,			*
 *   n > 0  if token n should be returned.			*
 *								*
 * SPECIAL CASE: If kind is RPAREN_TOK and there is a 		*
 * procedure identifier on the top of the stack, with RPAREN_TOK*
 * just below it, then accept the RPAREN_TOK as good.  This is  *
 * needed to handle						*
 *								*
 *      expr -> LPAREN_TOK taggedProcId RPAREN_TOK  		*
 *								*
 * since we can't know whether the RPAREN_TOK will come along.  *
 ****************************************************************/

int end_word_l(int kind)
{
  struct list *next_down;
  char *proc, *next_proc;
  int top, next_top, col, line, tok;
  Boolean open;

  recent_begin_from_semicolon = FALSE;

  /*-------------------------------------------------*
   * Get the top of the shadow stack, to see what we *
   * are expecting.				     *
   *-------------------------------------------------*/

  shadow_top(current_shadow_stack, &top, &proc, &open);

  /*-----------------------------------------------------------*
   * Set up yylval for an end token such as %If.  In this case *
   * yylval.name is the whole lexeme, beginning with %, for a  *
   * reserved word, and is the name of the procedure, without  *
   * %, for a procedure id.  For reserved words, force the     *
   * first letter to upper case.			       *
   *-----------------------------------------------------------*/

  one_context_l(kind);
  if(kind == 0) {
    Boolean need_to_capitalize = 'a' <= yytext[1] && yytext[1] <= 'z';
    char* name_in_table;
    if(need_to_capitalize) yytext[1] += 'A' - 'a';
    name_in_table = id_tb_check(yytext);
    if(name_in_table != NULL) { /* reserved word, since %name is in table */
      yylval.ls_at.name = name_in_table;
      if(need_to_capitalize) yytext[1] += 'a' - 'A';  /* restore */
    }
    else {
      if(need_to_capitalize) yytext[1] += 'a' - 'A';  /* restore */
      yylval.ls_at.name = id_tb0(yytext+1);
    }
  }
  else {
    yylval.ls_at.name = NULL;
  }  

  /*--------------------------------------------------------*
   * If this end word matches expected end word, return it. *
   *--------------------------------------------------------*/

  if(panic_mode || top == 0 || end_match_l(top, proc, kind)) {
    if(!panic_mode) pop_shadow();
    if(kind > LAST_RESERVED_WORD_TOK && kind != PROC_ID_TOK) return kind;
    if(kind == 0) return END_TOK;
    return PERIOD_TOK;
  }

  /*-------------------------------------------------------------*
   * Handle the special case where kind is RPAREN_TOK and top is *
   * PROC_ID_TOK.  						 *
   *-------------------------------------------------------------*/

  if(kind == RPAREN_TOK && top == PROC_ID_TOK) {
    LIST* cs = current_shadow_stack;
    if(cs != NIL && labs(top_i(cs->tail)) == RPAREN_TOK) return RPAREN_TOK;
  }

# ifdef DEBUG
    if(trace) {
      trace_t(316, yytext, kind);
      if(top <= LAST_RESERVED_WORD_TOK) fprintf(TRACE_FILE, "%s\n", proc);
      else if(top == PROC_ID_TOK) fprintf(TRACE_FILE, "%%%s\n", proc);
      else fprintf(TRACE_FILE, "%s\n", token_name(top,yylval));
    }
# endif

  /*---------------------------------------------------------------------*
   * Otherwise check end markers in shadow stack.  If none match this    *
   * end marker, then ignore this end marker. But never ignore %package. *
   *---------------------------------------------------------------------*/

  if(!lcequal(yytext, "%package")) {
    next_down = current_shadow_stack;
    while(next_down != NULL) {
      shadow_top(next_down, &next_top, &next_proc, &open);
      if(end_match_l(next_top, next_proc, kind)) {
	goto accept_bad_end;  /* Below */
      }
      next_down = next_down->tail;
    }
    echo_token_l(yytext);
    soft_syntax_error1(END_IGNORED_ERR, id_tb0(yytext), 0);
    shadow_top_nums(current_shadow_nums, &line, &col);
    if(top == PROC_ID_TOK) {
      err_print(EXPECTED_STR1_ERR, proc);
      err_print(MATCHING_BEGIN_ERR, line);
    }
    else if(top > LAST_RESERVED_WORD_TOK) {
      err_print(EXPECTED_STR2_ERR, token_name(top,yylval));
      err_print(MATCHING_LEFT_ERR, line);
    }
    else if(top != 0) {
      err_print(EXPECTED_STR2_ERR, proc);
      err_print(MATCHING_BEGIN_ERR, line);
    }
    show_syntax(top);
    return 0;
  }

 accept_bad_end:

  /*------------------------------------------------------------*
   * Push this end word back and return an end marker that will *
   * match this case. But don't push back %package. 		*
   *------------------------------------------------------------*/

  shadow_top_nums(current_shadow_nums, &line, &col);
  if(!lcequal(yytext, "%package") && !seen_package_end) {
    yylval.ls_at.line = -1;
    if     (kind == 0) push_back_token(END_TOK, yylval, END_K, yytext);
    else if(kind == 1) push_back_token(PERIOD_TOK, yylval, END_K, yytext);
    else               push_back_token(kind, yylval, RPAREN_K, yytext);
    pop_shadow();
  }
  else {
    echo_token_l(yytext);
    if(top == PROC_ID_TOK) proc = concat_string("%", proc);
    else if(top > LAST_RESERVED_WORD_TOK) proc = token_name(top,yylval);
    syntax_error1(PACKAGE_ENDED_EXPECTING_ERR, proc, 0);
    err_print_str("  (");
    err_print(MATCHING_BEGIN_ERR, line);
    suppress_panic = 1;
  }
  yytext[0] = '\0';  /* Suppress echo */

# ifdef DEBUG
   if(trace) trace_t(317);
# endif

  tok = end_provided_l(top, proc, line);
  show_syntax(top);
  return tok;
}


/****************************************************************
 *			INDENTATION_WARN			*
 ****************************************************************
 * indentation_warn(TRUE) enables indentation warning, at least *
 * as far as variable suppress_indent_warning is concerned.	*
 * It does not affect suppress_indentation_errs, which is under *
 * programmer control.  indentation_warn(FALSE) turns off	*
 * indentation checking, regardless of the state of		*
 * suppress_indentation_errs.					*
 ****************************************************************/

void indentation_warn(Boolean k)
{
  suppress_indent_warning = !k;
}


/****************************************************************
 *			CHECK_INDENTATION_L			*
 ****************************************************************
 * We have just skipped over blanks at the start of a line, and *
 * need to check that the first nonblank character is not       *
 * badly indented.						*
 ****************************************************************/

void check_indentation_l(void)
{
  int ch, line, col, top;
  char *proc;
  Boolean open;

  if(err_flags.suppress_indentation_errs || suppress_indent_warning) return;

  /*----------------------------------------------------------*
   * If the next character is a newline or EOF, then this is  *
   * a blank line.  Do not do an indentation check.           *
   *----------------------------------------------------------*/

  ch = input_yy();
  if(ch == EOF) return;
  unput_yy(ch);
  if(ch == '\n') return;

  /*---------------------------------------------------------*
   * Do not do indentation check for import dcls -- they are *
   * currently messed up.  Fix maybe?			     *
   *---------------------------------------------------------*/

  shadow_top(current_shadow_stack, &top, &proc, &open);
  if(top == IMPORT_TOK) return;

  /*--------------------------------------------*
   * Get the column of the embedding construct. *
   *--------------------------------------------*/

  shadow_top_nums(current_shadow_nums, &line, &col);

  if(chars_in_this_line < col) {
    warn0(INDENTATION_ERR, 0);
    err_flags.suppress_indentation_errs = TRUE;
  }
}


/****************************************************************
 *			CHECK_FOR_PAREN_L			*
 ****************************************************************
 * Return tok provided the top of the shadow stack is a 	*
 * reasonable top for token tok.  For example, token EQUAL_TOK	*
 * can occur when the top of the shadow stack is LET_TOK.	*
 *								*
 * If tok is unreasonable, return the top of the shadow stack,  *
 * and push back tok.						*
 *								*
 * Report an error when tok is unreasonable.  It is presumed    *
 * that yytext holds the lexeme for tok.			*
 ****************************************************************/

int check_for_paren_l(int tok)
{
  int t, col, shadow_tok, kind, line;
  char *proc;
  Boolean t_is_choose, t_is_let, open;

  if(panic_mode) return tok;

  /*-----------------------------------------------------------------------*
   * If there is nothing on the shadow stack, then just return this token. *
   *-----------------------------------------------------------------------*/

  if(current_shadow_stack == NIL) return tok;

  /*------------------------------------------------------------*
   * If there is something on the shadow stack, get it and	*
   * check it against this token.				*
   *------------------------------------------------------------*/

  shadow_top(current_shadow_stack, &t, &proc, &open);
  t_is_choose = t == CHOOSE_TOK || t == LOOP_TOK || t == SUBCASES_TOK;
  t_is_let    = t == LET_TOK     || t == DEFINE_TOK ||
       		t == AWAIT_TOK   || t == ATOMIC_TOK  ||
	        t == CUTHERE_TOK || t == UNIQUE_TOK;

  if (  tok == CASE_TOK     && t != PATTERN_TOK   && !t_is_choose
      			    && t != CONTEXT_TOK   && !t_is_let

     || tok == CONTINUE_TOK && t != LOOP_TOK      && t != SUBCASES_TOK

     || tok == DO_TOK       && !t_is_choose       && t != FOR_TOK
      			    && !t_is_let	  && t != WHILE_TOK

     || tok == ELSE_TOK     && !t_is_choose       && t != TRY_TOK 
			    && t != RBRACE_TOK    && t != IF_TOK 
      			    && !t_is_let

     || tok == THEN_TOK	    && t != IF_TOK        && t != STREAM_TOK
			    && t != AWAIT_TOK     && t != TRY_TOK
      			    && t != FOR_TOK       && t != MIX_TOK
			    && !t_is_let

     || tok == WITH_TOK     && t != MIX_TOK 	  && t != STREAM_TOK
			    && !t_is_choose       && t != SPECIES_TOK
      			    && t != PACKAGE_TOK   && t != TEAM_TOK

     || tok == WHEN_TOK	    && !t_is_let	  && !t_is_choose

     || tok == LOOPCASE_TOK  && t != LOOP_TOK      && t != SUBCASES_TOK

     || tok == COMMA_TOK    && t != RPAREN_TOK    && t != RBRACE_TOK
			    && t != ASSUME_TOK    && t != EXPECT_TOK
			    && t != ANTICIPATE_TOK && t != VAR_TOK
			    && t != RBRACK_TOK    && t != IMPORT_TOK
			    && t != MISSING_TOK   && t != RLAZY_TOK
		            && t != AWAIT_TOK     && t != ABSTRACTION_TOK
      			    && t != ADVISORY_TOK  && t != CONTEXT_TOK
      			    && t != BRING_TOK     && t != RROLE_SELECT_TOK
      			    && t != PROC_ID_TOK

     || tok == EQUAL_TOK    && t != SPECIES_TOK   && t != DIRECTORY_TOK
			    && t != RELATE_TOK    && !t_is_let
			    && t != ABBREV_TOK
 			    && t != RELET_TOK     && t != EXPAND_TOK
			    && t != MATCH_TOK     && t != LOOP_TOK
      			    && t != PATTERN_TOK

     || tok == PLUS_EQUAL_TOK && !t_is_let	  && t != CONTEXT_TOK

     || tok == AMPERSAND_TOK && t != RELATE_TOK   && t != RBRACK_TOK
      			     && t != AWAIT_TOK    && t != RBRACK_COLON_TOK

     || tok == DBL_ARROW_TOK && t != DEFAULT_TOK  && t != PATTERN_TOK
			     && t != TRAP_TOK     && t != EXPAND_TOK
       			     && t != UNTRAP_TOK   && !t_is_let
			     && t != RPAREN_TOK
      			     && t != ADVISORY_TOK && t != ASK_TOK
			     && t != CONTEXT_TOK   && !t_is_choose) {

    /*-------------------------------------------------*
     * This token is out of place.  Decide what to do. *
     *-------------------------------------------------*/

#   ifdef DEBUG
      if(trace) trace_t(318, tok, t);
#   endif
    
    /*--------------------------------------------------------------*
     * Some obvious replacements for this token suggest themselves. *
     *--------------------------------------------------------------*/

#   ifdef NEVER
      if(tok == EQUAL_TOK && t == PATTERN_TOK) {
        if(pat_equal_ok) return tok;
        echo_token_l(yytext);
        yytext[0] = '\0';
        soft_syntax_error1(MEANT_ERR, "=>", 0);
        show_syntax(t);
        return DBL_ARROW_TOK;
      }
#   endif

#   ifdef NEVER
      if(tok == EQUAL_TOK && (t == ASSIGN_TOK)) {
        echo_token_l(yytext);
        yytext[0] = '\0';
        soft_syntax_error1(MEANT_ERR, ":=", 0);
        show_syntax(t);
        return COLON_EQUAL_TOK;
      }
#   endif

#   ifdef NEVER
      if(tok == COLON_EQUAL_TOK && 
         (t == LET_TOK || t == RELET_TOK)) {
        echo_token_l(yytext);
        yytext[0] = '\0';
        soft_syntax_error1(MEANT_ERR, "=", 0);
        show_syntax(t);
        return EQUAL_TOK;
      }
#   endif
   
    if(tok == AMPERSAND_TOK) {
      echo_token_l(yytext);
      yytext[0] = '\0';
      soft_syntax_error1(MEANT_ERR, std_id[CONS_SYM], 0);
      show_syntax(t);
      return symbolic_id_l(CONS_SYM);
    }
      
    soft_syntax_error1(UNEXPECTED_TOKEN_ERR, yytext, 0);
    shadow_top_nums(current_shadow_nums, &line, &col);
    err_print(CURRENT_CONTEXT_ERR, line);
    show_syntax(t);

    /*-------------------------------------------------------------*
     * If this is a declaration, give up and return the bad token. *
     *-------------------------------------------------------------*/

    shadow_top(current_shadow_stack, &shadow_tok, &proc, &open);
    kind = token_kind(shadow_tok);
    if(kind == DCL_BEGIN_K) return tok;

    /*---------------------------------------------------------------*
     * Otherwise, artificially end the construct that we are in now. *
     *---------------------------------------------------------------*/

    pop_shadow();
    push_back_token(tok, yylval, SEPARATOR_K, yytext);
    yytext[0] = '\0';  /* Suppress echo */
    return end_provided_l(t, proc, line);
  }

  return tok;
}


/****************************************************************
 *			REQUIRE_UC				*
 ****************************************************************
 * Complain if yytext does not begin with an upper case letter. *
 ****************************************************************/

void require_uc(void)
{
  register char c = *yytext;
  if(c < 'A' || 'Z' < c) {
    warn1(REQUIRE_UC_ERR, yytext, 0);
  }
}


/****************************************************************
 *			REQUIRE_LC				*
 ****************************************************************
 * Complain if yytext does not begin with a lower case letter.  *
 ****************************************************************/

void require_lc(void)
{
  register char c = *yytext;
  if(c < 'a' || 'z' < c) {
    warn1(REQUIRE_LC_ERR, yytext, 0);
  }
}


/****************************************************************
 *			CHECK_CASE				*
 ****************************************************************
 * Check that the case of the reserved word describe by att is  *
 * the same as ls.attr.  ls.attr is either LC_ATT or UC_ATT.	*
 ****************************************************************/

void check_case(struct lstr ls, ATTR_TYPE att)
{
  if(att == UC_ATT) {
    if(ls.attr == LC_ATT) {
      warn1(REQUIRE_UC_ERR, ls.name + 1, ls.line);
    }
  }    
  else {
    if(ls.attr == UC_ATT) {
       warn1(REQUIRE_LC_ERR, lower_case(ls.name + 1), ls.line);
    }
  }
}


/****************************************************************
 *			LINE_TOK_L				*
 ****************************************************************
 * Return the token that should be seen when  ==== is read.	*
 * That is LINE_TOK if all is well, or is an end word that      *
 * matches the top of the shadow stack if there are 		*
 * begin-words or brackets that have not been matched.		*
 ****************************************************************/

int line_tok_l()
{
  char *name;
  int tok, line, col;
  Boolean open;

  if(current_shadow_stack == NIL || panic_mode) return LINE_TOK;
  shadow_top(current_shadow_stack, &tok, &name, &open);
  if(tok == EXTENSION_TOK || tok == TEAM_TOK || tok == PACKAGE_TOK) {
    return LINE_TOK;
  }
  shadow_top_nums(current_shadow_nums, &line, &col);
  push_back_token(LINE_TOK, yylval, 0, yytext);
  yytext[0] = '\0';  /* Suppress repeat echo */
  pop_shadow();
  return end_provided_l(tok, name,line);
}
