/****************************************************************
 * File:    lexer/lexsup.c
 * Purpose: Support routines for lexer.
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
 * This file contains assorted support functions for the lexer. 	*
 * Included are								*
 *									*
 *   functions for managing reserved word lexemes and characteristics,	*
 *									*
 *   functions for recovering the lexemes of other kinds of tokens,	*
 *									*
 *   functions for setting up the contexts for various kinds of tokens, *
 *									*
 *   a function for handling semicolons,				*
 *									*
 *   functions for managing imports,					*
 *									*
 *   functions for reading string constants and long comments.		*
 ************************************************************************/

#include <string.h>
#include <ctype.h>
#include "../misc/misc.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../parser/tokens.h"
#include "../standard/stdfuns.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../tables/tables.h"
#include "../dcls/dcls.h"
#include "../error/error.h"
#include "../patmatch/patmatch.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
extern char yytext[];
extern int yyleng;

/****************************************************************
 *			RW_NAMES				*
 ****************************************************************
 * rw_names[k] is the name of token k + RW_OFFSET, which must   *
 * be a reserved word.  The name must be preceded by a % sign.  *
 * The last entry should have value NULL.			*
 ****************************************************************/

PRIVATE char* FARR rw_names[] =
{/* 0 			*/		NULL,
 /* ABBREV_TOK 		*/ 		"%Abbrev",
 /* ABSTRACTION_TOK	*/		"%Abstraction",
 /* ADVISORY_TOK 	*/		"%Advise",
 /* AND_TOK 		*/ 		"%_and_",
 /* ASSUME_TOK 		*/ 		"%Assume",
 /* ASK_TOK		*/		"%Ask",
 /* ATOMIC_TOK 		*/		"%Atomic",
 /* AWAIT_TOK 		*/ 		"%Await",
 /* BODY_TOK 		*/ 		"%Implementation",
 /* BRING_TOK 		*/ 		"%Bring",
 /* BUT_IF_FAIL_TOK	*/		"%_butIfFail_",
 /* BY_TOK 		*/ 		"%By",
 /* CASE_TOK 		*/ 		"%Case",
 /* CATCHING_TOK	*/		"%Catching",
 /* CHOOSE_TOK 		*/ 		"%Choose",
 /* CLASS_TOK		*/		"%Class",
 /* CONST_TOK		*/		"%Const",
 /* CONSTRUCTOR_TOK	*/		"%Constructor",
 /* CONSTRAINT_TOK	*/		"%Constraint",
 /* CONTEXT_TOK 	*/ 		"%Context",
 /* CONTINUE_TOK 	*/ 		"%Continue",
 /* DEFAULT_TOK 	*/ 		"%Default",
 /* DESCRIPTION_TOK 	*/ 		"%Description",
 /* DIRECTORY_TOK 	*/ 		"%Directory",
 /* DO_TOK 		*/ 		"%Do",
 /* ELSE_TOK 		*/ 		"%Else",
 /* EXCEPTION_TOK 	*/ 		"%Exception",
 /* EXECUTE_TOK 	*/ 		"%Execute",
 /* EXPECT_TOK 		*/ 		"%Expect",
 /* EXPORT_TOK 		*/ 		"%Export",
 /* EXTENSION_TOK 	*/ 		"%Extension",
 /* EXTRACT_TOK		*/		"%Extract",
 /* LCEXTRACT_TOK	*/		"%Extract",
 /* FOR_TOK 		*/ 		"%For",
 /* FROM_TOK		*/		"%From",
 /* HOLDING_TOK		*/		"%Holding",
 /* IF_TOK 		*/ 		"%If",
 /* IMPLEMENTS_TOK 	*/		"%Implements",
 /* IMPLIES_TOK		*/		"%_implies_",
 /* IMPORT_TOK 		*/ 		"%Import",
 /* INTERFACE_TOK 	*/		"%Interface",
 /* ISA_TOK		*/		"%Is_a",
 /* LET_TOK 		*/ 		"%Let",
 /* LOOP_TOK 		*/ 		"%Loop",
 /* LOOPCASE_TOK 	*/ 		"%Loopcase",
 /* MATCH_TOK 		*/ 		"%Match",
 /* MATCHING_TOK 	*/ 		"%Matching",
 /* MISSING_TOK 	*/ 		"%Missing",
 /* MIXED_TOK		*/		"%Mixed",
 /* OPEN_TOK 		*/ 		"%Open",
 /* OPERATOR_TOK 	*/ 		"%Operator",
 /* OR_TOK 		*/ 		"%_or_",
 /* PACKAGE_TOK 	*/ 		"%Package",
 /* PATTERN_TOK 	*/ 		"%Pattern",
 /* LCPATTERN_TOK 	*/ 		"%Pattern",
 /* RELATE_TOK 		*/ 		"%Relate",
 /* SPECIES_TOK 	*/ 		"%Species",
 /* SPECIES_AS_VAL_TOK	*/		"%SpeciesAsValue",
 /* STREAM_TOK 		*/ 		"%Stream",
 /* SUBCASES_TOK 	*/ 		"%Subcases",
 /* TARGET_TOK 		*/ 		"%Target",
 /* TEAM_TOK 		*/ 		"%Team",
 /* THEN_TOK 		*/ 		"%Then",
 /* TRAP_TOK 		*/ 		"%Trap",
 /* LCTRAP_TOK 		*/ 		"%Trap",
 /* VAR_TOK 		*/ 		"%Var",
 /* WHICH_TOK		*/		"%First",
 /* WHERE_TOK		*/		"%_where_",
 /* WHEN_TOK		*/		"%When",
 /* WITH_TOK		*/		"%With",
 /* WHILE_TOK 		*/ 		"%While",
 					NULL
};


