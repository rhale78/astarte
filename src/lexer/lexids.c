/****************************************************************
 * File:    lexer/lexids.c
 * Purpose: Handling identifiers in the lexer.
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
 * This file contains functions for handling identifiers in the		*
 * lexer.								*
 ************************************************************************/

#include <string.h>
#include <ctype.h>
#include "../misc/misc.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../lexer/lexer.h"
#include "../standard/stdids.h"
#include "../parser/parser.h"
#include "../parser/tokens.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../classes/classes.h"
#include "../error/error.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
extern char yytext[];
extern int yyleng;


/****************************************************************
 *			GET_MY_ID_L				*
 ****************************************************************
 * String text consists of "my" or "My" or "my-" or "My-"	*
 * followed by some white space (for "my" or "My") followed by  *
 * an identifier x.  Return string my-x (if x does not start    *
 * with an upper case letter) or My-x (if x starts with an	*
 * upper case letter).						*
 *								*
 * The result string is in the id table.			*
 ****************************************************************/

char* get_my_id_l(char *text)
{
  char *p, *pref;
  p = text + 2;
  if(*p == '-') p++;
  else {
    while(isspace(*p)) p++;
  }
  pref = (*p >= 'A' && *p <= 'Z') ? "My-" : "my-";
  return concat_id(pref, p);
}


/****************************************************************
 *			NORMAL_ID_L				*
 ****************************************************************
 * Return the token for an entity identifier whose lexeme is	*
 * 'name'.							*
 ****************************************************************/

/*-------------------------------------------------------------*
 * get_is_paren_ahead gets and memoizes in *is_paren_ahead the *
 * status of what follows the current token.                   *
 *-------------------------------------------------------------*/

PRIVATE int get_is_paren_ahead(int *is_paren_ahead)
{
  if(*is_paren_ahead < 0) *is_paren_ahead = paren_ahead();
  return *is_paren_ahead;
} 

/*-------------------------------------------------------------*/

int normal_id_l(char *name)
{
  int op, is_paren_ahead;
  char *op_name;

  /*---------------------------------------*
   * If this id is too long, truncate it.  *
   *---------------------------------------*/

  if(strlen(name) > MAX_ID_SIZE) {
    syntax_error(ID_TOO_LONG_ERR, 0);
    name[MAX_ID_SIZE + 1] = 0;
  }

  /*------------------------------------------------------*
   * Check for a type id.  If this is a type id, then let *
   * class_id_l get its value.				  *
   *------------------------------------------------------*/

  {char* newname = new_name(name, FALSE);
   if(get_ctc_tm(newname) != NULL) {
     return class_id_l(newname, strlen(newname), FALSE);
   }
  }

  /*----------------------------------------*
   * Intern the name into the string table. *
   *----------------------------------------*/

  name  = id_tb0(name);

# ifdef DEBUG
    if(trace_lexical) {
      trace_t(326, name, toint(force_no_op), toint(opContext));
    }
# endif

  /*--------------------------------------------------------------------*
   * rest_context_l sets up yylval.ls_at.line, yylval.ls_at.column.     *
   *									*
   * If open_col is nonnegative, then it is the column where the word 	*
   * 'open' starts just before this word.  That column should be taken	*
   * as the column of this identifier.	(Presumably, this is a 		*
   * procedure identifier.)						*
   *--------------------------------------------------------------------*/

  rest_context_l(RBRACE_TOK);
  yylval.ls_at.name = name;
  if(open_col >= 0) yylval.ls_at.column = open_col;
  open_col = -1;

  /*------------------------------------------------------------------*
   * is_paren_ahead tells whether the value ahead is a ) or such.  It *
   * is -1 if no information has been obtained yet. 		      *
   *------------------------------------------------------------------*/

  is_paren_ahead = -1;

  /*------------------------------------------------------------*
   * If name has "my-" or "My-" as a prefix, then remove that 	*
   * prefix for the purpose of checking for operators.		*
   *------------------------------------------------------------*/

  op_name = (prefix("my-", name) || prefix("My-", name))
                ? id_tb0(name + 3)
                : name;
 
  /*----------------------------------------------------------*
   * Check for a binary or unary operator, if not suppressed. *
   *----------------------------------------------------------*/

  if(!force_no_op && !long_force_no_op) {

    /*---------------------------------------------------------------*
     * This is a binary operator if it is in a context that admits a *
     * binary operator, and it is not followed by a ) or such, and   *
     * if it has been declared to be a binary operator. 	     *
     *---------------------------------------------------------------*/

    if(opContext) {
      Boolean xx;
      op = operator_code_tm(op_name, &xx);

      /*------------------------------------------------------------*
       * If this is a binary operator, not followed by something    *
       * such as a right parenthesis that would suppress it being   *
       * taken as a binary operator, return it.  If it is BAD_OP,   *
       * then complain, and return L1_OP.			    *
       *							    *
       * A binary operator cannot be followed by a binary operator, *
       * so set opContext = 0 for next token.			    *
       *------------------------------------------------------------*/

      if(op != 0 && !get_is_paren_ahead(&is_paren_ahead)) {
	opContext = 0;
        if(op == BAD_OP) {
          op = L1_OP;
	  semantic_error1(BAD_OP_ERR, display_name(name), 0);
        }
        yylval.ls_at.tok = op;
	return op;
      }
    } /* end if(opContext) */

    /*----------------------------------------------------------------*
     * This is a unary operator if it is not in a context that forces *
     * no operator and it is declared to be a unary operator and the  *
     * next token is not ) or such. 				      *
     *								      *
     * A unary operator cannot be followed by a binary operator.      *
     *----------------------------------------------------------------*/

    if(is_unary_op_tm(op_name) && !get_is_paren_ahead(&is_paren_ahead)) {
      opContext = 0;
      yylval.ls_at.tok = UNARY_OP;
      return UNARY_OP;
    }
  } /* end if(!force_no_op) */

  /*------------------------------------------------------------*
   * Clear force_no_op flag - we are not returning an operator. *
   *------------------------------------------------------------*/

  force_no_op = 0;

  /*------------------------------------------------------------------*
   * This is a procedure identifier if it has the form of a procedure *
   * id and is not followed by ) or such.  Note that a procedure      *
   * id can be followed by a period or end marker, so we ask whether  *
   * paren_ahead returns 1.		 			      *
   *								      *
   * A procedure id cannot be followed by a binary operator. 	      *
   *------------------------------------------------------------------*/

  if(is_proc_id(name) && get_is_paren_ahead(&is_paren_ahead) != 1) {
    opContext = 0;
    push_shadow_str(name);
    yylval.ls_at.tok = PROC_ID_TOK;
    return PROC_ID_TOK;
  }

  /*-------------------------------------------------------------*
   * If not any of the special identifiers, then it must be an   *
   * UNKNOWN_ID_TOK. 						 *
   *-------------------------------------------------------------*/

  else {
    opContext = 1;
    yylval.ls_at.tok = UNKNOWN_ID_TOK;
    return UNKNOWN_ID_TOK;
  }
}


