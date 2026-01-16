/****************************************************************
 * File:    tables/globtbl.c
 * Purpose: Translator table manager for global ids.
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

#include <string.h>
#include <memory.h>
#include "../misc/misc.h"
#include "../tables/tables.h"
#include "../dcls/dcls.h"
#include "../exprs/expr.h"
#include "../exprs/prtexpr.h"
#include "../classes/classes.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../unify/unify.h"
#include "../unify/constraint.h"
#include "../error/error.h"
#include "../lexer/modes.h"
#include "../parser/parser.h"
#include "../patmatch/patmatch.h"
#include "../generate/prim.h"
#include "../generate/generate.h"
#include "../infer/infer.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


PRIVATE Boolean def_global_tm(EXPR *id, TYPE *ty,
			      MODE_TYPE *mode, GLOBAL_ID_CELL *gic, 
			      EXPECTATION *exp,
			      LIST *who_sees, Boolean from_expect,
			      EXPR_TAG_TYPE kind, int line,
			      char *def_package_name);
PRIVATE Boolean def_entity_tm(EXPR *id, TYPE *ty,
			      MODE_TYPE *mode, GLOBAL_ID_CELL *gic, 
			      EXPECTATION *exp, LIST *who_sees,
			      Boolean from_expect, int line,
			      char *def_package_name);
PRIVATE Boolean def_expander_tm(EXPR *id, TYPE *ty,
				MODE_TYPE *mode, GLOBAL_ID_CELL *gic, 
				LIST *who_sees,
				int kind, int line);
PRIVATE void 
install_patfun_expectation_tm(GLOBAL_ID_CELL *gic, TYPE *t, char *name, 
			       STR_LIST *who_sees);

PRIVATE void warn_if_hidden_expectation(char *name, TYPE *t, Boolean check);

/****************************************************************
 * 			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			suppress_expectation_descriptions	*
 ****************************************************************
 * Normally, when an identifier is expected in the export part  *
 * of a package, an entry is made in the .asd file indicating   *
 * that the identifier is defined here.				*
 *								*
 * suppress_expectation_descriptions should be set true to	*
 * suppress this action.					*
 *								*
 * XREF: This is set in the parser during an expect declaration	*
 * to suppress duplicate entries in the .asd file.		*
 ****************************************************************/

Boolean suppress_expectation_descriptions = FALSE;

/****************************************************************
 *			disallow_polymorphism			*
 ****************************************************************
 * This variable is set true to disallow a polymorphic		*
 * declaration, as in a var declaration. 			*
 *								*
 * XREF: This variable is set in the parser during a var	*
 * declaration.							*
 ****************************************************************/

Boolean disallow_polymorphism = FALSE;     

/****************************************************************
 *			copy_expect_types			*
 ****************************************************************
 * This variable is set true if should copy types at expect dcl.*
 * See expect_global_id_tm.					*
 *								*
 * (Note: normally, types are copied.  However, during 		*
 * initialization, it is not necessary to copy, and suppressing *
 * the copy saves time.)					*
 ****************************************************************/

Boolean copy_expect_types = TRUE;  

/****************************************************************
 *			force_prim				*
 ****************************************************************
 * Normally, when a default definition is made, no primitive    *
 * information is stored, since the primitive might be		*
 * overridden.  If force_prim is TRUE, then primitive info will *
 * be installed even for defaults.				*
 *								*
 * XREF: This is used in stdfuns.c to force primitive		*
 * information into underriding definitions.			*
 ****************************************************************/

Boolean force_prim = FALSE;

/************************************************************************
 *			global_id_table					*
 ************************************************************************
 * global_id_table is only use here and in tables/tableman.c to		*
 * initialize the table.						*
 *									*
 * Table global_id_table is a hash table associating a 			*
 * GLOBAL_ID_CELL with each identifier defined in the outer		*
 * environment.  Each global id cell provides information about the	*
 * expectations and definitions of the identifier.			*
 *									*
 * One feature of the global_id_table that makes it almost unreasonably	*
 * complicated is management of imports.  Suppose, for example, that    *
 * package A contains							*
 *									*
 *   Import "B".							*
 *   Import "C".							*
 *									*
 * The declarations made in package B should not be visible when 	*
 * package C is being read, to prevent them from affecting how package	*
 * C is compiled.  However, definitions made by package B are present	*
 * in global_id_table.  To prevent them from being seen, various	*
 * pieces of information are marked by the packages that can see them.  *
 * See entry 'visible_in' below, for various parts.			*
 * 									*
 * The entries in a global id cell are as follows.			*
 *									*
 * expectations	A chain of expectations for this identifier.  See	*
 *		EXPECTATION below for a description of this chain.	*
 *									*
 * container	A polymorphic type that contains all expectation	*
 *		types for this id.  This is used in type inference in	*
 *		the initial stage, before overloading is looked at, to	*
 *		limit search in	the overload stage.  			*
 *									*
 *									*
 * entparts	A chain of definitions for this identifier.  See	*
 *		ENTPART below for a description of this chain.		*
 *									*
 * expand_info	Information about pattern and expand rules for this	*
 *		id.  See EXPAND_INFO below.				*
 *									*
 * descr_chain	A chain of descriptions for this id.  See DESCRIP_CHAIN	*
 *		below for a description of this chain.			*
 *									*
 * role_chain	A chain of roles for this id.  See ROLE_CHAIN below	*
 *		for a description of this chain.			*
 *									*
 * restriction_types This list can contain a list of polytypes for an	*
 *		     irregular function.  The types in this list	*
 *		     indicate information about how the function	*
 *		     actually behaves.  For example, type 		*
 *		     Natural -> Natural would indicate that, when  	*
 *		     the argument has type Natural, the result has type *
 *		     natural as well.  The restriction list for id	*
 *		     forget is [`a -> `a], indicating the the result	*
 *		     always has the same type as the argument.		*
 *									*
 *		     Restriction types are visible to all packages, so  *
 *		     that the compiler can make use of their 		*
 *		     information, even in packages where they might	*
 *		     normally be invisible.				*
 *									*
 *									*
 *======================================================================*
 *			EXPECTATION					*
 *									*
 * The cells in the expectation chain represent disjoint polymorphic	*
 * types that are the expectations of this identifier.  Two cells	*
 * can have overlapping types only if they do not have overlapping	*
 * visibility.								*
 *									*
 * Each cell in the expectation chain for an id contains the following  *
 * fields, describing an expectation of this identifier.		*
 *									*
 * type		The (polymorphic) type of this expectation.  		*
 *									*
 * package_name	Name of the package where this expectation occurs.	*
 *		Generally, it is the package where the most		*
 * 		general expectation represented by this cell		*
 *		occurred.  This name is in the string table, so package *
 *		names can be compared for equality using ==.		*
 *									*
 * line_no	Line where this expecation occurs.			*
 *									*
 * visible_in	Not every cell in the chain is visible to every		*
 *		package.  For packages other than standard, visible_in	*
 *		is a list of names of packages in which this		*
 *		expectation is visible.  All strings in visible_in are	*
 *		in the string table.	 				*
 *									*
 *		When package_name is "standard", visible_in is a list 	*
 *		of those packages that CANNOT see this expectation.	*
 *		So, when an expectation is installed by the standard	*
 *		package (or by the compiler preamble, where primitives	*
 *		are installed) that expectation will have a visible_in	*
 *		field of NIL, indicating that all packages can see it.	*
 *		Packages can be deleted from the visibility list for	*
 *		an expectation when that expectation is generalized	*
 *		and taken over by another package.			*
 *									*
 * irregular	False normally, true for an irregular expectation.	*
 *									*
 * old		This bit is set to 1 if this cell existed during a	*
 *		pass of type inference.  It is used to recognize	*
 *		cells that have been installed during the current	*
 *		declaration.						*
 *									*
 * in_body	This bit is set to 1 in an expectation that was placed  *
 *		by the implementation part of the main package.		*
 *									*
 * next		The next cell in the chain.  				*
 *									*
 *======================================================================*
 * 			ENTPART						*
 *									*
 * Each ENTPART cell describes a definition of this identifier.	The	*
 * types of different cells in the chain can overlap, provided		*
 * defaults/overrides are used.						*
 *									*
 * The primitive and arg fields go together.  They describe the 	*
 * instruction that does a primitive operation.  In the following,	*
 * by parameter, we mean an entity that is on the stack when an		*
 * instruction is executed.  By instruction-argument, we mean additional*
 * information associated with the instruction itself.			*
 *									*
 * primitive	0 	   if not primitive or standard thing.		*
 *									*
 * 		PRIM_CONST if a primitive constant. arg is the		*
 *		     	   instruction that loads the constant.		*
 *									*
 *		PRIM_FUN   if a primitive function with one parameter.  *
 *			   Arg is instruction that computes this	*
 *			   primitive.					*
 *									*
 *		PRIM_OP    if a primitive function with two parameters. *
 *			   Arg is the instruction that computes this	*
 *			   primitive.					*
 *									*
 *		TY_PRIM_FUN if a primitive function with one parameter	*
 *			    that also has an instruction-argument 	*
 *			    giving the offset in the global environment *
 *			    that holds the type of this function.	*
 *			    Arg is the instruction.			*
 *									*
 *		TY_PRIM_OP  a primitive function with two parameters	*
 *			    that also has an instruction-argument	*
 *			    giving the offset in the global environment *
 *			    that holds the type of this function.	*
 *			    Arg is the instruction.			*
 *									*
 *		STD_CONST  if a standard constant.  Arg is the value    *
 *			   of the constant, which must be between	*
 *			   0 and 255.					*
 *									*
 *		STD_BOX    if a standard box.  Arg is the number of 	*
 *			   the box.					*
 *									*
 *		EXCEPTION_CONST if an exception.  Arg is the global	*
 *			   label where this exception is declared.	*
 *									*
 *		STD_FUN    if a unary function that wants its argument  *
 *			   fully evaluated.  This must be an instruction*
 *			   that follows UNARY_PREF_I.  Arg is the	*
 *			   instruction number.				*
 *									*
 *		STD_OP     if a standard binary function that wants its *
 *			   arguments fully evaluated.  This must be	*
 *			   an instruction that follows BINARY_PREF_I.	*
 *			   Arg is the number of the instruction.	*
 *									*
 *		LIST_FUN   if a unary function that does not want 	*
 *			   its argument evaluated.  This must be an 	*
 *			   instruction that follows LIST1_PREF_I.	*
 *			   Arg is the number of the instruction.	*
 *									*
 *		LIST_OP	   if a binary function that does not want	*
 *			   its arguments evaluated.  This must be an	*
 *			   instruction that follows LIST2_PREF_I.	*
 *			   Arg is the number of the instruction.	*
 *									*
 *		HERM_FUN   An instruction that takes () as a parameter. *
 *			   (() is popped before the instruction is 	*
 *			   done.)  Arg is the instruction.		*
 *									*
 *		PRIM_CAST  if a cast (no computation).			*
 *									*
 *		PRIM_ENUM_CAST If a cast, except that it has an		*
 *			   argument telling an upper bound for casting	*
 *			   It casts a natural number to some enumerated *
 *			   species, but fails on a number that is too	*
 *			   large.					*
 *									*
 *		PRIM_WRAP  if a wrapper.  (This primitive wraps a value *
 *			   with its species using WRAP_I.		*
 *									*
 *		PRIM_UNWRAP if an unwraper.  (This primitive unwraps	*
 *			    using UNWRAP_I.)				*
 *									*
 *		PRIM_QWRAP if a quasi-wrap.  Arg is the tag.		*
 *									*
 *		PRIM_QUNWRAP if a quasi-unwrap.  Arg is the target	*
 *			     tag.  (Only succeeds if parameter has this *
 *			     tag.)					*
 *									*
 *		PRIM_DWRAP  if a double-wrapper.  (Combination of	*
 *			    PRIM_WRAP and PRIM_QWRAP.)			*
 *									*
 *		PRIM_DUNWRAP if a double-unwrapper.  (Combination of	*
 *			     PRIM_UNWRAP and PRIM_QUNWRAP.)		*
 *									*
 *		PRIM_QTEST if a tag tester.  Arg is the tag that is	*
 *			   tested for.					*
 *									*
 *		PRIM_QEQ   if equality tester, but can be applied using *
 *			   QEQ_APPLY_I rather than  REV_APPLY.		*
 *									*
 *		PRIM_EXC_WRAP  if a constructor for a complex exception.*
 *			       Arg is the global label where the	*
 *			       exception is defined.			*
 *									*
 *		PRIM_EXC_UNWRAP if a destructor for a complex exception.*
 *			        Arg is the global label where the	*
 *			        exception is defined.			*
 *									*
 *		PRIM_EXC_TEST   if a tester for an exception.  Arg is	*
 *				the global label where the exception is *
 *				defined.				*
 *									*
 *		PRIM_NOT_OP	if an instruction that should be followed
 *				by NOT_I.  Arg is the instruction.	*
 *									*
 *		PRIM_SELECT	if a selector.  Extra information	*
 *			        includes:				*
 *				  bit irregular, which tells		*
 *				    whether the objectbeing selected	*
 *				    from is wrapped with a type;	*
 *				  arg, which is the tag when the 	*
 *				    object being selected from has an	*
 *				    integer tag and is -1 otherwise;	*
 *				  a selection list kept in 		*
 *				    selection_info.  Selection_info is	*
 *				    a list such as			*
 *				    [LEFT_SIDE, LEFT_SIDE, RIGHT_SIDE], *
 *				    which calls for taking the left of  *
 *				    the left of the right of the object.*
 *									*
 *		PRIM_MODIFY	if a modifier.  A modifier takes two	*
 *				objects on the stack.  The top is an	*
 *				object y to be inserted into another	*
 *				object x, which is just below y in the	*
 *				stack. Additional information is as for	*
 *				PRIM_SELECT. It tells where y is to be	*
 *				put inside x.				*
 *									*
 * mode		The low order byte of the define_mode of the definition.*
 *									*
 * from_expect  0 normally, 1 if this part was generated by an expect 	*
 *		declaration in an import (and so can be replaced by a   *
 *		definition in the same import).				*
 *									*
 * trapped	0 normally, 1 if this part is an exception that was	*
 *		declared to be trapped by default.			*
 *									*
 * irregular	0 normally, 1 if this is a PRIM_SELECT primitive that	*
 *		does an unwrap.						*
 *									*
 * hidden	0 normally, 1 if this definition has been marked 	*
 *		hidden by missing{hide}.				*
 *									*
 * ty		The (polymorphic) type of this definition.		*
 *									*
 * package_name	The name of the package that issues this definition.	*
 *		This string is in the string table.			*
 *									*
 * attributed_package_name The name of the package to which this 	*
 *			   definition is attributed.  Normally, this 	*
 *			   is the same as package_name.  When this 	*
 *			   definition is from an expectation and its	*
 *			   mode contains 'from pkg', then the attributed*
 *			   package is pkg.				*
 *									*
 * line_no	The line number where this definition occurs.		*
 *									*
 * selection_info Selection list for PRIM_SELECT or PRIM_MODIFY.  	*
 *		  This is a list of the values LEFT_SIDE and RIGHT_SIDE	*
 *		  List [RIGHT_SIDE, LEFT_SIDE] indicates that this	*
 *		  id is a selector that takes the right of the left	*
 *		  of its argument.					*
 *									*
 * visible_in	A list of the packages that can see this part, if	*
 *		package_name is not "standard", and a list of 		*
 *		packages that cannot see this part, if package_name	*
 *		is "standard".  The strings in this list are in the	*
 *		string table.						*
 *									*
 * next		The next part in the chain.				*
 *									*
 * =====================================================================*
 *			EXPAND_INFO					*
 *									*
 * The expand_info field of a global id cell is either NULL, indicating	*
 * that there is no expand information, or points to an EXPAND_INFO	*
 * node containing the following fields.				*
 *									*
 * patfun_expectations	This is a chain of cells that give expectations	*
 *			for pattern functions.  During type inference	*
 *			we need to know what type to associate with	*
 *			a pattern function.  This table gives us that	*
 *			information.  Note that there must be a rule	*
 *			with the same type as each expectation here,	*
 *			since when it comes time to do the translations,*
 * 			a restrictive rule cannot be selected.		*
 *									*
 *			Each node in this chain has a type, a		*
 *			visibility list and a package name, with the	*
 *			usual interpretation of visibility lists.	*
 *									*
 * patfun_rules		This is a chain of expansion rules for this 	*
 *			id as a pattern function.  See EXPAND_PART	*
 *			below for details about this chain.		*
 *									*
 * expand_rules		This is a chain of expansion rules for this	*
 *			id as an ordinary function.  See EXPAND_PART	*
 *			below for details about this chain.		*
 *									*
 * =====================================================================*
 * 			EXPAND_PART					*
 *									*
 * An EXPAND_PART node give information about expansions and pattern	*
 * function translations.  The polymorphic types of different		*
 * expansions can overlap.  The fields are as follows.			*
 *									*
 * u.rule	The translation rule.  (A PAT_RULE_E expr.)		*
 *									*
 * QWRAP_INFO	-1 normally.  If qwrap_info >= 0, then this node	*
 *		represents a pattern function that does a PRIM_QUNWRAP,	*
 *		and qwrap_info is the qwrap tag.			*
 *									*
 * mode		The low order byte of the define_mode of this		*
 *		declaration.						*
 *									*
 * package_name	The package where this expansion or pattern rule	*
 *		is declared.						*
 *									*
 * line_no	The line number where this expansion or pattern rule	*
 * 		is declared.						*
 *									*
 * ty		The (polymorphic) type of the function			*
 *									*
 * visible_in	The visibility list (in usual form).			*
 *									*
 * selection_info Selection list for PRIM_SELECT or PRIM_MODIFY.  	*
 *		  This is a list of the values LEFT_SIDE and RIGHT_SIDE	*
 *		  List [RIGHT_SIDE, LEFT_SIDE] indicates that this	*
 *		  id is a selector that takes the right of the left	*
 *		  of its argument.					*
 *									*
 * next		The next cell in the chain.				*
 *									*
 * =====================================================================*
 *			DESCRIP_CHAIN					*
 *									*
 * The cells in the description chain each give information about	*
 * descriptions that have been declared for this id.  Each cell		*
 * has the following fields.						*
 *									*
 * descr	The description.					*
 *									*
 * type		The polymorphic type over which this description 	*
 *		applies.						*
 *									*
 * mode		The mode of the description declaration.		*
 *									*
 * package_name	The name of the package where this description		*
 *		was made.						*
 *									*
 * visible_in	The visibility list, in the usual form.			*
 * 									*
 * line		The line where this description was issued.		*
 *									*
 * next		The next cell in the chain.				* 
 *									*
 * =====================================================================*
 *			ROLE_CHAIN					*
 *									*
 * The role chain gives information about roles that have been declared *
 * for this id.  The types of different cells can overlap.  The fields	*
 * are as follows.							*
 *									*
 * role		The role.						*
 *									*
 * type		The type over which this role applies.			*
 *									*
 * package_name	The name of the package where this role was declared.	*
 *									*
 * line_no	The line where this description occurs.			*
 *									*
 * visibile_in	The visibility list, in the ususal form.		*
 *									*
 * next		The next cell in the chain.				*
 ************************************************************************/

