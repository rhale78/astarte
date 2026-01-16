/****************************************************************
 * File:    parser/parser.y
 * Purpose: Yacc parser for Astarte
 * Author:  Karl Abrahamson
 ****************************************************************/

%{
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

#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#ifdef UNIX
# include <sys/unistd.h>
#endif
#include "../parser/tokmod.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../exprs/expr.h"
#include "../exprs/brngexpr.h"
#include "../standard/stdtypes.h"
#include "../standard/stdfuns.h"
#include "../standard/stdids.h"
#include "../unify/unify.h"
#include "../unify/constraint.h"
#include "../infer/infer.h"
#include "../dcls/dcls.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../parser/parser.h"
#include "../parser/asd.h"
#include "../lexer/lexer.h"
#include "../lexer/modes.h"
#include "../ids/open.h"
#include "../ids/ids.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/vartbl.h"
#include "../clstbl/dflttbl.h"
#include "../clstbl/abbrev.h"
#include "../clstbl/meettbl.h"
#include "../error/error.h"
#include "../patmatch/patmatch.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../evaluate/instruc.h"
#include "../machstrc/machstrc.h"
#include "../machdata/except.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
#ifdef MSWIN
# define STRICT
# include <windows.h>
#endif
#ifdef SMALL_STACK
# undef alloca
# define alloca alloc
#endif
#define yylex yylexx

PRIVATE void 		begin_export		(void);
PRIVATE void 		end_dcl			(void);
PRIVATE void 		start_dcl		(int);
PRIVATE Boolean 	do_dcl_p		(void);
PRIVATE void 		debug_at_dcl_panic	(void);
PRIVATE void 		debug_at_dcl_recover	(void);

/************************************************************************
 *			stream_st					*
 ************************************************************************
 * Used to control processing of stream and mix expressions.  The top   *
 * of this stack is STREAM_ATT when reading a stream expression, and    *
 * is MIX_ATT when reading a mix expression.				*
 ************************************************************************/

PRIVATE INT_STACK stream_st = NULL;

/************************************************************************
 *			if_dcl_st					*
 *			if_dcl_then_st					*
 ************************************************************************
 * These stacks are used to control processing of if-declarations	*
 * Their use is describe in parser.y under ifDcl.			*
 ************************************************************************/

PRIVATE INT_STACK	if_dcl_st      = NULL;
PRIVATE INT_STACK	if_dcl_then_st = NULL;

/************************************************************************
 *			abbrev_code					*
 *			abbrev_val					*
 ************************************************************************
 * These are used in processing abbrev-declarations.  See		*
 * abbrevDcl for a description of how they are used.			*
 ************************************************************************/

PRIVATE int	abbrev_code;
PRIVATE TYPE*	abbrev_val = NULL;

/************************************************************************
 *			propagate_var_opt_st				*
 ************************************************************************
 * propagate_var_opt_st is a stack whose top is the current var-mode    *
 * to be propagated across a semicolon.					*
 ************************************************************************/

PRIVATE MODE_STACK propagate_var_opt_st = NULL;

/************************************************************************
 *			patexp_dcl_kind					*
 ************************************************************************
 * This tells whether a pattern or expect declaration is being		*
 * processed.  It is either PAT_DCL_E or EXPAND_E.			*
 ************************************************************************/

PRIVATE EXPR_TAG_TYPE patexp_dcl_kind;

/************************************************************************
 *			in_context_dcl					*
 *			context_dcl_id					*
 ************************************************************************
 * These are used for reading a context declaration.  See parser.y under*
 * contextDcl for their use.						*
 ************************************************************************/

PRIVATE char*	context_dcl_id;
PRIVATE Boolean in_context_dcl = FALSE;

/************************************************************************
 *			abbrev_var_table				*
 ************************************************************************
 * This table is used for keeping track of copies of variable from	*
 * abbreviations.  See typeExpr.					*
 ************************************************************************/

PRIVATE HASH2_TABLE*  abbrev_var_table = NULL;

/************************************************************************
 *			var_initial_val					*
 *			var_content_type				*
 ************************************************************************
 * These variables are used while processing var expressions and	*
 * var declarations.  See varDcl or varExpr for specifics.		*
 ************************************************************************/

PRIVATE EXPR*	var_initial_val;
PRIVATE TYPE*	var_content_type;

/************************************************************************
 *			all_init					*
 ************************************************************************
 * all_init is set true if the initializer for a var declaration or	*
 * expression has the word 'all' in it.  It is short-lived: it is set   *
 * just before finishing an initializer, and should be consumed before  *
 * reading any more expressions.					*
 ************************************************************************/

PRIVATE Boolean all_init;

/************************************************************************
 *			holding_phrase_required				*
 ************************************************************************
 * holding_phrase_required is true when a holding phrase is required in *
 * a var declaration.							*
 ************************************************************************/

PRIVATE Boolean holding_phrase_required = FALSE;

/************************************************************************
 *			in_bring_dcl					*
 ************************************************************************
 * in_bring_dcl is true when processing a bring declaration.  It is 	*
 * used to distinguish a bring declaration from a bring expression.	*
 ************************************************************************/

PRIVATE Boolean in_bring_dcl = FALSE;

/************************************************************************
 *			advisory_dcl					*
 ************************************************************************
 * Used in advisory declarations to distinguish between declarations	*
 * and expressions.							*
 ************************************************************************/

PRIVATE Boolean advisory_dcl;

/************************************************************************
 *			suppress_reset					*
 ************************************************************************
 * Used to prevent start_dcl from clobbering the effects of an		*
 * advisory declaration prematurely.					*
 ************************************************************************/

PRIVATE Boolean suppress_reset;


/************************************************************************
 *			dcl_line					*
 ************************************************************************
 * This variable keeps track of the start line of a declaration, so     *
 * that the parts of a declaration can refer to it.			*
 ************************************************************************/

PRIVATE int dcl_line;

/************************************************************************
 *			which_result					*
 *			match_kind_result				*
 *			choose_from_result				*
 ************************************************************************
 * When nonterminal which is processed, it puts its answer into 	*
 * which_result, in addition to returning it as its attribute.		*
 *									*
 * When nonterminal clwhich is processed, it puts 0 into 		*
 * match_kind_result if there is no 'matching' phrase, and puts	1 there	*
 * if there is a 'matching' phrase.					*
 *									*
 * When there is a matching phrase, clwhich puts the variable the	*
 * target of the match into choose_from_result.  It is ref-counted.	*
 ************************************************************************/

PRIVATE ATTR_TYPE which_result;
PRIVATE int match_kind_result;
PRIVATE EXPR *choose_from_result;

%}


/****************************************************************/
/* 				TOKENS				*/
/****************************************************************/

/****************************************************************
 *                     -------NOTE-------			*
 * If you modify this list of tokens 				*
 *								*
 * 1. Check the tables in leser/lexsup.c to see if any need	*
 *    updating.							*
 *								*
 * 2. Check file messages/syntax.txt to see if it needs		*
 *    updating.							*
 *								*
 * 3. touch file parser/stmptok before remake.			*
 ****************************************************************/


/************************* OPERATORS *****************************
 *								 *
 * Each operator really represents a class of symbols, since new *
 * operators can be defined.  One or more members is given for   *
 * each.							 *
 *								 *
 * The lval for these tokens holds the line number and the name	 *
 * of the operator.						 *
 *****************************************************************/

%token <ls_at> L1_OP			/* Operator _opL1_		*/
%token <ls_at> L2_OP			/* Operator _opL2_		*/
%token <ls_at> L3_OP			/* Operator _opL3_		*/
%token <ls_at> L4_OP			/* Operator _opL4_		*/
%token <ls_at> L5_OP			/* Operator _opL5_		*/
%token <ls_at> L6_OP			/* Operator _opL6_		*/
%token <ls_at> L7_OP			/* Operator _opL7_		*/
%token <ls_at> L8_OP			/* Operator _opL8_		*/
%token <ls_at> L9_OP			/* Operator _opL9_		*/
%token <ls_at> R1_OP			/* Operator _opR1_		*/
%token <ls_at> R2_OP			/* Operator _opR2_		*/
%token <ls_at> R3_OP			/* Operator _opR3_		*/
%token <ls_at> R4_OP			/* Operator _opR4_		*/
%token <ls_at> R5_OP			/* Operator _opR5_		*/
%token <ls_at> R6_OP			/* Operator _opR6_		*/
%token <ls_at> R7_OP			/* Operator _opR7_		*/
%token <ls_at> R8_OP			/* Operator _opR8_		*/
%token <ls_at> R9_OP			/* Operator _opR9_		*/
%token <ls_at> COMPARE_OP		/* operators ==, >, etc.	*/
%token <ls_at> BAR_OP			/* operators at same prec as |  */
%token <ls_at> COLON_EQUAL_OP		/* operators at same prec as := */

%token <ls_at> UNARY_OP		        /* Unary operators        	*/


/************************* RESERVED WORDS *******************************
 *									*
 * Each reserved word that has a matching end word has an attribute	*
 * holding the line number that it occurs on.				*
 *									*
 * END_TOK is any end marker %W, and its attribute gives the word W (as *
 * the name field) and the line number and column on which it occurs.   *
 *									*
 * The following should be noted if changing these tokens.		*
 *									*
 *  (1) FIRST_RESERVED_WORD_TOK must be the first reserved word listed, *
 *      and LAST_RESERVED_WORD_TOK must be the last reserved word	*
 *	listed.  They are defined in tokmod.h.				*
 *									*
 *  (2) LINE_TOK should be listed just after the reserved words, since	*
 *      it has an entry in token_kinds.					*
 *									*
 *  (3) If a reserved word is added or deleted, or the order is		*
 *      changed, then							*
 *        (a) update tables in lexsup.c.				*
 *									*
 *        (b) update messages/syntax.txt.				*
 *									*
 ************************************************************************/

%token <ls_at>	ABBREV_TOK		/* RW Abbrev			*/
%token <ls_at>  ABSTRACTION_TOK		/* RW Abstraction		*/
%token <ls_at>	ADVISORY_TOK		/* RW Advise			*/
%token  	AND_TOK		        /* RW _and_			*/
%token <ls_at>	ASSUME_TOK		/* RW Assume			*/
%token <ls_at>  ASK_TOK			/* RW Ask			*/
%token <ls_at>  ATOMIC_TOK		/* One of the following reserved*
					 * words.  The attr field of 	*
					 * lval tells which one.	*
					 *   Atomic 	(ATOMIC_ATT)	*
					 *   CutHere	(CUTHERE_ATT)	*
					 *   Unique     (FIRST_ATT)     */
%token <ls_at>	AWAIT_TOK		/* RW Await			*/
%token		BODY_TOK		/* RW implementation		*/
%token <ls_at>	BRING_TOK		/* RW Bring			*/
%token          BUT_IF_FAIL_TOK		/* RW _butIfFail_		*/
%token		BY_TOK			/* RW by			*/
%token <ls_at>	CASE_TOK		/* One of the following reserved*
					 * words.  The attr field of	*
					 * lval tells which one.	*
					 *   case 	(CASE_ATT)	*
					 *   exitcase   (UNTIL_ATT)	*/
%token <int_at>	CATCHING_TOK		/* One of the fllowing reserved	*
					 * words.  The attr field of 	*
					 * lval tells which one.	*
					 *   catchingEachThread (2)	*
					 *   catchingTermination (1)	*/
%token <ls_at>	CHOOSE_TOK		/* RW Choose			*/
%token <ls_at>  CLASS_TOK		/* RW Class			*/
%token <ls_at>  CONST_TOK		/* RW Const			*/
%token <ls_at>	CONSTRAINT_TOK		/* RW constraint.  Attribute 	*
					 * gives line number.		*/
%token 		CONSTRUCTOR_TOK		/* RW constructor		*/
%token <ls_at>	CONTEXT_TOK		/* RW Context			*/
%token <ls_at>	CONTINUE_TOK		/* RW continue			*
					 * lval.line holds the line.	*/
%token <ls_at>	DEFAULT_TOK		/* RW Default			*/
%token <ls_at>	DESCRIPTION_TOK		/* RW Description or description*
					 *  attr is 			*
					 *     LC_ATT for description, 	*
					 *     UC_ATT for Description	*/
%token <ls_at>  DIRECTORY_TOK		/* RW Directory			*/

%token <ls_at>	DO_TOK			/* RW do			*
					 * lval.line holds the line.	*/
%token 		ELSE_TOK		/* RW else			*/
%token <ls_at>	EXCEPTION_TOK		/* RW Exception			*/
%token <ls_at>  EXECUTE_TOK		/* RW Execute			*/
%token <ls_at>	EXPECT_TOK		/* RW Expect			*/
%token 		EXPORT_TOK		/* RW export			*/
%token <ls_at>	EXTENSION_TOK		/* RW Extension			*/
%token <ls_at>	EXTRACT_TOK		/* RW Extract			*/
%token <ls_at>	LCEXTRACT_TOK		/* RW extract			*/
%token <ls_at>	FOR_TOK			/* RW For			*/
%token		FROM_TOK		/* RW from			*/
%token 		HOLDING_TOK		/* RW holding			*/
%token <ls_at>	IF_TOK			/* RW If			*/
%token 		IMPLEMENTS_TOK		/* RW implements		*/
%token		IMPLIES_TOK		/* RW _implies_			*/
%token <ls_at>	IMPORT_TOK		/* RW Import			*/
%token 		INTERFACE_TOK		/* RW interface			*/
%token          ISA_TOK			/* RW isKindOf			*/
%token <ls_at>	LET_TOK			/* RW Let			*/
%token <ls_at>	LOOP_TOK		/* RW Loop			*/
%token <ls_at>	LOOPCASE_TOK		/* RW loopcase 			*
					 * lval.line is the line.	*/
%token <ls_at>	MATCH_TOK		/* RW Match 		 	*/
%token		MATCHING_TOK		/* RW matching			*/
%token <ls_at>	MISSING_TOK		/* RW Missing			*/
%token <ls_at>	MIXED_TOK		/* RW mixed			*
					 * lval.line holds the line.	*/
%token <ls_at>  OPEN_TOK		/* RW open			*
					 * lval.line holds the line.	*/
%token <ls_at>	OPERATOR_TOK		/* RW Operator			*/
%token  	OR_TOK			/* RW _or_			*/
%token <ls_at>  PACKAGE_TOK		/* RW Package		 	*/
%token <ls_at>	PATTERN_TOK		/* RW Pattern or Expand		*/
%token <ls_at>	LCPATTERN_TOK		/* RW pattern			*
					 * lval.line holds the line.	*/
%token <ls_at>  RELATE_TOK		/* RW Relate			*/
%token <ls_at>	SPECIES_TOK		/* RW Species or species	*
					 *  attr is 			*
					 *     LC_ATT for species, 	*
					 *     UC_ATT for Species	*/
%token <ls_at>  SPECIES_AS_VAL_TOK	/* RW speciesAsValue		*/
%token <ls_at>  STREAM_TOK		/* RW Stream or Mix		*/
%token <ls_at>	SUBCASES_TOK		/* RW Subcases			*/
%token <ls_at>	TARGET_TOK		/* RW target			*
					 * lval.line holds the line.	*/
%token <ls_at>	TEAM_TOK		/* RW Team			*/
%token 		THEN_TOK		/* RW then			*/
%token <ls_at>	TRAP_TOK		/* One of the following reserved*
					 * words.  The attr field of	*
					 * lval tells which one.	*
					 *   Trap 	(TRAP_ATT)	*
					 *   Untrap	(UNTRAP_ATT)	*/
%token <ls_at>	LCTRAP_TOK		/* RW trap			*/
%token <ls_at>  VAR_TOK			/* RW Var			*/
%token <int_at>	WHICH_TOK		/* One of the following reserved*
					 * words.  The lval tells which *
					 * one.				*
					 *   all 	(ALL_ATT)	*
					 *   first 	(FIRST_ATT)	*
					 *   one 	(ONE_ATT) 	*
					 *   perhaps 	(PERHAPS_ATT)	*/
%token		WHERE_TOK		/* RW _where_			*/
%token		WHEN_TOK		/* RW when			*/
%token 		WITH_TOK		/* RW with			*/
%token <ls_at>	WHILE_TOK		/* RW While 			*/

%token <ls_at>	END_TOK			/* %X, for some X		*
					 * lval.name is the lexeme,	*
					 * lval.line is the line, and	*
					 * lval.column is the column.	*/


/********************** SPECIAL SYMBOLS *********************************
 *									*
 * Parentheses have an ls_at that gives the line and column number	*
 * where the symbol occurs.  						*
 *									*
 * WARNINGS:								*
 *									*
 *  (1) LINE_TOK must be the first symbol, since the lexer assumes that *
 *      it comes immediately after LAST_RESERVED_WORD_TOK.		*
 *									*
 *  (2) The parentheses and brackets should be contiguous, and each     *
 *      left parenthesis should be just before the matching right	*
 *      parenthesis.					  		*
 *									*
 *  (3) tokmod.h assumes that LPAREN_TOK is the first special		*
 *      symbol, and PERIOD_TOK is the last special symbol.		*
 ************************************************************************/

%token 		LINE_TOK		/* symbol ==...=		*/

%token <ls_at>	LPAREN_TOK		/* symbol ( 			*/
%token 		RPAREN_TOK		/* symbol )			*/

%token <ls_at>  LBRACE_TOK		/* symbol {			*/
%token 		RBRACE_TOK		/* symbol }			*/

%token <ls_at>	LBRACK_TOK		/* symbol [ 			*/
%token 		RBRACK_TOK		/* symbol ]			*/

%token <ls_at>  LWRAP_TOK		/* symbol << or ?<<		*/
					/* attr is			*/
					/*    0  for <<			*/
					/*    1  for ?<<		*/

%token <ls_at>	RWRAP_TOK		/* symbol >> or >>?		*/
					/* attr is			*/
					/*    0  for >>			*/
					/*    1  for >>?		*/

%token <ls_at>	LBRACK_COLON_TOK	/* symbol [:			*/
%token		RBRACK_COLON_TOK	/* symbol :]			*/

%token <ls_at>  LROLE_SELECT_TOK	/* symbol {:			*/
%token 		RROLE_SELECT_TOK	/* symbol :}			*/
%token <ls_at>	LROLE_MODIFY_TOK	/* symbol /{:			*
					 * This is a special case: it   *
					 * comes after its mattching    *
					 * right parenthesis.		*/

%token <ls_at>  LLAZY_TOK		/* symbol (:			*/
%token 		RLAZY_TOK		/* symbol :)			*/

%token <ls_at>  LPAREN_DBLBAR_TOK	/* symbol (||			*/
%token 		RPAREN_DBLBAR_TOK	/* symbol ||)			*/

%token<ls_at>	LBOX_TOK		/* symbols			*
					 * <:	(attr = 0)		*
					 * <:#	(attr = SHARED_ATT)	*
					 * <:%	(attr = NONSHARED_ATT)	*/

%token<ls_at>	RBOX_TOK		/* symbols			*
					 * :>	(attr = 0)		*
					 * #:>	(attr = SHARED_ATT)	*
					 * %:>	(attr = NONSHARED_ATT)	*/

%token 		COMMA_TOK		/* symbol ,			*/
%token 		COLON_TOK		/* symbol :			*/
%token 		SEMICOLON_TOK		/* symbol ;			*/
%token <ls_at>	ROLE_MARK_TOK		/* symbol =>>			*/
%token 		SGL_ARROW_TOK		/* symbol ->			*/
%token 		DBL_ARROW_TOK		/* symbol =>			*/
%token 		AMPERSAND_TOK		/* symbol &			*/
%token 		EQUAL_TOK		/* symbol =			*/

%token <int_at>	PLUS_EQUAL_TOK		/* One of the following symbols *
					 * lval tells which one.	*
					 *  <--  (ARROW1_ATT)      	*
					 *  <--? (ARROW2_ATT)		*/

%token		COLON_EQUAL_TOK		/* symbol :=			*/
%token 		BAR_TOK			/* symbol |			*/
%token <ls_at>	QUESTION_MARK_TOK	/* symbol ?			*/
%token <ls_at> 	PERIOD_TOK		/* symbol .			*/


/*************************** CONSTANTS **********************************
 * Each constant has a string attribute (lval.name) describing its	*
 * value as well as a line number (lval.line).				*
 ************************************************************************/

%token <ls_at> CHAR_CONST_TOK		/* a character constant 	*/
%token <ls_at> NAT_CONST_TOK		/* a natural number constant	*/
%token <ls_at> REAL_CONST_TOK		/* a real constant 		*/
%token <ls_at> STRING_CONST_TOK		/* a string constant		*/

/************************** IDENTIFIERS ********************************/

%token <ls_at> PAT_VAR_TOK		/* a pattern variable.  The	*
					 * attribute gives line number  *
					 * and name, after deleting the *
					 * leading '?'.			*/

%token <ls_at> PROC_ID_TOK		/* a procedure identifier.  The *
					 * attribute gives line number	*
					 * and name.			*/

/*--------------------------------------------------------------*
 * Do not reorder the following. The order is used in lexids.c. *
 *--------------------------------------------------------------*/

%token <ls_at>	UNKNOWN_ID_TOK		/* an unbound identifier	*
					 * The attribute gives the 	*
					 * line number and name.	*/

%token <ctc_at> COMM_ID_TOK		/* a community identifier.  The *
					 * attribute points to the class*
					 * table cell for the community.*/

%token <ls_at>  FAM_ID_TOK		/* A family identifier		    *
					 *				    *
					 * lval.name = the identifier.      *
					 * lval.line = line number          */

%token <ls_at>  TYPE_ID_TOK		/* A type identifier		    *
					 *				    *
					 * lval.name = the identifier.	    *
					 * lval.line = line number          */

%token <ls_at>  FAM_VAR_TOK		/* a family variable.  lval.name is *
					 * the name, in the form `x#G, if   *
					 * the variable looks like G`x.     *
					 * The beginning of the name tells  *
					 * what kind of variable it is.  For*
					 * example, REAL``~aa has name      *
					 * ``~aa#REAL.			    */

%token <ls_at>  TYPE_VAR_TOK		/* a species variable.  Similar to  *
					 * FAM_VAR_TOK.			    */

%token <ctc_at> GENUS_ID_TOK		/* a genus identifier.  The 	*
					 * attribute points to the class*
					 * table cell for this id.	*/

%token <ntype_at>TYPE_ABBREV_TOK	/* abbreviation of a type expr  */
%token <ntype_at>FAM_ABBREV_TOK		/* abbreviation of a family     */
%token <ntype_at>FAM_MACRO_TOK		/* abbrev F(A) = B              */
%token <ntype_at>FAM_VAR_ABBREV_TOK	/* abbreviation of a fam var	*/

%token <ctc_at>	GENUS_ABBREV_TOK	/* abbreviation of a genus	*/
%token <ctc_at>	COMM_ABBREV_TOK 	/* abbreviation of a community  */

/*----------------*
 * Error Recovery *
 *----------------*/

%token RECOVER

/****************************************************************
 * 		OPERATORS AND PRECEDENCE			*
 ****************************************************************
 * TOKEN PRECEDENCE (low to high)				*
 * ------------------------------				*
 ****************************************************************/

/*-------------*
 * Error token *
 *-------------*/

%nonassoc	error

/*------------------------*
 * Low precedence symbols *
 *------------------------*/

%nonassoc	EQUAL_TOK
%nonassoc	BY_TOK
%right		COLON_EQUAL_TOK COLON_EQUAL_OP

/*---------------------*
 * Role marks and tags *
 *---------------------*/

%left		COLON_TOK
%right		ROLE_MARK_TOK

/*-----------------*
 * |, _butIfFail_   *
 *-----------------*/

%left		BUT_IF_FAIL_TOK
%left		BAR_TOK BAR_OP

/*-----------------*
 * Basic operators *
 *-----------------*/

%right		IMPLIES_TOK R1_OP
%left 		WHERE_TOK L1_OP

%right		OR_TOK R2_OP
%left		L2_OP

%right		AND_TOK R3_OP
%left		L3_OP

%nonassoc 	COMPARE_OP

%right 		R4_OP
%left		L4_OP

%right		R5_OP
%left		L5_OP LROLE_MODIFY_TOK

%right		R6_OP
%left 		L6_OP

%right		R7_OP
%left 		L7_OP

%right 		R8_OP
%left		L8_OP

%right		R9_OP
%left		L9_OP

/*-----------------*
 * Unary operators *
 *-----------------*/

%right          UNARY_OP


/*-------------------------------------------*
 * Precedence for handling pattern functions *
 *-------------------------------------------*/

%nonassoc	IDENTIFIER

/*---------------------------------------*
 * Symbols that begin entity expressions *
 *---------------------------------------*/

%nonassoc ASSUME_TOK AWAIT_TOK ASK_TOK ATOMIC_TOK BRING_TOK CHAR_CONST_TOK CHOOSE_TOK EXCEPTION_TOK EXTRACT_TOK FOR_TOK IF_TOK TARGET_TOK LPAREN_TOK LBOX_TOK LBRACE_TOK LBRACK_TOK LBRACK_COLON_TOK LCEXTRACT_TOK LWRAP_TOK LLAZY_TOK LROLE_SELECT_TOK LPAREN_DBLBAR_TOK  LET_TOK LOOP_TOK MATCH_TOK MISSING_TOK MIXED_TOK NAT_CONST_TOK OPEN_TOK PAT_VAR_TOK PROC_ID_TOK QUESTION_MARK_TOK REAL_CONST_TOK CONTEXT_TOK SPECIES_AS_VAL_TOK STREAM_TOK STRING_CONST_TOK TEAM_TOK TYPE_VAR_TOK TYPE_ID_TOK COMM_ID_TOK GENUS_ID_TOK TYPE_ABBREV_TOK FAM_ABBREV_TOK FAM_VAR_ABBREV_TOK GENUS_ABBREV_TOK COMM_ABBREV_TOK VAR_TOK TRAP_TOK UNKNOWN_ID_TOK WHILE_TOK

/*------------------------------------*
 * Precedence for species expressions *
 *------------------------------------*/

%left		CONSTRAINT_TOK
%right	 	SGL_ARROW_TOK
%right	 	FAM_ID_TOK FAM_MACRO_TOK FAM_VAR_TOK

/*-----------------------------*
 * Juxtaposition (application) *
 *-----------------------------*/

%left 		JUXTAPOSITION

/*---------------*
 * LCPATTERN_TOK *
 *---------------*/

%right 		LCPATTERN_TOK

/*-----------------------------------------------------------------------*
 * LINE_TOK needs precedence to override initGenerate in packageMainPart *
 * productions.								 *
 *-----------------------------------------------------------------------*/

%left 		LINE_TOK

/****************************************************************/
/*			NONTERMINALS: TYPES			*/
/****************************************************************/