/****************************************************************
 *			SYMBOLIC_ID_L				*
 ****************************************************************
 * Same as normal_id_l, but for a symbolic or standard id.	*
 ****************************************************************/

int symbolic_id_l(int sym)
{
  return normal_id_l(std_id[sym]);
}


/****************************************************************
 *			   SPLIT_ID_L				*
 ****************************************************************
 * Find the root and qualifier of a type or family variable.	*
 ****************************************************************/

PRIVATE void split_id_l(char *id, char **root, char **qualifier)
{
  int i;
  char tempId[MAX_ID_SIZE + 1];

  /*-----------------------------*
   * Copy the root id to tempId. *
   *-----------------------------*/

  for (i = 0; id[i] != '`'; i++) tempId[i] = id[i];
  tempId[i] = '\0';

  /*-------------------------------------*
   * Set the root and qualifier strings. *
   *-------------------------------------*/

  *root = (*tempId == 0) ? "" : new_name(tempId, FALSE);
  *qualifier = id + i;
}


/****************************************************************
 *		  	   QUAL_CLASS_ID_L			*
 ****************************************************************
 * This is the same as CLASS_ID_L, but for qual = 1, except     *
 * that it does not do the shared initial processing of 	*
 * class_id_l.							*
 ****************************************************************/

PRIVATE int qual_class_id_l(char *text)
{
  char *root, *qualifier, tempId[MAX_ID_SIZE + 4];
  int root_tok, result;
  CLASS_TABLE_CELL *c;

  /*--------------------------------------------------------------------*
   * If the id has a qualifier, then split it into its root and its 	*
   * qualifier.  Rebuild the identifier to the form `qual#root. 	*
   * Note: if the root is ANY, make the root empty, so that ANY`x and   *
   * `x will be the same.						*
   *--------------------------------------------------------------------*/

  split_id_l(text, &root, &qualifier);
  if(strcmp(root, std_type_id[ANYG_TYPE_ID]) == 0) root = "";
  sprintf(tempId, "%s#%s", qualifier, root);
  yylval.ls_at.name = id_tb0(tempId);
  yylval.ls_at.line = current_line_number;

  /*--------------------------------------------*
   * If this is `x, then it is a type variable.	*
   *--------------------------------------------*/

  if(*root == 0) {
    result = TYPE_VAR_TOK;
  }

  /*------------------------------------------------------*
   * If this is G`x, then find out what kind of variable. *
   * If the variable domain is an abbreviation, get what  *
   * the abbreviation refers to.			  *
   *------------------------------------------------------*/

  else {
    c  = get_ctc_tm(root);
    root_tok = (c == NULL) ? UNKNOWN_ID_TOK : MAKE_TOK(c->code);
    if      (root_tok == GENUS_ID_TOK) {
      result = TYPE_VAR_TOK;
    }
    else if (root_tok == COMM_ID_TOK) {
      result = FAM_VAR_TOK;
    }
    else if (root_tok == GENUS_ABBREV_TOK || root_tok == COMM_ABBREV_TOK) {
      sprintf(tempId, "%s#%s", qualifier, c->ty->ctc->name);
      yylval.ls_at.name = id_tb0(tempId);
      result = (root_tok == COMM_ABBREV_TOK) ? FAM_VAR_TOK : TYPE_VAR_TOK;
    }
	
    else {

      /*------------------------------------------------------*
       * We have G`x, where G is not a community or genus.    *
       * Complain, and replace it by `x.  		      *
       *------------------------------------------------------*/

      syntax_error1(QUAL_ERR, display_name(root), 0);
      sprintf(tempId, "%s#", qualifier);
      yylval.ls_at.name = id_tb0(tempId);
      result = TYPE_VAR_TOK;
    }    
  } /* end else(*root != 0) */

  return result;
}