/****************************************************************
 *			TOKEN_KINDS				*
 ****************************************************************
 * token_kinds[k] is the kind of token k + RW_OFFSET, which must*
 * be a reserved word.  The kind is one of the following.	*
 *								*
 *   DCL_BEGIN_K	if token can only start a dcl		*
 *								*
 *   EXPR_BEGIN_K	if token can only start an expression	*
 *			and does not allow semicolons
 *								*
 *   SEMI_BEGIN_K	if tok can only start an expression,	*
 *			and allows semicolons.			*
 *								*
 *   DE_BEGIN_K		if token can begin either a dcl or	*
 *			an expression.				*
 *								*
 *   OTHER_K		if token cannot start either a dcl or   *
 *			an expression.				*
 ****************************************************************/

PRIVATE char token_kinds[] =
{/* 0 			*/		OTHER_K,
 /* ABBREV_TOK 		*/ 		DCL_BEGIN_K,
 /* ABSTRACTION_TOK 	*/ 		DCL_BEGIN_K,
 /* ADVISORY_TOK 	*/		DCL_BEGIN_K,
 /* AND_TOK 		*/ 		OTHER_K,
 /* ASSUME_TOK 		*/ 		DE_BEGIN_K,
 /* ASK_TOK		*/	        SEMI_BEGIN_K,
 /* ATOMIC_TOK 		*/		EXPR_BEGIN_K,
 /* AWAIT_TOK 		*/ 		EXPR_BEGIN_K,
 /* BODY_TOK 		*/ 		OTHER_K,
 /* BRING_TOK 		*/ 		DCL_BEGIN_K,
 /* BUT_IF_FAIL_TOK	*/		OTHER_K,
 /* BY_TOK 		*/ 		OTHER_K,
 /* CASE_TOK 		*/ 		OTHER_K,
 /* CATCHING_TOK	*/		OTHER_K,
 /* CHOOSE_TOK 		*/ 		EXPR_BEGIN_K,
 /* CLASS_TOK		*/		DCL_BEGIN_K,
 /* CONST_TOK		*/		DCL_BEGIN_K,
 /* CONSTRUCTOR_TOK	*/		OTHER_K,
 /* CONSTRAINT_TOK	*/		OTHER_K,
 /* CONTEXT_TOK 	*/ 		DE_BEGIN_K,
 /* CONTINUE_TOK 	*/ 		OTHER_K,
 /* DEFAULT_TOK 	*/ 		DCL_BEGIN_K,
 /* DESCRIPTION_TOK 	*/ 		DCL_BEGIN_K,
 /* DIRECTORY_TOK 	*/ 		DCL_BEGIN_K,
 /* DO_TOK 		*/ 		OTHER_K,
 /* ELSE_TOK 		*/ 		OTHER_K,
 /* EXCEPTION_TOK 	*/ 		DE_BEGIN_K,
 /* EXECUTE_TOK 	*/ 		DCL_BEGIN_K,
 /* EXPECT_TOK 		*/ 		DCL_BEGIN_K,
 /* EXPORT_TOK 		*/ 		OTHER_K,
 /* EXTENSION_TOK 	*/ 		DCL_BEGIN_K,
 /* EXTRACT_TOK		*/		EXPR_BEGIN_K,
 /* LCEXTRACT_TOK	*/		OTHER_K,
 /* FOR_TOK 		*/ 		EXPR_BEGIN_K,
 /* FROM_TOK		*/		OTHER_K,
 /* HOLDING_TOK		*/		OTHER_K,
 /* IF_TOK 		*/ 		EXPR_BEGIN_K,
 /* IMPLEMENTS_TOK 	*/		OTHER_K,
 /* IMPLIES_TOK		*/		OTHER_K,
 /* IMPORT_TOK 		*/ 		DCL_BEGIN_K,
 /* INTERFACE_TOK 	*/		OTHER_K,
 /* ISA_TOK		*/		OTHER_K,
 /* LET_TOK 		*/ 		DE_BEGIN_K,
 /* LOOP_TOK 		*/ 		EXPR_BEGIN_K,
 /* LOOPCASE_TOK 	*/ 		OTHER_K,
 /* MATCH_TOK 		*/ 		SEMI_BEGIN_K,
 /* MATCHING_TOK 	*/ 		OTHER_K,
 /* MISSING_TOK 	*/ 		DE_BEGIN_K,
 /* MIXED_TOK		*/		OTHER_K,
 /* OPEN_TOK 		*/ 		OTHER_K,
 /* OPERATOR_TOK 	*/ 		DCL_BEGIN_K,
 /* OR_TOK 		*/ 		OTHER_K,
 /* PACKAGE_TOK 	*/ 		EXPR_BEGIN_K,
 /* PATTERN_TOK 	*/ 		DCL_BEGIN_K,
 /* LCPATTERN_TOK 	*/ 		OTHER_K,
 /* RELATE_TOK 		*/ 		DCL_BEGIN_K,
 /* SPECIES_TOK 	*/ 		DCL_BEGIN_K,
 /* SPECIES_AS_VAL_TOK	*/		EXPR_BEGIN_K,
 /* STREAM_TOK 		*/ 		EXPR_BEGIN_K,
 /* SUBCASES_TOK 	*/ 		OTHER_K,
 /* TARGET_TOK 		*/ 		OTHER_K,
 /* TEAM_TOK 		*/ 		DE_BEGIN_K,
 /* THEN_TOK 		*/ 		OTHER_K,
 /* TRAP_TOK 		*/ 		EXPR_BEGIN_K,
 /* LCTRAP_TOK 		*/ 		OTHER_K,
 /* VAR_TOK 		*/ 		DE_BEGIN_K,
 /* WHICH_TOK		*/		OTHER_K,
 /* WHERE_TOK		*/		OTHER_K,
 /* WHEN_TOK		*/		OTHER_K,
 /* WITH_TOK		*/		OTHER_K,
 /* WHILE_TOK 		*/ 		EXPR_BEGIN_K
};