%type <int_at>		abbrevDclBody
%type <str_at>		abbrevId
%type <expr_at>		advisoryExpr
%type <expr_at>		advisory
%type <ls_at>		allButProcId
%type <expr_at>		angleContents  
%type <expr_at>		angleList
%type <expr_at>		askExpr
%type <ls_at>		assumeDcl
%type <int_at>		assumeDclBody
%type <expr_at>		awaitExpr
%type <type_at>		basicFamId
%type <type_at>		basicFamVar
%type <ls_at>		binOp
%type <list_at>		bringBody
%type <expr_at>		bringExpr
%type <expr_at>		caseBody	
%type <expr_at>		cases 	
%type <ctc_at>		cgId
%type <list_at>		cgIdList
%type <expr_at>		chooseExpr 	
%type <str_at>		classBad	
%type <list_at>		classParts
%type <str_at>		classId 
%type <expr_at>		clwhich
%type <list_at>		classUnion	
%type <cuc_at>		classUnionElement
%type <ctc_at>		communityId
%type <type_at>		constraint
%type <list_at>		constraintList
%type <expr_at>		contextExpr
%type <expr_at>		continue 	
%type <int_at>		defaultDclBody
%type <expr_at>		defLetBody
%type <expr_at>		defLetCases
%type <int_at>		defletEqual
%type <expr_at>		descriptionEntry
%type <expr_at>		descriptionExpr
%type <int_at>		directoryDclBody
%type <ls_at>		end
%type <int_at>		exceptionDclBody
%type <int_at>		expectDclBody
%type <expr_at>		expr		
%type <list_at>		exprList	
%type <expr_at>		exprOrTaggedProcId
%type <type_at>		fam
%type <type_at>		famId
%type <type_at>		famVar
%type <expr_at>		forExpr
%type <list_at>		forBodyExprs
%type <expr_at>		functionExpr 	
%type <ctc_at>		genusId
%type <int_at>		getDclContext
%type <int_at>		getLine
%type <cuc_at>		guard
%type <cuc_at>		guards
%type <str_at>		id		
%type <list_at> 	idList	
%type <expr_at>		ifExpr
%type <list_at>		iterators
%type <expr_at>		letExpr
%type <expr_at>		listExpr 
%type <expr_at>		loopExpr 	
%type <expr_at>		matchExpr
%type <list_at>		meetDclRHS
%type <int_at>		opDclBody
%type <expr_at>		openExpr
%type <ls_at>		oper
%type <int_at>		optAllOne
%type <int_at>		optAmpersand
%type <str_at>		optClassId
%type <str_at>		optClassInherit
%type <int_at>		optColon
%type <expr_at>		optDefLetThen
%type <expr_at>		optElseExpr
%type <expr_at>		optExpr
%type <list_at>		optExprList
%type <int_at>		optExtension
%type <expr_at>		optFrom
%type <rtype_at>	optHolding
%type <list_at>		optInherits
%type <expr_at>		optInit
%type <int_at>		optOpen
%type <ls_at>		optStringConst
%type <int_at>		optSucceed
%type <rtype_at>	optTag		
%type <expr_at>		optThenExpr
%type <int_at>		optTrap
%type <rtype_at>	optTypeExpr 	
%type <expr_at>		optWhen
%type <list_at>		optWiths
%type <expr_at>		patternDclBody
%type <str_at>		PUClassId
%type <str_at>		PUEntId
%type <list_at>		PUEntIdList
%type <str_at>		precedence
%type <expr_at>		realsubcases
%type <int_at>		recover
%type <rtype_at>	restrTypeExpr
%type <rtlist_pair_at>	restrTypeExprs
%type <list_at>		roleSelectBody
%type <rtype_at>	simpleTypeExpr 	
%type <expr_at>		singleExpr
%type <mode_at>		someVarOpts
%type <expr_at>		streamExpr
%type <expr_at>		streamParts
%type <expr_at>		subcases
%type <expr_at>		taggedEntOrProcId	
%type <expr_at>		taggedId
%type <expr_at>		taggedUnaryOp
%type <expr_at>		taggedProcId
%type <expr_at>		teamExpr
%type <expr_at>		teamExprParts
%type <expr_at>		teamLetExpr
%type <expr_at>		teamPart
%type <expr_at>		teamParts
%type <expr_at>		topDefLetCases
%type <expr_at>		trapExpr
%type <int_at>		tryMode
%type <int_at>		typeDclBody
%type <rtype_at>	typeExpr 	
%type <rtlist_pair_at>	typeExprList   
%type <rtlist_pair_at>	typeExprs
%type <str_at>		unknownId
%type <expr_at>		varBody
%type <expr_at>		varExpr
%type <list_at>		varList
%type <list_at>		varListMem
%type <mode_at>		varOpt
%type <mode_at>		varOptList
%type <mode_at>		varOpts
%type <int_at>		which
%type <expr_at>		whileCase
%type <cuc_at>		whileGuard
%type <cuc_at>		whileGuards
%type <list_at>		withList

%%
/****************************************************************
 *			IMPORTANT NOTES				*
 ****************************************************************
 *								*
 * Some nonterminals return, as their attribute, a pointer to	*
 * a structure whose storage is managed by reference counts.	*
 * All such pointers have their references included in the	*
 * reference count.  When the reference is created, the count	*
 * must be bumped up.  When the nonterminal is lost, the count	*
 * must be dropped.  In some cases, one nonterminal just	*
 * returns, as its attribute, the attribute of another		*
 * nonterminal.  In that case, nonthing needs to be done to	*
 * the reference count.						*
 ****************************************************************/


/*****************************************************************
 *			 PACKAGES				 *
 *****************************************************************
 * package is the start nonterminal.				 *
 *								 *
 * Contexts:							 *
 *								 *
 *  interface_package	true when reading an interface package,  *
 *  			false otherwise.			 *
 *****************************************************************/

package		: /*----------------------------------------------------*
		   * The lexer will prepend the standard package to the *
		   * input.  Here, we parse the standard package.	*
		   *----------------------------------------------------*/

		  PACKAGE_TOK id INTERFACE_TOK dcls BODY_TOK 

		  /*------------------------------------------------*
		   * Now produce the primitive functions that need  *
		   * to override those in the standard package, and *
		   * set up for the package being read.		    *
		   *------------------------------------------------*/

			{ov_std_funs();
			 main_context = EXPORT_CX;
			 gen_code     = main_gen_code;
			 interface_package = FALSE;
#			 ifdef DEBUG
			   set_traces(trace_arg);
#			 endif
			 show_all_reports = set_show_all_reports;
			}

		  /*----------------------------------------*
		   * Next comes the package being compiled. *
		   *----------------------------------------*/

		  PACKAGE_TOK id optInherits

		  /*----------------------------------------------------*
		   * Install the package name.  Note that 		*
		   * main_package_name will be changed to the interface	*
		   * package name below if it turns out that there 	*
		   * is a separate interface package.			*
		   *							*
		   * Also install primitives that should only be	*
		   * installed for packages other than the standard	*
		   * package.						*
		   *----------------------------------------------------*/

			{have_read_main_package_name = TRUE;
			 main_package_name = install_package_name($8, $9);
			 drop_list($9);
			 special_std_funs();
			}

		  /*--------------------------------------------*
		   * Read the declarations in the main package. *
		   *--------------------------------------------*/

		  packageMainPart end

		  /*----------------------------------------------------*
		   * At the end of the package, check that expectations *
		   * have been met, that definitions do not conflict,   *
		   * and generate the end of the package in the 	*
		   * generated code.					*
		   *----------------------------------------------------*/

			{check_end_p(&($1), PACKAGE_TOK, &($12));
			 if(!error_occurred) {
			   overlap_all_parts_tm();
			   if(!interface_package) {
			     check_expects();
			     check_tf_expects();
			   }
			 }
			 end_package_g();
		        }

		| error
			{clean_up_and_exit(1);}
		;

/*--------------------------------------------------------------------*
 * Each production for packageMainPart begins with optLines, to allow *
 * ==== lines to come before the word export or interface, etc.       *
 *--------------------------------------------------------------------*/

packageMainPart : /*------------------------------------------------*
		   * Case of a simple package, with no export part. *
		   *------------------------------------------------*/

		  optLines initGenerate setBody dcls
			{}

		| /*---------------------------------------------------*
		   * Case of a package with an export part and a body. *
		   *---------------------------------------------------*/
		  
		  optLines initGenerate export body
			{}

		| /*-------------------------------*
		   * Case of an interface package. *
		   *-------------------------------*/

		  optLines INTERFACE_TOK
			{gen_code = main_gen_code = FALSE;
			 interface_package = TRUE;
			 begin_export();
		        }
		  dcls
			{close_index_files((Boolean) !error_occurred);}

		| /*------------------------------------*
		   * Case of an implementation package. *
		   *------------------------------------*/

		  optLines 

			{about_to_load_interface_package = TRUE;}

		  IMPLEMENTS_TOK STRING_CONST_TOK 

		  /*-------------------------------------------------*
		   * At this point, the lexer inserts the content of *
		   * the interface package.  Parse it.  We must	     *
		   * also change main_package_name to the name of    *
		   * the interface package, and set		     *
		   * main_package_imp_name to the name of the	     *
		   * implementation package.			     *
		   *-------------------------------------------------*/

		  PACKAGE_TOK id optInherits
			{about_to_load_interface_package = FALSE;
			 main_package_imp_name = main_package_name;
			 main_package_name = install_package_name($6,$7);
			 file_info_st->is_interface_package = TRUE;
			 main_context = EXPORT_CX;
			 gen_code     = main_gen_code;
			 seen_export = TRUE;
			}
		  initGenerate optLines INTERFACE_TOK 
			{begin_export();}
		  dcls 

		  /*----------------------------------------------------*
		   * When the lexer sees the end of the export part 	*
		   * of the imported package, it will provide token	*
		   * BODY_TOK.						*
		   *----------------------------------------------------*/

		  BODY_TOK

		  /*-----------------------------------------------------*
		   * Now we are back reading the implementation package. *
		   *-----------------------------------------------------*/

		  setBody dcls
			{}
		;

export		: EXPORT_TOK 
			{begin_export();}
		  dcls	
			{}
		;

body		: /* empty */	
			{close_index_files((Boolean) !error_occurred);}

		| BODY_TOK setBody dcls
			{}
		;

initGenerate	: /* empty */	%prec JUXTAPOSITION /* lower than LINE_TOK */ 
			{init_generate_g(main_file_name);}
		;


setBody		: /* empty */	
			{/*----------------------------------------*
			  * Start reading the implementation part. *
			  *----------------------------------------*/

			 write_g(BEGIN_IMPLEMENTATION_DCL_I);
  			 outer_context = BODY_CX;
 			 main_context = (UBYTE) outer_context;
			 gen_code = main_gen_code;
			 close_index_files((Boolean) !error_occurred);
		        }
		;

optInherits	: /* empty */
			{$$ = NIL;}
		
		| optInherits ISA_TOK idList
			{bump_list($$ = append($1, $3));}
		;


/************************************************************************
 *			DECLARATIONS                                	*
 ************************************************************************
 * Contexts:								*
 *									*
 *  dcl_context is false inside expressions, and true at the outer	*
 *		part of a declaration.  This is used to distinguish	*
 *		let/define/var	expressions from let/define/var	*
 *		declarations.						*
 *									*
 * Nonterminals:							*
 *									*
 *  dcls	A sequence of declarations				*
 *  dcl		A single declaration					*
 ************************************************************************/

dcls		: /* empty */			{}
		| dcls startDcl dcl endDcl	{}
		| dcls LINE_TOK			{}
		;

dcl		: executeDcl	{}
		| extensionDcl	{}
		| defaultDcl	{}
		| exceptionDcl	{}
		| advisoryDcl   {}
		| contextDcl	{}
		| letDcl	{}
		| bringDcl	{}
		| varDcl	{}
		| teamDcl	{}
		| expectDcl	{}
		| missingDcl	{}
		| abbrevDcl	{}
		| assumeDcl	{}
		| descriptionDcl {}
		| operatorDcl	{}
		| patternDcl	{}
		| directoryDcl  {}
		| importDcl	{}
		| ifDcl		{}

		/*---------------------------------------------------*
		 * A declaration that must occur inside an extension *
		 * declaration should produce an error if it occurs  *
		 * outside an extension.			     *
		 *---------------------------------------------------*/

		| trueExtension	
			{semantic_error(BARE_EXTENSION_ERR, 0);}

		/*----------------------------------------------*
		 * Catch parse errors at the declaration level.	*
		 * dcl_panic_l will try to find the next	*
		 * declaration.					*
		 *----------------------------------------------*/

		| error		
			{delay_reports = 0;
			 debug_at_dcl_panic();
			 if(dcl_panic_l()) {
			   yyclearin;
			   yyerrok;
			 }
			}
		  recover
			{debug_at_dcl_recover();}
		;

startDcl	: /* empty */
			{start_dcl(1);}
		;

endDcl		: /* empty */
			{end_dcl();}
		;


/****************************************************************
 *			 DECLARATION MODES			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  this_mode		holds the mode that was read.  This is  *
 *			where other declarations are supposed	*
 * 			to look for the mode.			*
 *								*
 *  propagate_mode	holds the mode that is to be propagated *
 *			across a semicolon.			*
 *								*
 *  defopt_mode		the mode output from defOpts		*
 *								*
 * Nonterminals:						*
 *								*
 *  dclMode		optional {mode}, where mode is a	*
 *			list of modes for a declaration.	*
 *			The mode is put into this_mode.		*
 *								*
 *  defOpts		the contents of {...}.			*
 *								*
 *  defOpt		one option in {...}.			*
 ****************************************************************/

dclMode		: /* empty */				%prec BAR_TOK
		        {if(recent_begin_from_semicolon) {
			   do_copy_mode(&this_mode, &propagate_mode);
			 }
			 else {
			   do_copy_mode(&this_mode, &null_mode);
			   do_copy_mode(&propagate_mode, &null_mode);
			 }
			 dcl_context = FALSE;
		        }	

		| dclMode LBRACE_TOK initDefOpt defOpts 
		  RBRACE_TOK 
			{/*---------------------------------------------*
			  * transfer the mode information read by	*
			  * defOpts to this_mode now.  This is 		*
			  * necessary since we want this mode in	*
			  * place before the lookahead after RBRACE_TOK	*
			  * is read.  (ImportDcl needs this.)		*
			  *---------------------------------------------*/

			  modify_mode(&this_mode, &defopt_mode, FALSE);
			  opContext = 0; /* For token after optColon */
			}
		  optColon
			{/*-------------------------------------------*
			  * If there is no colon, then this_mode is   *
			  * already set up correctly.  If there is a  *
			  * colon, then we need to set it up again.   *
			  *-------------------------------------------*/

			 if($7) {
			   /*-----------------------------------------*
			    * Clear the incoming mode, transfer modes *
			    * to this empty mode, and then propagate  *
			    * this new mode.			      *
			    *-----------------------------------------*/

			   do_copy_mode(&this_mode, &defopt_mode);
			   do_copy_mode(&propagate_mode, &this_mode);
			 }
		        }
  		;

initDefOpt	: /* empty */			%prec JUXTAPOSITION
			{SET_LIST(defopt_mode.noexpects, NIL);
			 SET_LIST(defopt_mode.visibleIn, NIL);
			 defopt_mode = null_mode;
			}
		;

optColon	: /* empty */
			{$$ = 0;}

		| COLON_TOK
			{$$ = 1;}
		;

defOpts		: /* empty */

		| someDefOpts
		;

someDefOpts	: defOpt

		| defOpts COMMA_TOK defOpt
		;

defOpt		: ASSUME_TOK
			{check_case($1, LC_ATT);
			 pop_shadow();
			 add_mode(&defopt_mode, ASSUME_MODE);
			}

		| DEFAULT_TOK
			{check_case($1, LC_ATT);
			 pop_shadow();
			 add_mode(&defopt_mode, DEFAULT_MODE);
			}

		| LCPATTERN_TOK
			{add_mode(&defopt_mode, PATTERN_MODE);
			}

		| MISSING_TOK
			{check_case($1, LC_ATT);
			 pop_shadow();
			 add_mode(&defopt_mode, MISSING_MODE);
			}

		| /*----------------------------------*
		   * Simple modes, such as 'override'. *
		   *----------------------------------*/

		  PUEntId 
			{handle_simple_mode(&defopt_mode, $1, TRUE);
			 if(strcmp($1, "protected") == 0) {
			   SET_LIST(defopt_mode.visibleIn,
				    str_cons(current_package_name, NIL));
			 }
			}

		| /*----------------*
		   * Modes 'tag x'. *
		   *----------------*/

		  PUEntId id
			{if(!pat_rule_opts_ok || strcmp($1, "tag") != 0) {
			   syntax_error1(BAD_MODE_ERR, $1, 0);
			 }
			 else if(defopt_mode.patrule_tag == NULL) {
			   defopt_mode.patrule_tag = id_tb0($2);
			 }
			 else if(strcmp(defopt_mode.patrule_tag,$2) != 0){
			   semantic_error(DUP_TAG_ERR, 0);
			 }
			}

		| /*---------------------------------------------*
		   * Modes noExpect{x,y,z} and protected{x,y,z}. *
		   *---------------------------------------------*/

		  PUEntId LBRACE_TOK idList RBRACE_TOK
			{if(strcmp($1, "noExpect") == 0) {
			   SET_LIST(defopt_mode.noexpects, 
				    append(defopt_mode.noexpects, $3));
			   add_mode(&defopt_mode, PARTIAL_NO_EXPECT_MODE);
			 }
			 else if(strcmp($1, "protected") == 0) {
			   SET_LIST(defopt_mode.visibleIn, 
				    append(defopt_mode.visibleIn, $3));
			   if(!str_memq(current_package_name, 
					defopt_mode.visibleIn)) {
			     SET_LIST(defopt_mode.visibleIn,
				      str_cons(current_package_name, 
					       defopt_mode.visibleIn));
			   }
			   add_mode(&defopt_mode, PROTECTED_MODE);
			 }
			 else {
			   syntax_error1(BAD_MODE_ERR, $1, 0);
			 }
			}

		| /*------------------*
		   * Mode 'from pkg'. *
		   *------------------*/

		  FROM_TOK id
			{if(defopt_mode.u.def_package_name == NULL) {
			   defopt_mode.u.def_package_name = $2;
			 }
			 else if(strcmp(defopt_mode.u.def_package_name, 
				        $2) != 0) {
			   semantic_error(DUPLICATE_MODE_PACKAGE_ERR, 0);
			 }
			}
		;


/****************************************************************
 *			 DIRECTORY DECLARATIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  directoryDcl	A directory declaration.		*
 *								*
 *  directoryDclBody	The content of a directory declaration. *
 ****************************************************************/

directoryDcl	: DIRECTORY_TOK directoryDclBody end
			{check_end_p(&($1), DIRECTORY_TOK, &($3));
			 if($2) check_empty_block_p(&($1));
			}
		;

directoryDclBody : /* empty */
			{$$ = 1;}

		 | /*-----------------------*
		    * Directory id = "dir". *
		    *-----------------------*/

		   id EQUAL_TOK STRING_CONST_TOK
			{if(do_dcl_p()) {

			   /*-----------------------------------*
			    * You cannot redefine identifiers 	*
			    * standard or none.			*
			    *-----------------------------------*/

			   if(strcmp($1, STANDARD_AST_NAME) == 0 ||
			     strcmp($1, "none") == 0) {
			     syntax_error1(CANNOT_DO_DIR_DEF_ERR, $1, 
					   0);
			   }

			   else {
			     def_import_dir($1, $3.name);
			   }
		         }
			 $$ = 0;
			}

		| /*---------------*
		   * Directory id. *
		   *---------------*/

		  id
			{if(do_dcl_p()) set_import_dir($1, TRUE);
			 $$ = 0;
			}

		| /*------------------*
		   * Directory "dir". *
		   *------------------*/

		  STRING_CONST_TOK
			{if(do_dcl_p()) set_import_dir($1.name, FALSE);
			 $$ = 0;
			}
		;


/****************************************************************
 *			 IMPORT DECLARATIONS			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  this_mode		The mode read by dclMode.		*
 *								*
 *  new_import_ids	When an import declaration has a	*
 *			from-phrase, as in			*
 *			   Import map,fold from "listfuns".	*
 *			the list of identifiers to be imported	*
 *			is stored in new_import_ids.  If that	*
 *			list is empty, then new_import_ids is	*
 * 			NIL.  If there is no 'from' phrase,	*
 *			then new_import_ids is set to 		*
 *			(LIST*) 1.				*
 *								*
 * Nonterminals:						*
 *								*
 *  importDcl		An import declaration.  The production  *
 *			for an import actually performs the	*
 *			import, reading the imported package.	*
 *								*
 *  interfaceOrExport	Either 'interface' or 'export', 	*
 *			possibly preceded by ==== lines.	*
 *								*
 *  optIdList		An optional list of comma-separated	*
 *			identifiers followed by the word 'from'.*
 *			Returns the list of identifiers, as	*
 *			strings.				*
 ****************************************************************/

importDcl	: /*----------------------------------------*
		   * Import[{mode}] [x,y,z from] "package". *
		   *----------------------------------------*/

		  IMPORT_TOK dclMode optIdList STRING_CONST_TOK 

		  /*--------------------------------------------*
		   * The lexer will insert the imported package *
		   * here.  Read it.				*
		   *--------------------------------------------*/

		  PACKAGE_TOK id optInherits

			/*----------------------------------------------*
			 * Set new_import_ids to the default value, 1.	*
			 * Check that the declaration mode is		*
			 * acceptable for an import, and install the	*
			 * package name in the file info table.		*
			 *						*
			 * Warning: The value of new_import_ids is	*
			 * read by the lexer when the lexer processes	*
			 * STRING_CONST_TOK above, in order to set	*
			 * up the import id list that is actually used	*
			 * during the import.  But the parser asks for	*
			 * STRING_CONST_TOK before optIdList is reduced,*
			 * in the case when optIdList is the empty	*
			 * string (because there is no 'from' phrase).  *
			 * So it important to keep new_import_ids set	*
			 * to 1, since we can't presume that optIdList	*
			 * will do the job for us.			*
			 *----------------------------------------------*/
			    
			{SET_LIST(new_import_ids, (LIST *) 1);

			 if(do_dcl_p()) {
			   check_modes(IMPORT_DCL_MODES, 0);
			   install_package_name($6, $7);
			 }
			}

		   /*-----------------------------------------------*
		    * Read the export part of the imported package. *
		    *-----------------------------------------------*/

		   interfaceOrExport dcls BODY_TOK 

			{start_dcl(0);}

		   /*-----------------------------------------------*
		    * Now read the %Import at the end of the import *
		    * declaration.  Generate the import declaration *
		    * in the generated code.			    *
		    *-----------------------------------------------*/

		   end
			{check_end_p(&($1), IMPORT_TOK, &($13));
			 if(do_dcl_p() && $4.name != NULL) {
			   if(gen_code) {
			     generate_import_dcl_g($4.name);
			   }
			 }
			}

		| /*--------------------------------------------------*
		   * If the package to be imported has already been   *
		   * read, the lexer will not return STRING_CONST_TOK *
		   * when it reads the name of the package, but	      *
 		   * instead will return RECOVER.  This production    *
 		   * handles that case.				      *
		   *--------------------------------------------------*/

		  IMPORT_TOK dclMode optIdList RECOVER end
			{check_end_p(&($1), IMPORT_TOK, &($5));
			 SET_LIST(new_import_ids, (LIST *) 1);
			}
		

	        | /*----------------------------------------*
	           * Case of an empty import -- do nothing. *
	           *----------------------------------------*/

		  IMPORT_TOK end
			{check_end_p(&($1), IMPORT_TOK, &($2));
			 check_empty_block_p(&($1));
			}
		;

interfaceOrExport : optLines EXPORT_TOK

		  | optLines INTERFACE_TOK

		  | error
			{syntax_error(NO_INTERFACE_ERR, 0);}
		  ;

optLines	: /* empty */

		| optLines LINE_TOK
		;

optIdList	: /* empty */
			{new_import_ids = (LIST *) 1;}

		| idList FROM_TOK
			{new_import_ids = $1; /* Inherits ref */}
		;


/****************************************************************
 *			 EXECUTE DECLARATIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  executeDcl	An execute declaration.  Note that		*
 *								*
 *		   Execute %Execute				*
 *								*
 *		must do nothing.				*
 ****************************************************************/

executeDcl	: EXECUTE_TOK 
			{dcl_context = FALSE;}
  		  optExpr end
			{EXPR *e;
			 check_end_p(&($1), EXECUTE_TOK, &($4));
			 if(do_dcl_p()) {
			   if(skip_sames($3) != hermit_expr) {
			     bump_expr(e = make_execute_p($3, $1.line));
			     issue_dcl_p(e, EXECUTE_E, 0);
			     drop_expr(e);
			   }
			 }
			 pop_local_assumes_and_finger_tm();
			 drop_expr($3);
			}
		;


/****************************************************************
 *		IF DECLARATIONS					*
 ****************************************************************
 * Contexts:							*
 *								*
 *  if_dcl_st	A stack whose top holds the value of the	*
 *		condition that an if-declaration is testing.	*
 *		Actually, it holds				*
 *		  1   if only the then-part should be compiled  *
 *		  2   if only the else-part should be compiled  *
 *		  0   if neither the then-part nor the		*
 *		      else-part should be compiled.		*
 *		A stack is used to account for nested		*
 *		if-declarations.  if_dcl_st is NULL when not	*
 *		inside an if-declaration.			*
 *								*
 *  if_dcl_then_st Tells whether a declaration occurs in the 	*
 *		   then-part of an if-declaration, or the	*
 *		   else-part of an if-declaration.  (Only	*
 *		   meaningful inside an if-declaration.)  The	*
 *		   top holds 1 in the then-part, and 2 in	*
 *		   the else-part.				*
 *								*
 * Note:							*
 *								*
 * do_dcl_p() examines if_dcl_st and if_dcl_then_st to decide	*
 * whether to compile a declaration.  Each declaration calls	*
 * do_dcl_p() to ask if it should do anything.			*
 *								*
 * Nonterminals:						*
 *								*
 *  ifDcl	An if-declaration, indicating conditional	* 
 *		compilation.					*
 *								*
 *  optThenDcls	The then-part of an if-declaration.		*
 *								*
 *  optElseDcls	The else-part of an if-declaration.		*
 ****************************************************************/

ifDcl		: IF_TOK expr 
			{EXPR *cond;
			 Boolean ok;
			 if(do_dcl_p()) {

			   /*-------------------------------------------*
			    * Get the condition being tested.  Simplify *
			    * it.  The condition should simplify to	*
			    * either true or false.			*
			    *-------------------------------------------*/

			   bump_expr(cond = same_e($2, $2->LINE_NUM));
			   bump_type(cond->ty = boolean_type);
			   process_ids(cond, 0);
			   pop_scope_to_tm(NIL);
			   ok = (Boolean) !local_error_occurred && 
					   infer_type_tc(cond);
			   if(ok) {
			     simplify_expr(&cond);
			     SET_EXPR(cond, skip_sames(cond));
			     if(EKIND(cond) != CONST_E) {
			       syntax_error(CANNOT_EVAL_COND_ERR, 0);
			       push_int(if_dcl_st, 0);
			     }
			     else {
			       if(cond->STR == NULL) push_int(if_dcl_st, 2);
			       else push_int(if_dcl_st, 1);
			     }
			   }

			   /*-------------------------------------------*
			    * If the condition is bad, we push 0, 	*
			    * indicating that neither the then-part	*
			    * nor the else-part should be compiled.	*
			    *-------------------------------------------*/

			   else {
			     push_int(if_dcl_st, 0);
			   }
			   drop_expr(cond);
			 }

			 /*---------------------------------------------*
			  * If this declaration is being skipped 	*
			  * (do_dcl_p() returns false) then push 0 	*
			  * so that both the then-part and the		*
			  * else-part will be skipped.			*
			  *---------------------------------------------*/

			 else {
			   push_int(if_dcl_st, 0);
			 }
			}

		  optThenDcls optElseDcls end
			{check_end_p(&($1), IF_TOK, &($6));
			 pop_local_assumes_and_finger_tm();
			 pop(&if_dcl_st);
		        }
		;

optThenDcls	: /* empty */

		| THEN_TOK 
			{push_int(if_dcl_then_st, 1);}
		  dcls
			{pop(&if_dcl_then_st);}
		;

optElseDcls	: /* empty */

		| ELSE_TOK 
			{push_int(if_dcl_then_st, 2);}
		  dcls
			{pop(&if_dcl_then_st);}
		;


/****************************************************************
 *		EXTENSION DECLARATIONS				*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  extensionDcl	A declaration				*
 *			   Extension				*
 *			     dcl				*
 *			     ...				*
 *			     dcl				*
 *			   %Extension				*
 *								*
 *  extensions		The list of declarations inside the	*
 *			extension-declaration.			*
 *								*
 *  extension		One of the declarations inside an	*
 *			extension-declaration.			*
 *								*
 *  trueExtension	One of the declarations that can only	*
 *			occur inside an extension-declaration.	*
 ****************************************************************/

extensionDcl	: EXTENSION_TOK 

		  /*---------------------------------------------*
		   * In an extension declaration, ... should be  *
		   * interpreted by the lexer as a single token. *
		   *---------------------------------------------*/

			{check_case($1, UC_ATT);
			 if($1.attr != UC_ATT) {
			   begin_word_l(EXTENSION_TOK);
			 }
			 inside_extension  = TRUE;
			 if(do_dcl_p()) {
			   if(gen_code) {
 			     write_g(BEGIN_EXTENSION_DCL_I);
			   }
			 }
			}

		  extensions end
			{check_end_p(&($1), EXTENSION_TOK, &($4));
			 if(do_dcl_p()) {

			   /*-------------------------------------------*
			    * Close out the extension.  We need to 	*
			    *					   	*
			    * 1. Do any meet-declarations that result 	*
			    *    from Meet{ahead} ... declarations.	*
			    *						*
			    * 2. Check the hierarchy for consistency.	*
			    *						*
			    * 3. Create the join table.			*
			    *						*
			    * 4. Do any declarations, such as let-dcls,	*
			    *    that were deferred to the end of 	*
			    *    the extension.				*
			    *-------------------------------------------*/
			     
			   process_ahead_meets_tm();
			   close_classes_p();
			   if(gen_code) {
 			     write_g(END_EXTENSION_DCL_I);
			   }
			   handle_deferred_dcls_p();
			 }
			 inside_extension  = FALSE;
			}
		;

extensions	: /* empty */

		| extensions startDcl extension endDcl

		| extensions LINE_TOK
		;