HASH2_TABLE* global_id_table = NULL;


/************************************************************************
 *			expect_id_table					*
 *			anticipate_id_table				*
 ************************************************************************
 * Tables expect_id_table and anticipate_id_table store expectations	*
 * and anticipations, respectively. They are used when checking that    *
 * all expected or anticipated definitions have been made.		*
 * Table anticipate_id_table is also used when converting anticipations *
 * to expectations when a genus or community is extended.		*
 *									*
 * Each key is a string, the identifier being expected or anticipated.  *
 *									*
 * In expect_id_table, the associated value is a list of types.  When   *
 * expectation x:T is made, T is added to the list of types associated  *
 * with string x in expect_id_table.					*
 *									*
 * In anticipate_id_table, the associated value is a list of pointers   *
 * to EXPECT_TABLE structures, where each such structure gives a type,	*
 * a visibility list and a package name.  When anticipation x:T is made *
 * in a package pname other than standard, visible to packages in list  *
 * L, then an entry (T,L,pname) is made under key x in 			*
 * anticipate_id_table.  When an anticipation is made in package	*
 * standard, entry (T,NIL,"standard") is made.				*
 ************************************************************************/

HASH2_TABLE*  expect_id_table     = NULL;
HASH2_TABLE*  anticipate_id_table = NULL;

/*===============================================================
 *			GENERAL FUNCTIONS
 *===============================================================*/

/****************************************************************
 *			GET_GIC_TM				*
 *			GET_GIC_HASH_TM				*
 ****************************************************************
 * Return the global table cell for identifier name, or NULL if *
 * there is none.  If force is true, then name need not be in 	*
 * the name table. If force is false, then name is presumed	*
 * already to be in the name table, and is not looked up.	*
 *								*
 * For get_gic_hash_tm, hash must be the hash value of string	*
 * 'name'.  It can only be used when the name is in the string	*
 * table.							*
 ****************************************************************/

GLOBAL_ID_CELL* get_gic_hash_tm(char *name, LONG hash)
{
  HASH2_CELLPTR h;
  HASH_KEY u;

  u.str = name;
  h     = locate_hash2(global_id_table, u, hash, eq);
  if(h->key.num == 0) return NULL;
  return h->val.gic;
}  

/*----------------------------------------------------------*/

GLOBAL_ID_CELL* get_gic_tm(char *name, Boolean force)
{
  char* modname = force ? new_name(name,TRUE) : name;
  return get_gic_hash_tm(modname, strhash(modname));
}


/****************************************************************
 *			IS_GLOBAL_ID_TM				*
 ****************************************************************
 * Return true if name is a global id.  Name does not need to	*
 * be in the string table.					*
 ****************************************************************/

Boolean is_global_id_tm(char *name)
{
  return get_gic_tm(name, TRUE) != NULL;
}


/****************************************************************
 * 			GET_COVER_TYPE				*
 ****************************************************************
 * If exp is a regular expectation, return its type.  If it is  *
 * an irregular expectation with function type A -> B, the	*
 * return type A -> `x, where `x is a new variable.		*
 ****************************************************************/

PRIVATE TYPE* get_cover_type(EXPECTATION *exp)
{
  return exp->irregular 
	  ? function_t(find_u(exp->type->TY1), var_t(NULL))
	  : exp->type;
}


/****************************************************************
 * 			GET_PART_COVER_TYPE			*
 ****************************************************************
 * Same as get_cover_type, above, but for parts.		*
 ****************************************************************/

TYPE* get_part_cover_type(PART *pt)
{
  return pt->irregular 
	  ? function_t(find_u(pt->ty->TY1), var_t(NULL))
	  : pt->ty;
}


/*===============================================================
 *			GETTING TYPE LISTS
 *===============================================================*/

/****************************************************************
 *			EXPECTATION_TYPE_LIST			*
 ****************************************************************
 * Return a list of the types in the expectation nodes linked	*
 * from exp.  The types are copies of the ones found in the 	*
 * table.  If package_name is NULL, return all types in the	*
 * chain.  If package_name is not NULL, then only return those	*
 * that are visible to package package_name.			*
 ****************************************************************/

TYPE_LIST* expectation_type_list(EXPECTATION *exp, char *package_name)
{
  TYPE_LIST *result;
  EXPECTATION *p;

  result = NIL;
  for(p = exp; p != NULL; p = p->next) {
    if(package_name == NULL || visible_expectation(package_name, p)) {
      result = type_cons(copy_type(p->type, 0), result);
    }
  }
  return result;
}


/****************************************************************
 *			ENTPART_TYPE_LIST			*
 ****************************************************************
 * Return a list of the types in chain pts.  If include_hidden	*
 * is true, then include parts marked hidden.  Otherwise, skip	*
 * them.  The types in the result list are copies of those in	*
 * the table.							*
 ****************************************************************/

TYPE_LIST* entpart_type_list(ENTPART *pts, Boolean include_hidden)
{
  TYPE_LIST *result;
  ENTPART *p;

  result = NIL;
  for(p = pts; p != NULL; p = p->next) {
    if(include_hidden || !(p->hidden)) {
      result = type_cons(copy_type(p->ty, 0), result);
    }
  }
  return result;
}


/*===============================================================
 *			HIDDEN IDENTIFIERS
 *===============================================================*/

/*****************************************************************
 *			MARK_HIDDEN				 *
 *****************************************************************
 * Mark each definition of name whose type overlaps t as hidden. *
 *****************************************************************/

void mark_hidden(char *name, TYPE *t)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  PART *p;

  u.str = name;
  h     = locate_hash2(global_id_table, u, strhash(u.str), eq);
  if(h->key.num != 0) {
    for(p = h->val.gic->entparts; p != NULL; p = p->next) {
      if(!disjoint(p->ty, t)) p->hidden = 1;
    }
  }
}


/*===============================================================
 *			HANDLING 
 *	   	    expect_id_table, 
 *		   anticipate_id_table 
 *	           local_expect_table
 *===============================================================*/

/****************************************************************
 *			    ADD_EXPECTATION_TO_TABLE		*
 ****************************************************************
 * Add ty to the list of types associated with identifier id    *
 * in table tbl.  Reduce the list, so that no type is subsumed  *
 * by any other.						*
 *								*
 * hash_val must be strhash(id).				*
 ****************************************************************/

PRIVATE void 
add_expectation_to_table(HASH2_TABLE **tbl, char *id, LONG hash_val,
			 TYPE *ty)
{
  HASH_KEY u;
  HASH2_CELLPTR h;

  u.str = id;
  h     = insert_loc_hash2(tbl, u, hash_val, eq);
  if(h->key.str == NULL) {
    h->key.str = id;
    bump_list(h->val.list = type_cons(ty, NIL));
  }
  else {
    SET_LIST(h->val.list, add_type_to_list(ty, h->val.list, TYPE_L));
  }
}


/****************************************************************
 *			    NOTE_EXPECTATION_TM			*
 ****************************************************************
 * If kind is EXPECT_ATT, then add ty to the type list 		*
 * associated with identifier id in table expect_id_table.	*
 *								*
 * If kind is ANTICIPATE_ATT, then add 				*
 * (ty, who_sees,package_name) to the    			*
 * list associated with identifier id in table 			*
 * anticipate_id_table.  Also, if this anticipation is not from *
 * a second-import, add id:ty (with mode and package name) to	*
 * global_anticipations. 					*
 *								*
 * If mode contains INCOMPLETE_MODE, then suppress		*
 * this action -- do nothing.  					*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

void note_expectation_tm(char *id, TYPE *ty, STR_LIST *who_sees, 
			 int kind, MODE_TYPE *mode,
			 char *package_name, int line)
{
  if(!has_mode(mode, INCOMPLETE_MODE)) {
    LONG hash_val = strhash(id);
    id = id_tb10(id, hash_val); 

#   ifdef DEBUG
      if(trace_defs) {
        trace_t(348, kind, id);
        trace_ty(ty);
        tracenl();
      }
#   endif

    /*----------------------*
     * Handle expectations. *
     *----------------------*/

    if(kind == EXPECT_ATT) {
      add_expectation_to_table(&expect_id_table, id, hash_val, ty);
    }

    /*-----------------------*
     * Handle anticipations. *
     *-----------------------*/

    else { /* kind != EXPECT_ATT */
      HASH_KEY u;
      HASH2_CELLPTR h;
      HEAD_TYPE ht;

      /*--------------------------------------------------------------*
       * For an anticipation, we need to build an EXPECT_TABLE entry. *
       *--------------------------------------------------------------*/

      EXPECT_TABLE* et = (EXPECT_TABLE*) alloc_small(sizeof(EXPECT_TABLE));
      bump_type(et->type = ty);
      bump_list(et->visible_in = who_sees);
      et->package_name = package_name;

      /*--------------------------------------------------------------*
       * Insert an entry into the hash table, if there isn't one yet. *
       *--------------------------------------------------------------*/

      u.str = id;
      h     = insert_loc_hash2(&anticipate_id_table, u, hash_val, eq);
      if(h->key.str == NULL) {
        h->key.str = id;
        h->val.list = NIL;
      }

      /*------------------------------------*
       * Add this EXPECT_TABLE to the list. *
       *------------------------------------*/

      ht.expect_table = et;
      SET_LIST(h->val.list, general_cons(ht, h->val.list, EXPECT_TABLE_L));

      /*-------------------------------------------------*
       * If this anticipation is not being done as part	 *
       * of a second-import, then add it to the		 *
       * anticipations to second-import.		 *
       *-------------------------------------------------*/

      add_global_anticipation(id, ty, who_sees, mode, package_name, line);
    }
  } /* end if(!has_mode(mode, INCOMPLETE_MODE)) */
}


/****************************************************************
 *			NOTE_EXPECTATIONS_P			*
 ****************************************************************
 * Note expectation x:t for each x in list l, placing 		*
 * the expectation in table tbl. MODE is the mode,		*
 * and package_name is the name of the package making		*
 * these expectations.						*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

void note_expectations_p(STR_LIST *l, TYPE *t, int kind, 
			 MODE_TYPE *mode, char *package_name, int line)
{
  STR_LIST *p, *who_sees;

  for(p = l; p != NIL; p = p->tail) {
    char* name = new_name(p->head.str, TRUE);

    /*-----------------------------------------------------------------*
     * For an anticipation, we need to know who sees the anticipation. *
     *-----------------------------------------------------------------*/

    if(kind == EXPECT_ATT) {
      who_sees = NIL;
    }
    else {
      bump_list(who_sees = get_visible_in(mode, name));
    }

    note_expectation_tm(name, t, who_sees, kind, mode, package_name, line);
    drop_list(who_sees);

  } /* end for(p= ...) */
}


/****************************************************************
 *		   LOCAL_EXPECTATION_LIST_TM			*
 ****************************************************************
 * Return the list of local expectations for identifier id, if	*
 * other is false, and the other local expectations for id if	*
 * other is true.						*
 ****************************************************************/

TYPE_LIST* local_expectation_list_tm(char *id, Boolean other)
{
  HASH_KEY u;
  HASH2_CELLPTR h;
  HASH2_TABLE *which;
  LONG hash;

  hash  = strhash(id);
  u.str = id_tb10(id, hash);
  which = other ? current_other_local_expect_table 
		: current_local_expect_table;
  h = locate_hash2(which, u, hash, eq);
  if(h->key.num == 0) return NIL;
  return h->val.list;
}


/*===============================================================
 *		    CREATING EXPECTATIONS: TOP LEVEL
 *===============================================================*/