/****************************************************************
 *			RW_TBL					*
 ****************************************************************
 * rw_tbl gives token kinds and names for auxiliary tokens.     *
 * The first column is the token, the second the kind, and      *
 * the third the name, with a leading % (for a reserved word).  *
 * All reserved words that have no entry in rw_names must have  *
 * an entry here.  Some additional tokens that are not defined  *
 * for the parser have entries here.				*
 ****************************************************************/

PRIVATE struct rw_tbl_struct {
  int tok;
  int kind;
  char *str;
} FARR rw_tbl[] =
{
 {EXPAND_TOK,		DCL_BEGIN_K,	"%Expand"},
 {ANTICIPATE_TOK,	DCL_BEGIN_K,	"%Anticipate"},
 {DEFINE_TOK,		DE_BEGIN_K,	"%Define"},
 {RELET_TOK,		EXPR_BEGIN_K,	"%Relet"},
 {MIX_TOK, 		EXPR_BEGIN_K,	"%Mix"},
 {UNIQUE_TOK, 		EXPR_BEGIN_K,	"%Unique"},
 {CUTHERE_TOK, 		EXPR_BEGIN_K,	"%CutHere"},
 {TRY_TOK, 		EXPR_BEGIN_K,	"%Try"},
 {UNTRAP_TOK, 		EXPR_BEGIN_K,	"%Untrap"},
 {EXITCASE_TOK,		0,		"%Exitcase"},
 {CATCHINGEACH_TOK,	0,		"%CatchingEachThread"},
 {CATCHINGTERM_TOK,	0,		"%CatchingTermination"},
 {ALL_TOK,		0,		"%All"},
 {ONE_TOK,		0,		"%One"},
 {PERHAPS_TOK,		0, 		"%Perhaps"},
 {LWRAPQ_TOK,		EXPR_BEGIN_K,	"?<<"},
 {RWRAPQ_TOK,		0,		">>?"},
 {LNONSHARED_TOK,	EXPR_BEGIN_K,	"<:%"},
 {RNONSHARED_TOK,	0,		"%:>"},
 {LSHARED_TOK,		EXPR_BEGIN_K,	"<:#"},
 {RSHARED_TOK,		0,		"#:>"},
 {0,			0,		NULL}
};