extension	: trueExtension	{}
		| expectTypeDcl	{}
		| defaultDcl	{}
		| advisoryDcl	{}
		| abbrevDcl	{}
		| missingDcl	{}
		| descriptionDcl {}
		| error		
			{
#			 ifdef DEBUG
			   if(trace) {
			     trace_t(451);
			     trace_t(455);
			   }
#			 endif
			 if(dcl_panic_l()) {
			   yyclearin;
			   yyerrok;
			 }
			}
		  recover
			{
#			 ifdef DEBUG
			   if(trace) {
			     trace_t(453);
			     trace_t(455);
			   }
#			 endif
			}
		;

trueExtension	: typeDcl	{}
		| cgDcl		{}
		| classDcl	{}
		| relateDcl	{}
		;


/****************************************************************
 *			SPECIES DECLARATIONS			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  union_context 	tells classUnion how to deal with 	*
 *			discriminators, and what to permit in	*
 *			a list.					*
 * 								*
 *  this_mode		the declaration mode, from dclMode.	*
 *								*
 * Nonterminals:						*
 *								*
 *  typeDcl	 Species ... %Species				*
 *								*
 *  typeDclBody	 contents of type dcl.				*
 *								*
 *  optBar	 Either | or nothing.				*
 ****************************************************************/

typeDcl		: SPECIES_TOK 
			{check_case($1, UC_ATT);
			 if($1.attr != UC_ATT) {
			   begin_word_l(SPECIES_TOK);
			 }
			 delay_reports++;
			}
		  typeDclBody end
			{check_end_p(&($1), SPECIES_TOK, &($4));
			 if($3) check_empty_block_p(&($1));
			 if(delay_reports > 0) delay_reports--;
			}
		;

typeDclBody 	: /* empty */
			{$$ = 1;}

		| /*--------------------------------------------*
		   * Species[{mode}] S [isKindOf C] = E.	*
		   * Species[{mode}] S(D) [isKindOf C] = E.	*
		   *--------------------------------------------*/

		  dclMode PUClassId optTypeExpr optInherits EQUAL_TOK

		  	/*---------------------------------------*
		  	 * It is important to expect the species *
			 * or family being declared here, to	 *
 			 * permit recursive definitions.	 *
			 *					 *
			 * We must also set union_context to	 *
			 * TYPE_DCL_CX to tell classUnion how to *
			 * read the rhs of the declaration.	 *
		  	 *---------------------------------------*/

			{union_context = TYPE_DCL_CX;
			 if(do_dcl_p() && inside_extension) {
			   check_modes(TYPE_DCL_MODES, 0);
			   expect_tf_p($2, $3.type, &this_mode, FALSE);
			 }
		        }

		  optBar classUnion optStringConst
			{if(do_dcl_p() && inside_extension) {
			   if(!local_error_occurred) {
			     declare_tf_p($2, $3, $8, &this_mode);

			     /*----------------------------*
			      * Install the relationships. *
 			      *----------------------------*/

			     if($4 != NIL) {
			       try_extends_tm($4, $2, $3.type, TRUE, 
					      &this_mode);
			     }

			     /*--------------------------*
			      * Install the description. *
			      *--------------------------*/

			     if($9.name != NULL) {
			       create_description_p($2, $9.name, NULL, 
						    &this_mode, 1,
						    current_line_number);
			     }
			   }
			 }
			 drop_rtype($3);
			 drop_list($8);
			 $$ = 0;
		        }
		;

optBar		: /* empty */	{}
		| BAR_TOK	{}
		;


/****************************************************************
 *			ABSTRACTION DECLARATIONS		*
 ****************************************************************
 * Contexts:							*
 *								*
 *  this_mode		the declaration mode, from dclMode.	*
 *								*
 * Nonterminals:						*
 *								*
 *  cgDcl	 Abstraction A descr.				*
 *		 Abstraction C(A) descr.			*
 *								*
 ****************************************************************/

cgDcl		: ABSTRACTION_TOK dclMode unknownId 
		  optTypeExpr optInherits optStringConst end
			{check_end_p(&($1), ABSTRACTION_TOK, &($7));
			 check_modes(CG_DCL_MODES, 0);
			 if(do_dcl_p() && inside_extension) {
			   int tok = ($4.type == NULL)
				       ? GENUS_ID_TOK
				       : COMM_ID_TOK;
			   declare_cg_p($3, tok, 2, &this_mode);

			   /*----------------------------*
			    * Install the relationships. *
 			    *----------------------------*/

			   if($5 != NIL) {
			     try_extends_tm($5, $3, $4.type, TRUE, &this_mode);
			   }

			   /*--------------------------*
			    * Install the description. *
			    *--------------------------*/

			   if($6.name != NULL) {
			     create_description_p($3, $6.name, NULL, 
						  &this_mode, 1,
						  current_line_number);
			   }
			 }
			}

		| ABSTRACTION_TOK end
			{check_end_p(&($1), ABSTRACTION_TOK, &($2));
			 check_empty_block_p(&($1));
			}
		;


/************************************************************************
 *			CLASS UNION					*
 ************************************************************************
 * Contexts:								*
 *									*
 *  union_context	TYPE_DCL_CX or RELATE_DCL_CX 			*
 *			telling what kind of declaration this is part	*
 *			of.						*
 *									*
 * Nonterminals:							* 
 *									*
 *  classUnion	  rhs of a species dcl or lhs of a relate dcl.		*
 *									*
 * 		  Returns a list of CLASS_UNION_CELLs			*
 *		  for the members.  The meaning of			*
 *		  the list members depend on their tok			*
 *		  field value.						*
 *									*
 *		   tok			   Meaning			*
 *		  -------------------  	   --------			*
 *		  COMM_ID_TOK		   A community identifier	*
 *					     name: name of community	*
 *									*
 *		  FAM_ID_TOK		   A family identifier		*
 *					     name: name of family	*
 *									*
 *		  TYPE_ID_TOK		   A species expression		*
 *					     name: name of constructor	*
 *					     type: the species expr	*
 *					     role: the role of the	*
 *						   species expr		*
 *					   This is also what is returned*
 *					   for a genus id, where the	*
 *					   species is a wrapped species.*
 *									*
 *		  TYPE_LIST_TOK		   A list of species expresions *
 *					     name: name of constructor  *
 *					     types: list of types	*
 *					     roles: parallel list of 	*
 *						    roles		*
 *					   This form is only used when  *
 *					   there is more than one type, *
 *					   as when using a curried	*
 *					   constructor.			*
 *									*
 * classUnionElement   An individual element of a classUnion.  		*
 *									*
 * optWiths	       A phrase with A = B.				*
 ************************************************************************/

classUnion	: classUnionElement 
			{if($1 == NULL) $$ = NIL;
			 else {
			   HEAD_TYPE h;
			   h.cuc = $1;
			   bump_list($$ = general_cons(h, NIL, CUC_L));
			 }
			}

		| classUnionElement BAR_TOK classUnion
			{if($1 == NULL) $$ = $3;
			 else {
			   HEAD_TYPE h;
			   h.cuc = $1;
			   bump_list($$ = general_cons(h, $3, CUC_L));
			 }
			}
		;

classUnionElement : /*--------------------------------------------------*
		     * E [with A = B] 					*
		     * This can only occur in a genus or species	*
		     * declaration.					*
		     *--------------------------------------------------*/

		    getLine restrTypeExprs optWiths
			{$$ = get_discrim_p(NULL, $2, $3, 0, $1, 0);
			 drop_list($2.types);
			 drop_list($2.roles);
			 drop_list($3);
			}

		| /*----------------------------------------------------*
		   * constr(D) [with A = B] 		   		*
		   * This can only occur in a species declaration.	*
		   *----------------------------------------------------*/

		  getLine optRoleMode PUEntId typeExprs optWiths
			{$$ = get_discrim_p($3, $4, $5, 0, $1, &defopt_mode);
			 drop_list($4.types);
			 drop_list($4.roles);
			 drop_list($5);
			}

		| /*----------------------------------------------------*
		   * D op E [with A = B] 				*
		   * This can only occur in a species declaration	*
		   *----------------------------------------------------*/

		  getLine restrTypeExpr binOp typeExpr optWiths
			{RTLIST_PAIR pair;
			 TYPE *t;
			 ROLE *r;
			 
			 bump_type(t = pair_t($2.type, $4.type));
			 bump_role(r = pair_role($2.role,$4.role));
			 bump_list(pair.types = type_cons(t, NIL));
			 bump_list(pair.roles = role_cons(r, NIL));
			 $$ = get_discrim_p($3.name, pair, $5, 0, $1, 0);
			 drop_list(pair.types);
			 drop_list(pair.roles);
			 drop_rtype($2);
			 drop_rtype($4);
			 drop_list($5);
			}

		| /*-----------------------------------------------*
		   * id				   		   *
		   * This can only occur in a species declaration. *
		   *-----------------------------------------------*/

		  getLine optRoleMode PUEntId 
			{
			 RTLIST_PAIR pair;
			 bump_list(pair.types = type_cons(hermit_type, NIL));
			 bump_list(pair.roles = role_cons(NULL, NIL));
			 $$ = get_discrim_p($3, pair, NULL, 0, $1,
					    &defopt_mode);
			 drop_list(pair.types);
			 drop_list(pair.roles);
			}

		| /*-----------------------------------------------*
		   * famId				     	   *
		   * This can only occur in a relate declaration.  *
		   *-----------------------------------------------*/

		  getLine famId
			{$$ = fam_cu_mem_p($2->ctc, $1);
			 drop_type($2);
			}
		;

optWiths	: /* empty */
			{$$ = NIL;}

		| WITH_TOK withList
			{$$ = $2;}
		;

withList	: /* empty */
			{$$ = NIL;}

		| withList typeExpr EQUAL_TOK typeExpr
			{bump_list($$ = type_cons($2.type, 
						  type_cons($4.type, $1)));
			 drop_rtype($2);
			 drop_rtype($4);
			}
		;


/****************************************************************
 *			CLASS DECLARATIONS			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  this_mode		the declaration mode, from dclMode.	*
 *								*
 * Nonterminals:						*
 *								*
 *  classDcl	 Class C ... %Class				*
 *								*
 *  optClassInherit   Optional "isKindOf superclass".		*
 *								*
 ****************************************************************/

classDcl	: CLASS_TOK id optTypeExpr optClassInherit 
		  optStringConst
			{
			 /*---------------------------------------------*
			  * It is important to create the genus and	*
			  * to expect the species here, so that they	*
			  * can be used inside the class.		*
			  *---------------------------------------------*/

			 if(do_dcl_p() && inside_extension) {
			   delay_reports++;
			   check_imported("object.ast", TRUE);
			   do_class_preamble($2, $3.type, $4);
			   if($5.name != NULL) {
			     create_description_p($2, $5.name, NULL, 
						  &this_mode, 1, 
						  current_line_number);
			   }
			 }

			 /*-----------------------------------------------*
			  * Set up the global lists for reading the parts *
			  * of the class.				  *
			  *-----------------------------------------------*/

			 SET_EXPR(initializer_for_class,
				   same_e(hermit_expr, $1.line));
			 SET_LIST(class_vars, NIL);
			 SET_LIST(class_consts, NIL);
			 SET_LIST(class_expects, NIL);
			}
		  classParts end
			{check_end_p(&($1), CLASS_TOK, &($8));
			 if(do_dcl_p() && inside_extension) {
			   declare_class_p($2, $3, $4, $1.line);
			   remember_class_info($2, $4, $1.line);
			   if(delay_reports > 0) delay_reports--;
			 }

			 SET_EXPR(initializer_for_class, NULL);
			 SET_LIST(class_vars, NIL);
			 SET_LIST(class_consts, NIL);
			 SET_LIST(class_expects, NIL);
			}
		;

optClassInherit	: /* empty */
			{$$ = std_type_id[OBJECT_TYPE_ID];
			}

		| ISA_TOK id
			{$$ = $2;}
		;

classParts	: /* empty */
			{}

		| classParts constDcl		{}
		| classParts varDclForClass	{}
		| classParts expectDclForClass	{}
		;


/****************************************************************
 *			RELATE DECLARATIONS			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  union_context 	tells classUnion what to do with	*
 *			discriminators, and what to permit in	*
 *			lists.					*
 *								*
 *  this_mode		the declaration mode, from dclMode.	*
 *								*
 * Nonterminals:						*
 *								*
 *  relateDcl 	Relate{mode} A | B | ... isA C.			*
 *		Relate{mode} A & B = C.				*
 *								*
 *  meetDclRHS	 C, which can be one of the following forms,	*
 *		 where id is an identifier.  The attribute is   *
 *	         a list of up to three ids, as shown below.	*
 *								*
 *		    form	attribute			*
 *		    ----	---------			*
 *		    id		[id]				*
 *		    id1(id2)	[id,id2]			*
 *		    [id]	["List", id]			*
 *		    ()		["()"]				*
 *								*
 *		 The attribute is NIL for an error.		*
 ****************************************************************/

relateDcl	: RELATE_TOK dclMode relateCheck
		  classUnion ISA_TOK cgIdList end
			{check_end_p(&($1), RELATE_TOK, &($7));
			 if(do_dcl_p() && inside_extension) {
			   if(!local_error_occurred) {
			     LIST *p;
			     for(p = $6; p != NIL; p = p->tail) {
			       extend_tm($4, p->head.ctc, TRUE, &this_mode);
			     }
			   }
			 }

			 drop_list($4);
			}

		| RELATE_TOK dclMode relateCheck
		  classUnion AMPERSAND_TOK PUClassId
		  EQUAL_TOK meetDclRHS end
			{char *item4;

			 check_end_p(&($1), RELATE_TOK, &($9));

			 /*--------------------------------------------------*
			  * Item $4 should really be a PUClassId, like $6,   *
			  * but that leads to parser conflicts.		     *
			  *						     *
			  * The classUnion must be just a single identifier. *
			  * An unknown id will be given as a constructor.    *
			  *--------------------------------------------------*/

			 item4 = get_name_from_cucs_p($4);

			 if(do_dcl_p() && inside_extension) {
			   if(item4 == NULL || $6 == NULL) {
			     syntax_error(BAD_MEET_ERR, 0);
			   }
			   else {
			     perform_meet_p(item4, $6, $8);
			   }
			 }
			 drop_list($8);
			}

		| RELATE_TOK end
			{check_end_p(&($1), RELATE_TOK, &($2));
			 check_empty_block_p(&($1));
			}
		;


relateCheck	: /* empty */	                  	%prec BY_TOK
			{union_context = RELATE_DCL_CX;
			 check_modes(RELATE_DCL_MODES, 0);
			}
		;

meetDclRHS	: /*----*
		   * id *
		   *----*/

		  PUClassId
			{bump_list($$ = str_cons($1, NIL));}

		| /*-----------------*
		   * id(id) or id id *
		   *-----------------*/

		  PUClassId LPAREN_TOK PUClassId RPAREN_TOK
			{bump_list($$ = str_cons($1, str_cons($3, NIL)));
			 pop_assume_finger_tm();
		        }

		| PUClassId PUClassId
			{bump_list($$ = str_cons($1, str_cons($2, NIL)));}

		| /*----*
		   * () *
		   *----*/

		  LPAREN_TOK RPAREN_TOK
			{bump_list($$ = str_cons(std_id[HERMIT_TYPE_ID], NIL));
			 pop_assume_finger_tm();
		        }

		| /*------*
		   * [id] *
		   *------*/

		  LBRACK_TOK PUClassId RBRACK_TOK
			{bump_list($$ = str_cons(std_type_id[LIST_TYPE_ID], 
						 str_cons($2, NIL)));
		        }

		| /*--------*
		   * <:id:> *
		   *--------*/

		  LBOX_TOK PUClassId RBOX_TOK
			{if($1.attr != $3.attr) {
			   syntax_error(MISMATCHED_BRACES_ERR, $1.line);
			 }
			 bump_list($$ = str_cons(std_type_id[BOX_TYPE_ID], 
						 str_cons($2, NIL)));
		        }
		;

/****************************************************************
 *			EXPECT SPECIES DECLARATIONS		*
 ****************************************************************
 * Contexts:							*
 *								*
 *  this_mode		declaration mode from dclMode.		*
 *								*
 * Nonterminals:						*
 *								*
 *  expectTypeDcl	Expect[{mode}] species S %Expect	*
 *			or					*
 *			Expect[{mode}] species S(A) %Expect	*
 ****************************************************************/

expectTypeDcl	: EXPECT_TOK end
			{check_end_p(&($1), EXPECT_TOK, &($2));
			 check_empty_block_p(&($1));
			}

		| EXPECT_TOK dclMode SPECIES_TOK 
			{check_case($3, LC_ATT);
			 if($3.attr == UC_ATT) pop_shadow();
			}
		  PUClassId optTypeExpr optStringConst end
			{if($1.attr != EXPECT_ATT) {
			   syntax_error1(WORD_EXPECTED_ERR, "expect", 0);
			 }
			 check_end_p(&($1), EXPECT_TOK, &($8));

			 if(do_dcl_p()) {
			   check_modes(EXPECT_TYPE_MODES, 0);

			   expect_tf_p($5, $6.type, &this_mode, TRUE);

			   /*-----------------------------------------*
			    * Issue the description, if there is one. *
			    *-----------------------------------------*/

			   if($7.name != NULL) {
			     create_description_p($5, $7.name, NULL, 
						  &this_mode, 1,
						  current_line_number);
			   }
			 }
			}
		;


/****************************************************************
 *			DEFAULT DECLARATIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  defaultDcl		Default V => E %Default			*
 *								*
 *  defaultDclBody	V => E					*
 ****************************************************************/

defaultDcl	: DEFAULT_TOK dclMode defaultDclBody end
			{check_end_p(&($1), DEFAULT_TOK, &($4));
			 check_modes(DEFAULT_DCL_MODES, 0);
			 if($3) check_empty_block_p(&($1));
			}
		;

defaultDclBody	: /* empty */
			{$$ = 1;}

		| genusId DBL_ARROW_TOK typeExpr
			{if(do_dcl_p()) {
			   default_tm($1, $3.type, &this_mode);
			 }
			 drop_rtype($3);
			 $$ = 0;
		        }

		| communityId DBL_ARROW_TOK famId
			{if(do_dcl_p()) {
			   default_tm($1, $3, &this_mode);
			 }
			 drop_type($3);
			 $$ = 0;
			}

		| genusId DBL_ARROW_TOK QUESTION_MARK_TOK
			{if(do_dcl_p()) {
			   default_tm($1, NULL, &this_mode);
			 }
			 $$ = 0;
			}

		| communityId DBL_ARROW_TOK QUESTION_MARK_TOK
			{if(do_dcl_p()) {
			   default_tm($1, NULL, &this_mode);
			 }
			 $$ = 0;
			}
		;


/****************************************************************
 *			EXCEPTION DECLARATIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  exceptionDcl	Exception [trap] ex [S] [D] %Exception	*
 *								*
 *			  -- where S is a species and D is a    *
 *			  -- description string.		*
 *								*
 *  exceptionDclBody	[trap] ex [S]				*
 ****************************************************************/

exceptionDcl	: EXCEPTION_TOK 
			{check_case($1, UC_ATT);}
		  exceptionDclBody end
			{check_end_p(&($1), EXCEPTION_TOK, &($4));
			 if($3) check_empty_block_p(&($1));
			}
		;

exceptionDclBody : /* empty */
			{$$ = 1;}

		 | dclMode optTrap PUEntId optTypeExpr optStringConst
			{if(do_dcl_p()) {
			   check_modes(EXCEPTION_DCL_MODES, 0);
			   exception_p($3, $2, $4, $5.name, &this_mode);
			 }
			 drop_rtype($4);
			 $$ = 0;
		        }
		 ;

optTrap		 : /* empty */
			{$$ = 0;}

		 | LCTRAP_TOK
			{$$ = ($1.attr == TRAP_ATT);}
		 ;


/****************************************************************
 *			ADVISORY DECLARATION			*
 ****************************************************************
 * Contexts:							*
 *								*
 *   advisory_dcl	is true when an advisory is a		*
 *			declaration and false when it is an	*
 *			expression.				*
 *								*
 *   suppress_reset	This is set true to suppress setting,	*
 *			in start_dcl, of variables that are	*
 *			set in an advisory declaration.  If	*
 *			they were reset, the effect of the	*
 *			advisory declaration would be lost.	*
 *								*
 * Nonterminals:						*
 *								*
 *  advisoryDcl		Advise ... %Advise			*
 *								*
 *  advisoryExpr	Advise ... %Advise, when in a context	*
 *			where the advise should be deferred.	*
 *								*
 *  advisory		The content of an advisoryDcl or	*
 *			advisoryExpr.				*
 ****************************************************************/

advisoryDcl	: ADVISORY_TOK 
			{advisory_dcl = TRUE;}
		  advisory end
			{check_end_p(&($1), ADVISORY_TOK, &($4));}

		| ADVISORY_TOK end
			{check_end_p(&($1), ADVISORY_TOK, &($2));
			 check_empty_block_p(&($1));
			}
		;

advisoryExpr	: ADVISORY_TOK 
			{advisory_dcl = FALSE;}
		  advisory end
			{check_end_p(&($1), ADVISORY_TOK, &($4));
			 if($3 == NULL) bump_expr($$ = hermit_expr);
			 else $$ = $3;
			}
			
		| ADVISORY_TOK end
			{check_end_p(&($1), ADVISORY_TOK, &($2));
			 check_empty_block_p(&($1));
			}
		;

advisory	: /*----------------------------------------------------*
		   * Advise id [all|one].  Possibilities for id are 	*
		   *							*
		   *   showlocals					*
		   *   show						*
		   *   noshow						*
		   *   trackTrappedFailure				*
		   *   notrackTrappedFailure				*
		   *   warn						*
		   *   nowarn						*
		   *   tro						*
		   *   notro						*
		   *----------------------------------------------------*/

		  id optAllOne
			{if(do_dcl_p()) {
			   handle_option($1, $2);
			   suppress_reset = TRUE;
			 }
			 $$ = NULL;
			}

		| /*--------------------------------*
		   * Advise overloads k [all|one].  *
		   *--------------------------------*/

		  id NAT_CONST_TOK optAllOne

			/*-----------------------------------------------*
			 * Only allow up to 9999 overloads from one dcl. *
			 *-----------------------------------------------*/

			{if(strcmp($1, "overloads") != 0 
			    || strlen($2.name) > 4) {
			   syntax_error1(OVERLOADS_TOO_LARGE_ERR, $1, 0);
			 }
			 else if(do_dcl_p()) {
			   sscanf($2.name, "%d", &max_overloads);
			   if($3) main_max_overloads = max_overloads;
			 }
			 suppress_reset = TRUE;
			}
			

		| /*--------------------------------*
		   * Advise notro f,g,h  [all|one]. *
		   * Advise warn a,b,c   [all|one]. *
		   * Advise nowarn a,b,c [all|one]. *
		   * Advise debug a,b,c		    *
		   *--------------------------------*/

		  id idList optAllOne
			{LIST *p;

			 char* name = $1;
			 int is_not_notro = strcmp(name, "notro");
			 if(strcmp(name, "warn") != 0 &&
			    strcmp(name, "nowarn") != 0 &&
		            strcmp(name, "debug") != 0 &&
			    is_not_notro != 0) {
			   syntax_error1(BAD_ADVISORY_ERR, name, 0);
			 }

			 else if(do_dcl_p()) {

			   /*-----------------------------------------*
			    * For Advise debug a,b,c, first do simple *
			    * debug option, to set variable trace.    *
			    *-----------------------------------------*/

			   if(strcmp(name, "debug") == 0) {
			     handle_option("debug", $3);
			   }

			   /*----------------------------------------------*
			    * Handle each id in the list. Each is handled  *
			    * by concatenating the main word (tro, debug,  *
 			    * etc.) with the id from the list, and passing *
			    * the concatenated string to handle_option,    *
			    * except for notro options, which are handled  *
			    * by no_tro_tm.				   *
			    *----------------------------------------------*/

			   for(p = $2; p != NIL; p = p->tail) {
			     if(is_not_notro == 0) {
			       no_tro_tm(p->head.str, $3);
			     }
			     else {
			       char opt[100];
			       strcpy(opt, name);
			       strncat(opt, name_tail(p->head.str), 90);
			       handle_option(opt, $3);
			     }
			     suppress_reset = TRUE;
			   }
			 }
			 drop_list($2);
			 $$ = NULL;
		       }

		| /*-----------------*
		   * Role advisories *
		   *-----------------*/

		  PUEntIdList ROLE_MARK_TOK idList optTag
			{if(advisory_dcl) {
			   if(do_dcl_p()) do_for_advisory($1,$3,$4.type);
			   $$ = NULL;
			 }
			 else {
			   bump_expr($$ =
				     make_for_advisory($1,$3,$4,$2.line));
			 }
			 drop_list($1);
			 drop_list($3);
			 drop_rtype($4);
			}

		| /*------------------------------------------*
		   * Role advisories giving role completions. *
		   *------------------------------------------*/

		  id DBL_ARROW_TOK id
			{add_role_completion_tm($1, $3);
			}

		| /*----------------------------------------------*
		   * Message advisories.  These are shown in the  *
		   * program listing, but only if no listing is   *
		   * being generated.				  *
		   *----------------------------------------------*/

		  STRING_CONST_TOK
			{if(do_dcl_p() && !should_list()) {
			   fprintf(ERR_FILE, "\n%s\n", $1.name);
			 }
			}

		| /*-----------------------------------------------*
		   * : A : B				   	   *
		   *						   *
		   * Unify A and B and report the result.  This is *
		   * just for debugging.			   *
		   *-----------------------------------------------*/

		  COLON_TOK typeExpr COLON_TOK typeExpr
			{if(do_dcl_p()) {
#			   ifdef DEBUG
			     if(UNIFY($2.type, $4.type, 0)) {
			       err_print_str("\nYield: ");
			       err_print_ty($2.type);
			       err_nl();
			     }
			     else {
			       err_print_str("\nUnification failed\n");
			     }
#			   endif
			 }
			 drop_rtype($2);
			 drop_rtype($4);
			}
		;

optAllOne	: /* empty */
			{$$ = 1;}

		| WHICH_TOK
			{if($1 == ONE_ATT) $$ = 0;
			 else if($1 == ALL_ATT) $$ = 1;
			 else {
			   syntax_error1(WORD_EXPECTED_ERR, "all or one", 0);
			   $$ = 0;
			 }
			}
		;


/****************************************************************
 *		CONTEXT DECLARATIONS				*
 ****************************************************************
 * Contexts:							*
 *								*
 *  in_context_dcl	True when processing a context dcl.	*
 *			It is tested when 'target' is used, to  *
 *			see that 'target' can be used.		*
 *								*
 *  context_dcl_id	The main id (r below) in the context	*
 *			declaration.				*
 *								*
 * Nonterminals:						*
 *								*
 *   contextDcl		Context [extension] r [isKindOf s]*	*
 *			  case x1 => e1				*
 *			  ...					*
 *			  case xn => en				*
 *			  bring a,b,c				*
 *			  ...					*
 *			%Context				*
 *								*
 *  contextCases	The cases above				*
 *								*
 *  contextCase		One case above.				*
 ****************************************************************/

contextDcl	: CONTEXT_TOK optExtension id optInherits
			{LIST *cntxt;
			 Boolean context_found;
			 context_dcl_id = $3;
			 in_context_dcl = TRUE;
			 cntxt = get_context_tm($3, FALSE, &context_found, 0);
			 if($2 == 0) {
			   if(context_found) {
			     semantic_error1(CONTEXT_EXISTS_ERR, 
					     display_name($3), 0);
			   }
			   else add_context_tm($3, NULL, NULL);
			 }
			}
		  contextCases end
			{check_end_p(&($1), CONTEXT_TOK, &($7));
			 if($4 != NIL) {
			   do_context_inherits($3, $4);
			   drop_list($4);
			 }
			 in_context_dcl = FALSE;
			 pop_local_assumes_and_finger_tm();
			}

		| CONTEXT_TOK end
			{check_end_p(&($1), CONTEXT_TOK, &($2));
			 check_empty_block_p(&($1));
			}
		;

optExtension	: /* empty */
			{$$ = 0;}

		| EXTENSION_TOK
			{check_case($1, LC_ATT);
			 if($1.attr == UC_ATT) pop_shadow();
			 $$ = 1;
			}
		;

contextCases	: /* empty */ 			{}

		| contextCases contextCase	{}
		;

contextCase	: CASE_TOK id DBL_ARROW_TOK expr
			{if(do_dcl_p()) {
			   add_context_tm(context_dcl_id, $2, $4);
			 }
			 drop_expr($4);
			}

		| CASE_TOK BRING_TOK 
			{check_case($2, LC_ATT);
			 pop_shadow();
			}
		  PUEntIdList
			{if(do_dcl_p()) {
			   add_simple_contexts(context_dcl_id, $4, $1.line);
			 }
			 drop_list($4);
			}
		;