/****************************************************************
 *			EXPECT_ENT_ID_P				*
 ****************************************************************
 * Do an expect or anticipate declaration name:v_type, with     *
 * role v_role.	This expectation is done by the current package.*
 *								*
 * If this expectation makes a definition (because it occurs    *
 * in the export part of a package) then mark the definition as *
 * attributed to package def_package_name.  If def_package_name *
 * is NULL, then the current package name is used.		*
 *								*
 * context = EXPECT_ATT 	for an expect declaration	*
 *         = ANTICIPATE_ATT 	for an anticipate declaration	*
 *								*
 * Report the expectation if report is true.			*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

void expect_ent_id_p(char *name, TYPE *v_type, ROLE *v_role, 
		     int context, MODE_TYPE *mode, int line, 
		     Boolean report, char *def_package_name)
{
  char *id;
  STR_LIST *who_sees;
  TYPE_LIST *defs;
  EXPR *id_as_expr;
  EXPECT_LIST *exps;
  Boolean make_define_entry;
  int report_kind;

  if(local_error_occurred || v_type == NULL_T) return;

  /*------------------------------------------------------------*
   * make_define_entry is true if we should do a definition 	*
   * after doing the expectation.  If this expectation is being *
   * exported, then we need to make a definition for it.  If    *
   * we are reading the export part of the main package, also   *
   * make a definition.  That definition will be used to check  *
   * whether there is an expectation in the export part of the  *
   * main package that is met by a package that is imported in  *
   * the body.  See overlap_parts_tm.				*
   *								*
   * As a special case, do not make a definition if MISSING_MODE*
   * is used, since in that case we don't expect to see a 	*
   * definition.						*
   *------------------------------------------------------------*/

  make_define_entry = 
    context == EXPECT_ATT && !has_mode(mode, MISSING_MODE) &&
    (main_context == IMPORT_CX || 
     main_context == INIT_CX   || 
     main_context == EXPORT_CX);

  bump_type(v_type);
  bump_role(v_role);

  /*-----------------------------------------------*
   * Get the id, as a string and as an expression. *
   *-----------------------------------------------*/

  id = new_name(name, TRUE);
  bump_expr(id_as_expr = new_expr1t(GLOBAL_ID_E, NULL_E, v_type, 0));
  bump_role(id_as_expr->role = v_role);
  id_as_expr->STR = id;

# ifdef DEBUG
    if(trace_defs) trace_t(49, main_context, nonnull(id));
# endif

  /*----------------*
   * Expect the id. *
   *----------------*/

  bump_list(who_sees = get_visible_in(mode, id));
  bump_list(exps = expect_global_id_tm(id, v_type, v_role, 
				       who_sees, FALSE, FALSE, 
				       context == EXPECT_ATT, TRUE,
				       mode, current_package_name,
				       line, id_as_expr));

  /*---------------------------------------------------------------*
   * Simulate a definition, if this expectation is being imported, *
   * or is in the export part of the main package.		   *
   * If no entry was made, then there must already                 *
   * have been an  entry.  Suppress the report.			   *
   *---------------------------------------------------------------*/

  if(exps != NIL && make_define_entry) {
    bump_list(defs = 
	      define_global_id_tm(id_as_expr, NULL, 0, mode, TRUE,
				  0, line, who_sees, def_package_name));
    if(defs == NIL) report = FALSE;
    else drop_list(defs);
  }

  /*------------------------------------------------------------*
   * If this is an anticipation, then issue definitions of 	*
   * dispatchers that go along with the anticipation, unless	*
   * this is suppressed by the mode including noAuto.		*
   *------------------------------------------------------------*/

  if(context == ANTICIPATE_ATT && !has_mode(mode, NOAUTO_MODE)) {
    issue_dispatch_definitions(name, v_type);
  }

  /*----------------------------------------*
   * Report the expectation, if called for. *
   *----------------------------------------*/

  if(report) {
    report_kind = (context == EXPECT_ATT) ? EXPECT_E : ANTICIPATE_E;
    report_dcl_p(id, report_kind, 0, v_type, v_role);
  }

  drop_list(exps);
  drop_list(who_sees);
  drop_expr(id_as_expr);

  /*--------------------------------------------------*
   * Perform auxiliary declarations, such as assumes. *
   *--------------------------------------------------*/
  
  {Boolean global = has_mode(mode, NO_EXPORT_MODE) == 0;
   if(has_mode(mode, ASSUME_MODE)) assume_tm(id, v_type, global);
   if(has_mode(mode, MISSING_MODE)) issue_missing_tm(id, v_type, mode);
   if(has_mode(mode, PATTERN_MODE)) patfun_assume_tm(id, global);
  }

  drop_type(v_type);
  drop_role(v_role);
}


/****************************************************************
 *			EXPECT_ENT_IDS_P			*
 ****************************************************************
 * Do expect_ent_id_p for each name in list l.			*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

void expect_ent_ids_p(STR_LIST *l, TYPE *v_type, ROLE *v_role, 
		     int context, MODE_TYPE *mode, int line, 
		     Boolean report, char *def_package_name)
{
  STR_LIST *p;

  if(local_error_occurred || v_type == NULL_T) return;

  /*-------------------------------------*
   * Perform each expectation in list l. *
   *-------------------------------------*/

  bump_list(l);
  for(p = l; p != NIL; p = p->tail) {
    expect_ent_id_p(p->head.str, v_type, v_role, context, mode, line,
		    report, def_package_name);
  }
  drop_list(l);
}


/*===============================================================
 *		   CREATING EXPECTATIONS: DETAILED LEVEL
 *===============================================================*/

/****************************************************************
 *			    INIT_EXPECTATION_TM			*
 ****************************************************************
 * Initialize exp with type t, visibility list who_sees, 	*
 * is_irregular and line.					*
 ****************************************************************/

PRIVATE void 
init_expectation_tm(EXPECTATION *exp, TYPE *t, LIST *who_sees,
		    Boolean is_irregular, int line)
{
  memset((char *) exp, 0, sizeof(EXPECTATION));
  exp->package_name         = current_package_name;
  exp->line_no              = line;
  bump_type(exp->type       = t);
  bump_list(exp->visible_in = who_sees);
  exp->irregular            = is_irregular;
  exp->in_body		    = main_context == BODY_CX;
}


/****************************************************************
 * 			TRANSFER_IRREGULAR			*
 ****************************************************************
 * to_exp is taking over some of the visibility from from_exp.  *
 * Be sure that their irregular status is consistent.  Also,    *
 * if both are irregular, then the one with the larger domain   *
 * (which must be to_exp) should also have a larger codomain.   *
 * If the domains are equal, the codomains should be equal.	*
 *								*
 * spec_name is the display name of this expectation, for 	*
 * error printing.						*
 ****************************************************************/

PRIVATE void 
transfer_irregular(EXPECTATION *from_exp, EXPECTATION *to_exp, 
		   char *spec_name)
{
  int from_irr = from_exp->irregular;
  if(from_irr != to_exp->irregular) {
    semantic_error1(SPEC_NONSPEC_ERR, spec_name, 0);
  }
  else if(from_irr) {
    int ov = half_overlap_u(find_u(from_exp->type)->TY2, 
			    find_u(to_exp->type)->TY2);
    if(ov != EQUAL_OR_CONTAINED_IN_OV) {
      semantic_error1(BAD_IRREGULAR_CODOMAIN_OVERLAP_ERR, spec_name, 0);
      err_print(PACKAGE_HAS_ERR, from_exp->package_name, from_exp->line_no);
      err_print_ty(from_exp->type);
      err_nl();
      err_print(PACKAGE_HAS_ERR, to_exp->package_name, to_exp->line_no);
      err_print_ty(to_exp->type);
      err_nl();
    }
  }
}


/****************************************************************
 * 			TRANSFER_VISIBILITY			*
 ****************************************************************
 * Remove from from_exp all visibility that is shared between	*
 * from_exp and to_exp.						*
 ****************************************************************/

PRIVATE void 
transfer_visibility(EXPECTATION *from_exp, EXPECTATION *to_exp)
{
  remove_visibility(&(from_exp->package_name), &(from_exp->visible_in),
		    to_exp->package_name, to_exp->visible_in);
}


/****************************************************************
 *		  PARTIALLY_INSTALL_NEW_EXPECTATION		*
 ****************************************************************
 * Expectation new_exp is about to be installed with existing	*
 * chain old_exps.  We need to be sure that no two expectation  *
 * cells that are visible to the same package have overlapping  *
 * expectations.  Scan the cells of old_exps, doing the		*
 * following.							*
 *								*
 *   If an old cell is found whose polymorphic type is a subset *
 *   of the polymorphic type of new_exp, and whose visibility   *
 *   has a nonempty intersection with the visibility of new_exp,*
 *   then remove the intersecting visibility from the old cell. *
 *   The new cell is more general, and takes over the old	*
 *   expectation.						*
 *								*
 *   If an old cell is found whose polymorphic type is a	*
 *   superset of the polymorphic type of new_exp, and whose	*
 *   visibility has a nonempty intersection with the visibility *
 *   of new_exp, then the visibility intersection is removed	*
 *   from new_exp, and taken over by the old cell, since the	*
 *   old cell is more general.					*
 *								*
 *   If an old cell has the same polymorphic type as new_exp,	*
 *   and has a common visibility, then we can either let	*
 *   the old cell take the intersecting visibility or let the	*
 *   new cell take it.						*
 *								*
 * spec_name is the name of the id being expected.  It is 	*
 * needed for error reports.  report_exp must be an expression  *
 * node for id spec_name, and err_line is the line where any	*
 * errors should be reported.  (report_exp is not currently	*
 * used.)							*
 *								*
 * This function sets *result to a list of the expectation	*
 * cells in chain old_exp that cover part of this expectation.  *
 * *result should be NULL at entry.  *result is ref-counted.	*
 *								*
 * Return value:						*
 *   0 if no errors, no subsumed expectations were deleted	*
 *   1 if no errors, some expectations were deleted from in_gic.*
 *   2 if there were errors.					*
 ****************************************************************/

PRIVATE int
partially_install_new_expectation
  (EXPECTATION *new_exp, EXPECTATION *old_exps, 
   char *spec_name, EXPECT_LIST **result, 
   EXPR *report_exp_unused, int err_line)
{
  Boolean there_were_subsumed_expectations = FALSE;
  int overlap;
  EXPECTATION *prior_exp;
  TYPE *new_exp_cover_type, *prior_exp_cover_type;

  /*------------------------------------------------------------*
   * new_exp normally covers its type, new_exp->type.  But if	*
   * new_exp is an irregular expectation, it covers all types   *
   * with the same domain.  Build the type, new_exp_cover_type, * 
   * used for overlap tests.					*
   *------------------------------------------------------------*/

  bump_type(new_exp_cover_type = get_cover_type(new_exp));

  /*--------------------------------------------------------*
   * Compare new_exp with each prior expectation prior_exp. *
   *--------------------------------------------------------*/

  for(prior_exp = old_exps;
      prior_exp != NULL && (new_exp->visible_in != NIL
	              || new_exp->package_name == standard_package_name);
      prior_exp = prior_exp->next) {

#   ifdef DEBUG
      if(trace_defs > 1) {
	trace_t(358, nonnull(spec_name));
	trace_ty(prior_exp->type);
	tracenl();
      }
#   endif

    /*----------------------------*
     * Skip over invisible cells. *
     *----------------------------*/

    if(!visible_intersect(new_exp->package_name, new_exp->visible_in, 
			  prior_exp->package_name, prior_exp->visible_in)) {
#     ifdef DEBUG
	if(trace_defs > 1) trace_t(359);
#     endif
      continue;
    }

    /*----------------------------------------------------------*
     * Find out how the types of new_exp and exp overlap.  If 	*
     * either is an irregular expectation, we need to replace 	*
     * its codomain type by a variable, so it covers all	*
     * functions types over its domain.				*
     *----------------------------------------------------------*/

    bump_type(prior_exp_cover_type = get_cover_type(prior_exp));
    overlap = overlap_u(new_exp_cover_type, prior_exp_cover_type);
    switch(overlap) {

      case DISJOINT_OV:

          /*-------------------------------------------------------*
           * We should ignore exp, since its type is disjoint with *
	   * the type being installed.  Move to the next cell.	   *
           *-------------------------------------------------------*/

#         ifdef DEBUG
	    if(trace_defs) trace_t(84);
#         endif

	  break;

      case EQUAL_OV:

	  /*------------------------------------------------------------*
	   * The type in new_exp is the same as the type in prior_exp.	*
 	   * If prior_exp has a package name different from standard,	*
	   * and we are not doing the standard package now, then we can	*
	   * just increase the visibility of exp.			*
	   * 								*
	   * There is a catch.  We cannot do this change if there is	*
	   * another expectation that has a visibility and a type	*
	   * in common with new_exp, since we would end up with		*
	   * overlapping expectations.  So we check whether there are   *
	   * any such expectation cells.  If there are, then the next	*
	   * case (CONTAINED_IN_OV) is general enough to do the job.	*
	   *------------------------------------------------------------*/

#         ifdef DEBUG
	    if(trace_defs) trace_t(519);
#         endif

	  if(new_exp->package_name != standard_package_name &&
	     prior_exp->package_name != standard_package_name) {

	    /*-----------------------------------------------------*
	     * Look for an expectation that would cause problems.  *
	     *-----------------------------------------------------*/

	    Boolean found_bad_exp = FALSE;
	    EXPECTATION *p;
	    for(p = prior_exp->next; p != NULL; p = p->next) {
	      TYPE* p_cover_type;
	      bump_type(p_cover_type = get_cover_type(p)); 
	      if(!disjoint(new_exp_cover_type, p_cover_type) &&
	         visible_intersect(new_exp->package_name, 
				   new_exp->visible_in, 
				   p->package_name, p->visible_in)) {

#               ifdef DEBUG
	          if(trace_defs) trace_t(520);
#               endif

		found_bad_exp = TRUE;
		drop_type(p_cover_type);
		break;
	      }
	      drop_type(p_cover_type);

	    } /* end for */

	    /*----------------------------------------------------*
	     * If no other expectation causes problems, then 	  *
	     * just increase the visibility of exp, and delete	  *
	     * that visibility from new_exp.			  *
	     *----------------------------------------------------*/

	    if(!found_bad_exp) {

#             ifdef DEBUG
		if(trace_defs > 1) trace_t(360);
#             endif

	      transfer_irregular(new_exp, prior_exp, spec_name);
	      SET_LIST(prior_exp->visible_in, 
		       str_list_union(prior_exp->visible_in, 
				      new_exp->visible_in));
	      remove_visibility(&(new_exp->package_name), 
				&(new_exp->visible_in),
				prior_exp->package_name, 
				prior_exp->visible_in);
	      set_list(result, exp_cons(prior_exp, *result));
	      break;
	    }
	  }

	  /*----------------------------------------------------*
           * Else fall through to next case -- handle prior_exp *
	   * as an expectation that that contains the new one 	*
	   * (improprerly). 			  		*
           *----------------------------------------------------*/

      case CONTAINED_IN_OV:

	  /*-------------------------------------------------------------*
	   * The type of prior_exp is a superset of the type of new_exp. *
	   * Leave prior_exp intact, and move to the next cell, after  	 *
           * deleting all visibility from new_exp that is covered by  	 *
	   * prior_exp.  (If prior_exp covers all of the visibility   	 *
	   * of new_exp, then new_exp will end up without any	      	 *
	   * visibility, and the loop will exit.)		      	 *
           *-------------------------------------------------------------*/

#         ifdef DEBUG
	    if(trace_defs > 1) trace_t(362);
#         endif

	  transfer_irregular(new_exp, prior_exp, spec_name);
	  transfer_visibility(new_exp, prior_exp);
	  set_list(result, exp_cons(prior_exp, *result));
	  break;

      case CONTAINS_OV:

	  /*--------------------------------------------------------*
	   * The type of new_exp is a superset of the type of	    *
	   * prior_exp.  Move visibility from prior_exp to new_exp. *
	   * If this results in prior_exp having no visibility,	    *
	   * then set the type field of prior_exp to NULL to force  *
	   * it to be deleted later.	   			    *
 	   *--------------------------------------------------------*/

#         ifdef DEBUG
	    if(trace_defs > 1) trace_t(510);
#         endif

	  transfer_irregular(prior_exp, new_exp, spec_name);
	  transfer_visibility(prior_exp, new_exp);
	  if(is_invisible(prior_exp->package_name, prior_exp->visible_in)) {
#           ifdef DEBUG
	      if(trace_defs > 1) trace_t(511);
#           endif
	    there_were_subsumed_expectations = TRUE;
	    SET_TYPE(prior_exp->type, NULL);
	  }
	  break;

      case BAD_OV:
	  bad_overlap(EXP_OVERLAP_ERR, spec_name, prior_exp->type, 
		      new_exp->type, prior_exp->package_name, 
		      prior_exp->line_no, err_line);
	  return 2;

    } /* end switch */

#   ifdef DEBUG
	if(trace_defs > 1) {
	  trace_t(361);
	  print_expectations(new_exp, 1);
	}
#   endif

    drop_type(prior_exp_cover_type);

  } /* end for(prior_exp = ...) */

  drop_type(new_exp_cover_type);

  return there_were_subsumed_expectations;
}