/*******************************************************************
 *			TOKEN_MAP				   *
 *******************************************************************
 * Some tokens represent more than one thing.  For example, 	   *
 * reserved words If and Try are both sent to the parser as the	   *
 * same token, IF_TOK.  Tokens such as TRY_TOK exist, and are	   *
 * define in parser/tokmod.h.  They are never sent to the 	   *
 * parser, however.						   *
 *								   *
 * Array token_map tells information about these shared tokens.	   *
 * The first column is the actual token, the second   		   *
 * the shared token, and the third the attribute that distinguishes*
 * them.  All of the tokens in the second column must have kind    *
 * ls_at in parser.y.				 		   *
 *								   *
 * Reserved words that can be begin words or separators,           *
 * depending on the case of their first letters, are also here.    *
 * They should be given with their attribute being UC_ATT.  This   *
 * is required for semicolons to push back the correct thing.	   *
 *								   *
 * The last entry must be three zeros.				   *
 *******************************************************************/

struct token_map FARR token_map[] =
{ 
  {ASSUME_TOK,		ASSUME_TOK,     UC_ATT},
  {AWAIT_TOK,		AWAIT_TOK,      UC_ATT},
  {DEFAULT_TOK,		DEFAULT_TOK,	UC_ATT},
  {DESCRIPTION_TOK,	DESCRIPTION_TOK,UC_ATT},
  {EXCEPTION_TOK,	EXCEPTION_TOK,	UC_ATT},
  {MISSING_TOK,		MISSING_TOK,	UC_ATT},
  {SPECIES_TOK,		SPECIES_TOK,	UC_ATT},

  {ATOMIC_TOK,		ATOMIC_TOK,	ATOMIC_ATT},
  {UNIQUE_TOK, 		ATOMIC_TOK, 	FIRST_ATT},
  {CUTHERE_TOK, 	ATOMIC_TOK, 	CUTHERE_ATT},
  {UNTRAP_TOK, 		TRAP_TOK, 	UNTRAP_ATT},
  {TRAP_TOK,		TRAP_TOK,	TRAP_ATT},
  {RELET_TOK,		LET_TOK,	RELET_ATT},
  {DEFINE_TOK,		LET_TOK,	DEFINE_ATT},
  {LET_TOK,		LET_TOK,	LET_ATT},
  {EXPECT_TOK,		EXPECT_TOK,	EXPECT_ATT},
  {ANTICIPATE_TOK,	EXPECT_TOK,	ANTICIPATE_ATT},
  {TRY_TOK,     	IF_TOK,     	TRY_ATT},
  {IF_TOK,		IF_TOK,		IF_ATT},
  {MIX_TOK,     	STREAM_TOK, 	MIX_ATT},
  {STREAM_TOK,		STREAM_TOK,	STREAM_ATT},
  {PATTERN_TOK,		PATTERN_TOK,	PATTERN_ATT},
  {EXPAND_TOK,		PATTERN_TOK,	EXPAND_ATT},
  {EXITCASE_TOK,	CASE_TOK,	UNTIL_ATT},
  {LNONSHARED_TOK,	LBOX_TOK,	NONSHARED_ATT},
  {RNONSHARED_TOK,	RBOX_TOK,	NONSHARED_ATT},
  {LWRAPQ_TOK,		LWRAP_TOK, 	1},
  {RWRAPQ_TOK,		RWRAP_TOK,	1},
  {0, 			0, 		0}
};  

/****************************************************************
 *			TOKEN_NAMES				*
 ****************************************************************
 * token_names[k] is the name of symbolic token k + TN_OFFSET.  *
 * It is only used for tokens from COMMA_TOK to PERIOD_TOK.	*
 ****************************************************************/

PRIVATE char* token_names[] =
{ /* 0			*/		NULL,
  /* LPAREN_TOK		*/		"(",
  /* RPAREN_TOK		*/		")",

  /* LBRACE_TOK		*/		"{",
  /* RBRACE_TOK		*/		"}",

  /* LBRACK_TOK		*/		"[",
  /* RBRACK_TOK		*/		"]",

  /* LWRAP_TOK		*/		"<<",
  /* RWRAP_TOK		*/		">>",

  /* LBRACK_COLON_TOK	*/		":]",
  /* RBRACK_COLON_TOK	*/		"[:",

  /* LROLE_SELECT_TOK	*/		"{:",
  /* RROLE_SELECT_TOK	*/		":}",
  /* LROLE_MODIFY_TOK	*/		"/{:",

  /* LLAZY_TOK		*/		"(:",
  /* RLAZY_TOK		*/		":)",

  /* LPAREN_DBLBAR_TOK  */		"(||",
  /* RPAREN_DBLBAR_TOK	*/		"||)",

  /* LBOX_TOK		*/		"<:",
  /* RBOX_TOK		*/		":>",

  /* COMMA_TOK 		*/		",",
  /* COLON_TOK		*/		":",
  /* SEMICOLON_TOK	*/		";",
  /* ROLE_MARK_TOK	*/		"~>",
  /* SGL_ARROW_TOK	*/		"->",
  /* DBL_ARROW_TOK	*/		"=>",
  /* AMPERSAND_TOK	*/		"&",
  /* EQUAL_TOK		*/		"=",
  /* PLUS_EQUAL_TOK	*/		"<-",
  /* COLON_EQUAL_TOK	*/		":=",
  /* BAR_TOK		*/		"|",
  /* QUESTION_MARK_TOK  */		"?",
  /* PERIOD_TOK 	*/		"."
};


