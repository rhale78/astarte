/****************************************************************
 * File:    parser/parsvars.c
 * Purpose: Globals for the parser.
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

#include "../misc/misc.h"
#include "../parser/parser.h"


/************************************************************************
 *			choose_st					*
 *			case_kind_st					*
 ************************************************************************
 * Stack choose_st controls how choose and loop expressions are 	*
 * processed.  See the description in parser.y under chooseExpr and	*
 * loopExpr to see how it is used.					*
 *									*
 * Stack case_kind_st holds information about a case kind within a 	*
 * choose or loop.							*
 ************************************************************************/

CHOOSE_INFO_STACK choose_st    = NULL;
INT_STACK	  case_kind_st = NULL;


/************************************************************************
 *			var_context_st					*
 ************************************************************************
 * The top of var_context_st is 1 if we are immediately inside a var    *
 * declaration that is part of a class, and is 0 if inside a var	*
 * declaration or expression that is not part of a class.  This is used *
 * to determine what to do when building the expression of a var	*
 * declaration.								*
 ************************************************************************/

INT_STACK	var_context_st = NULL;

/************************************************************************
 *			class_vars					*
 *			class_consts					*
 *			class_expects					*
 *			class_lets					*
 *			initializer_for_class				*
 ************************************************************************
 * class_vars holds a list of EXPR nodes, each describing one variable  *
 *            whas declared in a class.  The node is a SAME_E node 	*
 *            whose STR field holds the name of the variable, whose	*
 *	      type field holds the type, and whose SAME_E_DCL_MODE 	*
 *	      field holds the declaration mode.				*
 *									*
 * class_consts holds a list of EXPR nodes, each describing a constant  *
 *              that is declared in a class.  The fields of the node    *
 *		hold values similar to those in list class_vars.	*
 *									*
 * class_expects holds a list of EXPR nodes, each describing one 	*
 *		 expectation or anticipation that is part of a class.	*
 *		 The nodes hold fields similar to those of the nodes 	*
 *		 in list class_vars, but additionally have the SAME_MODE*
 *		 filed set to 8 for an expect declaration, and to	*
 *		 9 for an anticipation.					*
 *									*
 * initializer_for_class is an expression that is used to create the    *
 * 		         variables of an object and give them names.	*
 ************************************************************************/

EXPR_LIST* class_vars            = NULL;
EXPR_LIST* class_consts          = NULL;
EXPR_LIST* class_expects         = NULL;
EXPR*      initializer_for_class = NULL;

/************************************************************************
 *			defining_id					*
 ************************************************************************
 * defining_id is set to an identifier that is being defined in a let	*
 * or define declaration.  It is used to add an id			*
 * to the bad_compile_ids at a compilation error.			*
 *									*
 * When no identifier is being defined, or the id being defined is	*
 * unknown, defining_id is NULL.					*
 ************************************************************************/

char* defining_id = NULL;

/************************************************************************
 *			deflet_st					*
 *			deflet_line_st					*
 *			defining_id_st					*
 ************************************************************************
 * These stacks are used to control processing of let and define	*
 * declarations and expressions.  See parser.y under letDcl and		*
 * letExpr for a description of how they are used.			*
 ************************************************************************/

INT_STACK	deflet_st      = NULL;
INT_STACK	deflet_line_st = NULL;
STR_STACK	defining_id_st = NULL;

/************************************************************************
 *			num_params					*
 ************************************************************************
 * num_params is used during processing of let-by declarations.  It	*
 * is set to -1 before the first case is read, and thereafter to the	*
 * number of (curried) parameters of the function definition cases.	*
 * It is used to ensure that all cases have the same number of		*
 * parameters.								*
 ************************************************************************/

int num_params;

/************************************************************************
 *			list_expr_st					*
 *			list_map_st					*
 ************************************************************************
 * See parser.y under listExpr for a description of these stacks.	*
 ************************************************************************/

INT_STACK	list_expr_st = NULL;
EXPR_STACK	list_map_st  = NULL;

/************************************************************************
 *			new_import_ids					*
 ************************************************************************
 * When an import declaration is read, new_import_ids is set to the	*
 * list of ids that occur before 'from' in the declaration.  For	*
 * example, in declaration						*
 *									*
 *		Import x,y,z from "mypackage".				*
 *									*
 * new_import_ids is set to ["z", "y", "x"].				*
 *									*
 * If there is no 'from', then new_import_ids is set to (LIST*) 1.	*
 ************************************************************************/

STR_LIST*  new_import_ids = ((LIST*) 1);


/************************************************************************
 *			expect_context					*
 ************************************************************************
 * Set during processing of an expect or anticipate declaration.  It    *
 * is EXPECT_ATT for an expect declaration, and ANTICIPATE_ATT for	*
 * an anticipate declaration.						*
 ************************************************************************/

int expect_context;

/************************************************************************
 *			union_context					*
 ************************************************************************
 * union_context is set during a species, genus or community		*
 * declaration to the kind of declaration.  It is either TYPE_DCL_CX or	*
 * RELATE_DCL_CX.							*
 ************************************************************************/

int union_context;