/****************************************************************
 *		    DELETE_SUBSUMED_EXPECTATIONS		*
 ****************************************************************
 * Delete any expectations from chain gic->expectations that	*
 * have a null type field.					*
 ****************************************************************/

PRIVATE void delete_subsumed_expectations(GLOBAL_ID_CELL *gic)
{
  EXPECTATION **p;

  p = &(gic->expectations);
  while(*p != NULL) {
    if((*p)->type == NULL) {
      EXPECTATION* tmp = *p;
      *p               = tmp->next;
      tmp->next        = NULL;
      free_expectation(tmp);
    }
    else p = &(*p)->next;
  }

# ifdef DEBUG
    if(trace_defs > 1) {
      trace_t(521);
      print_expectations(gic->expectations, 1);
    }
# endif
}


/****************************************************************
 *			    EXPECT_GLOBAL_ID_TM			*
 ****************************************************************
 * This function places an expectation of name:t with role r	*
 * in the global id table. This expectation is visible to	*
 * all packages whose names are in list who_sees.		*
 * (For the standard package, who_sees is NIL, even though	*
 * all packages can see the expectation)			*
 *								*
 * This expectation is being made by the package named 		*
 * package_name.
 *								*
 * More than one cell in the expectation list might be affected *
 * by this action.  expect_global_id_tm returns a list of	*
 * all of the expectation cells corresponding to this		*
 * expectation, if successful, and NIL if not successful.	*
 *								*
 * If force is true, then it is assumed that name:t does not 	*
 * overlap any existing expectations or definitions, and no	*
 * test is made.  						*
 *								*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 *								*
 * line is the line number of the expectation.			*
 *								*
 * Errors are reported as occurring at expression report_exp.	*
 *								*
 * If cpy is true, the t is copied before inserting it in the 	*
 * table.  Otherwise, t is not copied.  When t is copied,       *
 * uninteresting constraints are removed from it.		*
 *								*
 * If add_to_local is true, then this expectation is added to 	*
 * current_local_expect_tables head, unless MODE includes 	*
 * MISSING_MODE.  If add_to_local is false, then this		*
 * expectation is added to current_other_local_expect_tables	*
 * head. 							*
 *								*
 * If warn_if_hidden is true, then a warning is given if this	*
 * is an expectation of an id that is hidden in the body. 	*
 *								*
 * If the main context is EXPORT_CX, then this identifier 	*
 * is written into the index file.				*
 *								*
 * If an expectation of an irregular function is created, then  *
 * an IRREGULAR_DCL_I declaration is issued.			*
 ****************************************************************/

EXPECT_LIST*
expect_global_id_tm(char *name, TYPE *t, ROLE *r, STR_LIST *who_sees,
		    Boolean force, Boolean cpy,
		    Boolean add_to_local, Boolean warn_if_hidden,
		    MODE_TYPE *mode, char *package_name, 
		    int line, EXPR *report_exp)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  GLOBAL_ID_CELL *gic;
  EXPECTATION *gic_exp, *new_exp;
  TYPE *cpyt;
  Boolean is_irregular;
  char *spec_name;
  LONG name_hash;
  int err_line;

  int there_were_subsumed_expectations = 0;
  EXPECT_LIST* result = NIL;

# ifdef DEBUG
    if(trace_defs) {
      trace_t(353, name);
      trace_ty(t);
      tracenl();
      trace_t(445);
      trace_rol(r);
      trace_t(354, force, add_to_local, cpy, get_define_mode(mode));
      print_str_list_nl(who_sees);
      trace_t(355, current_package_name);
      trace_t(470);
      print_str_list_nl(current_public_packages);
      trace_t(356);
      print_str_list_nl(current_private_packages);
    }
# endif

  bump_list(who_sees);
  spec_name = display_name(name);
  name_hash = strhash(name);
  bump_role(r);
  SET_ROLE(r, remove_knc(r, 0));

  /*------------------------------------------------------------*
   * Write the id into index_file, if in export part, and if    *
   * not suppressed by NODESCRIP_MODE in mode.			*
   * Also write the expectation into the description file.      *
   *------------------------------------------------------------*/

  if(doing_export() && index_file != NULL && !has_mode(mode, NODESCRIP_MODE)) {
    fprintf(index_file, "e%s\n", name);
    if(description_file != NULL && !suppress_expectation_descriptions) {
      fprintf(description_file, "e%s\n: ", name);
      fprint_ty(description_file, t);
      fprintf(description_file, "\n%c", 0);
      wrote_description = TRUE;
    }
  }

  /*------------------------------------------------------------------*
   * Copy the type if not suppressed -- need a copy for installation. *
   * When copying is requested, also reduce constraints, by removing  *
   * any that have no relevance when t stands alone.		      *
   *------------------------------------------------------------------*/

  bump_type(t);
  if(cpy) {
    bump_type(cpyt = copy_type(t, 0));
    reduce_constraints(cpyt, FALSE);
  }
  else bump_type(cpyt = t);

# ifdef DEBUG
    if(trace_defs) {
      trace_t(394);
      trace_ty(cpyt);
      tracenl();
    }
# endif

  /*------------------------------------------------------------*
   * Add this expectation to the global expectation table.  The *
   * global expectation table is used for second imports of	*
   * packages, so that the compiler knows what was expected	*
   * in those packages. 					*
   *								*
   * We never second-import the standard package, so its 	*
   * expectations are not recorded.				*
   *------------------------------------------------------------*/

  if(current_package_name != standard_package_name) {
    add_global_expectation(name, cpyt, r, who_sees, mode, 
			   package_name, line);
  }

  /*-----------------------------------------------------------------*
   * Add this expectation to the local expect table, if appropriate. *
   * Do not consider this a local expectation if it is marked 	     *
   * missing, since we expect no definition in that case.	     *
   *-----------------------------------------------------------------*/

  if(!has_mode(mode, MISSING_MODE)) {
    HASH2_TABLE** which = add_to_local ? &current_local_expect_table
			      : &current_other_local_expect_table;
    add_expectation_to_table(which, name, name_hash, cpyt);
  }

  /*----------------------------------------------------*
   * Warn if expectation of non-hidden id in body part. *
   *----------------------------------------------------*/

  if(warn_if_hidden) warn_if_hidden_expectation(name, cpyt, TRUE);

  /*----------------------------------------------------*
   * Get info from the mode, and check that mode is ok. *
   *----------------------------------------------------*/

  is_irregular = has_mode(mode, IRREGULAR_MODE);
  if(is_irregular && TKIND(find_u(t)) != FUNCTION_T) {
    semantic_error1(NONFUN_IRREGULAR_ERR, spec_name, 0);
  }

  /*---------------------------------------------------------*
   * Find out where the expectation goes in global_id_table. *
   *---------------------------------------------------------*/

  err_line = (report_exp == NULL) ? 0 : report_exp->LINE_NUM;
  u.str    = name = id_tb10(name, name_hash);
  h        = insert_loc_hash2(&global_id_table, u, name_hash, eq);

  /*-----------------------------------*
   * If not in table already, install. *
   *-----------------------------------*/

  if(h->key.num == 0) {
    h->key.str        = name;
    h->val.gic        = gic = allocate_gic();
    gic->expectations = gic_exp = allocate_expectation();
    init_expectation_tm(gic_exp, cpyt, who_sees, is_irregular, line);
    bump_type(gic->container = cpyt);
    bump_list(result  = exp_cons(gic_exp, NIL));
  }

  /*------------------------------------------------*
   * Otherwise, there was an entry present already. *
   * Modify the expectation chain.		    *
   *------------------------------------------------*/

  else { /* h->key.num != 0 */
    gic     = h->val.gic;
    gic_exp = gic->expectations;

#   ifdef DEBUG
      if(trace_defs > 1) {
        trace_t(357);
        print_expectations(gic_exp, 1);
      }
#   endif

    new_exp = allocate_expectation();
    init_expectation_tm(new_exp, cpyt, who_sees, is_irregular, line);

    /*----------------------------------------------------------*
     * Compare this expectation with existing expectations.     *
     * This process updates new_exp and existing expectations.  *
     * Any existing expectations that have types that contain	*
     * cpyt will take over visibility from new_exp; doing so	*
     * will cause that visibility to be deleted from new_exp.   *
     * Any existing expectations whose types are contained in	*
     * cpyt will give visibility to new_exp.			*
     *								*
     * When we are done with partially_install_new_expectation, *
     * new_exp will have what needs to be added.  If its 	*
     * visibility is null, then nothing needs to be added.	*
     * The result there_were_subsubsumed_expectations, is 	*
     *								*
     *    1  if any old expectations were completely 		*
     *	     subsumed by new_exp, and should be deleted.	*
     *       (partially_install_new_expectation has set their	*
     *       type fields to NULL, but not deleted them.  It	*
     *       is our responsibility to delete them.)		*
     *								*
     *    2  if	partially_install_new_exp encountered an error  *
     *								*
     *    0  otherwise.						*
     *								*
     * There is a special case when force is true.  Then, it is *
     * assumed that this new expectation does not overlap any   *
     * old expectations, so we skip this phase.			*
     *								*
     * Note: we are supposed to keep track of when an		*
     * expectation has been made in the implementation part,    *
     * in the in_body field of the EXPECTATION cell.  If this   *
     * expectation is being made in the implementation part,	*
     * and is subsumed by another expectation, then we can 	*
     * ignore it.  If this expecation subsumes another, then    *
     * a warning will be given anyway, so it is not important   *
     * to keep track of in_body.  So the in_body field is	*
     * ignored in partially_install_new_expecatation.		*
     *----------------------------------------------------------*/

    if(!force) {
      there_were_subsumed_expectations =
	partially_install_new_expectation(new_exp, gic_exp, spec_name, 
				         &result, report_exp, err_line);
    }

    /*--------------------------------------------------------*
     * Check for an error.  If there was an error, delete any *
     * expectations that got deleted and get out.	      *
     *--------------------------------------------------------*/

    if(there_were_subsumed_expectations == 2) {
      SET_LIST(result, NIL);
      delete_subsumed_expectations(gic);
      goto out;
    }

    /*--------------------------------------------------*
     * If new_exp is no longer needed, because its	*
     * information has been entirely put into old	*
     * expectations, then delete new_exp.		*
     *--------------------------------------------------*/

    if((new_exp->package_name != standard_package_name &&
       new_exp->visible_in == NIL)
       || new_exp->type == NULL) {

#     ifdef DEBUG
        if(trace_defs) trace_t(363);
#     endif

      free_expectation(new_exp);

    }

    /*----------------------------------------------------------*
     * If there is still a new_exp to install, then install it. *
     *----------------------------------------------------------*/

    else { /* new_exp is still visible */

#     ifdef DEBUG
        if(trace_defs) trace_t(364);  /* Installing the new expectation */
#     endif

      /*----------------------------------------------------------*
       * Delete any expectations that became completely subsumed. *
       * They have had their type fields set to NULL by 	  *
       * partially_install_new_expectation.			  *
       *----------------------------------------------------------*/

      if(there_were_subsumed_expectations) {
        delete_subsumed_expectations(gic);
      }

      /*----------------------------------------------------*
       * Put new_exp at the front of the expectation chain. *
       *----------------------------------------------------*/

      new_exp->next = gic->expectations;
      gic_exp = gic->expectations = new_exp;
      SET_LIST(result, exp_cons(new_exp, result));

      /*--------------------------*
       * Fix the containing type. *
       *--------------------------*/

      {TYPE_LIST *tl;
       bump_list(tl = expectation_type_list(new_exp, NULL));
       bump_type(gic->container = containing_class(tl));

#      ifdef DEBUG
         if(trace_defs > 1) {
	   trace_t(531);
	   print_type_list(tl); tracenl();
	   trace_t(347);
           trace_ty(gic->container); tracenl();
	 }
#      endif

       /*-------------------------------------------------------*
        * If this is an irregular expectation, and we are 	*
        * generating code, then generate an IRREGULAR_DCL_I 	*
	* instruction.						*
	*-------------------------------------------------------*/

       if(is_irregular) {
         generate_irregular_dcl_g(name, new_exp->type);
       }

       drop_list(tl);
      }
    } /* end else new_exp is still visible */
  } /* end else h->key.num != 0 */

out:

  /*--------------------------------------------*
   * Install the role into the table.		*
   *--------------------------------------------*/

  if(r != NULL) {
    install_role_tm(r, cpyt, who_sees, gic, report_exp, line);
  }

# ifdef DEBUG
    if(trace_defs) {
      LIST *pp;
      trace_t(365, name, name, get_define_mode(mode));
      trace_ty(t);
      trace_t(366);
      print_gic(gic, 2);
      trace_t(367);
      for(pp = result; pp != NIL; pp =pp->tail) {
	fprintf(TRACE_FILE, "%p ", pp->head.exp);
      }
      fprintf(TRACE_FILE, "]\n");
    }
# endif

  /*------------------------------------------------------------------*
   * Finish up.  If anything was successfully installed, then install *
   * any ahead descriptions that are waiting.			      *
   *------------------------------------------------------------------*/

  if(result != NIL) {
    do_ahead_description_tm(name, cpyt);
    result->ref_cnt--;
  }

  drop_role(r);
  drop_type(t);
  drop_type(cpyt);
  drop_list(who_sees);
  return result;
}


/****************************************************************
 *			    GET_EXPECTATION_TM			*
 ****************************************************************
 * Expression id must be a global id, with its type		*
 * installed.  Return the expectation that this id belongs to.  *
 * Return NULL if id does not belong to any expectations.	*
 ****************************************************************/

EXPECTATION* get_expectation_tm(EXPR *id)
{
  EXPECTATION *p;

  if(id->GIC == NULL) return NULL;

  for(p = id->GIC->expectations; p != NULL; p = p->next) {
    LIST* mark = finger_new_binding_list();
    if(unify_u(&(id->ty), &(p->type), TRUE)) {
      undo_bindings_u(mark);
      return p;
    }
  }

  return NULL;
}


/*===============================================================*
 *			ADDING AND GETTING RESTRICTION TYPES	 *
 *===============================================================*/

/****************************************************************
 *			    ADD_RESTRICTION_TYPE_TM		*
 ****************************************************************
 * Install restriction type name:ty to the table.    		*
 ****************************************************************/