/****************************************************************
 *			BRING DECLARATIONS			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  this_mode		declaration mode, from dclMode		*
 *								*
 *  dcl_line		Holds the number of the line where	*
 *			'Bring' occurs.				*
 *								*
 * Nonterminals:						*
 *								*
 *  bringDcl		Bring[{mode}] x,y,z:T [from e].		*
 *			or					*
 *			Bring[{mode}] x,y,z by f.		*
 ****************************************************************/

bringDcl	: BRING_TOK 
			{check_case($1, UC_ATT);
			 dcl_line = $1.line;
			 in_bring_dcl = 1;
			}
		  dclMode bringBody end
			{LIST *p;
			 check_modes(BRING_DCL_MODES, $1.line);
			 check_end_p(&($1), BRING_TOK, &($5));
			 if(do_dcl_p()) {
			   suppress_using_local_expectations = TRUE;
			   for(p = $4; p != NIL; p = p->tail) {
			     issue_dcl_p(p->head.expr, LET_E, &this_mode);
			   }
			   suppress_using_local_expectations = FALSE;
			 }
			 drop_list($4);
			 bring_context = 0;
			 in_bring_dcl = 0;
			}

		| BRING_TOK end
			{check_end_p(&($1), BRING_TOK, &($2));
			 check_empty_block_p(&($1));
			 bring_context = 0;
			}
		;


/****************************************************************
 *			 LET AND DEFINE DECLARATIONS		*
 ****************************************************************
 * Contexts:							*
 * 								*
 *  this_mode		declaration mode from dclMode		*
 *								*
 *  deflet_st		tells what kind the current let		*
 *			declaration is.  For a declaration, 	*
 *			its top is either LET_E or DEFINE_E.	*
 *			For an expression, its top is either	*
 *			-LET_E or -DEFINE_E.			*
 *								*
 *  deflet_line_st	The top of this stack tells the line	*
 *			number where the enclosing let or	*
 *			define declaration starts.		*
 *								*
 *  defining_id_st	The top of this stack tells the 	*
 *			identifier that is being defined.	*
 *								*
 *  defining_id		The id being defined, in a declaration.	*
 *								*
 *  choose_st		The top of this stack holds information	*
 *			about the kind of choice being made,	*
 *			in a definition by cases.  The fields	*
 *			are					*
 *								*
 *			  choose_from: 				*
 *			    The parameter.  This is target of	*
 *			    the matches.			*
 *								*
 *			  which:				*
 *			    The choice mode.			*
 *								*
 *  			  working_choose_matching_list:		*
 *			    This list collects the patterns	*
 *			    that occur in the separate cases.	*
 *			    It is used for doing exhaustiveness *
 *			    tests of the patterns.		*
 *								*
 * Nonterminals:						*
 *								*
 *  letDcl		Let ... %Let or Define ... %Define	*
 *								*
 *  defLetBody	  	The body of a let or define declaration*
 *			or expression.  This returns a		*
 *			DEFINE_E or LET_E expression.		*
 *								*
 *  defLetCases		The cases of a let f by ... declaration,*
 *			excluding a then case.			*
 *								*
 *  topDefLetCases	The cases of a let f by ... declaration,*
 *			possibly including a then case, and 	*
 *			possibly wrappeded in Await, CutHere or *
 *			Unique.					*
 *								*
 *  optDefLetThen	Optional then pat => action.		*
 *								*
 *  optSpeciesDescr	Optional bunch of species, descriptions *
 *			options for a let dcl.			*
 *								*
 *  optCase		CASE_TOK or nothing.			*
 ****************************************************************/

letDcl		: LET_TOK 

			/*----------------------------------------------*
			 * Token LET_TOK includes reserved word Relet.  *
			 * There are no relet declarations.		*
			 *----------------------------------------------*/

			{if($1.attr == RELET_ATT) {
			   syntax_error(RELET_DCL_ERR, 0);
			   $1.attr = LET_ATT;
			 }
			 push_int(deflet_st, let_expr_kind($1.attr));
			 push_int(deflet_line_st, $1.line);

			 /*-----------------------------------------------*
			  * We need to suppress binary operators, since	  *
			  * we might see a declaration such as		  *
			  *						  *
			  *   Let species Natural +(?x) = ...		  *
			  *						  *
 			  * and + would normally be taken to be a binary  *
			  * operator here.  Notice that there will have	  *
			  * to be a lookahead read after optSpeciesDescr, *
			  * and that lookahead will be read with	  *
			  * long_force_no_op being true.		  *
			  *-----------------------------------------------*/

			 long_force_no_op = TRUE;
			}

		  dclMode optSpeciesDescr 
			{long_force_no_op = FALSE;}

		  defLetBody end
			{check_end_p(&($1), LET_TOK, &($7));
			 check_modes(DEFINE_DCL_MODES, $1.line);
			 if(do_dcl_p()) {

			   /*-------------------------------------*
			    * Force an irregular definition to be *
			    * a let, not a define.		  *
			    *-------------------------------------*/

			   int kind = has_mode(&this_mode, IRREGULAR_MODE)
				         ? LET_E 
				         : let_expr_kind($1.attr);

			   /*---------------------------------------*
			    * If there is a description here, 	    *
			    * then suppress writing the expectation *
			    * into the index file, since it will    *
			    * be written with the description.	    *
			    *---------------------------------------*/

			   if(version_list.descrip != NULL) {
			     add_mode(&this_mode, NODESCRIP_MODE);
			   }
			   issue_dcl_p($6, kind, &this_mode);
			   issue_embedded_description($6, version_list.descrip,
						      &this_mode);
			 }

			 check_for_bad_compile_id();

			 pop_local_assumes_and_finger_tm();
			 drop_expr($6);
			 pop(&deflet_st);
			 pop(&deflet_line_st);
			 SET_LIST(version_list.types, NIL);
			 SET_LIST(version_list.roles, NIL);
			}

		| LET_TOK end
			{check_end_p(&($1), LET_TOK, &($2));
			 check_empty_block_p(&($1));
			}

		| LET_TOK error
			{check_for_bad_compile_id();
			 debug_at_dcl_panic();
			 if(dcl_panic_l()) {
			   yyclearin;
			   yyerrok;
			 }
			}
		  recover
			{debug_at_dcl_recover();}
		;

optSpeciesDescr	: /* empty */		%prec EQUAL_TOK
			{version_list.types   = NIL;
			 version_list.roles   = NIL;
			 version_list.descrip = NULL;
			}

		| optSpeciesDescr speciesDescrPart
			{}
		;

speciesDescrPart: /*-----------*
		   * species A *
		   *-----------*/

		  SPECIES_TOK 
			{check_case($1, LC_ATT);
			 if($1.attr == UC_ATT) pop_shadow();
			}
		  typeExpr
			{SET_LIST(version_list.types, 
				  type_cons($3.type, version_list.types));
			 SET_LIST(version_list.roles,
				  role_cons($3.role, version_list.roles));
			 drop_rtype($3);
			}

		| /*---------------------*
		   * description "descr" *
		   *---------------------*/

		  DESCRIPTION_TOK 
			{check_case($1, LC_ATT);
			 pop_shadow();
			}
		  STRING_CONST_TOK
			{if(version_list.descrip == NULL) {
			   version_list.descrip = $3.name;
			 }
			 else {
			   syntax_error(MULTI_DESCRIP_IN_WITH_ERR, 0);
			   version_list.descrip = NULL;
			 }
			}
		;

defLetBody	: /*-------------------*
		   * x = e  or e1 = e2 *
		   *-------------------*/

		  expr 
			{EXPR* id = get_applied_fun($1, FALSE);
			 char* def_id = (id == NULL) ? NULL : id->STR;
			 push_str(defining_id_st, def_id);
			 if(top_i(deflet_st) > 0) {
			   defining_id = def_id;
			 }
			}

		  defletEqual expr 
			{int eqkind = $3;

			 /*-------------------------------------------*
			  * If the definition has the form e <-...,   *
			  * then we are instructed to use unification.*
			  *-------------------------------------------*/

			 if(eqkind >= ARROW1_ATT) {
			   make_unify_pattern($1, eqkind, $1->LINE_NUM);
			 }

			 bump_expr($$ = perform_simple_define_p($1, $4));
			 pop(&defining_id_st);
			 drop_expr($1);
			 drop_expr($4);
			}

		| /*---------------------------------*
		   * T constructor e1 = e2 [then e3] *
		   *---------------------------------*/

		  classId CONSTRUCTOR_TOK expr
			{EXPR* id = get_applied_fun($3, FALSE);
			 char* def_id = (id == NULL) ? NULL : id->STR;
			 push_str(defining_id_st, def_id);
			 if(top_i(deflet_st) > 0) {
			   defining_id = def_id;
			 }
			}
 		  EQUAL_TOK expr optThenExpr
			{bump_expr($$ = 
				   build_constructor_def_p($1, $3, $6, $7));
			 drop_expr($3);
			 drop_expr($6);
			 drop_expr($7);
			}

		| /*----------------------*
		   * x by first ..., etc. *
		   *----------------------*/

		  exprOrTaggedProcId BY_TOK which

			/*-------------------------------------------------*
			 * Check that $1 is an identifier.  Store that	   *
			 * identifier into defining_id, for use in cases   *
			 * below.					   *
			 *						   *
			 * Also check the mode.  Mode perhaps is not	   *
			 * allowed here.				   *
			 * 						   *
		         * Build the target of the match, and return it as *
			 * $3. The target is only used when this is the	   *
			 * definition of a function.			   *
			 *						   *
			 * Also, if this is a function, we are 		   *
			 * beginning a choose-matching construct.  Push    *
			 * the target onto choose_st so that it can        *
			 * be found when building the match-expression 	   *
			 * that is the guard of each case.		   *
			 *-------------------------------------------------*/

			{EXPR* target;
			 EXPR* this_id = get_applied_fun($1, FALSE);
			 CHOOSE_INFO* inf = allocate_choose_info();

			 if(which_result == PERHAPS_ATT) {
			   syntax_error(PERHAPS_NOT_ALLOWED_ERR, 0);
			 }

			 if(this_id == NULL) {
			   syntax_error(BAD_DEF_HEAD_ERR, $1->LINE_NUM);
			   if(top_i(deflet_st) > 0) defining_id = NULL;
			   push_str(defining_id_st, NULL);
			   target = NULL;
			 }
			 else {
			   if(top_i(deflet_st) > 0) {
			     defining_id = this_id->STR;
			   }
			   push_str(defining_id_st, this_id->STR);
			   target = id_expr(HIDE_CHAR_STR "target", 
					    $1->LINE_NUM);
			 }
			 num_params = -1;  /* Not known */
			 bump_expr($<expr_at>$ = target);

			 bump_expr(inf->choose_from = target);
			 inf->which = which_result;
			 push_choose_info(choose_st, inf);
			 push_int(case_kind_st, NOCASE_ATT);
			}

		topDefLetCases
			{LIST *pl;
			 CHOOSE_INFO *inf;

			 /*-----------------------------------------*
			  * If the identifier is ok, then build the *
			  * definition.				    *
			  *-----------------------------------------*/

			 if($<expr_at>4 != NULL) {

			   /*----------------------------------------------*
			    * If this is a function definition, then handle*
			    * as a function definition.			   *
			    *----------------------------------------------*/

			   if(num_params > 0) {
			     bump_expr($$ = 
				make_def_by_cases_p($1, $<expr_at>4, $5,
						    labs(top_i(deflet_st))));

			     /*----------------------------------------------*
			      * Get the list of patterns.  Add the target    *
			      * and the function being defined to it, and    *
			      * install it into choose_matching_lists for    *
			      * the exhaustiveness checker.  		     *
			      * See patmatch/pmcompl.c. 		     *
			      *----------------------------------------------*/

			     inf = top_choose_info(choose_st);
			     pl  = inf->working_choose_matching_list;
			     if(pl != NIL) {
			       add_list_to_choose_matching_lists_p($1, pl);
			     }
			   }

			   /*------------------------------------------------*
			    * If this is not a function definition, then     *
			    * handle it as a non-function definition by      *
			    * cases.					     *
			    *------------------------------------------------*/

			   else {
			     EXPR* cases = 
				   build_choose_p(NULL, $5, FALSE, 
						  $5->LINE_NUM);
			     bump_expr($$ = 
				new_expr2(labs(top_i(deflet_st)), $1, cases, 
					  top_i(deflet_line_st)));
			   }
			 }
			 else bump_expr($$ = bad_expr);

			 /*---------------------------------------------*
			  * Pop the stacks that were pushed above, and  *
			  * clean up.					*
			  *---------------------------------------------*/

			 pop(&defining_id_st);
			 free_choose_info(top_choose_info(choose_st));
			 pop(&choose_st);
			 pop(&case_kind_st);
			 drop_expr($1);
			 drop_expr($<expr_at>4);
			 drop_expr($5);
			}

		| /*---------------------------------------------*
		   * This case catches let's without an = or by. *
		   *---------------------------------------------*/

		  expr		%prec error
			{syntax_error(DEFLET_NO_EQUAL_ERR, 
				      top_i(deflet_line_st));
			 drop_expr($1);
			 bump_expr($$ = bad_expr);
			}
		;

topDefLetCases	: defLetCases optDefLetThen
			{
			 /*----------------------------------------*
			  * Check that there is at least one case. *
			  *----------------------------------------*/

			 check_for_cases($1, top_i(deflet_line_st));

			 /*------------------------------*
			  * Build the result expression. *
			  *------------------------------*/

			 if($2 == NULL) $$ = $1;
			 else { 
			   bump_expr($$ = add_then_to_body($1, $2));
			   drop_expr($1);
			   drop_expr($2);
			 }
			}

		| AWAIT_TOK optAmpersand optExprList THEN_TOK
		  topDefLetCases end
			{check_end_p(&($1), AWAIT_TOK, &($6));
			 bump_expr($$ = make_await_expr($5, $3, $2, $1.line));
			 pop_local_assumes_and_finger_tm();
			 drop_list($3);
			 drop_expr($5);
			}

		| ATOMIC_TOK topDefLetCases end
			{check_end_p(&($1), ATOMIC_TOK, &($3));
			 SET_EXPR1_P($$, SINGLE_E, $2, $1.line);
			 $$->SINGLE_MODE = $1.attr;
			 pop_local_assumes_and_finger_tm();
			 drop_expr($2);
			}
		;

defLetCases	: /* empty */
                        {bump_expr($$ = make_empty_else_p());
			}

		| CASE_TOK 
			{case_kind_st->head.i = CASE_ATT;
			 if($1.attr != CASE_ATT) {
			   syntax_error(DEFLET_UNTIL_ERR, $1.line);
			 }
			}
		  expr defletEqual expr optWhen defLetCases
			{int eqkind = $4;

			 /*-------------------------------------------*
			  * If the case has the form e <-..., then we *
			  * are instructed to use unification.        *
			  *-------------------------------------------*/

			 if(eqkind >= ARROW1_ATT) {
			   make_unify_pattern($3, eqkind, $3->LINE_NUM);
			 }

			 bump_expr($$ = make_define_case_p($3, $5, $6, $7));
			 drop_expr($3);
			 drop_expr($5);
			 drop_expr($6);
			 drop_expr($7);
			}

		| optCase ELSE_TOK expr defletEqual expr
			{int eqkind = $4;

			 check_else_p();

			 /*-------------------------------------------*
			  * If the case has the form e <-..., then we *
			  * are instructed to use unification.        *
			  *-------------------------------------------*/

			 if(eqkind >= ARROW1_ATT) {
			   make_unify_pattern($3, eqkind, $3->LINE_NUM);
			 }

			 bump_expr($$ = make_define_else_case_p($3, $5));

			 drop_expr($3);
			 drop_expr($5);
			}

		| DO_TOK checkDo expr defLetCases
			 {EXPR* todo = $3;
			  if(todo == NULL) todo = same_e(NULL, $1.line);
			  else if(todo->ty != NULL) {
			    todo = same_e(todo, todo->LINE_NUM);
			  }
			  bump_type(todo->ty = hermit_type);
			  check_for_case_after_else_p($1.line);
			  bump_expr($$ = apply_expr(todo, $4, $1.line));
			  drop_expr($3);
			  drop_expr($4);
			 }

		| DO_TOK checkDo expr DBL_ARROW_TOK expr
		  defLetCases
			 {EXPR_LIST *pat_list;
			  EXPR *bdy, *pat;
			 
			  check_for_case_after_else_p($1.line);
			  bump_list(pat_list =
			    get_define_case_parts_p($3,NIL,hermit_expr,&bdy));
			  drop_expr(bdy);
			  bump_expr(pat = list_to_expr(pat_list, 0));
			  bump_expr($$ = 
			    make_do_dblarrow_case_p(pat, $5, $6, $1.line));
			  drop_expr($3);
			  drop_expr($5);
			  drop_expr($6);
			  drop_expr(pat);
			  drop_list(pat_list);
			 }
		;

optDefLetThen	: /* empty */
			{$$ = NULL;}

		| THEN_TOK expr DBL_ARROW_TOK expr
			{if(num_params == 0) {
			   syntax_error(THEN_NOT_ALLOWED_ERR, $2->LINE_NUM);
			 }

			 bump_expr($$ = 
				  new_expr2(PAIR_E, $2, $4, $2->LINE_NUM));
			 drop_expr($2);
			 drop_expr($4);
			}
		;

defletEqual	: EQUAL_TOK
			{$$ = 0;}

		| PLUS_EQUAL_TOK
			{$$ = $1;}
		;

optWhen		: /* empty */
			{$$ = NULL;}

		| WHEN_TOK expr
			{$$ = $2;}
		;

optCase		: /* empty */ 	{}
		| CASE_TOK	{}
		;


/****************************************************************
 *		CONST DECLARATIONS (IN CLASSES)			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  this_mode		declaration mode from dclMode		*
 *								*
 * Nonterminals:						*
 *								*
 *  constDcl	Const x,y,z: type %Const			*
 * 		  (in a class)					*
 ****************************************************************/

constDcl	: CONST_TOK dclMode idList COLON_TOK typeExpr end
			{check_end_p(&($1), CONST_TOK, &($6));
			 if(do_dcl_p()) {
			   add_class_consts($3, $5);
			 }
			 drop_list($3);
			 drop_rtype($5);	 
			}

		| CONST_TOK end
			{check_end_p(&($1), CONST_TOK, &($2));
			 check_empty_block_p(&($1));
			}
		;


/****************************************************************
 *			VAR DECLARATIONS			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  var_initial_val	This variable is set to the initial     *
 *			value of a var declaration or		*
 *			expression, or to NULL if there is	*
 *			no initial value.			*
 *								*
 *  var_content_type 	This holds a variable that is installed *
 *			as the content type of the boxes in	*
 *			a var declaration by make_var_expr_p.	*
 *								*
 *  all_init		This is set to true if the initializer  *
 *			has 'all' before it, and to false	*
 *			otherwise.				*
 *								*
 * Nonterminals:						*
 *								*
 *  varDcl		Var ... %Var				*
 *								*
 *  varBody		The contents of a var dcl or expression *
 *			(This is found below, under varExpr.)	*
 *			The attribute is an expression that 	*
 *			defines all of the boxes or arrays that	*
 *			are mentioned in the body.  It also	*
 *			computes the initial value, if there is	*
 *			one.					*
 ****************************************************************/

varDcl		: VAR_TOK 
			{push_int(var_context_st, 0);}
                  varBody end
			{check_end_p(&($1), VAR_TOK, &($4));
			 if(do_dcl_p()) {

			   /*-----------------------------------------*
			    * Issue the declaration, but don't allow  *
			    * it to be polymorphic.		      *
			    *-----------------------------------------*/

			  {EXPR *this_dcl;
			   bump_expr(this_dcl = same_e($3, $1.line));
			   this_dcl->TEAM_MODE = 1;
			   disallow_polymorphism = TRUE;
			   issue_dcl_p(this_dcl, TEAM_E, 0);
			   disallow_polymorphism = FALSE;
			   drop_expr(this_dcl);
			  }
			 }

			 /*---------------------------------------------*
			  * Pop propagate_var_opt_st if we are not at   *
			  * a semicolon, and drop the exprs that were   *
 			  * bumped by varBody.				*
			  *---------------------------------------------*/

			 if(!recent_begin_from_semicolon) {
			   pop(&propagate_var_opt_st);
			 }
			 drop_expr($3);
			 drop_expr(var_initial_val);
			 drop_type(var_content_type);
			 pop(&var_context_st);
			}

		| VAR_TOK end
			{check_end_p(&($1), VAR_TOK, &($2));
			 check_empty_block_p(&($1));
			}
		;


/****************************************************************
 *		   VAR DECLARATIONS IN CLASSES			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  var_initial_val	This variable is set to the initial     *
 *			value of a var declaration or		*
 *			expression, or to NULL if there is	*
 *			no initial value.			*
 *								*
 *  var_content_type 	This holds a variable that is installed *
 *			as the content type of the boxes in	*
 *			a var declaration by make_var_expr_p.	*
 *								*
 *  all_init		This is set to true if the initializer  *
 *			has 'all' before it, and to false	*
 *			otherwise.				*
 *								*
 * vars_for_class	This list is set up during processing	*
 *			of var_body.  It contains information	*
 *			about the variables that were declared	*
 * 			in the class.  The variables are in 	*
 *			this list in reverse order.		*
 *								*
 * Nonterminals:						*
 *								*
 *  varDclForClass	Var ... %Var (in a class). 		* 
 *								*
 *  varBody		The contents of a var dcl or expression *
 *			(This is found below, under varExpr.)	*
 *			The attribute is an expression that 	*
 *			defines all of the boxes or arrays that	*
 *			are mentioned in the body.  It also	*
 *			computes the initial value, if there is	*
 *			one.					*
 ****************************************************************/

varDclForClass	: VAR_TOK 
			{push_int(var_context_st, 1);
			 holding_phrase_required = 1;
			}
		  varBody end
			{check_end_p(&($1), VAR_TOK, &($4));
			 holding_phrase_required = 0;
			 if(do_dcl_p()) {
			   SET_EXPR(initializer_for_class, 
				    apply_expr($3, initializer_for_class, 
					       $3->LINE_NUM));
			 }

			 /*---------------------------------------------*
			  * Pop propagate_var_opt_st if we are not at a *
			  * semicolon, and drop the exprs that were 	*
			  * bumped by varBody. 				*
			  *---------------------------------------------*/

			 if(!recent_begin_from_semicolon) {
			   pop(&propagate_var_opt_st);
			 }
			 drop_expr($3);
			 drop_expr(var_initial_val);
			 drop_type(var_content_type);
			 pop(&var_context_st);
			}

		| VAR_TOK end
			{check_end_p(&($1), VAR_TOK, &($2));
			 check_empty_block_p(&($1));
			}
		;


/****************************************************************
 *			TEAM DECLARATIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  teamDcl	Team						*
 *		   Define ... %Define				*
 *		   ...						*
 *		   Define ... %Define				*
 *               %Team						*
 *								*
 *		(Actually, the declarations that can occur	*
 *		inside a team declaration are			*
 *		   Define					*
 *		   Let						*
 *		   Advise					*
 *		   Description)					*
 *								*
 *  teamParts	The declarations in a team			*
 *								*
 *  teamPart	One declaration in a team.			*
 *								*
 *  teamLetExpr A let or define in a team.			*
 ****************************************************************/
 
teamDcl		: TEAM_TOK teamParts end
			{EXPR *e;
			 check_end_p(&($1), TEAM_TOK, &($3));
			 bump_expr(e = same_e($2, $1.line));
			 e->TEAM_MODE = 1;
			 if(do_dcl_p()) issue_dcl_p(e, TEAM_E, 0);
			 drop_expr(e);
			 drop_expr($2);
			}
		; 

teamParts	: teamPart
			{$$ = $1;}

		| teamParts teamPart
			{bump_expr($$ = apply_expr($1, $2, $1->LINE_NUM));
			 drop_expr($1);
			 drop_expr($2);
			}
		;

teamPart	: teamLetExpr
			{$$ = $1;}

		| LINE_TOK
			{bump_expr($$ = same_e(hermit_expr, 
					       current_line_number));}

		| descriptionExpr
			{$$ = $1;}

		| advisoryExpr
			{$$ = $1;}

		| error		
			{
#			 ifdef DEBUG
			   if(trace) {
			     trace_t(451);
			     trace_t(454);
			   }
#			 endif
			 if(dcl_panic_l()) {
			   yyclearin;
			   yyerrok;
			 }
			}
		  recover
			{bump_expr($$ = hermit_expr);
#			 ifdef DEBUG
			   if(trace) {
			     trace_t(453);
			     trace_t(454);
			   }
#			 endif
			}
		;

teamLetExpr	: LET_TOK 
			/*----------------------------------------------*
			 * Token LET_TOK includes reserved word Relet.  *
			 * There are no relet declarations.		*
			 *----------------------------------------------*/

			{if($1.attr == RELET_ATT) {
			   syntax_error(RELET_DCL_ERR, 0);
			 }
			 push_int(deflet_st, -let_expr_kind($1.attr));
			 push_int(deflet_line_st, $1.line);
			}

		  dclMode optSpeciesDescr defLetBody end
			{Boolean added_mode_node = FALSE;

			 check_end_p(&($1), LET_TOK, &($6));
			 check_modes(DEFINE_DCL_MODES, $1.line);

			 /*--------------------------*
			  * Force let to be define. *
			  *--------------------------*/

			 {EXPR* ssresult = skip_sames($5);
			  if(EKIND(ssresult) == LET_E) {
			    ssresult->kind = DEFINE_E;
			  }
			 }

			 /*---------------------------------------------*
			  * Install mode node, if the mode is not the 	*
			  * default.  First, add NODESCRIP_MODE to      *
			  * the mode if there is a description.		*
			  *---------------------------------------------*/

			 if(version_list.descrip != NULL) {
			   add_mode(&this_mode, NODESCRIP_MODE);
			 }
			 $5->LINE_NUM = $1.line;
			 if(!mode_equal(&this_mode, &null_mode)) {
			   bump_expr($$ = same_e($5, $1.line));
			   $$->SAME_MODE = 5;
			   $$->SAME_E_DCL_MODE = copy_mode(&this_mode);
			   drop_expr($5);
			   added_mode_node = TRUE;
			 }
			 else {
			   $$ = $5;
			 }

			 /*---------------------------------------------*
			  * Install the version node, if appropriate.   *
			  *---------------------------------------------*/

			 if(version_list.types != NIL) {
			   if(!added_mode_node) {
			     SET_EXPR($$, same_e($5, $1.line));
			     $$->SAME_MODE = 5;  /* Mode holder */
			   }
		
			   /* Both of these take ref from version_list. */
			   $$->EL1 = version_list.types;  
			   $$->EL2 = version_list.roles;
			   version_list.types = NULL;
			   version_list.roles = NULL;
			 }

			 /*---------------------------------------------*
			  * Add a description from the version_list if	*
			  * there is one.  Just add it as another 	*
			  * component of the team.			*
			  *---------------------------------------------*/

			 if(version_list.descrip != NULL) {
			   int   line          = $$->LINE_NUM;
			   EXPR* id            = skip_sames($$)->E1;
			   EXPR* descr_dcl     = new_expr1(MANUAL_E, id, line);
			   descr_dcl->STR      = version_list.descrip;
			   descr_dcl->MAN_FORM = 0;
			   SET_EXPR($$, apply_expr(descr_dcl, $$, line));
			 }

			 /*-----------*
			  * Clean up. *
			  *-----------*/

			 pop_local_assumes_and_finger_tm();
			 pop(&deflet_st);
			 pop(&deflet_line_st);
			 version_list.descrip = NULL;
			}

		| LET_TOK end
			{check_end_p(&($1), LET_TOK, &($2));
			 check_empty_block_p(&($1));
			 bump_expr($$ = same_e(hermit_expr, $1.line));
			 pop_local_assumes_and_finger_tm();
			}
		;


/****************************************************************
 *			EXPECT DECLARATIONS			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  this_mode		declaration mode, from dclMode		*
 *								*
 *  expect_context	EXPECT_ATT for an expectation, 		*
 *			ANTICIPATE_ATT for an anticipation.	*
 *								*
 * Nonterminals:						*
 *								*
 *  expectDcl	 	Expect ... %Expect for entities		*
 *			Anticipate ... %Anticipate		*
 *								*
 *  expectDclBody	contents of expectDcl.			*
 *								*
 ****************************************************************/

expectDcl	: EXPECT_TOK 
			{expect_context = $1.attr;}
		  expectDclBody end 
			{check_end_p(&($1), EXPECT_TOK, &($4));
			 if($3) check_empty_block_p(&($1));
			}
		;