/****************************************************************
 *		  	   UNQUAL_CLASS_ID_L			*
 ****************************************************************
 * This is the same as CLASS_ID_L, but for qual = 0, except     *
 * that it does not do the shared initial processing of 	*
 * class_id_l.							*
 ****************************************************************/

PRIVATE int unqual_class_id_l(char *text)
{
  char *root;
  int root_tok;
  CLASS_TABLE_CELL *c;

  /*------------------------------------------------------------*
   * If not qualified, then this is a genus, community, species *
   * or family id, or an abbreviation.	Find out what it is.    *
   *------------------------------------------------------------*/

  root     = new_name(text, FALSE);
  c        = get_ctc_tm(root);
  root_tok = (c == NULL) ? UNKNOWN_ID_TOK : MAKE_TOK(c->code);

  /*-----------------------------------------------*
   * TYPE_ID_TOK, FAM_ID_TOK: lval has kind ls_at. *
   *-----------------------------------------------*/

  if(root_tok == TYPE_ID_TOK || root_tok == FAM_ID_TOK) {
    yylval.ls_at.name = root;
    yylval.ls_at.line = current_line_number;
  }

  /*---------------------------------------------*
   * UNKNOWN_CLASS_ID_TOK: lval has kind str_at. *
   *---------------------------------------------*/

  else if(root_tok == UNKNOWN_ID_TOK) yylval.str_at = root;

  /*---------------------------------------------------------------------*
   * TYPE_ABBREV_TOK, FAM_ABBREV_TOK, FAM_MACRO_TOK, FAM_VAR_ABBREV_TOK: *
   * lval has kind ntype_at. 						 *
   *---------------------------------------------------------------------*/

  else if(TYPE_ABBREV_TOK <= root_tok  && root_tok <= FAM_VAR_ABBREV_TOK) {
    bump_type(yylval.ntype_at.ty = c->ty);
    yylval.ntype_at.name = root;
    if(root_tok == TYPE_ABBREV_TOK || root_tok == FAM_MACRO_TOK) {
      SET_ROLE(abbrev_tok_role, c->role);
    }
  }

  /*------------------------------------------------------------------*
   * GENUS_ABBREV_TOK, COMM_ABBREV_TOK: lval has kind ctc_at, but   *
   * get the class-table-cell for the id being abbreviated, not the   *
   * one for the abbreviation id itself. 			      *
   *------------------------------------------------------------------*/

  else if(root_tok > TYPE_ABBREV_TOK) {
    yylval.ctc_at  = c->ty->ctc;
  }

  /*----------------------------------------------------*
   * COMM_ID_TOK, GENUS_ID_TOK: lval has kind ctc_at.   *
   *----------------------------------------------------*/
  
  else {
    yylval.ctc_at  = c;
  }

  return root_tok;

}


/****************************************************************
 *		  	   CLASS_ID_L				*
 ****************************************************************
 * Return the token for a species, family, genus or community	*
 * id, if  qual = 0, or for a species or family variable if	*
 * qual = 1.  							*
 *								*
 * Also set the attributes for the token.			*
 *								*
 * Handle renaming of ids for ids defined in the package 	*
 * implementation.  						*
 *								*
 * text is the lexeme, and textlen is its length.		*
 ****************************************************************/

int class_id_l(char *text, int textlen, Boolean qual)
{
  /*---------------------------------------*
   * If this id is too long, truncate it.  *
   *---------------------------------------*/

  if(textlen > MAX_ID_SIZE) {
    syntax_error(ID_TOO_LONG_ERR, 0);
    text[MAX_ID_SIZE + 1] = 0;
  }

  one_context_l(0);

  if(qual) return qual_class_id_l(text);
  else return unqual_class_id_l(text);
}