/************************************************************************
 *			index_file					*
 *			index_file_dir					*
 *			description_file				*
 *			wrote_description				*
 *			root_file_name					*
 ************************************************************************
 * index_file is the file to which we are writing index information.    *
 * It is file "indx.asx".						*
 *									*
 * index_file_dir is the directory where index_file is located.		*
 * 									*
 * description_file is the file into which descriptions are being	*
 * written.  If we are compiling file "thisfile.ast", then the		*
 * description file is "thisfile.asd".					*
 *									*
 * wrote_description is set true if something was written into 		*
 * description_file.							*
 *									*
 * root_file_name is the name that was written into indx.asx for this   *
 * file.  It is the root of the name of the file being compiled,        *
 * without an extension, and without directory.  This file name is	*
 * in internal format.							*
 ************************************************************************/

FILE*   index_file 	  = NULL;
char*   index_file_dir;
FILE*   description_file  = NULL;
Boolean wrote_description = 0;
char*   root_file_name 	  = NULL;

/************************************************************************
 *			main_file_name					*
 ************************************************************************
 * main_file_name is the full path name of the file being compiled.	*
 * This name is in internal form, meaning that character		*
 * INTERNAL_DIR_SEP_CHAR is used to separate directories.		*
 ************************************************************************/

char* main_file_name = NULL;

/************************************************************************
 *			have_read_main_package_name			*
 ************************************************************************
 * have_read_main_package_name is set true after reading the name of    *
 * the package being compiled.  If a syntax error occurs before this,   *
 * and we are not reading the standard package, it is presumed that the *
 * error is a bad heading.						*
 ************************************************************************/

Boolean have_read_main_package_name = FALSE;

/************************************************************************
 *			about_to_load_interface_package			*
 ************************************************************************
 * about_to_load_interface_package is true when a file being pushed     *
 * is the interface package for an implementation.			*
 ************************************************************************/

Boolean about_to_load_interface_package = FALSE;

/************************************************************************
 *			subcases_context				*
 ************************************************************************
 * Set to 1 by the lexer when it reads subcases.  Normally set back to	*
 * 0.  Used to recognize when nonterminal 'which' immediately follows   *
 * word 'subcases'.							*
 ************************************************************************/

Boolean subcases_context = 0;

/************************************************************************
 *			dcl_context					*
 ************************************************************************
 * 1 when processing a declaration, 0 when processing an expression.	*
 ************************************************************************/

Boolean dcl_context;


/************************************************************************
 *			inside_extension				*
 ************************************************************************
 * 1 when processing an extend declaration, 0 otherwise.		*
 ************************************************************************/

Boolean inside_extension = FALSE;


/************************************************************************
 *			interface_package				*
 ************************************************************************
 * True when reading an interface package.				*
 ************************************************************************/

Boolean interface_package;


/************************************************************************
 *			seen_export					*
 ************************************************************************
 * True in a package that has an export part.				*
 ************************************************************************/

Boolean seen_export = 0;


/************************************************************************
 *			compiling_standard_asi				*
 ************************************************************************
 * True when compiling standard.asi.  Some things are done differently	*
 * for that package than for others.					*
 ************************************************************************/

Boolean compiling_standard_asi;


/************************************************************************
 *			show_all_types					*
 ************************************************************************
 * show_all_types is							*
 *   1  if have seen an Advise show all %Advise declaration		*
 *   2  if have seen an Advise showlocals all %Advise declaration	*
 *   0  if neither of those are active.					*
 ************************************************************************/

UBYTE show_all_types = 0;


/************************************************************************
 *			echo_all_exprs					*
 ************************************************************************
 * echo_all_exprs is							*
 *   1  if have seen an Advise echo all %Advise declaration		*
 *   0  otherwise.							*
 ************************************************************************/

Boolean echo_all_exprs = 0;


/************************************************************************
 *			no_copy_abbrev_vars				*
 ************************************************************************
 * This variable controls how species abbreviations are handled.  	*
 * Normally, when an abbreviation is fetched, its variabls are copied.	*
 * When this variable is true, such copying is suppressed.		*
 ************************************************************************/

Boolean no_copy_abbrev_vars = 0;


/************************************************************************
 *			pat_rule_opts_ok				*
 ************************************************************************
 * pat_rule_opts_ok is true in a pattern declaration, where tag modes	*
 * are acceptable, and false elsewhere.					*
 ************************************************************************/

Boolean pat_rule_opts_ok = FALSE;

/************************************************************************
 *			open_pat_dcl					*
 ************************************************************************
 * This variable is set when a special pattern declaration is read.	*
 * Such declarations are only allowed in standard.ast, since they	* 
 * are quite unsafe if misused.  They are used for functions such as	*
 * orelse.								*
 ************************************************************************/

Boolean open_pat_dcl = 0;


/************************************************************************
 *			pat_dcl_has_pat_consts				*
 ************************************************************************
 * This is set true when reading a declaration of a pattern constant.   *
 ************************************************************************/

Boolean pat_dcl_has_pat_consts;


/************************************************************************
 *			stop_at_first_err				*
 ************************************************************************
 * If this variable is true, then the compiler should stop after the    *
 * first declaration that causes an error.				*
 ************************************************************************/

Boolean stop_at_first_err = FALSE;


/************************************************************************
 *			dcl_pat_fun					*
 ************************************************************************
 * When declaring a pattern function, this variable is set to hold the  *
 * pattern function being defined.					*
 ************************************************************************/

EXPR* dcl_pat_fun = NULL;

/************************************************************************
 *			outer_context					*
 ************************************************************************
 * The context (EXPORT_CX or BODY_CX) of the main package at the	*
 * current time.  It starts at EXPORT_CX, and will only be changed	*
 * to BODY_CX when the implementation part is started.			*
 ************************************************************************/

int outer_context = EXPORT_CX;