expectDclBody	: /* empty */
			{$$ = 1;}

		| dclMode PUEntIdList COLON_TOK typeExpr optStringConst
			{TYPE *t;

			 /*---------------------*
			  * Check the mode.	*
			  *---------------------*/

			 check_modes(EXPECT_DCL_MODES, 0);

			 /*---------------------------------------------*
			  * Now do the expectation or anticipation.     *
			  *---------------------------------------------*/

			 if(do_dcl_p()) {

			   /*-------------------------------------------*
			    * Copy the type. A copy is normally made to *
			    * separate this expectation from variables	*
			    * that occur in abbreviations.		*
			    *-------------------------------------------*/

			   bump_type(t = copy_expect_types
					  ? copy_type($4.type, 4) 
				          : $4.type);

			   /*-----------------------------------------------*
			    * If there is a description here, then suppress *
			    * generation of expectation entries in the      *
			    * .asd file, since such an entry will be made   *
			    * when the description is handled.		    *
			    *-----------------------------------------------*/

			   if($5.name != NULL) {
			     suppress_expectation_descriptions = TRUE;
			   }

			   /*-------------------------------------*
			    * Do the expectation or anticipation, *
			    * and note it. 			  *
			    *-------------------------------------*/

			   expect_ent_ids_p($2, t, $4.role, expect_context, 
					   &this_mode,
					   current_line_number, FALSE,
					   this_mode.u.def_package_name);

			   /*-------------------------------------------*
			    * Anticipations need to be noted in 	*
			    * anticipate_id_table.  That table is used  *
			    * in two places.  When an extensible genus  *
			    * or community is extended, anticipations   *
			    * are converted to expectations.  Also, at  *
			    * the end of the package, anticipations are *
			    * checked, to see that all required		*
			    * definitions were made.			*
			    *-------------------------------------------*/

			   if(expect_context == ANTICIPATE_ATT) {
			     note_expectations_p($2, t, ANTICIPATE_ATT, 
					         &this_mode,
						 current_package_name,
						 current_line_number);
			   }

			   /*-------------------------------------------*
			    * Table expect_id_table holds local 	*
			    * expectations.  If we are not doing an     *
			    * import, and this is an expectation, then  *
			    * add it to expect_id_table.		*
			    *-------------------------------------------*/

			   else if(main_context != IMPORT_CX && 
				   main_context != INIT_CX) {
			     note_expectations_p($2, t, EXPECT_ATT,
					         &this_mode,
						 current_package_name,
						 current_line_number);
			   }
			
			   /*-----------------------------------------*
			    * Issue the description, if there is one. *
			    *-----------------------------------------*/

			   if($5.name != NULL) {
			     suppress_expectation_descriptions = FALSE;
			     if(list_length($2) != 1) {
			       semantic_error(MULTI_DESCR_ERR, 0);
			     }
			     else {
			       create_description_p($2->head.str, $5.name,
						    $4.type, &this_mode,
						    0, $5.line);
			     }
			   }

			   drop_type(t);
			 }

			 drop_rtype($4);
			 drop_list($2);
			 $$ = 0;
			}

		| SPECIES_TOK id
			{if($1.attr == UC_ATT) pop_shadow();
			 semantic_error(BARE_EXTENSION_ERR, 0);
			 $$ = 0;
			}
		;

optStringConst	: /* empty */
		 	{$$.name = NULL;}

		| STRING_CONST_TOK
				{$$ = $1;}
		;


/****************************************************************
 *		EXPECT DECLARATIONS FOR CLASSES			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  this_mode		declaration mode, from dclMode		*
 *								*
 *  expect_context	EXPECT_ATT for an expectation, 		*
 *			ANTICIPATE_ATT for an anticipation.	*
 *								*
 * Nonterminals:						*
 *								*
 *  expectDclForClass	Expect ... %Expect in a class		*
 *			Anticipate ... %Anticipate in a class	*
 *								*
 *  expectDclForClassBody contents of expectDclForClass.	*
 *								*
 ****************************************************************/

expectDclForClass : EXPECT_TOK 
			{expect_context = $1.attr;}
		    expectDclForClassBody end 
			{check_end_p(&($1), EXPECT_TOK, &($4));
			}

		   | EXPECT_TOK end
			{check_end_p(&($1), EXPECT_TOK, &($2));
			 check_empty_block_p(&($1));
			}
		   ;

expectDclForClassBody : dclMode PUEntIdList COLON_TOK typeExpr optStringConst
			{
			 /*---------------------*
			  * Check the mode.	* 
			  *---------------------*/

			 check_modes(EXPECT_DCL_MODES, 0);
			 if(this_mode.u.def_package_name != NULL) {
			   semantic_error(DEF_PACKAGE_NAME_IN_CLASS_ERR, 0);
			 }

			 /*---------------------------------------------*
			  * Add the expectations to class_expects.	*
			  *---------------------------------------------*/

			 if(do_dcl_p()) {
			   add_class_expects($2, $4, $5.name);
			 }
	
			 drop_rtype($4);
			 drop_list($2);
			}

		| SPECIES_TOK id
			{if($1.attr == UC_ATT) pop_shadow();
			 semantic_error(SPECIES_EXPECTATION_IN_CLASS_ERR, 0);
			}
		;


/****************************************************************
 *			MISSING DECLARATION			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  this_mode		declaration mode, from dclMode		*
 *								*
 * Nonterminals:						*
 *								*
 *   missingDcl		Missing[{mode}] x,y,z: S %Missing	*
 ****************************************************************/

missingDcl	: MISSING_TOK 
			{check_case($1, UC_ATT);}
		  dclMode PUEntIdList COLON_TOK typeExpr end
			{LIST *l;

			 /*-------------------------------------*
			  * Check the modes and the end marker. *
			  *-------------------------------------*/

			 check_modes(MISSING_DCL_MODES, 0);
			 check_end_p(&($1), MISSING_TOK, &($7));

			 if(do_dcl_p()) {
			   for(l = $4; l != NIL; l = l->tail) {
			     issue_missing_tm(l->head.str,$6.type,&this_mode);
			   }
			 }
			 drop_list($4);
			 drop_rtype($6);
			}

		| MISSING_TOK end
			{check_end_p(&($1), MISSING_TOK, &($2));
			 check_empty_block_p(&($1));
			}
		;


/************************************************************************
 *			ABBREV DECLARATION				*
 ************************************************************************
 * Contexts:								*
 * 									*
 *  this_mode		declaration mode, from dclMode			*
 *									*
 *  abbrev_val		Holds value of abbrev right-hand side.  	*
 *			See below.					*
 *									*
 *  abbrev_code		Indicates what is on the right-hand 		*
 *			side.  See below.				*
 *									*
 *  no_copy_abbrev_vars  When an abbreviation is USED, its variables	*
 *			 must be copied.  That is the default when	*
 *			 an abbreviation is read.  However, in a 	*
 *			 declaration such as 				*
 *				Abbrev A = B.				*
 *			 where B is an abbreviation, we must not copy	*
 *			 the variables in B.  no_copy_abbrev_vars	*
 *			 is set true to indicate that variables are	*
 *			 not to be copied.				*
 *									*
 *  abbrev_tok_role	The role that is on the right-hand side of	*
 *			an abbrev declaration.				*
 *									*
 * Nonterminals:							*
 *									*
 *	abbrevDcl	: Abbrev A = B %Abbrev				*
 *									*
 *	abbrevDclBody	: contents of abbrev dcl.			*
 *									*
 *	abbrevVal	: The right hand side of an abbrev		*
 *			  declaration.  It sets variables		*
 *			  abbrev_val  and abbrev_code as		*
 *			  follows.					*
 *									*
 *	 	rhs	     abbrev_code         abbrev_val		*
 *	   ------------    ----------------     -------------		*
 *	   type expr V      TYPE_ABBREV_TOK           V			*
 *	   family var X	    FAM_VAR_ABBREV_TOK   X (as a type)		*
 *	   family id F	    FAM_ABBREV_TOK       F (as a type)		*
 *	   genus id V	    GENUS_ABBREV_TOK   V (a wrapped 		*
 *					         species V)		*
 *	   community id C   COMM_ABBREV_TOK      C (a wrapped		*
 *					         family C)		*
 *             ?            QUESTION_MARK_TOK    NULL			*
 ************************************************************************/

abbrevDcl	: ABBREV_TOK 
			{no_copy_abbrev_vars = TRUE;}
		  abbrevDclBody end
			{check_end_p(&($1), ABBREV_TOK, &($4));
			 if($3) check_empty_block_p(&($1));
			}
		;

abbrevDclBody	: /* empty */
			{$$ = 1;}

		| /*---------------*
		   * Abbrev A = X. *
		   *---------------*/

		  dclMode PUClassId EQUAL_TOK abbrevVal
			{check_modes(ABBREV_DCL_MODES, 0);
			 if(do_dcl_p()) {
			   abbrev_tm($2, abbrev_code, abbrev_val, 
				abbrev_tok_role,
			        !has_mode(&this_mode, NO_EXPORT_MODE));
			 }
			 $$ = 0;
			}

		| /*------------------*
		   * Abbrev A(T) = X. *
		   *------------------*/

		  dclMode PUClassId typeExpr EQUAL_TOK typeExpr
			{TYPE *t;
			 check_modes(ABBREV_DCL_MODES, 0);
			 bump_type(t = pair_t($3.type, $5.type));
			 SET_ROLE(abbrev_tok_role, $5.role);
			 if(do_dcl_p()) {
			   abbrev_tm($2, FAM_MACRO_TOK, t, 
				abbrev_tok_role,
				!has_mode(&this_mode, NO_EXPORT_MODE));
			 }
			 drop_type(t);
			 drop_rtype($3);
			 drop_rtype($5);
			 $$ = 0;
			}
		;

abbrevVal	: typeExpr
			{TYPE_TAG_TYPE kind = TKIND($1.type);
			 abbrev_code = 
			  (kind == WRAP_TYPE_T) ? GENUS_ABBREV_TOK :
			  (kind == WRAP_FAM_T)  ? COMM_ABBREV_TOK 
						: TYPE_ABBREV_TOK;
			 SET_TYPE(abbrev_val, $1.type);
			 SET_ROLE(abbrev_tok_role, $1.role);
			 drop_type($1.type);
			 drop_role($1.role);
			}

		| famVar
			{abbrev_code = FAM_VAR_ABBREV_TOK;
			 SET_TYPE(abbrev_val, $1);
			 drop_type($1);
			}

		| famId
			{abbrev_code = FAM_ABBREV_TOK;
			 SET_TYPE(abbrev_val, $1);
			 drop_type($1);
			}

		| QUESTION_MARK_TOK
			{abbrev_code = QUESTION_MARK_TOK;
			 SET_TYPE(abbrev_val, NULL);
			}
		;


/****************************************************************
 *			ASSUME DECLARATION			*
 ****************************************************************
 * Contexts:							*
 * 								*
 *  this_mode		declaration mode, from dclMode		*
 *								*
 *  dcl_context		Needed to know whether to do an outer	*
 *			assumption or a local one.		*
 *								*
 * Nonterminals:						*	
 *								*
 *  assumeDcl	   Assume ... %Assume.  Returns attribute of	*
 *		   ASSUME_TOK at beginning.			*
 *								*
 *  assumeDclBody  contents of assume dcl.			*
 ****************************************************************/

assumeDcl	: ASSUME_TOK 
			{check_case($1, UC_ATT);}
		  assumeDclBody end
			{check_end_p(&($1), ASSUME_TOK, &($4));
			 if($3) check_empty_block_p(&($1));
			 $$ = $1;
			}
		;

assumeDclBody	:  /* empty */
			{$$ = 1;}

		| /*------------------*
		   * Assume x,y,z: T. *
		   *------------------*/

		  getDclContext dclMode PUEntIdList COLON_TOK typeExpr
			{check_modes(ASSUME_DCL_MODES, 0);
			 if(do_dcl_p()) {
			   dcl_context = tobool($1);
			   assume_p($3, $5, &this_mode);
			 }
			 drop_list($3);
			 drop_rtype($5);
			 $$ = 0;
			}

		| /*------------------------*
		   * Assume x,y,z: pattern. *
		   *------------------------*/

		  getDclContext dclMode PUEntIdList COLON_TOK LCPATTERN_TOK
			{STR_LIST *p;
			 check_modes(ASSUME_DCL_MODES, 0);
			 if(do_dcl_p()) {
			   for(p = $3; p != NIL; p = p->tail) {
			     if($1) {
			       patfun_assume_tm(p->head.str, 
				!has_mode(&this_mode, NO_EXPORT_MODE));
			     }
			     else {
			       push_local_pattern_assume_tm(p->head.str);
			     }
			   }
			 }
			 drop_list($3);
			 $$ = 0;
			}

		| /*------------------*
		   * Assume x,y,z: ?. *
		   *------------------*/

		  getDclContext dclMode PUEntIdList COLON_TOK QUESTION_MARK_TOK
			{STR_LIST *p;
			 Boolean global;
			 check_modes(ASSUME_DCL_MODES, 0);
			 global = !has_mode(&this_mode, NO_EXPORT_MODE);
			 if(do_dcl_p()) {
			   for(p = $3; p != NIL; p = p->tail) {
			     char *id = p->head.str;
			     assume_tm(id, any_type, global);
			     assume_role_tm(id, NULL, global);
			     delete_patfun_assume_tm(id, global);
			   }
			 }
			 drop_list($3);
			 $$ = 0;
			}

		| /*-------------*
		   * Assume 0:T. *
		   *-------------*/

		  getDclContext dclMode NAT_CONST_TOK COLON_TOK typeExpr
			{check_modes(ASSUME_DCL_MODES, 0);
			 if(do_dcl_p()) {
			   dcl_context = tobool($1);
			   do_const_assume_tm(0, std_type_id[ANYG_TYPE_ID],
				$5.type,
				!has_mode(&this_mode, NO_EXPORT_MODE));
			 }
			 drop_rtype($5);
			 $$ = 0;
			}

		| /*----------------*
		   * Assume 0.0: T. *
		   *----------------*/

		  getDclContext dclMode REAL_CONST_TOK COLON_TOK typeExpr
			{check_modes(ASSUME_DCL_MODES, 0);
			 if(do_dcl_p()) {
			   dcl_context = tobool($1);
			   do_const_assume_tm(1, std_type_id[ANYG_TYPE_ID],
				$5.type,
				!has_mode(&this_mode, NO_EXPORT_MODE));
			 }
			 drop_rtype($5);
			 $$ = 0;
			}
		;

getDclContext	: /* empty */
			{$$ = dcl_context;}
		;


/************************************************************************
 *			PATTERN AND EXPAND DECLARATIONS			*
 ************************************************************************
 * Contexts:								*
 *									*
 *  this_mode		declaration mode, from dclMode			*
 *									*
 *  patexp_dcl_kind	PAT_DCL_E for a pattern dcl, EXPAND_E for	*
 *			an expand dcl.					*
 *									*
 *  pat_rule_opts_ok	Set true when dclMode should allow modes that	*
 *			only apply to pattern declarations.		*
 *			(The tag modes are of that nature.)		*
 *									*
 *  pat_dcl_has_pat_consts  Set true if a pattern declaration is 	*
 *			    defining a pattern constant.		*
 *									*
 *  dcl_pat_fun		The pattern function or constant being declared *
 *									*
 * Nonterminals:							*
 *									*
 *  patternDcl		Pattern ... %Pattern				*
 *			Expand ... %Expand				*
 *									*
 *  patternDclBody	Content of pattern or expand declaration.  	*
 *			For a pattern dcl:				*
 *			    The rules are entered in the		*
 *			    pattern rule table, and the function is	*
 *			    entered in the pattern function table.  	*
 *			    Variables are set as follows.		*
 *									*
 *				pat_fun_name is set to the name of the	*
 *					     pattern function f being	*
 *					     defined,			*
 *			        pat_fun_expr is set to an expression	*
 *			    		     node for f, including the	*
 *					     type of f.			*
 ************************************************************************/

patternDcl	: PATTERN_TOK 
			{if($1.attr == PATTERN_ATT) {
		           patexp_dcl_kind        = PAT_DCL_E;
			   pat_rule_opts_ok       = TRUE;
			   pat_dcl_has_pat_consts = 0;
			 }
			 else {
			   patexp_dcl_kind = EXPAND_E;
			 }
			}
		  dclMode optSpeciesDescr patternDclBody end
			{check_end_p(&($1), PATTERN_TOK, &($6));
			 if($5 == NULL_E) {
			   check_empty_block_p(&($1));
			 }
			 else {

			   /*---------------------------------*
			    * Check the mode for consistency. *
			    *---------------------------------*/

			   check_modes(PATTERN_DCL_MODES, 0);
			   if((this_mode.patrule_mode & 3) == 3 ||
			      (this_mode.patrule_mode & 0xc) == 0xc) {
			     semantic_error(PRMODE_OVERLAP_ERR, 0);
			   }

			   /*----------------------------------------*
			    * Suppress completeness checking for all *
			    * expand and pattern dcls.		     *
			    *----------------------------------------*/

			   add_mode(&this_mode, INCOMPLETE_MODE);
			   dcl_context = FALSE;

			   /*------------------------*
			    * Issue the declaration. *
			    *------------------------*/

			   if(do_dcl_p()) {
			     $5->SCOPE = pat_dcl_has_pat_consts;
			     issue_dcl_p($5, PAT_DCL_E, &this_mode);
			   }
			   drop_expr($5);
			 }
			 dcl_pat_fun = NULL_E;
			 pat_rule_opts_ok = FALSE;
			 SET_LIST(version_list.types, NIL);
			 SET_LIST(version_list.roles, NIL);
			}
		;

patternDclBody	: /* empty */
		 	{$$ = NULL_E;}

		| /*----------------*
		   * Pattern f = g. *
		   *----------------*/

		  expr 
			{EXPR* e = skip_sames($1);
			 EXPR_TAG_TYPE kind = EKIND(e);
			 if(kind != IDENTIFIER_E) {
			   syntax_error1(WORD_EXPECTED_ERR, "=>", 0);
			 }
			 else {
			   e->kind = PAT_FUN_E;
			   e->E3 = NULL;
			 }
			}
       		  EQUAL_TOK taggedEntOrProcId
			{bump_expr($$ = 
				   new_expr2(patexp_dcl_kind, 
					     $1, $4, $1->LINE_NUM));
			 $$->PAT_DCL_FORM = 1;
			 drop_expr($1);
			 drop_expr($4);
			}

		| /*-----------------*
		   * Pattern a => b. *
		   *-----------------*/

		  expr
			{EXPR *p, *e;
			 EXPR_TAG_TYPE kind;

			 e = skip_sames($1);
			 kind = EKIND(e);
			 if(kind != IDENTIFIER_E) {
			   p = get_applied_fun($1, TRUE);
			 }
			 else {
			   p = e;
			   pat_dcl_has_pat_consts = 1;
			 }
			 bump_expr($<expr_at>$ = p);
			 bump_expr(dcl_pat_fun = p);
		        }
		  DBL_ARROW_TOK expr
			{EXPR *e;
			 check_for_duplicate_ids_in_pattern_formal($1);
			 if(patexp_dcl_kind == PAT_DCL_E) {
			   bump_expr(e = new_expr1(PAT_RULE_E, NULL_E, 0));
			   e->E2     = $1;            /* inherits ref count */
			   e->E3     = $4;            /* inherits ref count */
			   e->PAT_RULE_MODE = this_mode.patrule_mode;
			   e->ETAGNAME      = this_mode.patrule_tag;
			   e->extra         = open_pat_dcl;
			   bump_expr($$ = new_expr2(PAT_DCL_E, $<expr_at>2, 
						    e, $1->LINE_NUM));
			 }
			 else {
			   bump_expr($$ = new_expr2(EXPAND_E, $1, $4, 
						    $1->LINE_NUM));
			 }
			 drop_expr($<expr_at>2);
			 SET_EXPR(dcl_pat_fun, NULL_E);
			}
		;


/****************************************************************
 *		 DESCRIPTION DECLARATONS AND EXPRESSIONS	*
 ****************************************************************
 * Contexts:							*
 * 								*
 *  this_mode		declaration mode, from dclMode		*
 *								*
 * Nonterminals:						*
 *								*
 *  descriptionDcl	Description[{mode}] x s %Description	*
 *								*
 *  descriptionExpr	Same as descriptionDcl, but used when	*
 *			the description is part of a team.	*
 *			It makes an EXPR node for the		*
 *			description, rather than declaring the	*
 *			description.				*
 *								*
 *  descriptionEntry	The content of a descriptionDcl or	*
 *			descriptionExpr.			*
 ****************************************************************/

descriptionDcl	: DESCRIPTION_TOK dclMode descriptionEntry end
			{check_case($1, UC_ATT);
			 check_end_p(&($1), DESCRIPTION_TOK, &($4));
			 if(do_dcl_p()) {
			   check_modes(DESCRIPTION_DCL_MODES, $1.line);
			   if(has_mode(&this_mode, AHEAD_MODE)) {
			     EXPR* id = skip_sames($3->E1);
			     insert_ahead_description_tm(id->STR,$3->STR, 
							 id->ty, &this_mode);
			   }
			   else if(inside_extension) {
			     defer_issue_description_p($3, &this_mode);
			   }
			   else if(!($3->MAN_FORM)) {
			     issue_dcl_p($3, MANUAL_E, &this_mode);
			   }
			   else issue_description_p($3, &this_mode);
			 }
			 drop_expr($3);
			}
		;

descriptionExpr	: DESCRIPTION_TOK dclMode descriptionEntry end
			{check_case($1, UC_ATT);
			 check_end_p(&($1), DESCRIPTION_TOK, &($4));
			 if(!mode_equal(&this_mode, &null_mode)) {
			   bump_expr($$ = same_e($3, $1.line));
			   $$->SAME_MODE = 5; 
			   $$->SAME_E_DCL_MODE = copy_mode(&this_mode);
			   drop_expr($3);
			 }
			 else {
			   $$ = $3;
			 }
			}
		;

descriptionEntry : /* empty */
			{bump_expr($$ = 
			    same_e(hermit_expr,current_line_number));
			}

		 | taggedEntOrProcId STRING_CONST_TOK		
			{bump_expr($$ = new_expr1(MANUAL_E, $1, $1->LINE_NUM));
			 $$->STR = $2.name;
			 drop_expr($1);
			}

		 | classId STRING_CONST_TOK
			{EXPR* id = id_expr($1, 0);
			 bump_expr($$ = new_expr1(MANUAL_E, id, 0));
			 $$->STR = $2.name;
			 $$->MAN_FORM = 1;
			}

		 | STRING_CONST_TOK STRING_CONST_TOK
			{EXPR* id = id_expr($1.name, 0);
			 bump_expr($$ = new_expr1(MANUAL_E, id, 0));
			 $$->STR = $2.name;
			 $$->MAN_FORM = 3;
			}
		 ;


/****************************************************************
 *			OPERATOR DECLARATONS			*
 ****************************************************************
 * Contexts:							*
 * 								*
 *  in_operator_dcl	True while processing an operator dcl.	*
 *			It is used by the lexer to realize that *
 *			"(:" should not be a token, but instead	*
 *			should be broken after the "(", as in   *
 *			   Operator op(::).			*
 * 								*
 * Nonterminals:						*
 *								*
 *  operatorDcl		Operator ... %Operator			*
 *								*
 *  opDclBody		contents of operator declaration.	*
 *			The operator is put into the operator	*
 *			table.					*
 *								*
 *  precedence		An indicator of precedence.  Returns	*
 *			identifier,  or "andf" for "and", or 	*
 *			"orf" for "or".				*
 ****************************************************************/

operatorDcl	: OPERATOR_TOK dclMode
			{in_operator_dcl = TRUE;
			 check_modes(OPERATOR_DCL_MODES, $1.line);
		        }
		  opDclBody end	
			{check_end_p(&($1), OPERATOR_TOK, &($5));
			 if($4) check_empty_block_p(&($1));
		        }
		;

opDclBody	: /* empty */
			{$$ = 1;}

		| optOpen PUEntId LPAREN_TOK precedence RPAREN_TOK
			{if(do_dcl_p()) {
			   operator_p($2,$4,tobool($1), &this_mode);
			 }
			 pop_assume_finger_tm();
			 $$ = 0;
			}

		| optOpen PUEntId
			{if($1) syntax_error(OPEN_UNARY_OP_ERR, 0);
			 if(do_dcl_p()) {
			   operator_p($2, NULL, FALSE, NULL);
			 }
			 $$ = 0;
			}
		;

precedence	: id
			{$$ = $1;}

		| WHERE_TOK
			{$$ = std_id[WHERE_ID];}

		| COLON_EQUAL_TOK
			{$$ = std_id[COLON_EQUAL_SYM];}
		;

optOpen		: /* empty */
			{$$ = FALSE;}

		| OPEN_TOK
			{$$ = TRUE;}
		;


/****************************************************************
 *			TYPE EXPRESSIONS			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  abbrev_var_table	This hash table is used to store copies	*
 *			of the variables that occur in		*
 *			type abbreviations.  Each declaration	*
 *			makes its own copies of variables 	*
 *			when the variables are encountered, but *
 *			must make only one copy, no matter	*
 *			how many times the variable occurs	*
 *			This table tells what to use as the	*
 *			copy on all but the first occurrence.	*
 *								*
 * Nonterminals:						*
 *								*
 *  typeExpr		A species expression.  The species	*
 *			expression is returned.			*
 *								*
 *  simpleTypeExpr	A type identifier or variable.		*
 *								*
 *  restrTypeExpr	Like typeExpr, but does not allow	*
 *			recovery at an unknown identifier.	*
 *								*
 *  typeExprList	A list of species expressions, separated*
 *			by commas.  Each component is permitted *
 *			to have a role.  The result is a pair	*
 *			of lists, holding the types and the	*
 *			roles of the list that was read.	*
 *								*
 *  typeExprs		Same as typeExprList, but the types	*
 *			are not separated by commas.		*
 *								*
 *  restrTypeExprs	Same as typeExprs, but cannot recover	*
 *			from an unknown id.			*
 *								*
 *  constraintList	A list of constraints, as a list of	*
 *			types.  Type (A,B) represents constraint*
 *			A >= B, and type A -> B represents	*
 *			constraint A >>= B.			*
 *								*
 *  optRoleMode		An optional mode.			*
 ****************************************************************/

typeExpr	: optRoleMode PUEntId
			{syntax_error1(UNKNOWN_ID_ERR, $2, 0);
			 $$.type = NULL;
			 $$.role = NULL;
			}

		| restrTypeExpr
			{$$ = $1;}
		;

restrTypeExpr	: simpleTypeExpr
			{$$ = $1;}

		| /*--------------------*
		   * F(A), C`a(A), etc. *
		   *--------------------*/

		  fam typeExpr			%prec FAM_ID_TOK
			{bump_type($$.type = fam_mem_t($1, $2.type));
			 $$.role = NULL;
			 drop_type($1);
			 drop_rtype($2);
			}

		| /*--------------------------*
		   * M(A) (M an abbreviation) *
		   *--------------------------*/

		  FAM_MACRO_TOK 
			{bump_role($<role_at>$ = abbrev_tok_role);}
		  typeExpr
			{TYPE *abbrv_ty;

			 bump_type(abbrv_ty = copy_type($1.ty, 7));
			 if(!UNIFY(abbrv_ty->TY1, $3.type, FALSE)) {
			   syntax_error1(BAD_FAM_MACRO_USE_ERR, $1.name, 
					 current_line_number);
			   bump_type($$.type = hermit_type);
			 }
			 else {
			   bump_type($$.type = abbrv_ty->TY2);
			 }
			 $$.role = $<role_at>2;  /* Ref from rhs */
			 drop_rtype($3);
			 drop_type($1.ty);
			 drop_type(abbrv_ty);
			}
			
		| /*---------*
		   * (A,B,C) *
		   *---------*/

		  LPAREN_TOK typeExprList RPAREN_TOK 
			{bump_rtype($$ =
			  list_to_type_expr($2.types, $2.roles, 0));
			 pop_assume_finger_tm();
			 drop_list($2.types);
			 drop_list($2.roles);
			}

		| /*-----*
		   * [A] *
		   *-----*/

		   LBRACK_TOK typeExpr RBRACK_TOK
			{bump_type($$.type = list_t($2.type));
			 $$.role = NULL;
			 drop_rtype($2);
			}

		| /*-------*
		   * <:A:> *
		   *-------*/

		  LBOX_TOK typeExpr RBOX_TOK
			{if($1.attr != 0 || $3.attr != 0) {
			   syntax_error(SPECIFIC_BOX_TYPE_ERR, $1.line);
			 }
			 bump_type($$.type = fam_mem_t(box_fam, $2.type));
			 $$.role = $2.role;
			 drop_type($2.type);
			}

		| /*-------*
		   * {A}   *
		   *-------*/

		  LBRACE_TOK simpleTypeExpr RBRACE_TOK
			{bump_type($$.type = new_type(MARK_T, $2.type));
			 bump_role($$.role = $2.role);
			 drop_rtype($2);
			}

		| /*--------*
		   * A -> B *
		   *--------*/

		   typeExpr SGL_ARROW_TOK typeExpr
			{bump_type($$.type = function_t($1.type, $3.type));
			 if($1.role != NULL || $3.role != NULL) {
			   bump_role($$.role = fun_role($1.role, $3.role));
			 }
			 else $$.role = NULL;
			 drop_rtype($1);
			 drop_rtype($3);
			}

		| /*---------*
		   * r =>> A *
		   *---------*/

		   optRoleMode PUEntId ROLE_MARK_TOK typeExpr
			{bump_rtype($$ = make_role_type_expr_p($2,$4));
			 if($$.role != NULL) {
			    $$.role->mode = copy_mode(&defopt_mode);
			 }
			 drop_rtype($4);
			}

		| /*----------------------------*
		   * A constraint (V >= B) or 	*
 		   * A constraint (V >>= B) or 	*
  		   * A constraint (V <= B)  or  *
		   * A constraint (V <<= B)     *
		   *----------------------------*/

		  typeExpr CONSTRAINT_TOK 
		  LPAREN_TOK constraintList RPAREN_TOK
			{TYPE_LIST *p;
			 Boolean gg;
			 for(p = $4; p != NIL; p = p->tail) {
			   TYPE* this_constraint = p->head.type;
			   gg = (TKIND(this_constraint) == FUNCTION_T);
			   add_lower_bound(this_constraint->TY1,
					   this_constraint->TY2,
					   gg, $2.line);
			 }
			 pop_assume_finger_tm();
			 drop_list($4);
			 $$ = $1;
			}
		;