/****************************************************************
 *			INIT_RESERVED_WORDS_L			*
 ****************************************************************
 * Put the reserved words in the identifier table, with a % 	*
 * sign in front of each.					*
 ****************************************************************/

void init_reserved_words_l(void)
{
  char** p;
  struct rw_tbl_struct* q;

  for(p = rw_names + 1; *p != NULL; p++) {
    stat_id_tb(*p);
  }

  for(q	= rw_tbl; q->tok != 0; q++) {
    stat_id_tb(q->str);
  }
}


/****************************************************************
 *				RW_NAME				*
 ****************************************************************
 * Return the name of reserved word token tok.  The name starts *
 * with a %. For example, the name of token Let is "%Let".	*
 *								*
 * This function does not take into account the attribute of    *
 * the token, so it does not deal with tokens that have		*
 * multiple names.  It does, however, handle tokens that are    *
 * pseudo-tokens, such as RELET_TOK, that only occur on the     *
 * lexer's shadow stack, and that are used in cases where the   *
 * parser uses a single token for more than one reserved word.  *
 * (The parser uses LET_TOK for both "Let" and "Relet", but the *
 * lexer pushes different tokens onto the shadow stack.)        *
 ****************************************************************/

char* rw_name(int tok)
{
  struct rw_tbl_struct *p;

  if(tok >= FIRST_RESERVED_WORD_TOK && tok <= LAST_RESERVED_WORD_TOK) {
    return rw_names[tok - RW_OFFSET];
  }

  for(p = rw_tbl; p->tok != 0; p++) {
    if(tok == p->tok) return p->str;
  }

  return "%?";
}


/****************************************************************
 *			LPAREN_FROM_RPAREN			*
 ****************************************************************
 * Return the name of the left-parenthesis that corresponds to  *
 * right parenthesis token tok.					*
 ****************************************************************/

char* lparen_from_rparen(int tok)
{
  return token_name(tok-1, yylval);
}


/****************************************************************
 *			TOKEN_NAME				*
 ****************************************************************
 * token_name(tok,lval) returns the name of token tok, whose    *
 * attribute is lval. tok must be a reserved word or left or    *
 * right parenthesis or one of the tokens from COMMA_TOK to	*
 * PERIOD_TOK.							*
 ****************************************************************/

char* token_name(int tok, YYSTYPE lval)
{
  struct token_map *p;

  /*------------------------------------------------*
   * Handle words that have more than one lexeme.   *
   * Translate these to their alternative tokens.   *
   *------------------------------------------------*/

  for(p = token_map; p->tok != 0; p++) {
    if(p->new_tok == tok && p->attr == lval.ls_at.attr) {
      tok = p->tok;
      break;
    }
  }

  /*----------------------------------*
   * Tokens LPAREN_TOK to PERIOD_TOK. *
   *----------------------------------*/

  if(tok >= LPAREN_TOK && tok <= PERIOD_TOK) {
    return token_names[tok - TN_OFFSET];
  }

  /*----------------------------------*
   * Reserved words.		      *
   *----------------------------------*/

  return rw_name(tok) + 1;
}


/****************************************************************
 *			TOKEN_KIND				*
 ****************************************************************
 * tok must be a reserved word that can start an expression or  *
 * declaration.							*
 *								*
 * token_kind(tok) is one of					*
 *								*
 *   DCL_BEGIN_K	if tok can only start a dcl		*
 *   EXPR_BEGIN_K	if tok can only start an expression	*
 *			and does not allow semicolons		*
 *   SEMI_BEGIN_K	if tok can only start an expression,	*
 *			and allows semicolons.			*
 *   DE_BEGIN_K		if tok can begin either a dcl or	*
 *			an expression.				*
 ****************************************************************/

int token_kind(int tok) 
{
  struct rw_tbl_struct *p;

  if(tok >= ABBREV_TOK && tok <= LINE_TOK) {
    return token_kinds[tok - RW_OFFSET];
  }

  for(p = rw_tbl; p->tok != 0; p++) {
    if(tok == p->tok) return p->kind;
  }

  return 0;
}


