%{
/******************************************************************
 * File:    lexer/lexer.lex
 * Purpose: Lexical analyzer for Astarte compiler.
 * Author:  Karl Abrahamson
 ******************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/************************************************************************ 
 *  Note: This is the core of the lexer, not the full lexer.  preflex.c *
 *  defines a front end that reads the characters, and a back end       *
 *  called yylexx that is the lexer actually employed by the parser.    *
 *									*
 ************************************************************************
 *									*
 * This lexer returns a token number and places attributes in      	*
 * yylval.  Certain identifiers are altered before being placed in	*
 * yylval.								*
 *									*
 *   (1) Type and family variables have the qualifier part moved to	*
 *       the front, followed by a #, followed by the root.  For example,*
 *       Frog`xx becomes `xx#Frog.  `x becomes just `x#.		*
 *									*
 *   (2) Pattern variables have the question mark before their names	*
 *       removed.							*
 *									*
 * To add a token							*
 * --------------							*
 *  (1) Add a line to this file for the lexeme.  See examples for 	*
 *      similar lexemes.  Call one of begin_word_l (for a word or 	*
 *      symbol with a matching right-bracket) zero_context_l (for a 	*
 * 	symbol that cannot be followed by a binary operator) or 	*
 * 	one_context_l (for a symbol that can be followed by a binary 	*
 * 	operator).							*
 *									*
 *  (2) Add the token to parser/parser.y.				*
 *									*
 *  (3) Possibly update the tables found in lexer/lexsup.c.		*
 *									*
 *  (4) Possibly update check_for_paren_l in lexer/lexerr.c (if the	*
 *      token is a separator that uses check_for_paren_l for error	*
 *	recovery, or if the token is a begin token that can contain     *
 *      separators in its body, not surrounded by other begin/end	*
 *	tokens).							*
 *									*
 *  (5) Possibly update rparen_l in lexer/lexerr.c (if the token is a 	*
 *      right bracket of some kind).					*
 *									*
 *  (6) Modify messages/syntax.txt to handle this token.  See parser.y.	*
 *									*
 * Note: if you change lex symbol {symRestChar}, also change	 	*
 * symbolic_id_chars in lexer/preflex.c and fix ast/standard.asi, where *
 * symblicIdChars is defined.						*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../exprs/expr.h"
#include "../lexer/lexer.h"
#include "../lexer/modes.h"
#include "../standard/stdids.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../dcls/dcls.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
int isatty(int fd);

static void read_long_comment_l(Boolean echo_comment_start);

#define UNPUT(c) if(c != EOF) unput(c)

/****************************************************************
 *			LEX DEFINITIONS AND RULES		*
 ****************************************************************/

%}

%array
%e 1300
%p 6000
%n 1000
%k 1000
%a 6000
%o 10000