simpleTypeExpr	: /*-------------------------------*
		   * Natural, or other identifiers *
		   *-------------------------------*/

		  TYPE_ID_TOK
			{bump_type($$.type = type_id_tok_p($1.name));
			 $$.role = NULL;
			}

		| /*------------*
		   * REAL, etc. *
		   *-----------*/

		  genusId
			{bump_type($$.type = 
				   type_id_tok_from_ctc_p($1));
			 $$.role = NULL;
			}

		| /*-------------------------------------*
		   * G`a or `a, or some variant of that. *
		   *-------------------------------------*/

		  TYPE_VAR_TOK
			{bump_type($$.type = tf_var_tm($1.name, TYPE_VAR_T));
			 $$.role = NULL; 
			}

		| /*------------------------*
		   * Abbreviations of types *
		   *------------------------*/

		  TYPE_ABBREV_TOK
			{bump_type($$.type = no_copy_abbrev_vars ? $1.ty :
				   copy_type1($1.ty, &abbrev_var_table, 7));
			 bump_role($$.role = abbrev_tok_role);
			 drop_type($1.ty);
			}

		| /*-----------------*
		   * () -- unit type *
		   *-----------------*/

		   LPAREN_TOK RPAREN_TOK
			{bump_type($$.type = hermit_type);
			 $$.role = NULL;
			 pop_assume_finger_tm();
			}

		;

optRoleMode	: initDefOpt
			{}

		| LBRACE_TOK initDefOpt defOpts RBRACE_TOK
			{}
		;

constraintList	: constraint
			{if($1 != NULL) {
			   bump_list($$ = type_cons($1, NIL));
			   drop_type($1);
			 }
			 else $$ = NIL;
			}

		| constraintList COMMA_TOK constraint
			{if($3 != NULL) {
			   bump_list($$ = type_cons($3, $1));
			   drop_list($1);
			   drop_type($3);
			 }
			 else $$ = $1;
			}
		;

constraint	: typeExpr allButProcId typeExpr
			{char* rel = $2.name;
			 if(rel == std_id[GE_SYM]) {
			   $$ = pair_t($1.type, $3.type);
			 }
			 else if(rel == std_id[GGEQ_SYM]) {
			   $$ = function_t($1.type, $3.type);
			 }
			 else if(rel == std_id[LE_SYM]) {
			   $$ = pair_t($3.type, $1.type);
			 }
			 else if(rel == std_id[LLEQ_SYM]) {
			   $$ = function_t($3.type, $1.type);
			 }
			 else {
			   syntax_error1(WORD_EXPECTED_ERR, 
					">= or >>= or <= or <<=", $2.line);
			   $$ = NULL;
			 }
			 bump_type($$);
			 drop_rtype($1);
			 drop_rtype($3);
			}
		;

fam		: famId
			{$$ = $1;}

		| famVar
			{$$ = $1;}
		;

typeExprList	: typeExpr
			{bump_list($$.types = type_cons($1.type, NIL));
			 bump_list($$.roles = role_cons($1.role, NIL));
			 drop_rtype($1);
			}

		| typeExpr COMMA_TOK typeExprList
			{bump_list($$.types = type_cons($1.type, $3.types));
			 bump_list($$.roles = role_cons($1.role, $3.roles));
			 drop_rtype($1);
			 drop_list($3.types);
			 drop_list($3.roles);
			}

		;

typeExprs	: typeExpr
			{bump_list($$.types = type_cons($1.type, NIL));
			 bump_list($$.roles = role_cons($1.role, NIL));
			 drop_rtype($1);
			}

		| typeExpr typeExprs
			{bump_list($$.types = type_cons($1.type, $2.types));
			 bump_list($$.roles = role_cons($1.role, $2.roles));
			 drop_rtype($1);
			 drop_list($2.types);
			 drop_list($2.roles);
			}

		;

restrTypeExprs	: restrTypeExpr
			{bump_list($$.types = type_cons($1.type, NIL));
			 bump_list($$.roles = role_cons($1.role, NIL));
			 drop_rtype($1);
			}

		| restrTypeExpr typeExprs
			{bump_list($$.types = type_cons($1.type, $2.types));
			 bump_list($$.roles = role_cons($1.role, $2.roles));
			 drop_rtype($1);
			 drop_list($2.types);
			 drop_list($2.roles);
			}
		;

optTypeExpr  	: /* empty */
			{$$.type = NULL;
			 $$.role = NULL;
			}

		| typeExpr
			{$$ = $1;}
		;


/************************************************************************
 *			EXPRESSIONS				 	*
 ************************************************************************
 * Nonterminals:						 	*
 *									*
 *  expr		An entity, or pattern expression.  Returns	*
 *			the expression, as a data structure.		*
 *									*
 *  exprList		A list of expressions, separated by commas.  	*
 *									*
 *  optExpr		An optional expression.  Returns the expression,*
 *			or hermit_expr if there is none.		*
 *									*
 *  roleSelectBody	contents of {:...:} or /{:...:}			*
 ************************************************************************/

expr		  

/*-----------*
 * CONSTANTS *
 *-----------*/
		: CHAR_CONST_TOK
			{bump_expr($$ = 
			  const_expr($1.name, CHAR_CONST, char_type, $1.line));
			}

		| NAT_CONST_TOK
			{TYPE *t;
			 EXPR *c;

			 /*-------------------------------*
			  * Get the assumed type, if any. *
			  *-------------------------------*/

			 t = get_local_const_assume_tm(0);
			 if(t == NULL) {
			   t = current_nat_const_assumption;
			 }
			 t = copy_type(t, 1);

			 /*-----------------------*
			  * Build the expression. *
			  *-----------------------*/

			 c = const_expr($1.name, NAT_CONST, natural_type, 
					$1.line);
			 bump_expr($$ = 
			   apply_expr(id_expr(std_id[NATCONST_ID], $1.line),
				      c, $1.line));
			 bump_type($$->ty = t);
			}

		| REAL_CONST_TOK
			{TYPE *t;
			 EXPR *c;

			 /*-------------------------------*
			  * Get the assumed type, if any. *
			  *-------------------------------*/

			 t = get_local_const_assume_tm(1);
			 if(t == NULL) {
			   t = current_real_const_assumption;
			 }
			 t = copy_type(t, 1);

			 /*-----------------------*
			  * Build the expression. *
			  *-----------------------*/

			 c = const_expr($1.name, REAL_CONST, rational_type, 
					$1.line);
			 bump_expr($$ = 
			   apply_expr(id_expr(std_id[RATCONST_ID], $1.line),
				      c, $1.line));
			 bump_type($$->ty = t);
			}

		| STRING_CONST_TOK
			{bump_expr($$ = 
			  const_expr($1.name, STRING_CONST, string_type, 
				     $1.line));
			}

/*-------------*
 * IDENTIFIERS *
 *-------------*/

		/*-----------------------------------------------------*
		 * We aren't keeping track of which identifiers have   *
		 * been defined while parsing, so the usual situation  *
 		 * is to see an UNKNOWN_ID_TOK.  This is not an error. *
		 *-----------------------------------------------------*/

		| UNKNOWN_ID_TOK			%prec IDENTIFIER
			{bump_expr($$ = 
				   tagged_id_p($1.name, NULL_RT, $1.line));
			}

		| PAT_VAR_TOK
			{bump_expr($$ = 
			    tagged_pat_var_p($1.name, NULL_RT, $1.line));
			}

		| QUESTION_MARK_TOK
			{bump_expr($$ = anon_pat_var_p(NULL_RT, $1.line));}

		| LPAREN_TOK taggedProcId RPAREN_TOK
			{EXPR *id;
			 id = skip_sames($2);
			 id->OFFSET = 0;
			 id->extra  = 0;
			 bump_expr($$ = $2);

			 /*-------------------------------------------------*
			  * The lexer will push PROC_ID_TOK onto the shadow *
			  * stack, and will fail to pop the parenthesis     *
			  * in this case.  So we must pop both here.	    *
			  *-------------------------------------------------*/

			 pop_shadow();
			 pop_shadow();
			 pop_assume_finger_tm();
			 drop_expr($2);
			}

		| LPAREN_TOK taggedUnaryOp RPAREN_TOK
			{$$ = $2;
			 pop_assume_finger_tm();
			}

		| MISSING_TOK PUEntId
			{EXPR *id;
			 check_case($1, LC_ATT);
			 pop_shadow();
			 bump_expr(id = tagged_id_p($2, NULL_RT, $1.line));
			 SET_EXPR(id, cp_old_id_tm(id, 1));
			 bump_expr($$ = same_e(id, $1.line));
			 $$->SAME_MODE = 4;
			 drop_expr(id);
			}

		| LCPATTERN_TOK PUEntId 
			{EXPR *id, *ssid;
			 bump_expr(id = tagged_id_p($2, NULL_RT, $1.line));
			 ssid            = skip_sames(id);
			 ssid->kind      = PAT_FUN_E;
			 ssid->E3        = NULL;
			 ssid->MARKED_PF = 1;
			 ssid->pat       = 1;
			 bump_expr($$ = cp_old_id_tm(id, 1));
			 drop_expr(id);
			 id = skip_sames($$);
			 if(id->kind == LOCAL_ID_E) {
			   semantic_error1(LOCAL_PATFUN_ERR, 
					   display_name(id->STR), 
					   $1.line);
			 }
			 id->PRIMITIVE = PAT_FUN;
			 id->pat = 1;
			}

		| LCEXTRACT_TOK
			{check_imported("pattern/listpat.ast", TRUE);
			 bump_expr($$ = id_expr(std_id[EXTRACTL_ID], $1.line));
			}

		| classBad
			{syntax_error1(COLON_EXP_ERR, display_name($1), 0);
			 bump_expr($$ = bad_expr);
			}
			
/*-------------------------------------------*
 * CARTESIAN PRODUCTS, PARENTHESES, BRACKETS *
 *-------------------------------------------*/

		| LPAREN_TOK exprList RPAREN_TOK
			{bump_expr($$ = list_to_expr($2, 0));
			 $$->LINE_NUM = $1.line;
			 drop_list($2);
			 pop_assume_finger_tm();
			}

		| LPAREN_DBLBAR_TOK exprList RPAREN_DBLBAR_TOK
			{bump_expr($$ = list_to_expr($2, 2));
			 $$->LINE_NUM = $1.line;
			 drop_list($2);
			}

		| LPAREN_TOK RPAREN_TOK
			{bump_expr($$ = same_e(hermit_expr, $1.line));
			 pop_assume_finger_tm();
			}

		| LBRACE_TOK expr optElseExpr RBRACE_TOK
			{EXPR *tst, *ex;
			 ex = $3;
			 tst = skip_sames($2);
			 if(skip_sames(ex) == hermit_expr) ex = NULL;
			 if(EKIND(tst) == IDENTIFIER_E && 
			    strcmp(tst->STR, std_id[FALSE_ID]) == 0) {
			      if(ex == NULL) {
				ex = id_expr(std_id[TESTX_ID], $1.line);
			      }
			      bump_expr($$ = apply_expr(
			          id_expr(std_id[FAIL_ID], $1.line), 
					  ex, $1.line));
			 }
			 else {
			   SET_EXPR2_P($$, TEST_E, $2, ex, $1.line);
			 }
			 drop_expr($2);
			 drop_expr($3);
			}

		| expr COLON_EQUAL_TOK expr

			/*----------------------------------------------*
			 * a := b becomes 				*
			 *						*
			 *    open(destination =>> a, source =>> b)	*
			 *----------------------------------------------*/
			 
			{bump_expr($$ = 
			   new_expr2(PAIR_E, $1, $3, $1->LINE_NUM));
			 $$->SCOPE = 1;  /* open pair */
			 bump_role($$->role = colon_equal_role); 
			 drop_expr($1);
			 drop_expr($3);
			}

		| LBOX_TOK RBOX_TOK
			{int id_index;
			 if($1.attr != $2.attr) {
			   syntax_error(MISMATCHED_BRACES_ERR, $1.line);
			 }
			 id_index = ($1.attr == NONSHARED_ATT) 
                                       ? EMPTYNONSHAREDBOX_ID :
				    ($1.attr == SHARED_ATT) 
                                       ? EMPTYSHAREDBOX_ID
				       : EMPTYBOX_ID;
			 bump_expr($$ = apply_expr(
					     id_expr(std_id[id_index], 
						     $1.line),
					     same_e(hermit_expr, $1.line), 
					     $1.line));
			 bump_type($$->ty = fam_mem_t(box_fam, NULL));
			}

		| LBOX_TOK expr RBOX_TOK
			{if($1.attr != $3.attr) {
			   syntax_error(MISMATCHED_BRACES_ERR, $1.line);
			 }
			 bump_expr($$ = make_box_expr($2, $1.attr));
			 drop_expr($2);
			}

		| LROLE_SELECT_TOK roleSelectBody RROLE_SELECT_TOK
			{bump_expr($$ = role_select_expr($2, $1.line));
			 drop_list($2);
			}

		| expr LROLE_MODIFY_TOK roleSelectBody RROLE_SELECT_TOK
			{bump_expr($$ = 
			    role_modify_expr($1, $3, $1->LINE_NUM));
			 drop_expr($1);
			 drop_list($3);
			}


/*----------------------------*
 * JUXTAPOSITION, APPLICATION *
 *----------------------------*/

		| expr expr	%prec JUXTAPOSITION
			{SET_EXPR2_P($$, APPLY_E, $1, $2, $1->LINE_NUM);
			 drop_expr($1);
			 drop_expr($2);
			}

		| expr BAR_TOK expr
			{SET_EXPR2_P($$, APPLY_E, $3, $1, $1->LINE_NUM);
			 drop_expr($1);
			 drop_expr($3);
			}

		| taggedProcId exprList end
			{EXPR* id = skip_sames($1);
			 struct lstr b;
			 b.name   = id->STR;
			 b.line   = id->LINE_NUM;
			 b.column = id->OFFSET;
			 id->OFFSET = 0;
			 id->extra  = 0;
			 check_end_p(&b, PROC_ID_TOK, &($3));

			 bump_expr($$ = build_proc_apply($1, $2));
			 drop_expr($1);
			 drop_list($2);
			}

		| taggedProcId end
			{EXPR* id = skip_sames($1);
			 struct lstr b;
			 b.name   = id->STR;
			 b.line   = id->LINE_NUM;
			 b.column = id->OFFSET;
			 check_end_p(&b, PROC_ID_TOK, &($2));

			 /*-------------------------------------------------*
			  * If this proc id is explicit, then we have 	    *
			  *						    *
			  *    Proc.					    *
			  *						    *
 		          * which should be replaced by 		    *
			  *						    *
			  *    Proc().					    *
			  *						    *
			  * If, on the other hand, this proc was placed by  *
			  * a semicolon, then this expression is just ().   *
			  *-------------------------------------------------*/

			 {EXPR* herm = same_e(hermit_expr, $1->LINE_NUM);
			  $$ = (id->extra) 
			           ? herm
			           : apply_expr($1, herm, $1->LINE_NUM);
			  bump_expr($$);
			 }

			 id->OFFSET = 0;
			 id->extra  = 0;
			 drop_expr($1);
			}

		| askExpr

		| LWRAP_TOK expr RWRAP_TOK
			{EXPR *wrap;
			 check_braces($1.attr, $3.attr);
			 wrap = global_sym_expr(
					$3.attr ? TRYWRAP_ID : WRAP_ID, 
				        $1.line);
			 bump_expr($$ = apply_expr(wrap, $2, $1.line));
			 drop_expr($2);
			}

/*-----------*
 * OPERATORS *
 *-----------*/

                | taggedUnaryOp expr		%prec UNARY_OP
			{EXPR *id;
			 bump_expr($$ = apply_expr($1, $2, $1->LINE_NUM));
			 id = skip_sames($1);
			 if(is_bang(id)) {
			   $$->pat = id->pat = 1;
			 }
			 drop_expr($2);
			 drop_expr($1);
			}

		| expr L1_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr L2_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr L3_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr L4_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr L5_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr L6_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr L7_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr L8_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr L9_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr R1_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr R2_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr R3_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr R4_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr R5_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr R6_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr R7_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr R8_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr R9_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr COMPARE_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr BAR_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr COLON_EQUAL_OP optTag expr
			{DO_OPERATOR($$, $1, $2, $3, $4);}

		| expr IMPLIES_TOK expr

  			{SET_EXPR2_P($$, LAZY_BOOL_E, $1, $3, $1->LINE_NUM);
			 $$->LAZY_BOOL_FORM = IMPLIES_BOOL;
			 drop_expr($1);
			 drop_expr($3);
			}
			
		| expr OR_TOK  expr
  			{SET_EXPR2_P($$, LAZY_BOOL_E, $1, $3, $1->LINE_NUM);
			 $$->LAZY_BOOL_FORM = OR_BOOL;
			 drop_expr($1);
			 drop_expr($3);
			}

		| expr AND_TOK  expr
  			{SET_EXPR2_P($$, LAZY_BOOL_E, $1, $3, $1->LINE_NUM);
			 $$->LAZY_BOOL_FORM = AND_BOOL;
			 drop_expr($1);
			 drop_expr($3);
			}

		| expr BUT_IF_FAIL_TOK expr
			{int line  = $1->LINE_NUM;
			 EXPR* id  = id_expr(new_temp_var_name(), line);
			 EXPR* let = new_expr2(LET_E, id, $1, line);
			 bump_expr($$ = try_expr(let, id, $3, 
						 TRYEACH_F, line));
			 drop_expr($1);
			 drop_expr($3);
			}

		| expr WHERE_TOK expr
			{bump_expr($$ = 
				   new_expr2(WHERE_E, $1, $3, $1->LINE_NUM));
			 drop_expr($1);
			 drop_expr($3);
			}

/*-------------------*
 * TARGET, EXCEPTION *
 *-------------------*/

		| TARGET_TOK
			{bump_expr($$ = typed_target(NULL_T, dcl_pat_fun, 
						     $1.line));
			 if(!in_context_dcl && dcl_pat_fun == NULL_E) {
			   syntax_error(TARGET_ERR, 0);
			 }
			}

		| EXCEPTION_TOK
			{check_case($1, LC_ATT);
			 pop_shadow();
			 bump_expr($$ =
				new_expr1(SPECIAL_E, NULL_E, $1.line));
			 $$->PRIMITIVE = PRIM_EXCEPTION;
			}

/*-----------*
 * TAG, ROLE *
 *-----------*/

		| expr COLON_TOK typeExpr
			{bump_expr($$ = attach_tag_p($1, $3));
			 drop_expr($1);
			 drop_rtype($3);
			}

		| allButProcId ROLE_MARK_TOK expr
			{bump_expr($$ = same_e($3, $3->LINE_NUM));
			 bump_role($$->role = 
				   basic_role(name_tail($1.name)));
			 drop_expr($3);
			}

		| PROC_ID_TOK ROLE_MARK_TOK expr
			{bump_expr($$ = same_e($3, $3->LINE_NUM));
			 bump_role($$->role = 
				   basic_role(name_tail($1.name)));
			 drop_expr($3);
			 pop_shadow();
			}

/*-------------------*
 * OTHER EXPRESSIONS *
 *-------------------*/

		| SPECIES_AS_VAL_TOK typeExpr
			{bump_expr($$ = 
			  build_species_as_val_expr($2.type, $1.line));
			}

		| singleExpr
		| matchExpr
		| letExpr
		| bringExpr
		| varExpr
		| teamExpr
		| ifExpr
		| chooseExpr
		| openExpr
		| contextExpr
		| loopExpr
		| forExpr
		| streamExpr
		| functionExpr
		| awaitExpr
		| listExpr 
		| trapExpr
		| assumeDcl
		    {bump_expr($$ = wsame_e(hermit_expr, $1.line));}
		;

exprOrTaggedProcId : expr
			{$$ = $1;}

		   | taggedProcId
			{EXPR* id = skip_sames($1);
			 $$ = $1;
			 pop_shadow();
			 id->OFFSET = 0;
			 id->extra  = 0;
			}
		   ;

exprList	: expr 
			{bump_list($$ = expr_cons($1, NIL));
			 drop_expr($1);
			}

		| expr COMMA_TOK exprList
			{bump_list($$ = expr_cons($1, $3));
			 drop_expr($1);
			 drop_list($3);
			}
		;


optExpr		: /* empty */
			{bump_expr($$ = 
			   same_e(hermit_expr, current_line_number));
		        }

		| expr	
			{$$ = $1;}
		;

optExprList	: /* empty */	{$$ = NIL;}
		| exprList	{$$ = $1;}
		;

roleSelectBody	: PUEntId ROLE_MARK_TOK expr
			{bump_list($$ = str_cons($1, expr_cons($3, NIL)));
			 drop_expr($3);
			}

		| roleSelectBody COMMA_TOK PUEntId ROLE_MARK_TOK expr
			{bump_list($$ = str_cons($3, expr_cons($5, $1)));
			 drop_list($1);
			 drop_expr($5);
			}
		;


/****************************************************************
 *			ASK EXPRESSIONS				*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *   askExpr	Ask x => f %Ask					*
 *								*
 * Ask x => f %Ask abbreviates f(x).				*
 * Ask x => f y %Ask abbreviates f x y.				*
 *								*
 * An expression of the form					*
 *								*
 *  Ask T x => f y %Ask						*
 *								*
 * is also supported, where T is a genus, species, etc. name.   *
 * It abbreviates the same thing as if T were not present, but  *
 * adds a SAME_E node to the top indicating that T was present. *
 * This is used in Let/Define declarations and expressions.	*
 ****************************************************************/

askExpr		: ASK_TOK optClassId expr DBL_ARROW_TOK expr end
			{EXPR *result;
			 check_end_p(&($1), ASK_TOK, &($6));
			 if($2 == NULL) {
			   result = make_ask_expr($3, $5, $1.line);
			 }
			 else {
			   int   line = $3->LINE_NUM;
			   EXPR* this = new_pat_var(HIDE_CHAR_STR "this", 
						    line);
			   EXPR* also = id_expr(std_id[ALSO_ID], line);
			   EXPR* arg  = new_expr2(PAIR_E, this, $3, line);
			   EXPR* pat  = apply_expr(also, arg, line);

			   result = same_e(make_ask_expr(pat, $5, $1.line), 
					   line);
			   result->SAME_MODE = 7;
			   result->STR       = id_tb0($2);
			   bump_expr(result->E3 = this);
  			 }
		         bump_expr($$ = result);
			 drop_expr($3);
			 drop_expr($5);
			}
		;


/****************************************************************
 *		ATOMIC, CUTHERE, UNIQUE EXPRESSIONS		*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  singleExpr	Atomic e %Atomic				*
 *		CutHere e %CutHere				*
 *		Unique e %Unique				*
 ****************************************************************/

singleExpr	: ATOMIC_TOK expr end
			{check_end_p(&($1), ATOMIC_TOK, &($3));
			 SET_EXPR1_P($$, SINGLE_E, $2, $1.line);
			 $$->SINGLE_MODE = $1.attr;
			 pop_local_assumes_and_finger_tm();
			 drop_expr($2);
			}
		;


/****************************************************************
 *			AWAIT EXPRESSIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  awaitExpr	Await[&] [x,y,z] then a %Await			*
 *								*
 *  optAmpersand: optional &					*
 ****************************************************************/

awaitExpr	: AWAIT_TOK optAmpersand optExprList THEN_TOK expr end
			{check_end_p(&($1), AWAIT_TOK, &($6));
			 bump_expr($$ = make_await_expr($5, $3, $2, $1.line));
			 pop_local_assumes_and_finger_tm();
			 drop_list($3);
			 drop_expr($5);
			}

		| LLAZY_TOK optAmpersand exprList RLAZY_TOK
			{bump_expr($$ = 
			   make_await_expr(list_to_expr($3, $2 + 4), 
					   NIL, $2, $1.line));
			 $$->extra = 1;
			 drop_list($3);
			}
		;

optAmpersand	: /* empty */
			{$$ = 0;}

		| AMPERSAND_TOK
			{$$ = 1;}
		;


/****************************************************************
 *			IF EXPRESSIONS				*
 *			TRY EXPRESSIONS				*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  ifExpr		If a [then b] [else c] %If		*
 *			Try a [then b] [else c] %Try		*
 *								*
 *  optThenExpr		[then b]				*
 *								*
 *  optElseExpr		[else c]				*
 *								*
 *  tryMode		Optional catchingEachThread,		*
 *			catchingAllExceptions.			*
 *			The attribute is TRY_F, TRYTERM_F,	*
 *			TRYEACH_F or TRYEACHTERM_F, indicating	*
 *			the mode.				*
 ****************************************************************/

ifExpr		: IF_TOK tryMode expr optThenExpr optElseExpr end
			{check_end_p(&($1), IF_TOK, &($6));
			 if($1.attr == IF_ATT) {
			   if($2 != TRY_F) {
			     semantic_error(MODE_WITH_IF_ERR, $1.line);
			   }
			   SET_EXPR3_P($$, IF_E, $3, $4, $5, $1.line);
			 }
			 else {
			   bump_expr($$ = try_expr($3, $4, $5, $2, $1.line));
			 }
			 pop_local_assumes_and_finger_tm();
			 drop_expr($3);
			 drop_expr($4);
			 drop_expr($5);
			}
		;

optThenExpr	: /* empty */
			{bump_expr($$ = 
			   same_e(hermit_expr, current_line_number));
			}

		| THEN_TOK expr
			{$$ = $2;}
		;

optElseExpr	: /* empty */
			{bump_expr($$ = 
			   same_e(hermit_expr, current_line_number));
			}

		| ELSE_TOK expr
			{$$ = $2;}
		;

tryMode		: /* empty */
			{$$ = 2;}

		| tryMode CATCHING_TOK
			{int oldmode = $1;
			 int newmode = $2;
			 if(((oldmode == TRYTERM_F || oldmode == TRYEACHTERM_F)
			     && newmode == 1) ||
			    (oldmode >= TRYEACH_F && newmode == 2)) {
			   syntax_error(DUPLICATE_TRY_MODE_ERR, 0);
			   $$ = $1;
			 }
			 else $$ = oldmode + newmode;
			}
		;


/****************************************************************
 *			TRAP EXPRESSIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  trapExpr	Trap x,y,z => a %Trap				*
 *		Untrap x,y,z => a %Untrap			*
 *		Trap all => a %Trap				*
 *		Untrap all => a %Untrap				*
 ****************************************************************/