/****************************************************************
 *			TOKEN_ATTR				*
 ****************************************************************
 * Return the attribute of a token that needs an attribute.	*
 ****************************************************************/

int token_attr(int tok)
{
  struct token_map *p;

  for(p = token_map; p->tok != 0; p++) {
    if(p->tok == tok) return p->attr;
  }
  return 0;
}


/****************************************************************
 *			IS_BEGIN_TOK				*
 ****************************************************************
 * Return true if tok is a begin token, of kind EXPR_BEGIN_K,	*
 * DCL_BEGIN_K or DE_BEGIN_K.					*
 ****************************************************************/

Boolean is_begin_tok(int tok)
{
  int kind = token_kind(tok);
  return (kind == EXPR_BEGIN_K || kind == DCL_BEGIN_K || kind == DE_BEGIN_K);
}


/****************************************************************
 *			IS_RESERVED_WORD_STR			*
 ****************************************************************
 * Return TRUE just when WORD is a reserved word.		*
 ****************************************************************/

Boolean is_reserved_word_str(CONST char *word)
{
  char rword[MAX_ID_SIZE + 2];

  /*------------------------------------------------------------*
   * Detect a reserved word W by looking for %W in the 		*
   * identifier table.  Be sure that the first character of W   *
   * is upper case.						*
   *------------------------------------------------------------*/

  rword[0] = '%';
  strcpy(rword+1, word);
  if('a' <= rword[1] && rword[1] <= 'z') rword[1] += 'A' - 'a';
  return id_tb_check(rword) != NULL;
}


/****************************************************************
 *			IS_RESERVED_WORD_TOK			*
 ****************************************************************
 * Return true if tok is a reserved word.			*
 ****************************************************************/

Boolean is_reserved_word_tok(int tok)
{
  struct rw_tbl_struct *p;

  if(tok >= FIRST_RESERVED_WORD_TOK && tok <= LAST_RESERVED_WORD_TOK) {
    return TRUE;
  }

  for(p = rw_tbl; p->tok != 0; p++) {
    if(tok == p->tok) {
      return p->str[0] == '%';
    }
  }

  return FALSE;
}


/****************************************************************
 *			    BEGIN_WORD_L			*
 ****************************************************************
 * Set attributes for a begin word, and push it onto the shadow *
 * stack.							*
 ****************************************************************/

void begin_word_l(int bw_tok)
{

  /*--------------------------------------------------------------------*
   * zero_context_l sets up yylval.ls_at.line and yylval.ls_at.column. 	*
   *									*
   * If open_col is positive, then it is the column where the word 	*
   * 'open' starts just before this word.  That column should be taken	*
   * as the column of this begin word.					*
   *--------------------------------------------------------------------*/
  
  zero_context_l(bw_tok);
  if(open_col >= 0) yylval.ls_at.column = open_col;
  open_col = -1;

  /*-------------------------*
   * Handle a reserved word. *
   *-------------------------*/

  if(bw_tok <= LAST_RESERVED_WORD_TOK) {
    yylval.ls_at.name = rw_name(bw_tok);
    yylval.ls_at.tok  = bw_tok;
    recent_begin_from_semicolon = FALSE;
  }

  /*--------------------------------------------------------------------*
   * Handle a parenthesis.  In this case, bw_tok should actually be 	*
   * the matching right parenthesis token. 				*
   *--------------------------------------------------------------------*/

  else {
    yylval.ls_at.name = id_tb0(yytext);
    yylval.ls_at.tok  = bw_tok;
  }

  if(!panic_mode) push_shadow(bw_tok);
}


/****************************************************************
 *			  REST_CONTEXT_L			*
 ****************************************************************
 * See one_context_l, zero_context_l.				*
 ****************************************************************/

void rest_context_l(int kind)
{
  /*--------------------------------------------------------------*
   * Keep import_start at 0.  But don't reset at }, since a } can *
   * occur in the options of an import declaration 		  *
   *--------------------------------------------------------------*/

  if(kind != RBRACE_TOK) import_start = 0;
  yylval.ls_at.line   = current_line_number;
  yylval.ls_at.column = chars_in_this_line;
  if(seen_package_end) warn0(TEXT_AFTER_END_PACKAGE_ERR, 0);
} 


/****************************************************************
 *			  ONE_CONTEXT_L				*
 ****************************************************************
 * Set attributes for a non-begin-word that can be followed by  *
 * an operator.  Reset import_start to 0 if kind is not 	*
 * RBRACE_TOK.							*
 ****************************************************************/

void one_context_l(int kind) 
{
  opContext = 1;
  force_no_op = 0;
  rest_context_l(kind);
}