idtail		    [a-zA-Z0-9!?'_]*
id		    [_a-zA-Z]{idtail}
reducedSymStartChar [#+\-*/\^=<>@&~$\\]
symStartChar 	    [#+\-*/\^!=<>@&~$_\\]
symRestChar	    [#+\-*/\^!=<>@&~$_\\?'%]
symbolicid	    {symStartChar}{symRestChar}*
spaceOrDash	    ("-"|[ \t\n\r][ \t\n\r]*)

A		[aA]
B		[bB]
C		[cC]
D		[dD]
E		[eE]
F		[fF]
G		[gG]
H		[hH]
I		[iI]
K		[kK]
L		[lL]
M		[mM]
N		[nN]
O		[oO]
P		[pP]
R		[rR]
S		[sS]
T		[tT]
U		[uU]
V		[vV]
W		[wW]

%%

"\\\n"		{register int c;

		 /*-----------------------------------------------------*
		  * This handles \ as the last character of a line.     *
		  * Echo the \ and \n, and print the line number of the *
		  * next line.						*
		  *-----------------------------------------------------*/

		 echo_token_l(yytext);
		 new_line_l();

		 /*------------------------------------------------------*
		  * Skip over white-space at the front of the next line. *
		  *------------------------------------------------------*/

		 c = input();
                 while(c == ' ' || c == '\t') {
		   echo_char_l(c);
		   c = input();
		 }
		 UNPUT(c);
		}

[ \t\r]*        {echo_token_l(yytext);}

"\n"[ \t]*	{
#		 ifdef DEBUG
		   if(trace_lexical) trace_t(338);
#		 endif

		 echo_token_l("\n");
		 print_reports_p(); 
		 new_line_l();
		 echo_token_l(yytext+1);
		 if(file_info_st != NULL &&
		    current_resume_long_comment_at_eol) {
		   read_long_comment_l(FALSE);
		 }
		 else if(err_flags.check_indentation) check_indentation_l();
		}

"%%".*		{echo_token_l(yytext);}

"<<<<"("<"*)	{read_long_comment_l(TRUE);}

"%<<<<"		{/*---------------------------------------------*
		  * This is used when a file is started in long *
		  * comment mode.				*
		  *---------------------------------------------*/

		 read_long_comment_l(FALSE);
		}

"<<<"		{syntax_error1(ID_NOT_ALLOWED_ERR, yytext, 0);
		 return normal_id_l(yytext);
		}

">>>"(">"*)	{syntax_error1(ID_NOT_ALLOWED_ERR, yytext, 0);
		 return normal_id_l(yytext);
		}

{A}bbrev	{require_uc();
		 begin_word_l(ABBREV_TOK); 
		 return(ABBREV_TOK);
		}

{A}bstraction	{require_uc();
		 begin_word_l(ABSTRACTION_TOK);
		 return(ABSTRACTION_TOK);
		}

{A}dvise	{require_uc();
		 begin_word_l(ADVISORY_TOK); 
		 return ADVISORY_TOK;
		}

{A}ll		{require_lc();
		 zero_context_l(0); 
		 yylval.int_at = ALL_ATT; 
		 return(WHICH_TOK);
		}

_and_		{if(opContext == 0) return symbolic_id_l(ANDF_ID);
		 else {
		   zero_context_l(0);
		   return(AND_TOK);
		 }
		}

{A}nticipate	{require_uc();
		 begin_word_l(ANTICIPATE_TOK); 
		 yylval.ls_at.attr = ANTICIPATE_ATT;
		 return(EXPECT_TOK);
		}

{A}sk		{require_uc();
		 begin_word_l(ASK_TOK);
		 return ASK_TOK;
	        }

{A}ssume	{begin_word_l(ASSUME_TOK);
		 yylval.ls_at.attr = *yytext == 'a' ? LC_ATT : UC_ATT;
		 return(ASSUME_TOK);
		}

{A}tomic	{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(ATOMIC_TOK); 
		 yylval.ls_at.attr = ATOMIC_ATT; 
		 return ATOMIC_TOK;
		}

{A}wait		{require_uc();
  		 push_assume_finger_tm();
		 begin_word_l(AWAIT_TOK);
		 return AWAIT_TOK;
		}

{B}ring		{begin_word_l(BRING_TOK); 
		 bring_context = 1;
		 yylval.ls_at.attr = *yytext == 'b' ? LC_ATT : UC_ATT;
		 return BRING_TOK;
		}

_butIfFail_	{zero_context_l(0);
		 return BUT_IF_FAIL_TOK;
		}

{B}y		{require_lc();
		 zero_context_l(0);
		 return(BY_TOK);
		}

{C}ase		{require_lc();
		 zero_context_l(0); 
		 yylval.ls_at.attr = CASE_ATT; 
		 return(check_for_paren_l(CASE_TOK));
	        }

{C}atchingTermination {require_lc();
		       zero_context_l(0);
		       yylval.int_at = 1;
		       return CATCHING_TOK;
		      }

{C}atchingEachThread    {require_lc();
		 	 zero_context_l(0);
		 	 yylval.int_at = 2;
		 	 return CATCHING_TOK;
			}

{C}hoose	{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(CHOOSE_TOK);
		 return(CHOOSE_TOK);
		}

{C}lass 	{require_uc();
		 begin_word_l(CLASS_TOK);
		 return(CLASS_TOK);
		}

{C}onst 	{require_uc();
		 begin_word_l(CONST_TOK);
		 return CONST_TOK;
		}

{C}onstraint	{require_lc();
		 zero_context_l(0);
		 return CONSTRAINT_TOK;
		}

{C}onstructor	{require_lc();
		 zero_context_l(0);
		 return CONSTRUCTOR_TOK;
		}

{C}ontext	{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(CONTEXT_TOK); 
		 return CONTEXT_TOK;
		}

{C}ontinue	{require_lc();
		 zero_context_l(0);
		 return(check_for_paren_l(CONTINUE_TOK));
		}

{C}utHere	{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(CUTHERE_TOK); 
		 yylval.ls_at.attr = CUTHERE_ATT; 
		 return ATOMIC_TOK;
		}

{D}efault	{begin_word_l(DEFAULT_TOK); 
		 yylval.ls_at.attr = *yytext == 'd' ? LC_ATT : UC_ATT;
		 return(DEFAULT_TOK);}

{D}efine	{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(DEFINE_TOK);
		 yylval.ls_at.attr = DEFINE_ATT;
		 return(LET_TOK);
		}

{D}escription	{begin_word_l(DESCRIPTION_TOK);
		 yylval.ls_at.attr = *yytext == 'd' ? LC_ATT : UC_ATT;
		 return(DESCRIPTION_TOK);
		}

{D}irectory	{require_uc();
		 begin_word_l(DIRECTORY_TOK);
		 return DIRECTORY_TOK;
		}

{D}o		{require_lc();
		 zero_context_l(0); 
		 return(check_for_paren_l(DO_TOK));
		}

{E}lse		{require_lc();
		 zero_context_l(0);
		 return(check_for_paren_l(ELSE_TOK));
		}

{E}xception	{begin_word_l(EXCEPTION_TOK); 
		 yylval.ls_at.attr = *yytext == 'e' ? LC_ATT : UC_ATT;
		 opContext = 1;
	         return EXCEPTION_TOK;
                }

{E}xecute	{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(EXECUTE_TOK);
		 return EXECUTE_TOK;
		}

{E}xitcase	{require_lc();
		 zero_context_l(0);
		 yylval.ls_at.attr = UNTIL_ATT;
		 return check_for_paren_l(CASE_TOK);
	        }

{E}xpand	{require_uc();
		 begin_word_l(EXPAND_TOK);
		 yylval.ls_at.attr = EXPAND_ATT;
		 return PATTERN_TOK;
	        }

{E}xpect	{require_uc();
		 begin_word_l(EXPECT_TOK); 
		 yylval.ls_at.attr = EXPECT_ATT;
		 return(EXPECT_TOK);
		}

{E}xport	{zero_context_l(0); 
		 return(EXPORT_TOK);
		}

Extract		{begin_word_l(EXTRACT_TOK);
		 return EXTRACT_TOK;
		}

extract	        {zero_context_l(0); 
                 return LCEXTRACT_TOK;
                }

{E}xtension	{if(*yytext == 'E') {
		   begin_word_l(EXTENSION_TOK);
		   yylval.ls_at.attr = UC_ATT;
		 }
		 else {
		   zero_context_l(0);
		   yylval.ls_at.attr = LC_ATT;
		   yylval.ls_at.name = rw_name(EXTENSION_TOK);
		   yylval.ls_at.tok  = EXTENSION_TOK;
		 }
		 return EXTENSION_TOK;
		}

{F}irst		{require_lc();
		 zero_context_l(0);
		 yylval.int_at = FIRST_ATT; 
		 return(WHICH_TOK);
		}

{F}or		{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(FOR_TOK);
		 return FOR_TOK;
		}

{F}rom		{require_lc();
		 zero_context_l(RBRACE_TOK);
		 return FROM_TOK;
		}

{H}olding	{require_lc();
		 zero_context_l(0);
		 return HOLDING_TOK;
		}

{I}f		{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(IF_TOK); 
		 yylval.ls_at.attr = IF_ATT; 
		 return(IF_TOK);
		}

{I}mport	{require_uc();
		 begin_word_l(IMPORT_TOK);
		 import_l(1);
		 return IMPORT_TOK;
		}

{I}mplementation {body_tok_l(); 
		  return(BODY_TOK);
		 }

{I}mplements	{import_l(2);
		 add_mode(&this_mode, PRIVATE_MODE);
		 return IMPLEMENTS_TOK;
		}

_implies_	{if(opContext == 0) return symbolic_id_l(IMPLIES_SYM);
		  else {
		    zero_context_l(0); 
		    return(IMPLIES_TOK);
		  }
		}

{I}nterface    	{return INTERFACE_TOK;}

{I}sKindOf	{require_lc();
		 return ISA_TOK;
		}

{L}et		{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(LET_TOK);
		 yylval.ls_at.attr = LET_ATT;
		 return(LET_TOK);
		}

{L}oop		{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(LOOP_TOK);
		 return LOOP_TOK;
		}

{L}oopcase	{require_lc();
		 zero_context_l(0);
		 return check_for_paren_l(LOOPCASE_TOK);
		}

{M}atch		{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(MATCH_TOK);
		 return(MATCH_TOK);
		}

{M}atching	{require_lc();
		 zero_context_l(0);
		 return(MATCHING_TOK);
		}

{M}issing	{begin_word_l(MISSING_TOK); 
		 yylval.ls_at.attr = *yytext == 'm' ? LC_ATT : UC_ATT;
		 return MISSING_TOK;
	        }

{M}ix		{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(MIX_TOK);
		 yylval.ls_at.attr = MIX_ATT;
		 return STREAM_TOK;}

{M}ixed		{require_lc();
		 zero_context_l(0);
		 return MIXED_TOK;
		}

{O}ne		{require_lc();
		 zero_context_l(0);
		 yylval.int_at = ONE_ATT; 
		 return(WHICH_TOK);
		}

{O}pen		{zero_context_l(0); 
		 open_col = chars_in_this_line;
		 return OPEN_TOK;
		}

{O}perator	{require_uc();
		 begin_word_l(OPERATOR_TOK);
		 return(OPERATOR_TOK);
		}

_or_		{if(opContext == 0) return symbolic_id_l(ORF_ID);
		 else {
		   zero_context_l(0);
		   return(OR_TOK);
		 }
		} 

{P}ackage	{require_uc();
		 begin_word_l(PACKAGE_TOK);
		 dcl_context = TRUE;
		 return PACKAGE_TOK;
		}

Pattern		{begin_word_l(PATTERN_TOK);
		 yylval.ls_at.attr = PATTERN_ATT;
		 return(PATTERN_TOK);
	        }

pattern		{zero_context_l(0);
		 return LCPATTERN_TOK;
		}

{P}erhaps	{require_lc();
		 zero_context_l(0);
		 yylval.int_at = PERHAPS_ATT; 
		 return WHICH_TOK;
		}

{R}elate	{require_uc();
		 begin_word_l(RELATE_TOK);
		 return RELATE_TOK;
		}

{R}elet		{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(RELET_TOK);
		 yylval.ls_at.attr = RELET_ATT;
		 return LET_TOK;
		}

{S}pecies	{if(*yytext == 'S') {
                   begin_word_l(SPECIES_TOK); 
		   yylval.ls_at.attr = UC_ATT;
		 }
		 else {
		   zero_context_l(0);
		   yylval.ls_at.attr = LC_ATT;
		   yylval.ls_at.name = rw_name(SPECIES_TOK);
		   yylval.ls_at.tok  = SPECIES_TOK;
		 }
		 return(SPECIES_TOK);
	         }

{S}peciesAsValue  {require_lc();
		 zero_context_l(0);
		 return SPECIES_AS_VAL_TOK;
		}

{S}tream	{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(STREAM_TOK);
		 yylval.ls_at.attr = STREAM_ATT;
		 return(STREAM_TOK);
		}

{S}ubcases	{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(SUBCASES_TOK);
		 subcases_context = 1;
		 return(SUBCASES_TOK);
		}

{T}arget	{require_lc();
		 one_context_l(0);
		 return TARGET_TOK;
		}

{T}eam		{require_uc();
		 begin_word_l(TEAM_TOK);
		 return(TEAM_TOK);
		}

{T}hen		{require_lc();
		 zero_context_l(0);
		 return(check_for_paren_l(THEN_TOK));
		}

Trap		{begin_word_l(TRAP_TOK);
		 push_assume_finger_tm();
		 yylval.ls_at.attr = TRAP_ATT;
		 return(TRAP_TOK);
		}

trap		{zero_context_l(0);
		 yylval.ls_at.attr = TRAP_ATT;
		 return LCTRAP_TOK;
		}

{T}ry		{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(TRY_TOK); 
		 yylval.ls_at.attr = TRY_ATT;
		 return(IF_TOK);
		}

{U}nique	{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(UNIQUE_TOK); 
		 yylval.ls_at.attr = FIRST_ATT; 
		 return ATOMIC_TOK;
		}

{U}ntrap	{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(UNTRAP_TOK);
		 yylval.ls_at.attr = UNTRAP_ATT;
		 return(TRAP_TOK);
		}

{V}ar		{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(VAR_TOK); 
		 yylval.ls_at.attr = 0;
		 return VAR_TOK;
		}

{W}ith		{require_lc();
		 zero_context_l(0);
		 return(check_for_paren_l(WITH_TOK));
		}

{W}hen		{require_lc();
		 zero_context_l(0);
		 return check_for_paren_l(WHEN_TOK);
		}

_where_		{zero_context_l(0);
		 return WHERE_TOK;
		}

{W}hile		{require_uc();
		 push_assume_finger_tm();
		 begin_word_l(WHILE_TOK);
		 return WHILE_TOK;
		}
		 
"("		{begin_word_l(RPAREN_TOK); 
		 push_assume_finger_tm();
		 return(LPAREN_TOK);
		}

")"		{int t; 
		 if((t = end_word_l(RPAREN_TOK))) return t;
		}

"{"		{begin_word_l(RBRACE_TOK); 
		 return(LBRACE_TOK);
		}

"}"		{int t;
		 t = end_word_l(RBRACE_TOK);
		 opContext = 0;
		 if(t) return t;
		}

"["		{begin_word_l(RBRACK_TOK); 
		 return(LBRACK_TOK);
		}

"]"		{int t; 
		 if((t = end_word_l(RBRACK_TOK))) return t;
		}

"[:"		{begin_word_l(RBRACK_COLON_TOK); 
		 return LBRACK_COLON_TOK;
		}

":]"		{int t; 
		 if((t = end_word_l(RBRACK_COLON_TOK))) return t;
		}

"<:"		{begin_word_l(RBOX_TOK); 
		 yylval.ls_at.attr = 0;
		 return LBOX_TOK;
		}

":>"		{int t; 
		 if((t = end_word_l(RBOX_TOK))) {
		   yylval.ls_at.attr = 0;
		   return t;
		 }
		}

"<:#"		{begin_word_l(RBOX_TOK); 
		 yylval.ls_at.attr = SHARED_ATT;
		 return LBOX_TOK;
		}

"#:>"		{int t; 
		 if((t = end_word_l(RBOX_TOK))) {
		   yylval.ls_at.attr = SHARED_ATT;
		   return t;
		 }
		}

"<:%"		{begin_word_l(RBOX_TOK); 
		 yylval.ls_at.attr = NONSHARED_ATT;
		 return LBOX_TOK;
		}

"%:>"		{int t; 
		 if((t = end_word_l(RBOX_TOK))) {
		   yylval.ls_at.attr = NONSHARED_ATT;
		   return t;
		 }
		}

"{:"		{begin_word_l(RROLE_SELECT_TOK); 
		 return LROLE_SELECT_TOK;
		}

":}"		{int t; 
		 if((t = end_word_l(RROLE_SELECT_TOK))) return t;
		}

"/{:"		{begin_word_l(RROLE_SELECT_TOK); 
		 return LROLE_MODIFY_TOK;
		}

"<<"		{begin_word_l(RWRAP_TOK);
 		 yylval.ls_at.attr = 0;
		 return LWRAP_TOK;
		}

"<<?"		{yyless(2);
		 begin_word_l(RWRAP_TOK);
 		 yylval.ls_at.attr = 0;
		 return LWRAP_TOK;
		}

">>"		{int t;
 		 yylval.ls_at.attr = 0;
		 if((t = end_word_l(RWRAP_TOK))) return t;
		}

"?<<"		{begin_word_l(RWRAP_TOK);
 		 yylval.ls_at.attr = 1;
		 return LWRAP_TOK;
		}

">>?"		{int t;
 		 yylval.ls_at.attr = 1;
		 if((t = end_word_l(RWRAP_TOK))) return t;
		}

"(:"		{
  		 /*-------------------------------------*
		  * Check for (:=)  in an operator dcl. *
		  *-------------------------------------*/

		 if(in_operator_dcl) {
  		   yyless(1);
		   begin_word_l(RPAREN_TOK);
		   return LPAREN_TOK;
		 }
		 else {
		   begin_word_l(RLAZY_TOK); 
		   return LLAZY_TOK;
		 }
		}

":)"		{int t; 
		 if((t = end_word_l(RLAZY_TOK))) return t;
		}

"(::)" 		{yyless(1);
		 begin_word_l(RPAREN_TOK); 
		 push_assume_finger_tm();
		 return(LPAREN_TOK);
		}

","		{/* Suppress changing import_start. */
  		 zero_context_l(RBRACE_TOK);  
		 return(check_for_paren_l(COMMA_TOK));
		}

":"		{zero_context_l(0); 
		 return(COLON_TOK);
		}

";"		{return do_semicolon_l();}

"->"		{zero_context_l(0); 
		 return(SGL_ARROW_TOK);
		}

"=>"		{zero_context_l(0); 
		 return(check_for_paren_l(DBL_ARROW_TOK));
		}

"|"		{zero_context_l(0); 
		 force_no_op = TRUE; 
		 return(BAR_TOK);
		}

"&"		{zero_context_l(0); 
		 return(check_for_paren_l(AMPERSAND_TOK));
		}

"~>"		{zero_context_l(0); 
		 return ROLE_MARK_TOK;
		}

"="		{zero_context_l(0); 
		 return(check_for_paren_l(EQUAL_TOK));
		}

"<-"		{zero_context_l(0); 
		 yylval.int_at = ARROW1_ATT;
		 return(check_for_paren_l(PLUS_EQUAL_TOK));
		}

"<-?"		{zero_context_l(0); 
		 yylval.int_at = ARROW2_ATT;
		 return(check_for_paren_l(PLUS_EQUAL_TOK));
		}

":="		{zero_context_l(0); 
		 return(COLON_EQUAL_TOK);
		}

"===="=*	{zero_context_l(0); 
		 return(line_tok_l());
		}

"?"		{one_context_l(0); 
		 return(QUESTION_MARK_TOK);
		}

[0-9]+		{one_context_l(0); 
		 yylval.ls_at.name = string_const_tb0(yytext);
		 return(NAT_CONST_TOK);
		}

[0-9]*"."[0-9]+({E}[+-]?[0-9]+)? {one_context_l(0); 
		 		  yylval.ls_at.name = string_const_tb0(yytext);
		 		  return(REAL_CONST_TOK);
				 }

"'"([^\\]|\\[^\{]|\\\{[0-9]+\})"'" {
		 one_context_l(0); 
	 	 yylval.ls_at.name = string_const_tb0(yytext+1);
		 return(CHAR_CONST_TOK);
		}

"\""		{return handle_string_l();}

"%\""		{read_string_l(TRUE);
		 one_context_l(0);
		 return STRING_CONST_TOK;
		}

"||"		{int tok = symbolic_id_l(DBL_BAR_SYM); 
		 force_no_op = TRUE;
		 return tok;
		}

"__"		{/*-----------------------------------------------*
		  * This cannot be followed by a binary operator. *
		  *-----------------------------------------------*/

		 int tok = symbolic_id_l(DBL_UNDERSCORE_SYM); 
		 zero_context_l(0);
		 force_no_op = TRUE;
		 return tok;
		}

"::"		{return normal_id_l(yytext);}

{symbolicid} 	{return normal_id_l(yytext);}

{id}		{return normal_id_l(yytext);}

{M}y{spaceOrDash}{symbolicid} {return normal_id_l(get_my_id_l(yytext));}

{M}y{reducedSymStartChar}{symRestChar}* {
		 return normal_id_l(get_my_id_l(yytext));
		}

{M}y{spaceOrDash}{id} {return normal_id_l(get_my_id_l(yytext));}

"."		{int t; 
		 if((t = end_word_l(1))) return t;
		}

"%"{M}y{spaceOrDash}[a-zA-Z]{idtail} {
			 int t;
			 fix_yytext_for_end_my_l();
			 if((t = handle_endword_l())) return t;
			}

"%"[a-zA-Z]{idtail}	{int t; 
			 if((t = handle_endword_l())) return t;
			}

"?"{id} 	{one_context_l(0);
		 yylval.ls_at.name = id_tb0(yytext+1);
		 if(is_reserved_word_str(yytext+1)) {
		   syntax_error1(PAT_VAR_IS_RESERVED_ERR, 
				 yylval.ls_at.name, 0);
		 }
		 return PAT_VAR_TOK;
	        }

"?"{M}y{spaceOrDash}{id} {char *myid;
			  one_context_l(0);
			  myid = get_my_id_l(yytext + 1);
			  yylval.ls_at.name = id_tb0(myid);
		 	  if(is_reserved_word_str(myid)) {
		   	    syntax_error1(PAT_VAR_IS_RESERVED_ERR, myid, 0);
		 	  }
		 	  return PAT_VAR_TOK;
	        	 }


{id}?("`"|"``"|"`*")("~"?)("?"|{id}) {return class_id_l(yytext, yyleng, TRUE);}

"%"			{yyleng = 0;
			 echo_token_l(yytext);
			 syntax_error(BAD_PERCENT_ERR, 0);
			}

.			{yyleng = 0;
			 echo_token_l(yytext);
			 syntax_error2(BAD_CHAR_ERR, 
				       (char *) tolong(yytext[0]), 
				       (char *) tolong(yytext[0]), 0);
			}
%%

/*****************************************************************
 *			YYWRAP					 *
 *****************************************************************
 * yywrap() is called by yylex when eof is reached.  It 	 *
 * complains about an unexpected eof.			         *
 *****************************************************************/

int yywrap() 
{
  if(file_info_st == NULL || (file_info_st->next != NULL && !reading_string)) {
    eof_err();
    return 1;
  }
  else {
#   ifdef DEBUG
      if(trace) trace_t(339);
#   endif
    suppress_panic = 1;
    return 1;
  }
}


/*****************************************************************
 *			INPUT_YY				 *
 *			UNPUT_YY				 *
 *****************************************************************
 * input_yy and unput_yy make the input and unput macros of flex *
 * available as functions to other files.			 *
 *****************************************************************/

int input_yy(void)
{
  register int result = input();
  if(result == 0) result = EOF;
  return result;
}

void unput_yy(int c) 
{
  if(c != EOF) unput(c);
}


/****************************************************************
 *			UNPUT_STR				*
 ****************************************************************
 * Unput string s.						*
 ****************************************************************/

void unput_str(char *s)
{
  int k;
  int len = strlen(s);
  for(k = len-1; k >= 0; k--) unput(s[k]);
}


/****************************************************************
 *			REALLOCATE_STRING_CONST			*
 ****************************************************************
 * Reallocate string_const, doubling its size.  Set p to point	*
 * to the same offset in the new array as it currently is in 	*
 * the old array.						*
 ****************************************************************/

static void reallocate_string_const(char HUGEPTR *p)
{
  char HUGEPTR new = (char HUGEPTR) 
        reallocate((char *)string_const, string_const_len, 
		   2*string_const_len, TRUE);
  *p = new + (*p - string_const);
  string_const = new;
  string_const_len *= 2;
}


/****************************************************************
 *			READ_STRING_L				*
 ****************************************************************
 * Read a string constant.  longstring is TRUE for a multiline  *
 * string, and FALSE for a single line string.	Echo it.	*
 ****************************************************************/

void read_string_l(Boolean longstring)
{
  char HUGEPTR p, dblchar[8];
  int c;
  int start_line = current_line_number;
  LONG len = 0;
  Boolean suppress_newline = FALSE;  /* Used to suppress newline being
					put into the string when the line
					ends on \. */

  reading_string = TRUE;
  yylval.ls_at.line = (SHORT) start_line;
  yylval.ls_at.column = (SHORT) chars_in_this_line;

  /*----------------------*
   * Echo the left quote. *
   *----------------------*/

  if(longstring) echo_token_l("%");
  echo_token_l("\"");

  /*---------------------------------------------*
   * Read the string and put it in string_const. *
   *---------------------------------------------*/

  p = string_const;
  c = input();
  echo_char_l(c);
  while(TRUE) {

    /*-------------------------------------------------------------------*
     * Check for end of string.  A short string ends at a quote mark     *
     * or at end of line.  Any string ends at an end of file (c == EOF). *
     *-------------------------------------------------------------------*/

    if(c == EOF || !longstring && ((c == '"' || c == '\n'))) {
      if(c == '\n') {
	syntax_error(NEWLINE_IN_STRING_ERR, start_line);
	unput('\n');
      }
      goto out;
    }

    /*---------------------------------------------------------------*
     * Get a \ sequence.  It has the form \n, \t, \\ or \{k} where k *
     * is a decimal constant. \{cr} is ignored, except that in a     *
     * long string it suppresses a final newline.		     *
     *---------------------------------------------------------------*/

    else if(c == '\\') {
      dblchar[0] = (char) c;
      dblchar[1] = (char) (input());
      echo_char_l(dblchar[1]);
      if(dblchar[1] != '\n') {
	if(dblchar[1] == '{') {
	  int i = 2;
	  for(; i < 6 && c != '}'; i++) {
	    c = input();
	    echo_char_l(c);
	    dblchar[i] = (char) c;
	  }
	  if(c != '}') {
	    syntax_error(NUMBER_CHAR_TOO_LARGE_ERR, 0);
	    dblchar[i] = '}';
	  }
	}
	c = char_val(dblchar, current_line_number);
	if(c == 0) syntax_error(NULL_IN_STRING_CONST_ERR,0);
	*(p++) = (char) c;
	len++;
      }

      else /* dblchar[1] == '\n' */ {

	/*------------------------------------------------*
	 * Handle \ at end of line as an ignored newline, *
	 * or as a suppressed newline at the end of a     *
	 * long string.					  *
	 *------------------------------------------------*/

	if(longstring) {
	  suppress_newline = TRUE;
	  goto read_newline;       /* below */
	}
	new_line_l();
	c = input();
	while(c == ' ' || c == '\t') {
	  echo_char_l(c);
	  c = input();
	}
	UNPUT(c);
      }
    }

    /*---------------------------------------------------------------*
     * Handle a newline in a long string.  Put the newline in 	     *
     * string_const, and check the next line for more of the string. *
     *---------------------------------------------------------------*/

    else if(c == '\n') {
  read_newline:
      if(!suppress_newline) *(p++) = '\n';
      suppress_newline = FALSE;
      len++;
      new_line_l();
      c = input();
      while (c == ' ' || c == '\t') {
	echo_char_l(c);
	c = input();
      }
      if(c != '%') {
	UNPUT(c);
	goto out;
      }
      else {
	c = input();
	if(c != '"') {
	  UNPUT(c);
	  unput('%');
	  goto out;
	}
	else {
	  echo_token_l("%\"");
	}
      }
    }

    /*--------------------------------------------------------------------*
     * If not anything special, just put it into the string and continue. *
     *--------------------------------------------------------------------*/

    else {
      *(p++) = (char) c;
      len++;
    }

    /*--------------------------------------------------------------*
     * Reallocate string_const if necessary, get the next character *
     * and loop.						    *
     *--------------------------------------------------------------*/

    if(len >= string_const_len - 3) {
      reallocate_string_const(&p);
    }

    /*------------------------------------------------------------------*
     * Get the next character and echo it. What follows the input line  *
     * is echo_char_l(c), inlined.					*
     *------------------------------------------------------------------*/

    c = input();
    if(should_list()) {            /* begin echo_char_l (inlined) */
      if(c == '\t') echo_tab_l();
      else {
	putc((char) c, LISTING_FILE);
	chars_in_this_line++;
      }
      err_flags.need_err_nl = TRUE; 
    } /* end if(should_list()) */  /* end echo_char_l */
  } /* end while(TRUE) */

out:

  if(c == 0) syntax_error(STRING_EOF_ERR,  start_line);

  /*-----------------------------------------------*
   * Zero-terminate string_const and build yylval. *
   *-----------------------------------------------*/

  *p = '\0';
  yylval.ls_at.name = string_const_tb0((char *) string_const);
  reading_string = FALSE;

# ifdef DEBUG
    if(trace_lexical) tracenl();
# endif

}


/****************************************************************
 *			READ_LONG_COMMENT_L			*
 ****************************************************************
 * Read a comment that starts with <<<<<... and ends with	*
 * ...>>>>>.							*
 *								*
 * Nothing has been echoed yet. If echo_comment_start is true,  *
 * then echo the <<<< symbol in yytext.  Otherwise, do not.	*
 *								*
 * When %> is encountered, return, but first tell the lexer     *
 * to resume a long comment when a newline is encountered.	*
 ****************************************************************/

static void read_long_comment_l(Boolean echo_comment_start)
{
  int greater_count;

  /*--------------*
   * Echo <<<<... *
   *--------------*/

  if(echo_comment_start) echo_token_l(yytext);

  /*--------------------------------------------------*
   * Scan over characters until >>>>  or %> is seen.  *
   *--------------------------------------------------*/

  while(TRUE) {
    register int c = input();

    /*-----------------------------------------------------------*
     * If this is the end of the file, stop reading the comment. *
     * Otherwise, echo c.					 *
     *-----------------------------------------------------------*/

    if(c == EOF) return;
    echo_char_line_l(c);

    /*----------------*
     * Check for %>   *
     *----------------*/

    while(c == '%') {
      c = input(); if(c == EOF) return; echo_char_line_l(c);
      if(c == '>') {
	current_resume_long_comment_at_eol = TRUE;

        /*-------------------------------------------------------*
	 * We need to do an indentation check before going back  *
	 * to lexer.  Skip over blanks and tabs, and then check. *
	 *-------------------------------------------------------*/

        while(TRUE) {
	  c = input();
	  if(c == EOF) break;
	  if(c == ' ' || c == '\t') echo_char_l(c);
	  else break;
        }
        UNPUT(c);
        if(err_flags.check_indentation) check_indentation_l();
        return;
      }
    }

    /*----------------*
     * Check for >>>> *
     *----------------*/

    if(c != '>') continue;

    greater_count = 0;
    while(TRUE) {
      greater_count++;
      c = input(); 
      if(c == '>') echo_char_line_l(c);
      else break;
    }

    /*------------------------------------------------------*
     * If greater_count >= 4, we have read >>>>, so we must *
     * return.  Note that the last character read is not a  *
     * '>', and has not been echoed.  It should be unput.   *
     *							    *
     * If greater_count is less than 4, we have read a 	    *
     * character that needs to be echoed.		    *
     *------------------------------------------------------*/

    if(greater_count >= 4) {
      UNPUT(c);   /* Character just after last > */
      current_resume_long_comment_at_eol = FALSE;
      return;
    }
    else {
      if(c != EOF) echo_char_line_l(c);
    }

  } /* end while(TRUE) */
}