trapExpr	: TRAP_TOK exprList DBL_ARROW_TOK expr end
			{EXPR *e;
			 EXPR_LIST *l;
			 check_end_p(&($1), TRAP_TOK, &($5));
			 e = $4;
			 for(l = $2; l != NIL; l = l->tail) {
			   e = new_expr2(TRAP_E, l->head.expr, e, $1.line);
			   e->TRAP_FORM = $1.attr;
			 }
			 bump_expr($$ = e);
			 pop_local_assumes_and_finger_tm();
			 drop_list($2);
			 drop_expr($4);
			}			 

		| TRAP_TOK WHICH_TOK 
			{if($2 != ALL_ATT) {
			   syntax_error1(WORD_EXPECTED_ERR, "all", 0);}
			 }
		  DBL_ARROW_TOK expr end
			{check_end_p(&($1), TRAP_TOK, &($6));
			 bump_expr($$ = new_expr2(TRAP_E, NULL_E, $5,$1.line));
			 $$->TRAP_FORM = $1.attr;
			 pop_local_assumes_and_finger_tm();
			 drop_expr($5);
			}			 

		| TRAP_TOK exprList end
			{syntax_error(BAD_TRAP_ERR, 0);
			 bump_expr($$ = bad_expr);
			}

		| TRAP_TOK WHICH_TOK end
			{syntax_error(BAD_TRAP_ERR, 0);
			 bump_expr($$ = bad_expr);
			}

		;


/****************************************************************
 *			STREAM, MIX EXPRESSIONS			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  stream_st		This is a stack.  Its top is either	*
 *			STREAM_ATT or MIX_ATT, telling what	*
 *			kind of expression is being processed	*
 *			(a stream or mix expression.)		*
 *								*
 * Nonterminals:						*
 *								*
 *  streamExpr		Stream a then b then c ... %Stream	*
 *			Mix a with b with c ... %Mix		*
 *								*
 *  streamParts		The contents of the stream or mix expr. *
 ****************************************************************/

streamExpr	: STREAM_TOK 
			{push_int(stream_st, $1.attr);}
		  streamParts end
			{check_end_p(&($1), STREAM_TOK, &($4));
			 $$ = $3;
			 $$->LINE_NUM = $1.line;

			 /*-----------------------------------------*
			  * The top level stream node is explicitly *
			  * built. 				    *
			  *-----------------------------------------*/

			 $$->mark = 0;
			 pop(&stream_st);
			 pop_local_assumes_and_finger_tm();
			}
		;

streamParts	: expr
			{$$ = $1;}

		| expr thenWith streamParts
			{SET_EXPR2_P($$, STREAM_E, $1, $3, $1->LINE_NUM);

			 /*-------------------------------------------*
			  * Indicate whether this is a stream or mix. *
			  *-------------------------------------------*/

			 $$->STREAM_MODE = top_i(stream_st);
	
			 /*------------------------------------------------*
			  * Indicate that this stream is implicitly built. *
			  * This will be set back to 0 for the top level,  *
			  * explicit stream after the streamParts are	   *
			  * built, above.				   *
			  *------------------------------------------------*/

			 $$->mark = 3;

			 drop_expr($1);
			 drop_expr($3);
			}
		;

thenWith	: THEN_TOK
			{if(top_i(stream_st) != STREAM_ATT) {
			   syntax_error1(WORD_EXPECTED_ERR, "with", 0);
			 }
			}

		| WITH_TOK
			{if(top_i(stream_st) == STREAM_ATT) {
			   syntax_error1(WORD_EXPECTED_ERR, "then", 0);
			 }
			}
		;


/****************************************************************
 *			OPEN EXPRESSIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *   openExpr		open If ... %If, etc.			*
 ****************************************************************/

openExpr	: OPEN_TOK singleExpr
			{$$ = $2;
			 $$->SCOPE = 1;
			}

		| OPEN_TOK trapExpr
			{$$ = $2;
			 $$->SCOPE = 1;
			}

		| OPEN_TOK ifExpr
  			{bump_expr($$ = open_if($2));
			 drop_expr($2);
			}

		| OPEN_TOK chooseExpr
			{bump_expr($$ = open_choose(skip_sames($2)));
			 drop_expr($2);
			}

		| OPEN_TOK loopExpr
			{bump_expr($$ = open_loop(skip_sames($2)));
			 drop_expr($2);
			}

		| OPEN_TOK streamExpr
			{bump_expr($$ = open_stream($2));
			 drop_expr($2);
			}

		| OPEN_TOK matchExpr
			{$$ = $2;
			 $$->SCOPE = 1;
			}

		| OPEN_TOK letExpr
			{$$ = $2;
			 if(EKIND(skip_sames($$)) == DEFINE_E) {
			   syntax_error(OPEN_DEFINE_ERR, $$->LINE_NUM);
			 }
			 else $$->SCOPE = 1;
			}
	
		| OPEN_TOK taggedProcId expr end
			{struct lstr b;
			 EXPR* id = skip_sames($2);
			 b.name   = id->STR;
			 b.line   = id->LINE_NUM;
			 b.column = id->OFFSET;
			 check_end_p(&b, PROC_ID_TOK, &($4));
			 id->OFFSET = 0;
			 id->extra  = 0;
			 SET_EXPR2_P($$, APPLY_E, $2, $3, $2->LINE_NUM);
			 drop_expr($2);
			 drop_expr($3);
			}

		| OPEN_TOK askExpr
			{$$ = $2;
			 $$->SCOPE = 0;
			}

		| OPEN_TOK LPAREN_TOK exprList RPAREN_TOK
			{bump_expr($$ = list_to_expr($3, 6));
			 $$->LINE_NUM = $1.line;
			 pop_assume_finger_tm();
			 drop_list($3);
			}
		;


/****************************************************************
 *			CONTEXT EXPRESSIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  contextExpr		Context r a => b %Context		*
 ****************************************************************/

contextExpr	: CONTEXT_TOK id expr DBL_ARROW_TOK expr end
			{check_end_p(&($1), CONTEXT_TOK, &($6));
			 bump_expr($$ = 
			     make_context_expr($2, $3, $5, $1.line, TRUE));
			 drop_expr($3);
			 drop_expr($5);
			 pop_local_assumes_and_finger_tm();
			}
		;


/****************************************************************
 *			LIST EXPRESSIONS			*
 ****************************************************************
 * Contexts:							*
 *								*
 *  list_expr_st	This stack holds, on its top, the kind	*
 *			of the current list expression, coded	*
 *			as follows.				*
 *			    0    [...]				*
 *			    1    indicates that each member x 	*
 *				 of a list expr should be 	*
 *				 replaced by f(x), where f is	*
 *				 the expression on top of	*
 *				 list_map_st.			*
 *								*
 *  list_map_st		See list_expr_st, option 1.		*
 *								*
 * Nonterminals:						*
 *								*
 *  listExpr		An expression of the form [:e:] or	*
 *			[:&e:} or [e1,...,en & r] or		*
 *			mixed(f) [a,b,c].			*
 *								*
 *  angleContents	The contents of [...].			*
 ****************************************************************/

listExpr	: LBRACK_TOK 
			{push_int(list_expr_st, 0);}
		  angleContents RBRACK_TOK
			{$$ = $3;
			 $$->LINE_NUM = $1.line;
			 pop(&list_expr_st);
			}

		| LBRACK_COLON_TOK optAmpersand expr RBRACK_COLON_TOK
			{bump_expr($$ = new_expr1(LAZY_LIST_E, $3, $1.line));
			 $$->OFFSET = $2;
			 drop_expr($3);
			}

		| MIXED_TOK
			{EXPR* f = global_sym_expr(DBL_HAT_SYM, $1.line);
			 push_int(list_expr_st, 1);
			 push_expr(list_map_st, f);
			}
		  LBRACK_TOK angleContents RBRACK_TOK
			{$$ = $4;
			 pop(&list_expr_st);
			 pop(&list_map_st);
			}
		;

angleContents	: /* empty */
		 	{bump_expr($$ = 
			   typed_global_sym_expr(NIL_ID, 
					        copy_type(any_list, 0),
					        current_line_number));
			}

		| angleList
			{$$ = $1;}
		;

angleList	: expr
			{EXPR* e = make_list_mem_expr($1);
			 EXPR* n = typed_global_sym_expr(NIL_ID, 
							copy_type(any_list,0),
							current_line_number);
			 bump_expr($$ = build_cons_p(e, n));
			 drop_expr($1);
			}

		| expr AMPERSAND_TOK expr
			{EXPR* e1 = make_list_mem_expr($1);
			 bump_expr($$ = build_cons_p(e1, $3));
			 drop_expr($1);
			 drop_expr($3);
			}

		| expr COMMA_TOK angleList
			{EXPR *e = make_list_mem_expr($1);
			 bump_expr($$ = build_cons_p(e, $3));
			 drop_expr($1);
			 drop_expr($3);
			}
		;


/****************************************************************
 *			FUNCTION EXPRESSIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  functionExpr	(p => e)				*
 ****************************************************************/

functionExpr	: LPAREN_TOK expr DBL_ARROW_TOK expr RPAREN_TOK
			{bump_expr($$ = 
				new_expr2(FUNCTION_E, $2, $4, $1.line));
			 pop_assume_finger_tm();
			 drop_expr($2);
			 drop_expr($4);
			}
		;


/****************************************************************
 *		 LET, DEFINE, RELET  EXPRESSIONS		*
 ****************************************************************
 * Contexts:							*
 * 								*
 *  this_mode		declaration mode from dclMode		*
 *								*
 *  deflet_st		tells what kind the current let		*
 *			expression is.  Its top is -LET_E	*
 *			or -DEFINE_E.  The sign is negative to  *
 *			indicate an expression; declarations 	*
 *			use positive numbers.			*
 *								*
 *  deflet_line_st	The top of this stack tells the line	*
 *			number where the enclosing let or	*
 *			define declaration starts.		*
 *								*
 *  defining_id		The id being defined, in the case of	*
 *			declarations of the form		*
 *			    Let f by ...			*
 *			(So here, defining_id would be f)	*
 *								*
 *  choose_st		The top of this stack holds information	*
 *			about the kind of choice being made,	*
 *			in a definition by cases.  The fields	*
 *			are					*
 *								*
 *			  choose_from: 				*
 *			    The parameter.  This is target of	*
 *			    the matches.			*
 *								*
 *			  which:				*
 *			    The choice mode.			*
 *								*
 *  			  working_choose_matching_list:		*
 *			    This list collects the patterns	*
 *			    that occur in the separate cases.	*
 *			    It is used for doing exhaustiveness *
 *			    tests of the patterns.		*
 *								*
 * Nonterminals:						*
 *								*
 *  letExpr		Let ... %Let or Define ... %Define	*
 *								*
 *  defLetBody	  	The body of a let or define declaration*
 *			or expression.  This returns a		*
 *			DEFINE_E or LET_E expression.  This is	*
 *			defined under letDcl, above.		*
 ****************************************************************/

letExpr		: LET_TOK 
			{push_int(deflet_st, -let_expr_kind($1.attr));
			 push_int(deflet_line_st, $1.line);
			}
		  defLetBody end
			{check_end_p(&($1), LET_TOK, &($4));
			 $$ = $3;
			 skip_sames($3)->LET_FORM = ($1.attr == RELET_ATT);
			 $$->LINE_NUM = $1.line;
			 pop_local_assumes_and_finger_tm();
			 pop(&deflet_st);
			 pop(&deflet_line_st);
			}

		| LET_TOK end
			{check_end_p(&($1), LET_TOK, &($2));
			 check_empty_block_p(&($1));
			 bump_expr($$ = same_e(hermit_expr, $1.line));
			 pop_local_assumes_and_finger_tm();
			}
		;


/****************************************************************
 *			TEAM EXPRESSIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  teamExpr		Team ... %Team				*
 *								*
 *  teamExprParts	The contents of the team.		*
 ****************************************************************/

teamExpr	: TEAM_TOK teamExprParts end
			{check_end_p(&($1), TEAM_TOK, &($3));
			 bump_expr($$ = same_e($2, $1.line));
			 $$->TEAM_MODE = 1;
			 drop_expr($2);
			}
		; 

teamExprParts	: letExpr
			{EXPR* ssresult = skip_sames($1);

			 /*--------------------------------*
			  * Force the let to be a define. *
			  *--------------------------------*/

			 if(EKIND(ssresult) == LET_E) {

			   /*-----------------------------------*
			    * Complain about a relet in a team. *
			    *-----------------------------------*/

			   if(ssresult->LET_FORM) {
			     syntax_error(RELET_IN_TEAM_ERR, $1->LINE_NUM);
			   }
			   ssresult->kind = DEFINE_E;
			   ssresult->DEFINE_FORM = 1;
			 }
			 $$ = $1;
			}

		| teamExprParts letExpr
			{EXPR *ss2 = skip_sames($2);
			 if(EKIND(ss2) == LET_E) {
			   ss2->kind = DEFINE_E;
			   ss2->DEFINE_FORM = 1;
			 }
			 bump_expr($$ = apply_expr($1, $2, $1->LINE_NUM));
			 drop_expr($1);
			 drop_expr($2);
			}
		;


/****************************************************************
 *			BRING EXPRESSIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  bringExpr		Bring ids:tag [from id].		*
 *			  or					*
 *			Bring ids:tag by id.			*
 *								*
 *  bringBody		The insides of a bringDcl or bringExpr.	*
 *								*
 *  optFrom		[from e]				*
 ****************************************************************/

bringExpr	: BRING_TOK 
			{check_case($1, UC_ATT);
			 dcl_line = $1.line;
			}
		  bringBody end
			{LIST *p;
			 EXPR *result = NULL;
			 check_end_p(&($1), BRING_TOK, &($4));
			 for(p = $3; p != NIL; p = p->tail) {
			   result = possibly_apply(result, p->head.expr, 
						   $1.line);
			 }
			 bump_expr($$ = result);
			 drop_list($3);
			 bring_context = 0;
			}

		| BRING_TOK end
			{check_end_p(&($1), BRING_TOK, &($2));
			 check_empty_block_p(&($1));
			 bump_expr($$ = same_e(hermit_expr, $1.line));
			 bring_context = 0;
			}
		;

bringBody	: /*-----------------------*
		   * Bring x,y,z [from g]. *
		   *-----------------------*/

		  PUEntIdList COLON_TOK typeExpr optFrom
			{MODE_TYPE *mode;
			 mode = in_bring_dcl ? &this_mode : NULL;
			 bump_list($$ = bring_exprs($1, $3, $4, dcl_line, 
						    mode));
			 drop_list($1);
			 drop_rtype($3);
			 drop_expr($4);
			}

		| /*-------------------*
		   * Bring x,y,z by f. *
		   *-------------------*/

		  PUEntIdList optTag BY_TOK taggedId
			{bump_list($$ = bring_by_exprs($1, $2, $4, dcl_line));
			 drop_list($1);
			 drop_rtype($2);
			 drop_expr($4);
			}			
		;

optFrom		: /* empty */
			{$$ = NULL;}

		| FROM_TOK expr
			{$$ = $2;}
		;


/****************************************************************
 *			VAR EXPRESSIONS				*
 ****************************************************************
 * Contexts:							*
 *								*
 *  propagate_var_opt_st The top of this stack is the var mode  *
 *			 to be propagated acros	a semicolon.	*
 *								*
 *  var_initial_val	This variable is set to the initial     *
 *			value of a var declaration or		*
 *			expression, or to NULL if there is	*
 *			no initial value.			*
 *								*
 *  var_content_type 	This holds a variable that is installed *
 *			as the content type of the boxes in	*
 *			a var declaration by make_var_expr_p.	*
 *								*
 *  all_init		This is set to true if the initializer  *
 *			has 'all' before it, and to false	*
 *			otherwise.				*
 *								*
 * Nonterminals:						*
 *								*
 *   varExpr	Var[{mode}] ... %Var				*
 *								*
 *   varBody	the contents of a var expr or declaration.  It	*
 *		sets the context variables above to reflect 	*
 *		what is in the expression.			*
 *								*
 *   varOpts	optional {mode} for a var expr or declaration.	*
 *								*
 *   varList	The list of variables.  It returns a list of    *
 *		descriptions of members.  Each member has the   *
 *		form name::type::bounds, where name is the 	*
 *		name of the variable, type is the tag and	*
 *		bounds is a list of the bounds that follow the	*
 *		name (NIL if none).				*
 *								*
 *   optHolding	An optional holding t phrase.			*
 ****************************************************************/

varExpr		: VAR_TOK 
			{push_int(var_context_st, 0);
			}
		  varBody end
			{check_end_p(&($1), VAR_TOK, &($4));
			 $$ = $3;
			 pop_local_assumes_and_finger_tm();
			 pop(&var_context_st);
			 drop_expr(var_initial_val);
			 drop_type(var_content_type);

			 /*---------------------------------------------*
			  * The following tests the status of the end	*
 			  * (%Var).					*
			  *---------------------------------------------*/
                          
			 if(!recent_begin_from_semicolon) {
			   pop(&propagate_var_opt_st);
			 }
			}

		| VAR_TOK end
			{check_end_p(&($1), VAR_TOK, &($2));
			 check_empty_block_p(&($1));
			 bump_expr($$ = same_e(hermit_expr, $1.line));
			 pop_local_assumes_and_finger_tm();
			}
		;

varBody		: /*---------------------------------------------------*
		   * Var [{mode}] x,y,z [holding T] [[all] := a] %Var  *
		   *---------------------------------------------------*/

		  varOpts varList optHolding optInit
			{bump_type(var_content_type = 
				    ($3.type != NULL) ? $3.type :
				    ($4 != NULL)      ? var_t(NULL)
						      : NULL);
			 bump_expr($$ =
				   make_var_expr_p($2, var_content_type, 
						   $4, all_init,
					           current_line_number, $1));
			 bump_expr(var_initial_val = $4);
			 drop_mode($1);
			 drop_list($2);
			 drop_rtype($3);
			 drop_expr($4);
			}
		;

varList		: varListMem
			{bump_list($$ = list_cons($1, NIL));
			 drop_list($1);
			}

		| varList COMMA_TOK varListMem
			{bump_list($$ = list_cons($3, $1));
			 drop_list($1);
			 drop_list($3);
			}
		;

varListMem	: PUEntId optTag
			{bump_list($$ = 
				   str_cons($1, type_cons($2.type, NIL)));
			}

		| PUEntId optTag LPAREN_TOK exprList RPAREN_TOK
			{bump_list($$ = 
				   str_cons($1, type_cons($2.type, $4))); 
			 pop_assume_finger_tm();
			 drop_list($4);
			}
		;

varOpts		: /* empty */
			{if(recent_begin_from_semicolon) {
			   $$ = copy_mode(top_mode(propagate_var_opt_st));
			 }
			 else {
			   push_int(propagate_var_opt_st, 0);
			   $$ = NULL;
			 }
			}

		| varOpts varOpt optColon
			{if($3) {
			   $$ = $2;
			   drop_mode(propagate_var_opt_st->head.mode);
			   propagate_var_opt_st->head.mode = copy_mode($$);
			   drop_mode($1);
			 }
			 else if($1 == NULL) {
			   $$ = $2;
			 }
			 else {
			   modify_mode($1, $2, TRUE);
			   $$ = $1;
			   drop_mode($2);
			 }
			}
		;

varOpt		: LBRACE_TOK varOptList RBRACE_TOK
			{$$ = $2;
			}
		;

varOptList	: /* empty */
			{$$ = NULL;}

		| someVarOpts
			{$$ = $1;}
		;

someVarOpts	: id
			{$$ = allocate_mode();
			 add_var_mode($$, $1);
			}

		| id LBRACE_TOK idList RBRACE_TOK
			{$$ = allocate_mode();
			 add_var_list_mode($$, $1, $3);
			 drop_list($3);
			}

		| someVarOpts COMMA_TOK id
			{$$ = ($1 == NULL) ?  allocate_mode() : $1;
			 add_var_mode($$, $3);
			}

		| someVarOpts COMMA_TOK id LBRACE_TOK idList RBRACE_TOK
			{$$ = ($1 == NULL) ? allocate_mode() : $1;
			 add_var_list_mode($$, $3, $5);
			 drop_list($5);
			}
		;

optHolding	: /* empty */				%prec BAR_TOK
			{$$.type = NULL_T;
			 $$.role = NULL;
			 if(holding_phrase_required) {
			   syntax_error(HOLDING_REQUIRED_ERR, 0);
			 }
			}

		| HOLDING_TOK typeExpr	
			{$$ = $2;}
		;

optInit		: /* empty */
		 	{$$ = NULL;}

		| COLON_EQUAL_TOK expr
			{$$ = $2;     /* inherits ref count */
			 all_init = FALSE;
			}

		| WHICH_TOK 
			{if($1 != ALL_ATT) {
			   syntax_error1(WORD_EXPECTED_ERR, "all", 0);
			 }
			}
		  COLON_EQUAL_TOK expr
			{$$ = $4;     /* inherits ref count */
			 all_init = TRUE;
			}
		;


/****************************************************************
 *			MATCH EXPRESSIONS			*
 ****************************************************************
 * Nonterminals:						*
 *								*
 *  matchExpr		Match a = b %Match or			*
 *			Extract a from b %Extract		*
 ****************************************************************/

matchExpr	: MATCH_TOK expr EQUAL_TOK expr end
			{check_end_p(&($1), MATCH_TOK, &($5));
			 SET_EXPR2_P($$, MATCH_E, $2, $4, $1.line);
			 drop_expr($2);
			 drop_expr($4);
			 pop_local_assumes_and_finger_tm();
			}

		| MATCH_TOK end
			{check_end_p(&($1), MATCH_TOK, &($2));
			 check_empty_block_p(&($1));
		 	 bump_expr($$ = same_e(hermit_expr, $1.line));
			}

		| EXTRACT_TOK expr FROM_TOK expr end
			{EXPR *extractor, *pat;
			 check_end_p(&($1), EXTRACT_TOK, &($5));
			 check_imported("pattern/listpat.ast", TRUE);
			 extractor = id_expr(std_id[EXTRACTL_ID], $1.line);
			 pat = apply_expr(extractor, $2, $1.line);
			 SET_EXPR2_P($$, MATCH_E, pat, $4, $1.line);
			 drop_expr($2);
			 drop_expr($4);
			 pop_local_assumes_and_finger_tm();
			}

		| EXTRACT_TOK end
			{check_end_p(&($1), EXTRACT_TOK, &($2));
			 check_empty_block_p(&($1));
		 	 bump_expr($$ = same_e(hermit_expr, $1.line));
			}
		;


/************************************************************************
 *			 CHOOSE EXPRESSIONS				*
 ************************************************************************
 * Contexts:								*
 *									*
 *  subcases_context	Normally 0, but set to 1 by the lexer just	*
 * 			after reading the word 'subcases'.  It is used  *
 *			to make 'which' return the choice-mode of the	*
 *			embedding choose or loop when no mode is given	*
 *			after 'subcases'.				*
 *									*
 *  choose_st		The top of this stack describes the choose or 	*
 *			loop expression that we are working on.  When a *
 *			new choose or loop is entered, a new frame is	*
 *			pushed onto this stack.  The following fields	*
 *			are provided.					*
 *									*
 *			  choose_kind:					*
 *  			    This tells what kind of choice construct is *
 *			    being processed.  It can be CHOOSE_TOK (for *
 *			    a choose) or LOOP_TOK (for a loop).		*
 *									*
 *  			  which:					*
 *			    This tells the choice mode of the current	*
 *			    choose or loop construct. The top is one of	*
 *			      FIRST_ATT					*
 *			      ONE_ATT					*
 *			      ALL_ATT					*
 *			      ONE_MIXED_ATT				*
 *			      ALL_MIXED_ATT				*
 *									*
 * 			  match_kind:					*
 *			    This tells whether the current choose or	*
 *			    loop construct has a matching ... phrase.	*
 *			    The top is 					*
 *			      0 for no 'matching' phrase, 		*
 *			      1 if there is a'matching' phrase.		*
 *									*
 * 			  choose_from:					*
 *			    When the choose or loop construct has a 	*
 *			    'matching' phrase, this is the expression	*
 *			    that is the common target of all of the	*
 *			    matches.					*
 *									*
 * 			  working_choose_matching_list:			*
 *			    In a choose-matching construct, this list	*
 *			    collects the patterns that occur in the	*
 *			    separate cases.  It is used for doing	*
 *			    exhaustiveness tests of the patterns.	*
 *									*
 *  case_kind_st							*
 *			    The top of this stack indicates what kind	*
 *			    of case is being processed.  It is one of	*
 *			      NOCASE_ATT  no case is being processed.	*
 *			      CASE_ATT	  case g => ...			*
 *			      UNTIL_ATT	  exitcase g => ...		*
 *			      WHILE_ATT	  loopcase g => ...		*
 *			      ELSE_ATT	  else => ...			*
 *			      UNTILELSE_ATT  exitcase else => ...	*
 *			      WHILEELSE_ATT  loopcase else => ...	*
 *									*
 * Nonterminals:							*
 *									*
 *  chooseExpr		Choose ... %Choose				*
 *									*
 *  cases		List of cases for choose and loop constructs.	*
 *			Returns the translated  expression.		*
 *									*
 *  guards		A list of guards (to the left of =>)		*
 *									*
 *  guard		A single guard					*
 *									*
 *  caseBody		The body part of one case.			*
 *									*
 *  subcases		Subcases ... %Subcases, possibly preceded by    *
 *			do phrases.					*
 *									*
 *  realsubcases	Subcases ... %Subcases				*
 *									*
 *  which		WHICH_TOK or WHICH_TOK MIXED_TOK, but puts 	*
 *			attribute into which_result.  Empty string is	*
 *			treated like 'first', except when doing 	*
 *			a subcases phrase - then the empty string is	*
 *			treated like the same as the embedding context.	*
 *									*
 *  clwhich		which followed by optional MATCHING_TOK expr	*
 *			This puts the target of the matches into	*
 *			choose_from_result when there is a 'matching'	*
 *			phrase.  If it is necessary to put the target	*
 *			into an identifier, then clwhich returns the	*
 *			expression that will do that let.  Otherwise,	*
 *			clwhich returns NULL.  It also sets 		*
 *			match_kind_info to 1 if there is a 'matching'	*
 *			phrase and to 0 if there is none.		*
 *									*
 *  optSucceed		optional 'do', possibly followed by modes	*
 *			catchingEachThread and/or catchingAllExceptions.*
 *			Its value is 0 if there is no do, and is the	*
 *			control	kind (TRY_F, etc.) that should be used	*
 *			when there is a do.				*
 *									*
 *  checkDo		The empty string, but check that a 'do' phrase	*
 *			is being used in an admissible context.		*
 *									*
 * NOTE									*
 *   chooseExpr creates an apply node for a do-phrase.  The fact that	*
 *   ids defined in the do-phrase go out of scope is not visible to the	*
 *   code generator.  If this is changed, then handling of open choose	*
 *   must also change, since open choose assumes this behavior.  	*
 *   (do-phrases are ignored by open.c:open_choose.)			*
 *									*
 * NOTE									*
 *   CLASS_UNION_CELLs are used for holding info on a list of cases. 	*
 *   The fields are:							*
 *     CUC_EXPR:  the guard expression					*
 *     special :  0 for a Boolean guard, 1 for a hermit (try) guard.	*
 *     tok     :  the case kind (CASE_ATT, UNTIL_ATT, ELSE_ATT, 	*
 *                WHILE_ATT)						*
 ************************************************************************/

chooseExpr	: CHOOSE_TOK clwhich
			{CHOOSE_INFO* inf = allocate_choose_info();
			 inf->choose_kind = CHOOSE_TOK;
			 inf->which       = which_result;
			 inf->match_kind  = match_kind_result;
			 inf->choose_from = choose_from_result;  /* inherits 
								    ref */
			 choose_from_result = NULL;
			 push_int(case_kind_st, NOCASE_ATT);
			 push_choose_info(choose_st, inf);
			}
		  cases end
			{check_end_p(&($1), CHOOSE_TOK, &($5));
			 bump_expr($$ = build_choose_p($2, $4, FALSE,$1.line));

			 /*---------------------------------------------*
			  * Pop the stacks that were used to manage	*
			  * the choose expression.			*
			  *---------------------------------------------*/

			 finish_choose();
			 drop_expr($2);
			 drop_expr($4);
			}
		;

which		: WHICH_TOK
			{$$ = which_result = $1;
			 subcases_context = 0;
			}

		| WHICH_TOK MIXED_TOK
			{ATTR_TYPE attr = $1;
			 if(attr == FIRST_ATT || attr == PERHAPS_ATT) {
			   syntax_error(FIRST_MIXED_ERR, 0);
			 }
			 else if(attr == ALL_ATT) attr = ALL_MIXED_ATT;
			 else if(attr == ONE_ATT) attr = ONE_MIXED_ATT;
			 $$ = which_result = attr;
			 subcases_context = 0;
			}

		| /* empty */
			{$$ = which_result = 
			       subcases_context 
				 ? top_choose_info(choose_st)->which 
				 : FIRST_ATT;
			 subcases_context = 0;
			}
		;