/****************************************************************
 *			   ZERO_CONTEXT_L			*
 ****************************************************************
 * Set attributes for a non-begin-word that cannot be followed 	*
 * by an operator.  Reset import_start to 0 if 			*
 * kind != RBRACE_TOK.  (RBRACE_TOK is special because it ends  *
 * the declaration options, and these options might be part of  *
 * an import declaration.  So we must not disturb things then.)	*
 ****************************************************************/

void zero_context_l(int kind)
{
  opContext = 0;
  force_no_op = 0;
  rest_context_l(kind);
}


/****************************************************************
 *			  DO_SEMICOLON_L			*
 ****************************************************************
 * Return appropriate token for a semicolon.  If called for,	*
 * also push back a token, so that a semicolon can represent	*
 * two tokens (an end and a begin).				*
 ****************************************************************/

int do_semicolon_l()
{
  Boolean open;
  int kind, tok, col, line;
  char *name;

  zero_context_l(0);
  if(panic_mode) return SEMICOLON_TOK;
  tok = kind = 0;

  /*--------------------------------------------------------------------*
   * If there is nothing on the shadow stack, just return the semicolon.*
   *--------------------------------------------------------------------*/

  if(current_shadow_stack == NIL) return SEMICOLON_TOK;

  /*-------------------------------------------------*
   * Echo and clear yytext to prevent further echos. *
   *-------------------------------------------------*/

  echo_token_l(yytext);
  yylval.ls_at.name = name = "(none)";
  yytext[0] = '\0';
  yyleng = 0;

  /*--------------------------------------------------------*
   * Check top of shadow stack to see if a semicolon is ok. *
   * Here, just get what is on top of the shadow stack.     *
   *--------------------------------------------------------*/

  shadow_top(current_shadow_stack, &tok, &name, &open);

  /*----------------------------------------------------*
   * If tok is PACKAGE_TOK or FOR_TOK, just return the 	*
   * semicolon. 					*
   *----------------------------------------------------*/

  if(tok == PACKAGE_TOK || tok == FOR_TOK) return SEMICOLON_TOK;

  /*---------------------------------------------*
   * Handle a reserved word on the shadow stack. *
   *---------------------------------------------*/

  shadow_top_nums(current_shadow_nums, &line, &col);
  if(tok <= LAST_RESERVED_WORD_TOK) {
    kind = token_kind(tok);
    yylval.ls_at.tok    = -tok;                 /* Sign indicates semicolon */
    yylval.ls_at.name   = name;
    yylval.ls_at.attr   = token_attr(tok);
    yylval.ls_at.column = col;
    yylval.ls_at.line   = current_line_number;
  }

  /*----------------------------------------------------------------*
   * If a semicolon is ok, replace the semicolon by an (end, begin) *
   * pair of the appropriate kind.  If the construct that the       *
   * semicolon ends is open, push (end open begin).		    *
   *----------------------------------------------------------------*/

  if(kind == SEMI_BEGIN_K || kind == DCL_BEGIN_K || kind == DE_BEGIN_K) {
    push_back_token(tok, yylval, BEGIN_K, NULL);
    if(open) push_back_token(OPEN_TOK, yylval, OTHER_K, NULL);

#   ifdef DEBUG
      if(trace_lexical) trace_t(319, yylval.ls_at.name);
#   endif

    pop_shadow();
    recent_begin_from_semicolon = TRUE;
    return END_TOK;  
  }

  /*--------------------------------------------*
   * Handle a procedure id on the shadow stack. *
   * Replace the semicolon by (end proc) where  *
   * proc is the procedure that is on top of    *
   * the shadow stack.  If the procedure call 	*
   * on the stack is open, push back (end open  *
   * proc).					*
   *--------------------------------------------*/

  if(tok == PROC_ID_TOK) {
    yylval.ls_at.tok  = -PROC_ID_TOK;   /* Sign indicates from semicolon */
    yylval.ls_at.name = name;
    yylval.ls_at.column = col;
    yylval.ls_at.line = current_line_number;
    push_back_token(tok, yylval, BEGIN_K, NULL);
    if(open) push_back_token(OPEN_TOK, yylval, OTHER_K, NULL);

#   ifdef DEBUG
      if(trace_lexical) trace_t(319, name);
#   endif

    pop_shadow();
    recent_begin_from_semicolon = TRUE;
    return END_TOK;
  }

  /*------------------------------------------------------------------------*
   * Otherwise, supply an end and push back the semicolon to be read again. *
   *------------------------------------------------------------------------*/

# ifdef DEBUG
    if(trace) trace_t(320, tok);
# endif

  unput_yy(';');
  if(tok <= LAST_RESERVED_WORD_TOK || tok == PROC_ID_TOK) {
    int errkind;
    errkind = (tok == PROC_ID_TOK) ? PROC_ENDED_ERR : END_PROVIDED_ERR;
    soft_syntax_error2(errkind, name, (char *) line, 0);
#   ifdef DEBUG
      if(trace_lexical) trace_t(319, name);
#   endif
    pop_shadow();
    recent_begin_from_semicolon = FALSE;
    return END_TOK;  
  }
  else {
    soft_syntax_error2(PAREN_PROVIDED_ERR, 
		       token_name(tok, yylval), (char *) line, 0);
#   ifdef DEBUG
      if(trace_lexical) trace_t(321, token_name(tok,yylval));
#   endif
    pop_shadow();
    return tok;
  }
}  
 