void add_restriction_tm(char *name, TYPE *ty)
{
  /*------------------------------------------------------*
   * First check that this restriction is reasonable.  It *
   * must be a subtype of some irregular expectation.     *
   *------------------------------------------------------*/

  EXPECTATION *p;
  GLOBAL_ID_CELL* gic = get_gic_tm(name, TRUE);
  Boolean found_expectation = FALSE;

  for(p = gic->expectations; p != NULL; p = p->next) {
    if(p->irregular) {
      int ov = half_overlap_u(ty, p->type);
      if(ov == EQUAL_OR_CONTAINED_IN_OV) {
        found_expectation = TRUE;
        break;
      }
    }
  }

  /*-------------------------------------------------------*
   * If we did not find an expectation, do not install the *
   * restriction type.					   *
   *-------------------------------------------------------*/

  if(!found_expectation) {
    semantic_error1(CANNOT_INSTALL_RESTRICTION_ERR, name, 0);
    err_print_ty(ty);
    err_nl();
    return;
  }

  /*-------------------------------------------------------------*
   * If there was an expectation, then just add this restriction *
   * to the restriction list.					 *
   *-------------------------------------------------------------*/

  SET_LIST(gic->restriction_types,
	   type_cons(copy_type(ty, 0), gic->restriction_types));
}


/****************************************************************
 *			    GET_RESTRICTION_TYPE_TM		*
 ****************************************************************
 * Expression id must be a global id that is a function, and    *
 * that has its type installed.  				*
 *								*
 * Return the codomain that id should have if restriction types *
 * are taken into account.  If the restriction indicates that	*
 * application must fail, then return NULL.			*
 ****************************************************************/

TYPE* get_restriction_type_tm(EXPR *id)
{
  TYPE_LIST *p, *restrictions;
  TYPE* result;
  TYPE* id_type;
  LIST *mark;
  EXPECTATION *exp;
  Boolean did_a_restriction_this_iteration;

  HASH2_TABLE* ty_b = NULL;

  id = skip_sames(id);

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(550);
      tracenl();
      print_expr(id, 1);
    }
# endif

  /*------------------------------------------------------------*
   * Get the expectation.  If none, then don't know anything	*
   * about the result, so return `a.				*
   *------------------------------------------------------------*/

  exp = get_expectation_tm(id);
  if(exp == NULL) return var_t(NULL);

  /*------------------------------------------------------------*
   * Get the initial value of the result, which comes from the	*
   * expectation of id.  Put the variables that occur in the	*
   * domain of id->ty into a hash table, so that we can copy	*
   * the result later without copying those variables.		*
   *------------------------------------------------------------*/

  bump_list(mark = finger_new_binding_list());

  {TYPE* exp_type;
   id_type = find_u(id->ty);
   bump_type(exp_type = copy_type(exp->type, 0));
   UNIFY(exp_type->TY1, id_type->TY1, TRUE);
   bump_type(result = exp_type->TY2);
  }

  replace_null_vars(&result);
  put_vars2_t(id_type->TY1, &ty_b);
  
  /*--------------------------------------------------------------------*
   * Suppose that id_type is A -> B.  For each restriction R -> S,	*
   * first check that A is a subset of R.  If so, then this restriction	*
   * can be used.  Unify R with A to bind variables that also occur	*
   * in S, and then unify S with the current result to force the	*
   * restriction.							*
   *									*
   * The bindings done by one restriction can cause another one to	*
   * be applicable.  We keep track, in the mark field of the list cells,*
   * of which restrictions have applied, and keep going until no new	*
   * restrictions apply.  Start by clearing the marks, just in case.	*
   *--------------------------------------------------------------------*/

  restrictions = id->GIC->restriction_types;
  if(restrictions != NULL) {
    for(p = restrictions; p != NIL; p = p->tail) p->mark = 0;

    do {
      did_a_restriction_this_iteration = FALSE;
      for(p = restrictions; p != NIL; p = p->tail) {
	if(!p->mark) {
	  int ov;
	  TYPE* p_type = find_u(p->head.type);

	  if(TKIND(p_type) != FUNCTION_T) continue;

	  ov = half_overlap_u(id_type->TY1, p_type->TY1);
	  if(ov == EQUAL_OR_CONTAINED_IN_OV) {
	    TYPE* cpy_p_type = copy_type(p_type, 0);
	    bump_type(cpy_p_type);
	    UNIFY(cpy_p_type->TY1, id_type->TY1, TRUE);
	    if(!UNIFY(cpy_p_type->TY2, result, TRUE)) {
	      drop_type(cpy_p_type);
	      SET_TYPE(result, NULL);
	      goto out;
	    }
	    did_a_restriction_this_iteration = TRUE;
	    p->mark = 1;
	    drop_type(cpy_p_type);
	  }
	}
      } /* end for(p = ...) */

    } while(did_a_restriction_this_iteration);

    for(p = restrictions; p != NIL; p = p->tail) p->mark = 0;

  } /* end if(restrictions != NULL) */

  /*---------------------------------------------------------*
   * Now copy the result, if any restrictions were done. Be  *
   * careful not to copy variables that occur in id->ty      *
   *---------------------------------------------------------*/

  SET_TYPE(result, copy_type1(result, &ty_b, 0));

 out:
  undo_bindings_u(mark);
  drop_list(mark);
  scan_and_clear_hash2(&ty_b, drop_hash_type);

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(550);
      trace_t(551);
      trace_ty(result);
      tracenl();
    }
# endif

  if(result != NULL) result->ref_cnt--;
  return result;
}


/*===============================================================*
 *			CREATING PARTS; GENERAL			 *
 *===============================================================*/

/****************************************************************
 *		CHECK_MISSING_FOR_DEFINE_TM			*
 ****************************************************************
 * Identifier id is being defined, and the_dcl is its		*
 * declaration.  This definition has a possibly restricted 	*
 * type t.  MODE is its mode.					*
 *								*
 * Check whether any global identifiers in the_dcl have		*
 * polymorphic types that include missing types.  If any	*
 * are found, issue a warning.					*
 *								*
 * If the_dcl is NULL, then don't do any test.  Also, don't     *
 * test when id is not a global id (which indicates a pattern   *
 * or expand declaration.)					*
 *								*
 * MODE is a safe pointer.  It only lives as long as this	*
 * function call.						*
 ****************************************************************/

PRIVATE void 
check_missing_for_define_tm(EXPR *id, TYPE *t, EXPR *the_dcl,
			    MODE_TYPE *mode)
{

  if(the_dcl != NULL && EKIND(id) == GLOBAL_ID_E) {

    /*------------------------------------------------------------*
     * Before doing the check, we must bind the type of id to t,  *
     * and must do default bindings, since those variable	  *
     * bindings can influence the checks.  We undo		  *
     * those bindings after doing this check.			  *
     *------------------------------------------------------------*/

    LIST *mark;
    bump_list(mark = finger_new_binding_list());

    unify_u(&(id->ty), &t, TRUE);
    bind_glob_bound_vars(t, mode);
    process_defaults_tc(the_dcl, mode, FALSE, FALSE);
    check_missing_tm(the_dcl);
    SET_LIST(glob_bound_vars, NIL);

    undo_bindings_u(mark);
    drop_list(mark);
  }
}
  

/****************************************************************
 *			    DEFINE_GLOBAL_ID_TM			*
 ****************************************************************
 * Declares expression id as either a global id or		*
 * a pattern function or an expander.  				*
 * who_sees is a list of the packages that can see this 	*
 * definition.							*
 *								*
 * This definition is done by the current package, at line line.*
 *								*
 * id has kind GLOBAL_ID_E if a global id, and			*
 * PAT_FUN_E if a pattern function or expander.			*
 * In the case of PAT_FUN_E, parameter 'kind' is PAT_DCL_E	*
 * for a pattern function and is EXPAND_E for an expand		*
 * declaration. 						*
 * 								*
 * Returns the list of types installed on success, NIL on	*
 * failure.							*
 *								*
 * If errok is true, no error message is generated on failure.  *
 *								*
 * MODE is the mode of the declaration (including override, 	*
 * default info).  It is a safe pointer: it does not live	*
 * longer than this function call.				*
 *								*
 * Line is the line number where this definition.		*
 *								*
 * If from_expect is true, then overlap tests with other parts	*
 * are skipped.  						*
 *								*
 * Expression the_dcl is checked for global ids with missing	*
 * types for each declaration issued.  If the_dcl is NULL, no   *
 * missing ids check is done.	  				*
 *								*
 * Parameter def_package_name is the name of the package that   *
 * this definition attributed to in its PART.  			*
 *								*
 * If polymorphism is disallowed (disallow_polymorphism is	*
 * true) then don't perform a polymorphic definition.  Instead	*
 * give an error message.					*
 ****************************************************************/