clwhich		: which
			{match_kind_result  = 0;
			 choose_from_result = NULL;
			 $$ = NULL;
			}

		| which MATCHING_TOK expr
			{EXPR *v;
			 match_kind_result = 1;
			 bump_expr($$ = force_eval_p($3, &v));
			 bump_expr(choose_from_result = v);
			 drop_expr($3);
			}
		;

cases		: /* empty */
			{bump_expr($$ = make_empty_else_p());
			}

		| DO_TOK checkDo expr cases
			{EXPR* todo = $3;
			 if(todo == NULL) todo = same_e(NULL, $1.line);
			 else if(todo->ty != NULL) {
			   todo = same_e(todo, todo->LINE_NUM);
			 }
			 bump_type(todo->ty = hermit_type);
			 check_for_case_after_else_p($1.line);
			 bump_expr($$ = apply_expr(todo, $4, $1.line));
			 drop_expr($3);
			 drop_expr($4);
			}

		| DO_TOK checkDo expr DBL_ARROW_TOK expr
			{check_for_case_after_else_p($1.line);
			 if(!top_choose_info(choose_st)->match_kind) {
			   syntax_error(DO_DBL_ARROW_NOT_MATCH_ERR, $1.line);
			 }
			}
		  cases	
			{bump_expr($$ = 
			   make_do_dblarrow_case_p($3, $5, $7, $1.line));
			 drop_expr($3);
			 drop_expr($5);
			 drop_expr($7);
			}

		| guards DBL_ARROW_TOK caseBody optWhen cases
			{CLASS_UNION_CELL *p;
			 bump_expr($$ = 
			   make_cases_p($1, 0, $3, $4, $5, $1->line));
			 drop_expr($3);
			 drop_expr($5);
			 for(p = $1; p != NULL; p = p->u.next) {
			   pop(&case_kind_st);
			 }
			 free_cucs_fun($1);
			}

		| whileCase
		;

guards		: guard
			{$$ = $1;}

		| guard guards
			{$1->u.next = $2;
			 $$ = $1;
			}
		;

guard		: CASE_TOK optSucceed
			{check_for_case_after_else_p($1.line);
			 if($1.attr == UNTIL_ATT) {
			   check_for_bad_loop_case_p($1.line);
			 }
			 push_int(case_kind_st, $1.attr);
			 if(top_choose_info(choose_st)->match_kind && $2) {
			   syntax_error(SUCCEED_IGNORED_ERR, 0);
			 }
			}
		  expr
			{LIST **wpl_head;
			 CHOOSE_INFO* inf = top_choose_info(choose_st);
			 $$ = case_cuc_p($1.attr,$2,$4, $4->LINE_NUM);
			 if(inf->match_kind) {
			   wpl_head = &(inf->working_choose_matching_list);
			   set_list(wpl_head, expr_cons($4, *wpl_head));
			 }
			 drop_expr($4);			
			}

		| CASE_TOK ELSE_TOK 
			{int else_att;
			 CHOOSE_INFO* inf = top_choose_info(choose_st);
			 check_else_p();
			 if($1.attr == UNTIL_ATT) {
			   check_for_bad_loop_case_p($1.line);
			   else_att = UNTIL_ELSE_ATT;
			 }
			 else else_att = ELSE_ATT;

			 /*----------------------------------------*
			  * Clear working pat lists to prevent any *
			  * check from being done.		   *
			  *----------------------------------------*/

			 if(inf->match_kind) {
			   SET_LIST(inf->working_choose_matching_list, NIL); 
			 }
			 push_int(case_kind_st, else_att);

			 $$ = case_cuc_p(else_att,0,NULL,current_line_number);
			}

		| ELSE_TOK 
			{CHOOSE_INFO* inf = top_choose_info(choose_st);
 			 check_else_p();

			 /*----------------------------------------*
			  * Clear working pat lists to prevent any *
			  * check from being done. 		   *
			  *----------------------------------------*/

			 if(inf->match_kind) {
			   SET_LIST(inf->working_choose_matching_list, NIL); 
			 }

			 push_int(case_kind_st, ELSE_ATT);
			 $$ = case_cuc_p(ELSE_ATT,0,NULL,current_line_number);
			}
		;

caseBody	: expr
			{int top_case = top_i(case_kind_st);
			 $$ = $1;
			 if((top_case == CASE_ATT || top_case == ELSE_ATT) && 
			    top_choose_info(choose_st)->choose_kind 
			    == LOOP_TOK) {
			   syntax_error(SUBCASES_EXPECTED_ERR, $1->LINE_NUM);
			 }
			}

		| subcases
			{$$ = $1;}
		;

subcases	: realsubcases
			{$$ = $1;}

		| DO_TOK expr subcases
			{EXPR* todo = $2;
			 if(todo == NULL || todo->ty != NULL) {
			   todo = same_e(todo, $1.line);
			 }
			 bump_type(todo->ty = hermit_type);
			 bump_expr($$ = apply_expr(todo, $3, $1.line));
			 drop_expr($2);
			 drop_expr($3);
			}
		;

realsubcases	: SUBCASES_TOK clwhich
			{int top_case = top_i(case_kind_st);
			 CHOOSE_INFO* embed_inf = top_choose_info(choose_st);
			 CHOOSE_INFO* inf = allocate_choose_info();
			 if(top_case == UNTIL_ATT 
			   || top_case == UNTIL_ELSE_ATT) {
			   syntax_error(SUBCASES_NOT_ALLOWED_ERR, 0);
			 }
			 inf->choose_kind = embed_inf->choose_kind;
			 if(embed_inf->choose_kind == LOOP_TOK) {
			   bump_expr(inf->loop_ref = embed_inf->loop_ref);
			 }
			 inf->which       = which_result;
			 inf->match_kind  = match_kind_result;
			 inf->choose_from = choose_from_result;  /* inherits
								    ref */
			 choose_from_result = NULL;
			 push_choose_info(choose_st, inf);
			 push_int(case_kind_st, NOCASE_ATT);
			}
		  cases end
			{check_end_p(&($1), SUBCASES_TOK, &($5));
			 check_for_cases($4, $1.line);
			 $4->mark = 2;
			 bump_expr($$ = build_choose_p($2, $4, TRUE, $1.line));
			 finish_choose();
			 drop_expr($2);
			 drop_expr($4);
			}
		;

optSucceed	: /* empty */
			{$$ = 0;}

		| DO_TOK tryMode
			{$$ = $2;}
		;


checkDo		: /* empty */
			{CHOOSE_INFO* inf = top_choose_info(choose_st);
		 	 int w = inf->which;
			 if((w == ONE_ATT || w == ONE_MIXED_ATT)
			    && top_i(case_kind_st) != NOCASE_ATT) {
			   syntax_error(ONE_DO_ERR, 0);
			 }
			}
		;


/****************************************************************
 *			LOOP EXPRESSIONS			*
 ****************************************************************
 * Contexts:  See choose expressions.				*
 *								*
 *  choose_st	In addition to the information described under  *
 *		choose expressions, the top of this stack holds *
 *								*
 *                loop_ref:					*
 *	 	    The current	loop expression.  This is used	*
 *		    to allow continue expressions to build 	*
 *		    something that refers back to the loop in	*
 *		    which it occurs.				*
 *								*
 * Nonterminals:						*
 *								*
 *  loopExpr	Loop ... %Loop or 				*
 *		While ... %While				*
 *								*
 *  whileCase	A 'loopcase' case.  The case translation is	*
 *		returned.					*
 *								*
 *  whileGuard	A guard for a loopcase.				*
 *								*
 *  whileGuards	A sequence of while guards -- what can be on	*
 *   		the left-hand side of => in a loopcase.		*
 *								*
 *  continue	A continue phrase.  A recur expression is	*
 *		returned.					*
 ****************************************************************/

loopExpr	: LOOP_TOK expr EQUAL_TOK expr clwhich
			{CHOOSE_INFO* inf = allocate_choose_info();
			 inf->choose_kind = LOOP_TOK;
			 inf->which	  = which_result;
			 inf->match_kind  = match_kind_result;
			 inf->choose_from = choose_from_result;  /* inherits
								    ref */
			 choose_from_result = NULL;
			 bump_expr(inf->loop_ref = 
				   new_expr1(LOOP_E, 
				   	   new_expr2(MATCH_E, $2, $4, $1.line),
					   $1.line));
			 push_choose_info(choose_st, inf);
			 push_int(case_kind_st, NOCASE_ATT);
			}
		  cases end
			{bump_expr($$ = handle_loop_p($1, $5, $7, $8));

			 /*-----------------------------------------*
			  * Note: handle_loop_p pops choose_st and  *
			  * local assumes.			    *
			  *-----------------------------------------*/

			 drop_expr($2);
			 drop_expr($4);
			 drop_expr($5);
			 drop_expr($7);
			}

		| LOOP_TOK clwhich
			{CHOOSE_INFO* inf = allocate_choose_info();
			 inf->choose_kind = LOOP_TOK;
			 inf->which	  = which_result;
			 inf->match_kind  = match_kind_result;
			 inf->choose_from = choose_from_result;  /* inherits
								    ref */
			 choose_from_result = NULL;
			 bump_expr(inf->loop_ref = 
				   new_expr1(LOOP_E, NULL_E, $1.line));
			 push_choose_info(choose_st, inf);
			 push_int(case_kind_st, NOCASE_ATT);
			}
		  cases end
			{bump_expr($$ = handle_loop_p($1,$2,$4,$5));

			 /*---------------------------------------------*
			  * Note: handle_loop_p pops choose_st and	*
			  * case_kind_st.				*
			  *---------------------------------------------*/

			 drop_expr($2);
			 drop_expr($4);
			}

		| WHILE_TOK expr DO_TOK expr end
			{EXPR *else_case, *continue_expr, *body_case;
			 CLASS_UNION_CELL* guard;
			 CHOOSE_INFO* inf = allocate_choose_info();
			 inf->choose_kind = LOOP_TOK;
			 inf->which       = PERHAPS_ATT;
			 inf->match_kind  = 0;
			 bump_expr(inf->loop_ref = 
				   new_expr1(LOOP_E, NULL_E, $1.line));
			 push_choose_info(choose_st, inf);
			 push_int(case_kind_st, WHILE_ATT);

			 else_case = make_else_expr_p(PERHAPS_ATT, 
						      NO_CASE_EX,
						      $4->LINE_NUM);
			 continue_expr = short_continue_p();
			 continue_expr->LINE_NUM = $4->LINE_NUM;
			 guard = case_cuc_p(WHILE_ATT, 0, $2, $2->LINE_NUM);
			 body_case = make_while_cases_p(guard, 0, $4, 
						       continue_expr, NULL, 
						       else_case,$2->LINE_NUM);

			 bump_expr($$ = handle_loop_p($1, NULL, body_case,$5));
			 drop_expr($2);
			 drop_expr($4);
			 free_cucs_fun(guard);
			}
		;

whileGuard	: LOOPCASE_TOK optSucceed 
			{check_for_case_after_else_p($1.line);
			 check_for_bad_loop_case_p($1.line);
			 push_int(case_kind_st, WHILE_ATT);
			 if(top_choose_info(choose_st)->match_kind && $2) {
			   syntax_error(SUCCEED_IGNORED_ERR, 0);
			 }
			}
		  expr 
			{LIST **wpl_head;
			 CHOOSE_INFO* inf = top_choose_info(choose_st);
			 $$ = case_cuc_p(WHILE_ATT, $2, $4, $4->LINE_NUM);
			 if(inf->match_kind) {
			   wpl_head = &(inf->working_choose_matching_list);
			   set_list(wpl_head, expr_cons($4, *wpl_head));
			 }
			 drop_expr($4);
		        }

		| LOOPCASE_TOK ELSE_TOK
			{CHOOSE_INFO* inf = top_choose_info(choose_st);
			 check_for_case_after_else_p($1.line);
			 check_for_bad_loop_case_p($1.line);
			 check_else_p();
			 push_int(case_kind_st, WHILE_ELSE_ATT);

			 /*----------------------------------------*
			  * Clear working pat lists to prevent any *
			  * check from being done. 		   *
			  *----------------------------------------*/

			 if(inf->match_kind) {
			   SET_LIST(inf->working_choose_matching_list, NIL); 
			 }
			 $$ = case_cuc_p(WHILE_ELSE_ATT, 0, NULL, 
					 current_line_number);
			}
		;

whileGuards	: whileGuard
			{$$ = $1;}

		| whileGuard whileGuards
			{$1->u.next = $2;
			 $$ = $1;
			}
		;

whileCase	: whileGuards DBL_ARROW_TOK optExpr continue optWhen cases
			{CLASS_UNION_CELL *p;
			 bump_expr($$ = make_while_cases_p($1, 0, $3, $4, 
							   $5, $6, $1->line));
			 drop_expr($3);
			 drop_expr($4);
			 drop_expr($5);
			 drop_expr($6);
			 for(p = $1; p != NULL; p = p->u.next) {
			   pop(&case_kind_st);
			 }
			 free_cucs_fun($1);
			}
		;


continue 	: /* empty */
			{bump_expr($$ = short_continue_p());}

		| CONTINUE_TOK
			{bump_expr($$ = short_continue_p());
			 $$->LINE_NUM = $1.line;
			}

		| CONTINUE_TOK expr
			{EXPR  *loop;
			 loop = top_choose_info(choose_st)->loop_ref;
			 if(loop->E1 == NULL_E) {
			   syntax_error(FULL_CONT_ERR, 0);
			 }
			 SET_EXPR1_P($$, RECUR_E, $2, $1.line);
			 $$->E2  = loop;  /* Not ref counted -- cyclic link */
			 $$->pat = $2->pat;
			 drop_expr($2);
			}
		;


/********************************************************************
 *                   FOR LOOP EXPRESSIONS                           *
 ********************************************************************
 * Nonterminals:						    *
 *  								    *
 *  forExpr	For x from l do e %For				    *
 *								    *
 * Actually, the for-loop below is more general than advertised,    *
 * but is being worked on.					    *
 ********************************************************************/

forExpr		: FOR_TOK iterators DO_TOK forBodyExprs end
			{check_end_p(&($1), FOR_TOK, &($5));
			 bump_expr($$ = make_for_expr_p($2, $4));
			 pop_local_assumes_and_finger_tm();
			 drop_list($2);
			 drop_list($4);
			}
		;

iterators	: expr FROM_TOK expr
			{bump_list($$ = 
			  expr_cons(new_expr2(PAIR_E, $1, $3, $1->LINE_NUM), 
				    NIL));
			 drop_expr($1);
			 drop_expr($3);
			}

		| iterators SEMICOLON_TOK expr FROM_TOK expr
			{bump_list($$ = 
			  expr_cons(new_expr2(PAIR_E, $3, $5, $3->LINE_NUM), 
			           $1));
			 drop_list($1);
			 drop_expr($3);
			 drop_expr($5);
			}
		;

forBodyExprs	: expr
			{bump_list($$ = expr_cons($1, NIL));
			 drop_expr($1);
			}

		| forBodyExprs THEN_TOK expr
			{bump_list($$ = expr_cons($3, $1));
			 drop_list($1);
			 drop_expr($3);
			}
		;


/************************************************************************
 *                   IDENTIFIERS AND MISC                           	*
 ************************************************************************
 *									*
 *	id		: Any identifier.				*
 *									*
 *      oper		: Any binary or unary operator			*
 *									*
 *	binOp		: Any binary operator				*
 *									*
 *	idList		: A comma-separated list of ids.		*
 *			  (The result list is in reverse order.  For    *
 *			  example, if x,y,z is read, then idList has	*
 *			  attribute ["z", "y", "x"].			*
 *									*
 *	allButProcId	: Any entity or unknown identifier except a 	*
 *			  procedure identifeir.				*
 *									*
 *	PUEntId		: An entity identifier or procedure identifier. *
 *			  or unknown identifier.			*
 *									*
 *	PUEntIdList	: A comma-separated list of PUEntIds.		*
 *			  The attribute is the list as a list of	*
 *			  strings.					*
 *									*
 *	taggedId	: An entity identifier, optionally with a tag.	*
 *									*
 *      taggedProcId	: A proceodure identifier, optionally with a	*
 *			  tag.						*
 *									*
 *	taggedEntOrProcId: A tagged id, proc id or unary operator.	*
 *									*
 *      taggedUnaryOp	: A unary operator, optionally with a tag.	*
 *									*
 *	genusId		: A genus identifier or abbreviation.		*
 *									*
 *	communityId	: A community identifier or abbreviation.	*
 *									*
 *	optClassId	: An optional genus, community, type or family	*
 *			  identifier.					*
 *	classId		: Any genus, community, type or family		*
 *			  identifier.					*
 *									*
 *	PUClassId	: Any class identifier or unknown identifier.	*
 *									*
 *	unknownId	: An unknown identifier.			*
 *									*
 *	cgId		: A genus or community identifier.		*
 *									*
 *	cgIdList	: A list of genus or community identifiers, 	*
 *			  separated by cgIdList.  			*
 *									*
 *      classBad	: Any class identifier or type/fam variable.	*
 *									*
 *	abbrevId	: Any class identifier that is an abbreviation.	*
 *									*
 *	famId		: A family identifier or abbreviation.		*
 *									*
 *	famVar		: A family variable or abbreviation.		*
 *									*
 *	optTag		: Optional :typeExpr				*
 *									*
 *      end		: end or period					*
 *									*
 *	recover		: either RECOVER or the empty string.		*
 ************************************************************************/

getLine		: /* empty */			%prec JUXTAPOSITION
		  {$$ = current_line_number;}
		;

oper		: UNARY_OP
			{$$ = $1;}

		| binOp
			{$$ = $1;}
		;

binOp		: L1_OP
			{$$ = $1;} 

		| L2_OP
			{$$ = $1;}

		| L3_OP
			{$$ = $1;}

		| L4_OP
			{$$ = $1;}

		| L5_OP
			{$$ = $1;}

		| L6_OP
			{$$ = $1;}

		| L7_OP
			{$$ = $1;}

		| L8_OP
			{$$ = $1;}

		| L9_OP
			{$$ = $1;}

		| R1_OP
			{$$ = $1;}

		| R2_OP
			{$$ = $1;}

		| R3_OP
			{$$ = $1;}

		| R4_OP
			{$$ = $1;}

		| R5_OP
			{$$ = $1;}

		| R6_OP
			{$$ = $1;}

		| R7_OP
			{$$ = $1;}

		| R8_OP
			{$$ = $1;}

		| R9_OP
			{$$ = $1;}

		| COMPARE_OP
		    	{$$ = $1;}

		| BAR_OP
		    	{$$ = $1;}

		| COLON_EQUAL_OP
		    	{$$ = $1;}
		;

unknownId	: UNKNOWN_ID_TOK
			{$$ = $1.name;}

		| PROC_ID_TOK
			{pop_shadow();
			 $$ = $1.name;
			}

		| oper
			{$$ = $1.name;}

		| classId
			{$$ = $1;}
		;

PUEntId 	: PROC_ID_TOK
			{$$ = $1.name;
			 pop_shadow();
			}

		| UNARY_OP
			{$$ = $1.name;}

		| UNKNOWN_ID_TOK
			{$$ = $1.name;}
		;

PUEntIdList 	: PUEntId   %prec BAR_TOK
			{bump_list($$ = str_cons($1, NIL));}

		| PUEntIdList COMMA_TOK PUEntId
			{bump_list($$ = str_cons($3, $1));
		 	 drop_list($1);
		       	}

		| PUEntIdList COMMA_TOK classId
			{syntax_error1(CLASS_ID_AFTER_ENT_ID_ERR, $3, 0);
			 $$ = NIL;
			}
		;

allButProcId	: oper
			{$$ = $1;}

		| UNKNOWN_ID_TOK
			{$$ = $1;}
		;

id		: allButProcId
			{$$ = $1.name;}

		| classId	
			{$$ = $1;}

		| PROC_ID_TOK
			{$$ = $1.name;
			 pop_proc_id();
			}
		;

idList		: id   %prec BAR_TOK
			{bump_list($$ = str_cons($1, NIL));}

		| idList COMMA_TOK id
			{bump_list($$ = str_cons($3, $1));}

		;


classId		: GENUS_ID_TOK
			{$$ = $1->name;}

		| COMM_ID_TOK
			{$$ = $1->name;}

		| TYPE_ID_TOK
			{$$ = $1.name;}

		| FAM_ID_TOK
			{$$ = $1.name;}

		| abbrevId
			{$$ = $1;}
		;

optClassId	: /* empty */		%prec EQUAL_TOK
			{$$ = NULL;}

		| classId
			{$$ = $1;}
		;

PUClassId	: classId 	
			{$$ = $1;}

		| UNKNOWN_ID_TOK 
			{$$ = $1.name;}

		| PROC_ID_TOK 
			{$$ = $1.name;
			 pop_shadow();
			}
		;

classBad	: classId
			{$$ = $1;}

		| TYPE_VAR_TOK
			{$$ = restore_cc_var_name($1.name);}

		| FAM_VAR_TOK
			{$$ = restore_cc_var_name($1.name);}
		;

abbrevId	: TYPE_ABBREV_TOK
			{$$ = $1.name;
			 drop_type($1.ty);
		        }

		| FAM_ABBREV_TOK
			{$$ = $1.name;
			 drop_type($1.ty);
		        }

		| FAM_MACRO_TOK
			{$$ = $1.name;
			 drop_type($1.ty);
			}

		| FAM_VAR_ABBREV_TOK
			{$$ = $1.name;
			 drop_type($1.ty);
		        }

		| GENUS_ABBREV_TOK
			{$$ = $1->name;}

		| COMM_ABBREV_TOK
			{$$ = $1->name;}
		;

basicFamId	: FAM_ID_TOK
			{bump_type($$ = type_id_tok_p($1.name));}

		| FAM_ABBREV_TOK
			{$$ = $1.ty;}
		;

famId		: basicFamId
			{$$ = $1;}

		| communityId
			{bump_type($$ = new_type(WRAP_FAM_T, NULL));
			 $$->ctc = $1;
			}

		| LBRACE_TOK famId RBRACE_TOK
			{bump_type($$ = new_type(MARK_T, $2));
			 drop_type($2);
			}
		;

basicFamVar	: FAM_VAR_TOK
			{bump_type($$ = tf_var_tm($1.name, FAM_VAR_T));}

		| FAM_VAR_ABBREV_TOK
			{bump_type($$ = 
				no_copy_abbrev_vars 
				  ? $1.ty
				  : copy_type1($1.ty, &abbrev_var_table, 7));
			 drop_type($1.ty);
		        }
		;

famVar		: basicFamVar
			{$$ = $1;}


		| LBRACE_TOK famVar RBRACE_TOK
			{bump_type($$ = new_type(MARK_T, $2));
			 drop_type($2);
			}
		;

genusId		: GENUS_ID_TOK
			{$$ = $1;}

		| GENUS_ABBREV_TOK
			{$$ = $1;}
		;

communityId	: COMM_ID_TOK
			{$$ = $1;}

		| COMM_ABBREV_TOK
			{$$ = $1;}
		;

cgIdList	: cgId
			{bump_list($$ = ctc_cons($1, NIL));}

		| cgIdList COMMA_TOK cgId
			{if($3 == NULL) $$ = $1;
			 else {
			   bump_list($$ = ctc_cons($3, $1));
			   drop_list($1);
			 }
			}
		;

cgId		: genusId
			{$$ = $1;}

		| communityId
			{$$ = $1;}

		| UNKNOWN_ID_TOK
			{unknown_id_error(UNKNOWN_ID_ERR, $1.name, $1.line);
			 $$ = NULL;
			}
		;

optTag		: /* empty */				%prec BAR_TOK
			{$$.type = NULL_T;
			 $$.role = NULL;
			}

		| COLON_TOK typeExpr	
			{$$ = $2;}
		;

taggedId	: UNKNOWN_ID_TOK optTag
			{bump_expr($$ = tagged_id_p($1.name, $2, $1.line));
			 drop_rtype($2);
			}
		;


taggedProcId	: PROC_ID_TOK 
			{indentation_warn(FALSE);}
                  optTag
			{EXPR *id;

			 bump_expr($$ = tagged_id_p($1.name, $3, $1.line));
			 id = skip_sames($$);
			 id->OFFSET = $1.column;
			 id->extra  = ($1.tok < 0);  /* From semicolon? */
			 drop_rtype($3);
			 indentation_warn(TRUE);
			}
		;

taggedUnaryOp	: UNARY_OP optTag
			{bump_expr($$ = tagged_id_p($1.name, $2, $1.line));
			 drop_rtype($2);
			}
		;

taggedEntOrProcId: taggedId
			{$$ = $1;
		        }

		| taggedProcId
			{$$ = $1;
			 pop_proc_id();
			}

		| taggedUnaryOp
			{$$ = $1;
		        }
		;

end		: END_TOK
			{$$ = $1;}
		| PERIOD_TOK
			{$$ = $1;}
		;

recover		: RECOVER	{$$ = 0;}

		| /* empty */	{$$ = 1;}
		;

%%

/****************************************************************
 *			START_DCL				*
 ****************************************************************
 * start_dcl is called at the beginning of each declaration.    *
 * It restores all context information to what it should be	*
 * when a declaration starts.					*
 *								*
 * If restart_errs is true, then do err_begin_dcl.  Otherwise,  *
 * don't.							*
 ****************************************************************/

PRIVATE void start_dcl(int restart_errs)
{
# ifdef MSWIN
    {MSG msg;
     PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);
    }
# endif

# ifdef DEBUG
    if(trace) trace_t(456);
# endif

  if(stop_at_first_err && error_occurred) clean_up_and_exit(1);
  if(restart_errs) err_begin_dcl();

  defining_id         = NULL;
  temp_var_num	      = 0;
  formal_num          = 1;
  dcl_context         = TRUE;
  in_operator_dcl     = FALSE;
  no_copy_abbrev_vars = FALSE;
  seen_open_expr      = FALSE;
  open_pat_dcl        = FALSE;
  err_flags.suppress_indentation_errs = FALSE;
  suppress_reset      = FALSE;
  generate_implicit_pop = FALSE;

  scan_and_clear_hash2(&tf_var_table, drop_hash_type);
  scan_and_clear_hash2(&abbrev_var_table, drop_hash_type);

  SET_TYPE(abbrev_val, 				NULL);
  SET_ROLE(abbrev_tok_role, 			NULL);
  SET_LIST(choose_matching_lists, 		NIL);
  SET_LIST(copy_choose_matching_lists, 		NIL);
  SET_LIST(choose_st,				NIL);
  SET_LIST(case_kind_st,			NIL);

  clear_local_assumes_tm();
  clear_new_binding_list_u();
}


/****************************************************************
 *			END_DCL					*
 ****************************************************************
 * end_dcl is called when a declaration is completed.  It does	*
 * some cleanup, and resets things that have been temporarily	*
 * changed by Advise ... one %Advise.				*
 ****************************************************************/

PRIVATE void end_dcl(void)
{
  if(stop_at_first_err && error_occurred) clean_up_and_exit(1);
  scan_and_clear_hash2(&tf_var_table, drop_hash_type);
  scan_and_clear_hash2(&abbrev_var_table, drop_hash_type);
  if(!suppress_reset) {
    err_reset_for_dcl();
    no_tro_restore_tm();
    show_types 		= show_all_types;
    echo_expr		= echo_all_exprs;
    should_show_patmatch_subst = always_should_show_patmatch_subst;
    suppress_tro 	= forever_suppress_tro;
    max_overloads	= main_max_overloads;
  }
  defining_id = NULL;
}


/****************************************************************
 *			BEGIN_EXPORT				*
 ****************************************************************
 * begin_export prepares the context for an export.		*
 ****************************************************************/

PRIVATE void begin_export(void)
{ 
  if(import_level == 0 && main_context != INIT_CX) {
    main_context = EXPORT_CX;
    gen_code     = main_gen_code;
    seen_export  = TRUE;
  }
}


/****************************************************************
 *			DO_DCL_P				*
 ****************************************************************
 * Return TRUE if the current declaration is not suppressed by	*
 * an if-dcl.							*
 ****************************************************************/

PRIVATE Boolean do_dcl_p(void)
{
  return if_dcl_st == NULL || top_i(if_dcl_st) == top_i(if_dcl_then_st);
}


/****************************************************************
 *			   DEBUG_AT_DCL_PANIC			*
 ****************************************************************
 * Print debug info at a panic start.				*
 ****************************************************************/

void debug_at_dcl_panic(void)
{
# ifdef DEBUG
    if(trace) {
      trace_t(451);
      trace_t(452);
    }
# endif
}


/****************************************************************
 *			   DEBUG_AT_DCL_RECOVER			*
 ****************************************************************
 * Print debug info at a panic recovery.			*
 ****************************************************************/

void debug_at_dcl_recover(void)
{
# ifdef DEBUG
    if(trace) {
      trace_t(453);
      trace_t(452);
    }
# endif
}