/****************************************************************
 *		        FIX_YYTEXT_FOR_END_MY_L	       		*
 ****************************************************************
 * yytext contains "%My" or "%my" followed by some white space  *
 * followed by an identifier P.  Replace the contents of yytext *
 * by "%My-" followed by P.					*
 ****************************************************************/

void fix_yytext_for_end_my_l(void)
{
  char *p, *q;

  yytext[1] = 'M';
  yytext[3] = '-';
  p = q = yytext + 4;
  if(isspace(*p)) {
    while(isspace(*p)) p++;
    while(*p != '\0') *(q++) = *(p++);
    *q = '\0';
  }
}


/****************************************************************
 *		        HANDLE_ENDWORD_L	       		*
 ****************************************************************
 * Handle an end word, such as %let.  If this is %package, it	*
 * might be necessary to end an import.  The lexeme of the      *
 * word is in yytext.						*
 ****************************************************************/

int handle_endword_l()
{
  int package_end, result;
  
  /*--------------------------------------------------------------*
   * Check if this end word is %Package.  If so, then this might  *
   * be the end of an import, or the end of the interface part	  *
   * of the main package being compiled.			  *
   *--------------------------------------------------------------*/

  package_end = lcequal(yytext+1, "package");
  if(import_level > 0 && package_end) {
    end_word_l(0);
    end_import_l();
    return BODY_TOK;
  }
  else {
    result = end_word_l(0);
    if(package_end) {
      seen_package_end = TRUE;
    }
    return result;
  }
}


/****************************************************************
 *			HANDLE_STRING_L				*
 ****************************************************************
 * Read in a string constant and deal with it.  Normally, that	*
 * requires only returning STRING_CONST_TOK.  When the string 	*
 * constant is a file to import, it involves setting things up	*
 * to import the file.  The token to return is returned by 	*
 * handle_string_l.						*
 ****************************************************************/

int handle_string_l()
{
  read_string_l(FALSE); 

# ifdef DEBUG
    if(trace_lexical && import_start != 0) {
      trace_t(324, import_start);
    }
# endif

  /*------------------------------------------------------------*
   * Handle the case where this string is the name of a package *
   * that is being imported.					*
   *------------------------------------------------------------*/

  if(import_start) {

    /*----------------------------------------------------------------*
     * If the name of the "file" to be imported starts with '-', then *
     * we are being asked to import primitives.	 Return RECOVER,      *
     * since no file is being imported.				      *
     *----------------------------------------------------------------*/

    if(string_const[0] == '-') {
      do_prim_import((char *)(string_const + 1));
      return RECOVER;
    }

    /*-----------------------------------------*
     * Otherwise, we are asked to read a file. *
     *-----------------------------------------*/

    else {
      import_level++;

      /*--------------------------------------------------------*
       * Try to push the package.  If the push succeeds, then 	*
       * report the imported file, and set up the context for 	*
       * this import.						*
       *--------------------------------------------------------*/

      if(push_input_file((char *) string_const)) {
	if(import_start == 1) {
	  report_dcl_p(current_file_name, IMPORT_E, 0, NULL, NULL);
	  main_context = IMPORT_CX;
#         ifdef DEBUG
            set_traces(trace_imports);
#         endif
	  yylval.ls_at.name = string_const_tb0(file_info_st->file_name);
	  clear_table_memory();
	}
      }

      /*----------------------------------------------------------*
       * Here, the push failed.  It might have failed because the *
       * package does not exist, but it can also fail because the *
       * package has already been read.				*
       *----------------------------------------------------------*/

      else {
#       ifdef DEBUG
	  if(trace) trace_t(325);
#       endif
	import_level--;
	yylval.ls_at.name = NULL;
	yytext[0] = '\0';
        return RECOVER;
      }
    } /* end else(read a file) */
  } /* end if(import_start) */

  /*------------------------------------------------------------*
   * We get here when this is just a string constant, or in the *
   * case of a successful import.  In either event, we return   *
   * token STRING_CONST_TOK.					*
   *------------------------------------------------------------*/
 
  one_context_l(0);
  return(STRING_CONST_TOK);
}