TYPE_LIST* 
define_global_id_tm(EXPR *id, EXPR *the_dcl,
		    Boolean errok_unused, MODE_TYPE *mode,
		    Boolean from_expect,
		    EXPR_TAG_TYPE kind, int line,
		    STR_LIST *who_sees, char *def_package_name)
{
  Boolean under_expect, ok, warn_if_hidden;
  LIST *mark;
  EXPECT_LIST *where_to_put;
  EXPR *true_id;
  TYPE *t, *id_ty;
  TYPE_LIST *result;
  GLOBAL_ID_CELL *gic;
  EXPECTATION *exp;
  char *name;

  result            = NIL;
  warn_if_hidden    = (kind != PAT_DCL_E);   /* Don't warn about hidden
						pattern functions */
  bump_expr(true_id = skip_sames(id));
  bump_type(id_ty   = copy_type(true_id->ty, 0));
  reduce_constraints(id_ty, FALSE);
  true_id->STR = name = new_name(true_id->STR, TRUE);

# ifdef DEBUG
    if(trace_exprs) {
      trace_t(374);
      print_expr(id,0);
    }

    if(trace_defs) {
      trace_t(375, name, name, toint(main_context), get_define_mode(mode));
      trace_ty(id_ty);
      tracenl();
      trace_t(445);
      trace_rol(true_id->role);
      trace_t(376, kind, from_expect, 
	      true_id->irregular, true_id->PRIMITIVE);
      trace_t(377);
      print_str_list_nl(who_sees);
    }
# endif

  /*---------------------------------------------------*
   * Check for a violation of disallowed polymorphism. *
   *---------------------------------------------------*/

  if(disallow_polymorphism && is_polymorphic(id_ty)) {
    polymorphism_error(true_id);
    goto out;
  }

  /*------------------------------*
   * Perform assume if requested. *
   *------------------------------*/

  if(has_mode(mode, ASSUME_MODE)) {
    assume_tm(name, id_ty, !has_mode(mode, NO_EXPORT_MODE));
  }

  /*-------------------------------------------------------------*
   *** Try to place this definition underneath an expectation. ***
   *-------------------------------------------------------------*/

  under_expect = FALSE;
  gic	       = get_gic_tm(name, FALSE);

  if(gic != NULL) {

#   ifdef DEBUG
      if(trace_defs) {
        trace_t(378, name);
        print_gic(gic, 1);
      }
#   endif

    for(exp = gic->expectations; exp != NULL; exp = exp->next) {

#     ifdef DEBUG
        if(trace_defs > 1) {
	  trace_t(379);
	  trace_ty(exp->type); tracenl();
        }
#     endif

      /*--------------------------------*
       * Ignore invisible expectations. *
       *--------------------------------*/

      if(!visible_intersect(current_package_name, who_sees, 
			  exp->package_name, exp->visible_in)) {
#       ifdef DEBUG
	  if(trace_defs > 1) trace_t(359);
#       endif
        continue;
      }

      /*-------------------------------------------*
       * Check the type of exp against the type of *
       * the definition.			   *
       *-------------------------------------------*/

      bump_type(t = copy_type(exp->type, 0));
      bump_list(mark = finger_new_binding_list());
      if(UNIFY(t, id_ty, TRUE)) {

        /*--------------------------------------------------*
         * This definition contributes to this expectation. *
         *--------------------------------------------------*/

#       ifdef DEBUG
	  if(trace_defs) {
	    trace_t(380, name);
	    trace_ty(t); tracenl();
	  }
#       endif

        under_expect = TRUE;

        /*------------------------------------------------------*
         * Copy the type to cement the unification done above,  *
         * then create a part. 					*
         *------------------------------------------------------*/

        SET_TYPE(t, copy_type(t, 0));
        reduce_constraints(t, FALSE);
#       ifdef DEBUG
          if(trace_defs) {
	    trace_t(394);
	    trace_ty(t); tracenl();
	  }
#       endif

        if(def_global_tm(true_id, t, mode,
	   gic, exp, who_sees, from_expect,
	   kind, line, def_package_name)) {

	  SET_LIST(result, type_cons(t, result));

	  /*------------------------------------------------------------*
	   * Check that no ids in the declaration have types that have 	*
	   * been declared missing, and are not implemented.  This	*
	   * cannot be done sooner, since the unification above can	*
	   * affect the outcome. 					*
           *------------------------------------------------------------*/

	  check_missing_for_define_tm(true_id, t, the_dcl, mode);

	  /*------------------------------------------------------------*
	   * Check that the role of the definition matches role of	*
	   * the  expectation. 						*
	   *------------------------------------------------------------*/

	  {ROLE *dcl_role, *mel;
	   STR_LIST *pack_names;
	   Boolean old_error_occurred = local_error_occurred;
	   local_error_occurred = FALSE;
           bump_role(dcl_role = get_role_tm(id, &pack_names));
	   bump_list(pack_names);
	   bump_role(mel = checked_meld_roles(id->role, dcl_role, 0));
	   if(local_error_occurred) {
	     report_role(dcl_role, display_name(name), pack_names);
	   }
	   else local_error_occurred = old_error_occurred;
	   drop_list(pack_names);
	   drop_role(mel);
           drop_role(dcl_role);
	  }
        } /* end if(def_global_tm...) */

        /*----------------------------------------------------------*
         * Check that this is not a definition in the implementation*
	 * part of a package of a globally visible id that has no   *
         * expectation 					    	    *
         *----------------------------------------------------------*/

        if(warn_if_hidden) warn_if_hidden_expectation(name, t, TRUE);

        undo_bindings_u(mark);

      } /* end if(unify..) */

      drop_list(mark);
      drop_type(t);

    } /* end for(exp = ...) */

  } /* end if(gic != NULL) */

  /*--------------------------------------------------------*
   * If this definition falls under an expectation, then we *
   * have already installed it, so there is nothing more to *
   * do.						    *
   *--------------------------------------------------------*/

  if(under_expect) goto out;

  /*------------------------------------------------------------------*
   * If this definition does not fall under any existing expectation, *
   * then we need to create a new expectation for this definition.    *
   *------------------------------------------------------------------*/

# ifdef DEBUG
    if(trace_defs) trace_t(381, name);
# endif

  {STR_LIST *who_sees1;
   bump_list(who_sees1 = get_visible_in(mode, name));
   bump_list(where_to_put = 
	     expect_global_id_tm(name, id_ty, true_id->role, who_sees1,
				 TRUE, FALSE, FALSE, warn_if_hidden,
				 mode, current_package_name, line, id));
   drop_list(who_sees1);
  }
	     
  /*------------------------------------------------------------------*
   * If no expectation was created, then we cannot do the definition. *
   *------------------------------------------------------------------*/

  if(where_to_put == NULL) {
    SET_LIST(result, NIL);
    goto out;
  }

  /*------------------------------------------------------------*
   * There can only be one expectation in list where_to_put,	*
   * since we already checked for expectations that might	*
   * include this type. 					*
   *------------------------------------------------------------*/

  exp = where_to_put->head.exp;

  drop_list(where_to_put);

  /*-----------------------------*
   * Create the definition part. *
   *-----------------------------*/

  gic = get_gic_tm(name, FALSE);
  ok  = def_global_tm(true_id, id_ty, mode, gic, exp, who_sees,
		      from_expect, kind, line, def_package_name);
  if(ok) {
    SET_LIST(result, type_cons(id_ty, NIL));

    /*----------------------------------------------------------*
     * Check that no ids in the declaration have types that have*
     * been declared missing, and are not implemented.  This	*
     * cannot be done sooner, since the unification above can	*
     * affect the outcome. 					*
     *----------------------------------------------------------*/

    check_missing_for_define_tm(true_id, true_id->ty, the_dcl, mode);
  }

 out:

# ifdef DEBUG
    if(trace_defs > 1) {
      trace_t(522);
      print_type_list(result); tracenl();
    }
# endif

  drop_expr(true_id);
  drop_type(id_ty);
  if(result != NIL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			    DEF_GLOBAL_TM			*
 ****************************************************************
 * This function farms out the work of actually installing the  *
 * part, depending on what part is done.  See def_patfun_tm,	*
 * def_expander_tm or def_entity_tm.				*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

PRIVATE Boolean 
def_global_tm(EXPR *id, TYPE *ty,
	      MODE_TYPE *mode, GLOBAL_ID_CELL *gic, 
	      EXPECTATION *exp,
	      LIST *who_sees, Boolean from_expect,
	      EXPR_TAG_TYPE kind, int line,
	      char *def_package_name)
{
  if(EKIND(id) == PAT_FUN_E) {
    return def_expander_tm(id, ty, mode, gic, who_sees, 
			   kind, line);
  }
  else {
    return def_entity_tm(id, ty, mode, gic, exp, who_sees, 
			 from_expect, line, def_package_name);
  }
}


/****************************************************************
 *			DEFINE_PRIM_ID_TM			*
 *			DEF_PRIM_TM				*
 ****************************************************************
 * These are specialized versions of define_global_id_tm and    *
 * def_global_tm for defining primitives.  They are here to	*
 * speed up initialization of the compiler.  			*
 *								*
 * define_prim_id_tm is roughly equivalent to 			*
 * define_global_id_tm, with the following parameters.		*
 *								*
 *   body		NULL					*
 *   errok		TRUE					*
 *   from_expect	FALSE					*
 *   kind		LET_E					*
 *   line		0					*
 *   who_sees		NIL					*
 ****************************************************************/

PRIVATE void def_prim_tm(EXPR *id, TYPE *ty, MODE_TYPE *mode,
			 GLOBAL_ID_CELL *gic)
{
  ENTPART *newpart;

  bump_expr(id);
  bump_type(ty);

  /*---------------------*
   * Install a new part. *
   *---------------------*/

  newpart = allocate_entpart();  /* Zeroed out. */

  /*---------------------------------------*
   * Set primitive info, if not a default. *
   *---------------------------------------*/

  if(!has_mode(mode, DEFAULT_MODE) || force_prim) {
    newpart->primitive = id->PRIMITIVE;
    newpart->arg       = id->SCOPE;
    bump_list(newpart->selection_info = id->EL3);
  }
  else {
    newpart->primitive = 0;
    newpart->selection_info = NIL;
  }

  /*------------------------------*
   * Set up the rest of the part. *
   *------------------------------*/

  newpart->irregular     = toint(id->irregular 
			         | has_mode(mode, IRREGULAR_MODE));
  newpart->trapped       = id->bound;
  newpart->package_name  = current_package_name;
  newpart->line_no       = 0;
  newpart->from_expect   = 0;
  bump_type(newpart->ty  = ty);

  newpart->mode = has_mode(mode, FORCE_MODE) 
                     ? 0
	             : (UBYTE) (get_define_mode(mode) & 0xFF);

  /*--------------------------------------------*
   * Add newpart to the front of gic->entparts. *
   *--------------------------------------------*/

  newpart->next = gic->entparts;
  gic->entparts = newpart;

  drop_expr(id);
  drop_type(ty);
}

/*--------------------------------------------------------------*/

void define_prim_id_tm(EXPR *id, MODE_TYPE *mode)
{
  Boolean under_expect = FALSE;
  LIST *mark;
  EXPECT_LIST *where_to_put;
  TYPE *t;
  GLOBAL_ID_CELL *gic;
  EXPECTATION *exp;

  TYPE* id_ty = id->ty;
  char* name  = id->STR;

  /*-------------------------------------------------------------*
   *** Try to place this definition underneath an expectation. ***
   *-------------------------------------------------------------*/

  under_expect = FALSE;
  gic	       = get_gic_tm(name, FALSE);

  if(gic != NULL) {
    for(exp = gic->expectations; exp != NULL; exp = exp->next) {

      /*----------------------------------------------------------*
       * Check the type of this expectation against the type of   *
       * the definition. 					  *
       *----------------------------------------------------------*/

      bump_type(t = copy_type(exp->type, 0));
      bump_list(mark = finger_new_binding_list());
      if(unify_u(&t, &(id_ty), TRUE)) {

	/*--------------------------------------------------*
	 * This definition contributes to this expectation. *
	 *--------------------------------------------------*/

	under_expect = TRUE;

	/*-----------------------------------------------------*
	 * Copy the type to cement the unification done above, *
	 * then create a part for this definition. 	       *
	 *-----------------------------------------------------*/

	SET_TYPE(t, copy_type(t, 0));
	def_prim_tm(id, t, mode, gic);

	undo_bindings_u(mark);
      }
      drop_list(mark);
      drop_type(t);

    } /* end for(exp = ...) */

  } /* end if(gic != NULL) */

  /*------------------------------------------------------------*
   * If this definition contributes to an expectation, there is *
   * nothing to do.						*
   *------------------------------------------------------------*/

  if(under_expect) return;

  /*-----------------------------------------------*
   * Create a new expectation for this definition. *
   *-----------------------------------------------*/

  bump_list(where_to_put = 
	    expect_global_id_tm(name, id_ty, id->role, NIL,
				TRUE, FALSE, FALSE, FALSE,
				mode, current_package_name, 0, id));

  if(where_to_put == NULL) return;

  /*------------------------------------------------------------*
   * There can only be one expectation in list where_to_put, 	*
   * since we already checked for expectations that might	*
   * include this type.						*
   *------------------------------------------------------------*/

  exp = where_to_put->head.exp;
  drop_list(where_to_put);

  gic = get_gic_tm(name, FALSE);
  def_prim_tm(id, id_ty, mode, gic);

# ifdef DEBUG
    if(trace_defs > 2) {
      trace_t(523, nonnull(name));
      trace_ty(id_ty); tracenl();
      trace_t(524);
      print_entpart_chain(gic->entparts, 1);
    }
# endif

}


/*===============================================================
 *			ENTITY DEFINITIONS
 *===============================================================*/

/****************************************************************
 *			    DEF_ENTITY_TM			*
 ****************************************************************
 * This function is part of define_global_id_tm.  It installs 	*
 * the definition in global id cell gic, in the entparts field. *
 * Parameter exp is the expectation that it contributes to.	*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

PRIVATE Boolean 
def_entity_tm(EXPR *id, TYPE *ty,
	      MODE_TYPE *mode, GLOBAL_ID_CELL *gic, 
	      EXPECTATION *exp, LIST *who_sees,
	      Boolean from_expect, int line,
	      char *def_package_name)
{
  PART *newpart;
  char *name;
  Boolean is_irregular;

  bump_expr(id);
  bump_type(ty);
  replace_null_vars(&ty);
  name = id_tb0(id->STR);
  if(def_package_name == NULL) {
    def_package_name = current_package_name;
  }

# ifdef DEBUG
    if(trace_defs) {
      trace_t(382, name, get_define_mode(mode),
	      toint(id->PRIMITIVE), toint(id->SCOPE),
	      toint(from_expect));
      trace_ty(ty); tracenl();
    }
# endif

  /*-----------------------------------------------------------------*
   * If this definition is from an expectation, and there is already *
   * a part present that completely subsumes this definition, then   *
   * don't add a new part.  A part subsumes this definition if it    *
   * is a from_expect part, it's visibility includes all of who_sees,*
   * it is not hidden, it has the same package name and attributed   *
   * package name, and its type includes ty.  We exclude the	     *
   * standard package, since rules for testing visibility lists for  *
   * it are different, and this kind of thing generally won't happen *
   * with the standard package.					     *
   *-----------------------------------------------------------------*/

  if(from_expect && current_package_name != standard_package_name) {
    ENTPART *p;
    for(p = gic->entparts; p != NULL; p = p->next) {
      if(p->from_expect && !p->hidden &&
	 p->package_name == current_package_name &&
	 p->attributed_package_name == def_package_name &&
	 str_list_subset(who_sees, p->visible_in)) {

        int ov = half_overlap_u(ty, p->ty);
	if(ov == EQUAL_OR_CONTAINED_IN_OV) return FALSE;
      }   
    }
  }

  /*--------------------------------------*
   * Check consistency of irregular mode. *
   *--------------------------------------*/

  is_irregular = has_mode(mode, IRREGULAR_MODE);
  if(EKIND(id) != PAT_FUN_E && exp->irregular != is_irregular) {
    semantic_error1(SPEC_NONSPEC_ERR, display_name(name), 0);
  }

  /*------------------------------------------------------------*
   * Check if strongly missing.  We are not allowed to make 	*
   * a definition of a strongly missing type.  			*
   *------------------------------------------------------------*/

  check_strong_missing_tm(name, ty);

  /*-----------------------*
   * Install a new part.   *
   *-----------------------*/

  newpart = allocate_entpart();

  /*-----------------------------------------------------*
   * Set primitive info, if not a default, or if forced. *
   *-----------------------------------------------------*/

  if(!has_mode(mode, DEFAULT_MODE) || force_prim) {
    newpart->primitive                = id->PRIMITIVE;
    newpart->arg                      = id->SCOPE;
    bump_list(newpart->selection_info = id->EL3);
  }
  else {
    newpart->primitive      = 0;
    newpart->selection_info = NIL;
  }

  /*------------------------------*
   * Set up the rest of the part. *
   *------------------------------*/

  newpart->irregular     = id->irregular | is_irregular;
  newpart->trapped       = id->bound;
  newpart->package_name  = current_package_name;
  newpart->attributed_package_name = def_package_name;
  newpart->line_no       = line;
  newpart->from_expect   = from_expect;
  newpart->in_body       = (outer_context == BODY_CX);
  bump_type(newpart->ty  = ty);
  bump_list(newpart->visible_in = who_sees);
  newpart->mode = has_mode(mode, FORCE_MODE)
                      ? 0
                      : (UBYTE) (get_define_mode(mode) & 0xFF);
  newpart->arg  = id->SCOPE;

  /*-------------------------------------------*
   * Add newpart to the front of gic->entparts *
   *-------------------------------------------*/

  newpart->next = gic->entparts; 
  gic->entparts = newpart;

  /*--------------------------------------------------------------*
   * Report the declaration for echoing. Don't report definitions *
   * that come from expectations, since they will be reported as  *
   * expectations if they need to be reported at all.             *
   *--------------------------------------------------------------*/

  if(!from_expect || show_all_reports) {
    report_dcl_p(display_name(name), DEFINE_E, mode, ty, id->role);
  }

# ifdef DEBUG
    if(trace_defs > 1) {
      trace_t(383, name);
      trace_ty(ty); tracenl();
      trace_t(524);
      print_entpart_chain(gic->entparts, 1);
    }
# endif

  drop_expr(id);
  drop_type(ty);
  return TRUE;
}


/*===============================================================
 *			EXPAND AND PATTERN HANDLING
 *===============================================================*/

/****************************************************************
 *			ADD_EXPAND_PART_TO_CHAIN		*
 ****************************************************************
 * Add part newpart to chain *part.  Normally, newpart is put   *
 * at the start of the chain.  But if underride mode is		*
 * selected, then newpart is added to the end of the chain.	*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

void add_expand_part_to_chain(EXPAND_PART **part, EXPAND_PART *newpart,
			      MODE_TYPE *mode)
{
  if(has_mode(mode, UNDERRIDES_MODE) && *part != NULL) {
    EXPAND_PART *pp;
    for(pp = *part; pp->next != NULL; pp = pp->next) {}
    pp->next      = newpart;
    newpart->next = NULL;
  }
  else {
    newpart->next = *part;
    *part = newpart;
  }
}


/****************************************************************
 *			DEF_EXPANDER_TM				*
 ****************************************************************
 * This function is part of define_global_id_tm, but for a 	*
 * pattern function or expand definition.  This function is	*
 * responsible for installing the definition part.  Parameter	*
 * gic is the global id cell for identifier id,			*
 * and parameter exp is the expectation to which this		*
 * definition contributes.					*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

PRIVATE Boolean def_expander_tm(EXPR *id, TYPE *ty,
				MODE_TYPE *mode, GLOBAL_ID_CELL *gic, 
				LIST *who_sees,
				int kind, int line)
{
  EXPAND_PART *newpart, **parts;
  char *name;

  bump_expr(id);
  bump_type(ty);
  replace_null_vars(&ty);
  name = id->STR;

# ifdef DEBUG
    if(trace_defs) {
      trace_t(525, name, get_define_mode(mode), kind, toint(id->PRIMITIVE));
      trace_ty(ty); tracenl();
    }
# endif

  /*------------------------------------------------------------*
   * If there is no expand_info record in gic, then create one. *
   *------------------------------------------------------------*/

  if(gic->expand_info == NULL) {
    gic->expand_info = allocate_expand_info();
  }

  /*------------------------------------------------------------*
   * Get the appropriate parts chain.				*
   *------------------------------------------------------------*/

  parts = (kind == PAT_DCL_E)
            ? &(gic->expand_info->patfun_rules)
            : &(gic->expand_info->expand_rules);

  /*-----------------------------------------------------*
   * Install a new part.  But don't install if this is a *
   * pattern declaration for a null pattern rule. 	 *
   *-----------------------------------------------------*/

  if(kind != PAT_DCL_E || id->E3 != NULL) {
    newpart = allocate_expand_part();

    /*----------------------------------------------------*
     * Set primitive info, if not a default or if forced. *
     *----------------------------------------------------*/

    if(id->patfun_prim && (!has_mode(mode, DEFAULT_MODE) || force_prim)) {
      newpart->primitive = PRIM_SELECT;
      newpart->arg       = id->PRIMITIVE;
      newpart->irregular = id->irregular;
      bump_list(newpart->selection_info = id->EL3);
    }
    else {
      newpart->primitive      = 0;
      newpart->selection_info = NIL;
    }

    /*------------------------------*
     * Set up the rest of the part. *
     *------------------------------*/

    newpart->mode	    = (UBYTE)(get_define_mode(mode) & 0xFF);
    newpart->package_name   = current_package_name;
    newpart->line_no        = line;
    newpart->in_body        = (outer_context == BODY_CX && 
			       str_memq(main_package_name, who_sees));
    bump_type(newpart->ty   = ty);
    bump_list(newpart->visible_in = who_sees);
    bump_expr(newpart->u.rule = id->E3);

    /*---------------------------*
     * Add newpart to the chain. *
     *---------------------------*/

    add_expand_part_to_chain(parts, newpart, mode);

  } /* end if(kind != PAT_DCL_E || ...) */

  /*-------------------------------------*
   * Set the pattern function max class. *
   *-------------------------------------*/

  if(kind == PAT_DCL_E) {
    install_patfun_expectation_tm(gic, ty, name, who_sees);
  }

  /*-------------------------------------*
   * Report the declaration for echoing. *
   *-------------------------------------*/

  {int report_kind = (kind == EXPAND_E) ? EXPAND_E : PAT_FUN_E;
   report_dcl_p(display_name(name), report_kind, mode, ty, id->role);
  }

# ifdef DEBUG
    if(trace_defs > 1) {
      trace_t(383, name);
      trace_ty(ty); tracenl();
      trace_t(524);
      print_expand_part_chain(*parts, 1);
    }
# endif

  drop_expr(id);
  drop_type(ty);
  return TRUE;
}


/****************************************************************
 *			GET_EXPAND_TYPE_AND_RULE_TM		*
 ****************************************************************
 * Search for a pattern translation rule or expansion rule for	*
 * fun matching context of pattern or expression ctxt and, for	*
 * pattern translation rules, consistent with unification mode	*
 * unif and lazy mode lazy.  Kind is				*
 *								*
 *  0 for a pattern function					*
 *  1 for a pattern constant					*
 *  2 for an expand function					*
 *								*
 * When found, set outformals to the formals for the rule, and 	*
 * outrule for the translation, and return TRUE.  Expression	*
 * pointers outformals and outrule are not ref-counted.		*
 * If no compatible rule is found, return FALSE.		*
 *								*
 * *status is set to the status of the search as follows.	*
 *								*
 *   0	when no restrictive match was found			*
 *								*
 *   1  when a restrictive match was found.			*
 *								*
 * *pack_name is set to the name of the package that contains	*
 * the translation, when TRUE is returned.			*
 ****************************************************************/

Boolean get_expand_type_and_rule_tm(EXPR *fun, EXPR *ctxt,
				    int unif, Boolean lazy, int kind,
				    EXPR **outformals, EXPR **outrule,
				    int *status, char **pack_name)
{
  EXPR *formals = NULL, *rule = NULL;
  TYPE *fun_type;
  EXPAND_PART *parts, *viable_part;
  LIST *umark;
  TYPE *part_ty;
  GLOBAL_ID_CELL *fun_gic;
  int viable_part_count;
  Boolean is_restrictive;
  Boolean mode_ok = TRUE;

# ifdef DEBUG
    Boolean tracing = trace_defs > 1 ||
                      trace_pm > 2 ||
                      (trace_pm > 1 && kind < 2);
# endif

  *status = 0;  /* default */

  bump_type(fun_type = find_u(fun->ty));
  SET_TYPE(fun_type, copy_type(fun_type, 0));

# ifdef DEBUG
   if(tracing) {
     trace_t(393);
     trace_t(386, nonnull(fun->STR));
     trace_ty(fun_type);
     tracenl();
     if(trace_exprs > 1) {
       trace_t(438);
       print_expr(ctxt, 1);
     }
     if(kind != 2) trace_t(387, toint(lazy), toint(unif));
   }
# endif

  /*---------------------------------------------------------*
   * Need the GIC to get the available rules.  For a pattern *
   * function, need pattern parts.  In the case of a pattern *
   * constant, don't complain about missing pattern parts -- *
   * the pattern translator will just use an equality check. *
   *---------------------------------------------------------*/

  fun_gic = fun->GIC;
  if(fun_gic == NULL) die(50);

  if(fun_gic->expand_info == NULL) {
    if(kind == 0) no_pat_parts_error(fun);
    drop_type(fun_type);
    return FALSE;
  }

  parts = (kind == 2) 
	     ? fun_gic->expand_info->expand_rules
  	     : fun_gic->expand_info->patfun_rules;
  if(parts == NULL && kind == 0) {
    no_pat_parts_error(fun);
    drop_type(fun_type);
    return FALSE;
  }

  /*------------------------------------------------------------*
   * Try each part.  Look for one that is not restrictive, and  *
   * whose mode matches the desired mode.  If we are looking	*
   * for a rule for a pattern function, then it might be	*
   * necessary to accept a restrictive rule.  (That should 	*
   * only happen when there are functions in the rule head,	*
   * such as in							*
   *								*
   *    Pattern f(?x) ++ ?y => ...				*
   *								*
   * where the type of x does not show up in the type of the 	*
   * pattern function ++.)  A restrictive rule is only accepted *
   * if it is the only one that matches.  Variable viable_part  *
   * holds the restrictive rule, if one was found, and		*
   * viable_part_count tells how many restrictive rules were	*
   * found.							*
   *------------------------------------------------------------*/

  viable_part       = NULL;
  viable_part_count = 0;
  for(; parts != NULL; parts = parts->next){

#   ifdef DEBUG
      if(trace_defs > 2) {
	trace_t(532);
	print_expand_part(parts, 1);
      }
#   endif

#   ifdef DEBUG
      if(trace_pm) {
	trace_t(388);
	short_print_expr(TRACE_FILE, parts->u.rule->E2);
      }
#   endif

    bump_list(umark = finger_new_binding_list());
    bump_type(part_ty = copy_type(parts->ty, 0));
    if(unify_u(&part_ty, &fun_type, TRUE)) {
      rule    = parts->u.rule;
      formals = rule->E2;

      /*-----------------------------------------------------------*
       * Get indications of whether mode is ok for a pattern rule. *
       *-----------------------------------------------------------*/

      if(kind != 2) {
	int     unif_mode = rule->PAT_RULE_MODE >> 2;
	int     lazy_mode = rule->PAT_RULE_MODE & 3;
	Boolean unif_ok   = unif_mode == 0 || ((1 << unif) & unif_mode);
	Boolean lazy_ok   = lazy_mode == 0 || ((1 << lazy) & lazy_mode);
	        mode_ok   = unif_ok && lazy_ok;

#       ifdef DEBUG
	  if(trace_pm) {
	    trace_t(389, lazy_mode, lazy_ok, unif_mode, mode_ok);
	  }
#       endif
      }

      if(mode_ok && mode_match_pm(formals, ctxt, FALSE, &is_restrictive)) {

	/*------------------------------------------------------*
	 * This rule matches. If it is nonrestrictive,		*
         * then return it.  If it is restrictive, note it. 	*
	 *------------------------------------------------------*/

#       ifdef DEBUG
	  if(trace_pm) {
	    trace_t(393);
	    trace_t(391);
	  }
#       endif

	if(!is_restrictive) {
	  *outformals = formals;
	  *outrule    = rule;
	  *status     = 0;   /* We don't care about any restrictive ones
				that we have seen any more.		   */
          *pack_name = parts->package_name;

	  if(kind != 2) SET_EXPR(fun->E3, rule);

	  /*----------------------------------------------------*
	   * Need to commit to type bindings, so not undone in  *
	   * viable_expr.					*
	   *----------------------------------------------------*/

	  commit_new_binding_list(umark);
	  drop_list(umark);
	  drop_type(part_ty);
	  drop_type(fun_type);
	  return TRUE;
	}

	else /* is_restrictive */ {

#         ifdef DEBUG
	    if(trace_pm) trace_t(541);
#         endif

	  /*-------------------------------*
	   * Record this restrictive part. *
	   *-------------------------------*/

	  *status = 1;
	  viable_part = parts;
	  viable_part_count++;
	}
      } /* end if(mode_ok && ...) */

#     ifdef DEBUG
	if(trace_pm) {
	  trace_t(393);
	  trace_t(392);
	}
#     endif

      undo_bindings_u(umark);

    } /* end if(unify...) */

    else {
#     ifdef DEBUG
        if(trace_pm) {
	  trace_t(540);
        }
#     endif
    }

    drop_type(part_ty);
    drop_list(umark);

  } /* end for(parts = ...) */

  /*--------------------------------------------------------------------*
   * If we get out of the for loop without returning, then either 	*
   * no rule was found or a restrictive rule was found.  If a		*
   * single restrictive rule was found, then return its part.		*
   *--------------------------------------------------------------------*/

  if(viable_part_count == 1) {

#   ifdef DEBUG
      if(trace_pm) trace_t(533);
#   endif

    /*-----------------------------------------*
     * Return to bindings done at viable_part. *
     * Then return the rule in viable_part.    *
     *-----------------------------------------*/

    bump_type(part_ty = copy_type(viable_part->ty, 0));
    unify_u(&part_ty, &fun_type, FALSE);
    rule = *outrule = viable_part->u.rule;
    *outformals = rule->E2;
    *pack_name = viable_part->package_name;

    if(kind != 2) SET_EXPR(fun->E3, rule);
    drop_type(part_ty);
    drop_type(fun_type);
    return TRUE;
  }

  /*----------------------------------------------------*
   * If we get here, then either no rule was found or	*
   * more than one restrictive rule were found.  The	*
   * search fails.					*
   *----------------------------------------------------*/

  drop_type(fun_type);
  return FALSE;
}


/****************************************************************
 *			REPORT_PAT_AVAIL			*
 ****************************************************************
 * Report the available pattern rules for pattern function      *
 * or constant fun, by giving mode an type.			*
 ****************************************************************/

void report_pat_avail(EXPR *fun)
{
  GLOBAL_ID_CELL *fun_gic;
  EXPAND_PART *parts;
  EXPR *cpyfun;
  EXPR_TAG_TYPE fun_kind = EKIND(fun);

  if(fun_kind != PAT_FUN_E && fun_kind != GLOBAL_ID_E) return;

  fun_gic = fun->GIC;
  if(fun_gic == NULL || fun_gic->expand_info == NULL) return;

  parts = fun_gic->expand_info->patfun_rules;
  if(parts == NULL) return;

  bump_expr(cpyfun = copy_expr(fun));
  err_print(PAT_AVAIL_ERR);
  for(; parts != NULL; parts = parts->next) {
    EXPR* rule    = parts->u.rule;
    EXPR* formals = rule->E2;
    TYPE* part_ty = parts->ty;
#   ifdef NEVER
      int unif_mode = rule->PAT_RULE_MODE >> 2;
      int lazy_mode = rule->PAT_RULE_MODE & 3;
#   endif

    SET_TYPE(cpyfun->ty, part_ty);
    err_print(SPACE3_ERR);
    err_short_print_expr(cpyfun);
    err_print(MODE_IS_ERR);
    err_shrt_pr_expr(formals);
    err_nl(); err_nl();
  }

  drop_expr(cpyfun);
}


/****************************************************************
 *			INSTALL_PATFUN_EXPECTATION_TM		*
 ****************************************************************
 * Install type t as a pattern function class in cell gic,	*
 * whose name is name.	Parameter who_sees is a list of the	*
 * names of the packages that can see this declaration.		*
 *								*
 * Note: If type t is a superset of a type that was already in  *
 * the patfun class expectations, then no change is made to the *
 * previous entry.  That is ok, since get_patfun_expectation_tm *
 * gets  the first visible entry, and the new entry is always   *
 * placed at the front of the chain.				*
 ****************************************************************/

PRIVATE void 
install_patfun_expectation_tm(GLOBAL_ID_CELL *gic, TYPE *t, char *name, 
			      STR_LIST *who_sees)
{
  PATFUN_EXPECTATION *pt;
  TYPE *cpyt;

  char* installation_package_name = current_package_name;
  EXPAND_INFO* pi = gic->expand_info;

# ifdef DEBUG
    if(trace_defs) {
      trace_t(495, name);
      trace_ty(t); tracenl();
    }
# endif

  /*---------------------------------*
   * We need to install a copy of t. *
   *---------------------------------*/

  bump_type(cpyt = copy_type(t, 0));

  /*---------------------------------------------------------*
   * Build the expand_info field if it is not already built. *
   *---------------------------------------------------------*/

  if(pi == NULL) pi = gic->expand_info = allocate_expand_info();

  /*---------------------------------------------------------*
   * Reduce the visibility of this type by locating other    *
   * parts that have a strictly larger polymorphic type, and *
   * giving them priority.  Also, check for bad overlaps.    *
   *---------------------------------------------------------*/

  for(pt = pi->patfun_expectations; 
      pt != NULL && !is_invisible(installation_package_name, who_sees); 
      pt = pt->next) {

    Boolean vi = visible_intersect(installation_package_name, who_sees, 
				   pt->package_name, pt->visible_in);
    if(vi) {
      int ov = overlap_u(cpyt, pt->type);
      if(ov == CONTAINED_IN_OV || ov == EQUAL_OV) {
        remove_visibility(&installation_package_name, &who_sees,
			  pt->package_name, pt->visible_in);
      }
      else if(ov == BAD_OV) {
	bad_patfun_type_error(name, pt->type, cpyt);
	break;
      }
    }
  }

  /*------------------------------------------------------------*
   * Now install what is left, if anything, at the front of the *
   * chain.							*
   *------------------------------------------------------------*/

  if(!is_invisible(installation_package_name, who_sees)) {
    PATFUN_EXPECTATION* pex = allocate_patfun_expectation();
    bump_type(pex->type       = cpyt);
    bump_list(pex->visible_in = who_sees);
    pex->package_name         = installation_package_name;
    pex->next                 = pi->patfun_expectations;
    pi->patfun_expectations   = pex;
  }

  drop_type(cpyt);

# ifdef DEBUG
    if(trace_defs) {
      trace_t(497, name);
      print_patfun_expectations(pi->patfun_expectations, 1);
    }
# endif

}


/****************************************************************
 *			GET_PATFUN_CLASS_TM			*
 ****************************************************************
 * Find the most general polymorphic type A of the pattern	*
 * function described by cell gic, such that A has a nonnull 	*
 * intersection with t.  Return the intersection of A and t.    *
 * Return NULL if no such type is found.			*
 ****************************************************************/

TYPE* get_patfun_expectation_tm(GLOBAL_ID_CELL *gic, TYPE *t)
{
  PATFUN_EXPECTATION *pex;
  EXPAND_INFO* pi = gic->expand_info;

  if(pi == NULL) return NULL;

  for(pex = pi->patfun_expectations; pex != NULL; pex = pex->next) {
    if(is_visible(pex->package_name, pex->visible_in, current_package_name)) {
      LIST *binding_list_mark; 
      bump_list(binding_list_mark = finger_new_binding_list());

      if(unify_u(&t, &(pex->type), TRUE)) {
	TYPE* result = copy_type(t, 0);
        undo_bindings_u(binding_list_mark);
	drop_list(binding_list_mark);
	return result;
      }
      drop_list(binding_list_mark);
    }
  }
  return NULL;
}


/*===============================================================
 *			ROLE HANDLING
 *===============================================================*/

/****************************************************************
 *			INSTALL_ROLE_TM				*
 ****************************************************************
 * Install role r, with type ty and visibility who_sees, into   *
 * global id cell gic.  Expression id is the identifier for	*
 * which r is a role.					        *
 ****************************************************************/

void install_role_tm(ROLE *r, TYPE *ty, STR_LIST *who_sees, 
		     GLOBAL_ID_CELL *gic, EXPR *id, int line)
{
  ROLE_CHAIN *rc;

  if(r != NULL) {

#   ifdef DEBUG
      if(trace_defs) {
        trace_t(526);
        trace_rol(r); tracenl();
        trace_t(297); trace_ty(ty); tracenl();
      }
#   endif

    /*---------------------------------------------------------*
     * Check the existing roles.  They must be consistent with *
     * this one.  If any of them subsume this one, then we can *
     * ignore this role installation.			       *
     *---------------------------------------------------------*/

    for(rc = gic->role_chain; rc != NULL; rc = rc->next) {
      if(visible_intersect(current_package_name, who_sees,
			   rc->package_name, rc->visible_in)) {

	int ov = half_overlap_u(ty, rc->type);
	if(ov != DISJOINT_OV) {

	  /*-------------------------------------------------------*
	   * The roles must be consistent. checked_meld_roles does *
	   * the consistency check.				   *
	   *-------------------------------------------------------*/

	  ROLE *mel;
	  bump_role(mel = checked_meld_roles(r, rc->role, id));
	  drop_role(mel);

	  /*--------------------------------------------------------*
	   * Check whether node rc subsumes the current definition. *
	   * If so, add the visibility of who_sees to this cell.    *
	   * We don't bother with this test in the standard package.*
	   *--------------------------------------------------------*/

	  if(current_package_name != standard_package_name &&
	     subrole(r, rc->role)) {

	    if(ov == EQUAL_OR_CONTAINED_IN_OV) {
	      if(rc->package_name == standard_package_name ||
		 str_list_subset(who_sees, rc->visible_in)) {

#               ifdef DEBUG
		  if(trace_defs) trace_t(528);
#               endif
	        return;
	      }
	    }
	  }
        } /* end if(ov != DISJOINT_OV) */
      } /* end if(visible_intersect...) */
    } /* end for(rc = ...) */

    /*--------------------*
     * Install this role. *
     *--------------------*/

    rc = allocate_role_chain();
    bump_role(rc->role       = r);
    bump_type(rc->type       = ty);
    bump_list(rc->visible_in = who_sees);
    rc->package_name         = current_package_name;
    rc->line_no 	     = line;
    rc->next 		     = gic->role_chain;
    gic->role_chain 	     = rc;

#   ifdef DEBUG
      if(trace_defs) {
        trace_t(527);
        print_role_chain(rc, 1);
      }
#   endif
  }
}


/*****************************************************************
 *			GET_ROLE_TM				 *
 *****************************************************************
 * Return the role that should be associated with identifier id. *
 * The kind of id must be GLOBAL_ID_E.				 *
 *								 *
 * *pack_names is set to a list of the packages that contain	 *
 * role information that is in the returned role.  It is not	 *
 * ref-counted.  If pack_names is NULL, nothing is returned in	 *
 * pack_names.							 *
 *****************************************************************/

ROLE* get_role_tm(EXPR *id, STR_LIST **pack_names)
{
  ROLE_CHAIN *rc;
  ROLE* result = NULL;

  if(pack_names != NULL) *pack_names = NIL;
  if(id->GIC == NULL) return NULL;

  for(rc = id->GIC->role_chain; rc != NULL; rc = rc->next) {
    if(is_visible(rc->package_name, rc->visible_in, current_package_name) &&
       !disjoint(id->ty, rc->type) &&
       !subrole(rc->role, result)) {

      result = checked_meld_roles(result, rc->role, id);
      if(pack_names != NULL) {
        *pack_names = str_cons(rc->package_name, *pack_names);
      }
    }
  }

# ifdef DEBUG
    if(trace_defs > 1 && result != NULL) {
      trace_t(529, nonnull(id->STR));
      trace_ty(id->ty); tracenl();
      trace_t(530);
      trace_rol(result);
    }
# endif

  return result;
}


/*===============================================================
 * 			ERROR CHECKING
 *===============================================================*/

/****************************************************************
 *			OVERLAP_PARTS_TM			*
 ****************************************************************
 * This function performs two kinds of tests that are done by	*
 * comparing parts in gic->entparts with one another.		*
 *								*
 * 1. See if there are any two definitions that conflict.  For	*
 * example, if there are two different definitions of 		*
 * x: Natural, and they are not allowed by default/override	*
 * rules, then write an error message.				*
 *								*
 * 2. Check whether there is an expectation that was  		*
 * made in the interface part of the main package, and a 	*
 * part in gic->entparts that satisfies that expectation, but 	*
 * was made in a package that was imported in the package body.	*
 * The scenario that we are looking for here is as follows.	*
 *								*
 *     Consider a scenario where we are compiling a		*
 *     package M.  Imagine that part p is a definition of 	*
 *     identifier x, and that q is an expectation of x.	  	*
 *								*
 *     Suppose expectation q was made while processing the 	*
 *     interface part of M, and so might be visible to another	*
 *     package (A) that	imports M in some other compilation.	*
 *     Suppose definition p came from another package (B)	*
 *     that was read while compiling the implementation part of *
 *     M.  That definition will not be visible to package A	*
 *     in the later compilation.  So, when A imports M, it	*
 *     must assume that M contains a definition of identifier	*
 *     x.  But suppose that A also imports package B.  Then B   *
 *     defines x.  It appears that there is a conflict. 	*
 *     The conflict would be avoided if A could see that M's	*
 *     definition of x is really made by package B.  So package *
 *     B should have been imported in the interface, to make    *
 *     it visible.	  				  	*
 *								*
 * Parameter 'name' is the name of the identifier that gic	*
 * describes.							*
 ****************************************************************/

PRIVATE void overlap_parts_tm(char *name, GLOBAL_ID_CELL *gic)
{
  PART *p, *q;
  char *p_package, *q_package;
  UBYTE p_mode, q_mode;
  TYPE *p_cover_type, *q_cover_type;
  Boolean p_fe, q_fe, p_sees_q, q_sees_p;
  Boolean p_is_main, q_is_main, p_is_main_interface;
  Boolean p_visible_to_main_imp, p_is_standard;

  /*-------------------------*
   * Try all pairs of parts. *
   *-------------------------*/

  for(p = gic->entparts; p != NULL && p->next != NULL; p = p->next) {

    /*------------------------------------------------------------*
     * Characteristics of p are computed here out of the q loop.  *
     * p_is_main_interface and p_is_visible_to_main_imp should	  *
     *  only be true if there is an implementation package that	  *
     * has a different name from the interface package.	      	  *
     *								  *
     * If p is an underriding part, then it does not need to be   *
     * compared with any other parts.				  *
     *------------------------------------------------------------*/

    p_mode = p->mode;
    if(p_mode & UNDERRIDES_MODE_MASK) continue;

    p_package     = p->package_name;
    p_is_main     = (p_package == main_package_name);
    p_is_standard = (p_package == standard_package_name ||
		     p_package == alt_standard_package_name);
    p_fe          = p->from_expect;
    if(main_package_imp_name != NULL) {
      p_is_main_interface   = p_is_main;
      p_visible_to_main_imp = visible_part(main_package_imp_name, p);
    }
    else {
      p_is_main_interface = p_visible_to_main_imp = FALSE;
    }

    /*-------------------------------------------------------------*
     * A part normally covers its type.  If the part is irregular, *
     * it covers all types with its domain.  get_part_cover_type   *
     * gives us the appropriate type to cover.			   *
     *-------------------------------------------------------------*/

    bump_type(p_cover_type = get_part_cover_type(p));

    /*----------------------------------------------------------*
     * Now look at each part after p in the chain.  That way,   *
     * we look at each pair just once.				*
     *----------------------------------------------------------*/

    for(q = p->next; q != NULL; q = q->next) {

      /*--------------------------------------------------------*
       * Do not compare parts where overlaps are permitted by   *
       * override/default/underride rules.  			*
       *--------------------------------------------------------*/

      q_mode = q->mode;
      if(q_mode & UNDERRIDES_MODE_MASK) continue;
      if((p_mode & DEFAULT_MODE_MASK) && (q_mode & OVERRIDES_MODE_MASK)) {
	continue;
      }
      if((q_mode & DEFAULT_MODE_MASK) && (p_mode & OVERRIDES_MODE_MASK)) {
	continue;
      }

      /*------------------------------------------------------------*
       * Do not compare parts that were both placed by the standard *
       * package.  This will also make sure that primitives are     *
       * not compared, since their package name is "standard", 	    *
       * except for primitives placed by Import "-..." imports.	    *
       *------------------------------------------------------------*/

      q_package = q->package_name;
      q_is_main = (q_package == main_package_name);
      if(p_is_standard && 
	 (q_package == standard_package_name ||
	  q_package == alt_standard_package_name)) continue;

      /*--------------------------------------------------------*
       * If p and q were both placed by expectations in the	*
       * export part of the main package, then do not compare   *
       * them.							*
       *--------------------------------------------------------*/

      q_fe = q->from_expect;
      if(q_fe && p_fe & q_is_main && p_is_main) continue;

      /*--------------------------------------------------------*
       * If at least one of p and q was placed by an expectation*
       * and both are attributed to the same package, then do   *
       * not check for an overlap.				*
       *--------------------------------------------------------*/

      if(p_fe || q_fe) {
	char* p_attributed = p->attributed_package_name;
        char* q_attributed = q->attributed_package_name;
	if(p_attributed == main_package_imp_name) {
	  p_attributed = main_package_name;
	}
	if(q_attributed == main_package_imp_name) {
	  q_attributed = main_package_name;
	}
        if(p_attributed == q_attributed) continue;
      }

      /*----------------------------------------------------------*
       * Find out whether the package that created p can see part *
       * q, and whether the package that created q can see p.     *
       * We need to be careful here with packages that are broken *
       * into separate interface and implementation packages.  If *
       * p is an expectation that was placed by the interface     *
       * package (name main_package_name) and q is visible to the *
       * implementation package (name main_package_imp_name),     *
       * then p can see q.  A similar check must be made when     *
       * checking if q can see p.				  *
       *----------------------------------------------------------*/
      
      p_sees_q = visible_part(p_package, q) || 
	         (p_is_main && visible_part(main_package_imp_name, q));
      q_sees_p = (p_visible_to_main_imp && q_is_main) ||
		 visible_part(q_package, p);

      /*----------------------------------------------------------------*
       * The code below determines when there is a bad overlap.  The	*
       * first part tests for various circumstances that indicate that	*
       * the check should be done.  Each such circumstance causes a 	*
       * jump to label check_overlap, below.  (I find this easier to	*
       * read than something that would be considered more 		*
       * "structured".)			 				*
       *								*
       * There is a bad overlap in the following circumstances. 	*
       * First, the types of p and q must overlap.  That test   	*
       * is made below because it is more expensive than the    	*
       * tests done here.  Second, one of the following must hold.	*
       *								*
       *  1. p_fe and q_fe and !p_sees_q and !q_sees_p.			*
       *								*
       *     In this case, we have two parts that were both placed 	*
       *     by imported expectations, in packages that do not 		*
       *     see one another because they were both imported by		*
       *     some other package.  Each of those packages will		*
       *     provide its definition, and those definitions will		*
       *     conflict.							*
       *								*
       *  2. p_fe and !q_fe and !p_sees_q.				*
       *								*
       *     In this case, we have a definition (p) that was placed	*
       *     by an imported expectation, and a true definition		*
       *     (not from an expectation) that cannot be seen		*
       *     by the package that created p.  The package that		*
       *     created p will have to make its own definition, and	*
       *     that definition will conflict with q.			*
       *								*
       *  3. q_fe and !p_fe and !q_sees_p.				*
       *								*
       *     This is by symmetry with case 2.				*
       *								*
       *  4. !p_fe and !q_fe						*
       *								*
       *     Then neither p nor q is from an expectation.  		*
       *     We must check them.					*
       *								*
       *  5. p_fe and q_fe and 						*
       *     ((p_is_main and q->in_body) || (q_is_main and p->in_body))	*
       *								*
       *     Then one of p and q was placed by an expectation from	*
       *     the export	part of the main package, and the other was	*
       *     placed by an import that was done while reading the	*
       *     implementation part of the main package. 	Also, they	*
       *     are attributed to different packages.			*
       *     We must check them, to see if a package should		*
       *     have been imported in the export part, or marked with a    *
       *     different attributed package.				*
       *----------------------------------------------------------------*/

      /*----------------*
       * Cases 1 and 2. *
       *----------------*/

      if(p_fe && !p_sees_q) {
	if(!q_fe || !q_sees_p) {
	  goto check_overlap;
	}
      }

      /*---------*
       * Case 3. *
       *---------*/

      if(q_fe && !p_fe && !q_sees_p) {
	goto check_overlap;
      }

      /*---------*
       * Case 4. *
       *---------*/

      if(!p_fe && !q_fe) {
	goto check_overlap;
      }

      /*---------*
       * Case 5. *
       *---------*/

      if(p_fe && q_fe && 
         ((p_is_main && q->in_body) || (q_is_main && p->in_body))) {
	goto check_overlap;
      }

      /*------------------------------------------------------*
       * If none of the above conditions is true, then we do  *
       * not need to consider parts p and q further.	      *
       *------------------------------------------------------*/

      continue;

    check_overlap:
      bump_type(q_cover_type = get_part_cover_type(q));
      if(!disjoint(p_cover_type, q_cover_type)) {

        /*------------------------------------------------------*
         * These parts conflict.  If one was placed in by	*
         * the export part of the main package, (and the other  *
	 * was not, since otherwise we would not get here) 	*
 	 * then report that a package was imported in the body  *
	 * when it should have been imported in the interface.	*
         * Otherwise, report conflicting definitions.		*
	 *------------------------------------------------------*/

	if(p_fe && p_is_main) {
	  warn2(HIDDEN_IMPORT_ERR, q_package, display_name(name), 0);
        }
	else if(q_fe && q_is_main) {
	  warn2(HIDDEN_IMPORT_ERR, p_package, display_name(name), 0);
        }
	else {
          parts_overlap_err(name, p, q);
	}

#       ifdef DEBUG
	  if(trace_defs > 1) {
	    trace_t(294);
	    print_entpart(p, 1);
	    trace_t(294);
	    print_entpart(q, 1);
	  }
#       endif

      } /* end if(!disjoint...) */

      drop_type(q_cover_type);

    } /* end for(q = ...) */

    drop_type(p_cover_type);

  } /* end for(p = ...) */
}


/****************************************************************
 *			OVERLAP_ALL_PARTS_TM			*
 ****************************************************************
 * Do overlap_parts_tm on every cell in the global id table.	*
 ****************************************************************/

PRIVATE void overlap_all_parts_help(HASH2_CELLPTR h)
{
  overlap_parts_tm(h->key.str, h->val.gic);
}

/*-----------------------------------------------------------*/

void overlap_all_parts_tm(void)
{
  scan_hash2(global_id_table, overlap_all_parts_help);
}


/****************************************************************
 *		    WARN_IF_HIDDEN_EXPECTATION			*
 ****************************************************************
 * Expectation or definition name:t is being issued.		*
 * Issue a warning of a hidden expectation of a nonhidden id	*
 * if the expectation is in the body, the			*
 * package has an export part and type t does not contain a	*
 * hidden identifier.						*
 *								*
 * If check is true, only warn if t contains a type that has	*
 * not yet been expected. If check is false, warn regardless of	*
 * whether t contains a type that has not been expected.	*
 ****************************************************************/

PRIVATE void warn_if_hidden_expectation(char *name, TYPE *t, Boolean check)
{
  TYPE* new_t = NULL;

  if(main_context == BODY_CX && seen_export && !is_hidden_id(name)
     && !has_hidden_id_t(t)) {
    if(check) {
      TYPE_LIST* local_expects = local_expectation_list_tm(name, 0);
      bump_list(local_expects);
      bump_type(new_t = missing_type(local_expects, t));
      drop_list(local_expects);
    }

    if(!check || new_t != NULL) {
      warn1(HIDDEN_EXP_NONHIDDEN_ID_ERR, name, 0);
      if(check) {
	err_print(FOR_EXAMPLE_STR_ERR, name);
	err_print_ty(new_t);
	err_nl();
      }
#     ifdef DEBUG
	if(trace) {
	  trace_t(349, name, toint(is_hidden_id(name)));
	}
#     endif
    }
    drop_type(new_t);
  } /* end if(main_context == BODY_CX ...) */
}
