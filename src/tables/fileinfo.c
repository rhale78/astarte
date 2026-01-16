/****************************************************************
 * File:    tables/fileinfo.c
 * Purpose: Maintain tables for packages that are currently active
 *          and for files that might become active.  
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

#include <memory.h>
#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include <ctype.h>
#include "../misc/misc.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../classes/classes.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../error/error.h"
#include "../lexer/modes.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../parser/tokens.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			main_package_name			*
 *			main_package_imp_name			*
 ****************************************************************
 * main_package_name is the name of the main package.  It is    *
 * set in parser.y, under nonterminal package.			*
 *								*
 * If we are compiling an implementation package, then 		*
 * main_package_name is set to the name of the interface	*
 * package, and main_package_imp_name is set to the name of	*
 * the implementation package.  If we are compiling a single-	*
 * file package, then main_package_imp_name is NULL.		*
 ****************************************************************/

char* main_package_name;
char* main_package_imp_name = NULL;

/****************************************************************
 *			standard_package_name			*
 *			alt_standard_package_name		*
 ****************************************************************
 * standard_package_name is the name of the standard package.   *
 * It is set in compiler.c.					*
 *								*
 * alt_standard_package_name is ALT_STANDARD_AST_NAME, but in   *
 * the string table.  It is also set in compiler.c.		*
 ****************************************************************/

char* standard_package_name;
char* alt_standard_package_name;

/****************************************************************
 *			import_seq_num				*
 ****************************************************************
 * Import_seq_num keeps track of the number of packages that	*
 * have been imported, not counting the standard package.  It	*
 * is used to sequence the packages.  Sequencing information	*
 * is used, for example, in deciding whether a pattern match	*
 * class applies to the current package. 			*
 ****************************************************************/

int import_seq_num = 0;

/****************************************************************
 *			file_info_st				*
 ****************************************************************
 * File_info_st is a stack of frames describing files being	*
 * read.  When a file is started, a frame for it is pushed	*
 * on this stack.  When the file is completed, the frame is	*
 * popped.							*
 *								*
 * Each frame holds the following fields.			*
 *								*
 *  package_name	The name of the package.		*
 *								*
 *  file_name		The name of the file.			*
 *								*
 *  file		The open FILE* value that should be	*
 *			read to get more characters from the	*
 *			file.					*
 *								*
 *  lexbuf		The flex buffer that is associated	*
 *			with this file.				*
 *								*
 *  line		The current line number of the file.	*
 *								*
 *  chars_in_line	The number of characters that have 	*
 *			already been read from the current line.*
 *								*
 *  import_seq_num	The sequence number of this import.	*
 *			(The n-th package to be started has     *
 *			sequence number n-1.)			*
 *								*
 *  context		The current context (EXPORT_CX, etc.)	*
 *			of this file.				*
 *								*
 *  is_interface_package  TRUE if this is an interface 		*
 *			  package.				*
 *								*
 *  shadow_stack	The shadow stack used by the lexer	*
 *			for this file.				*
 *								*
 *  shadow_nums		The number stack that is used by the	*
 *			lexer for this file.			*
 *								*
 *  public_packages	A list of the names of the packages 	*
 *			that can see this package during this   *
 *			import.  This consists of this package  *
 *			plus the packages on a direct import	*
 *			chain from the main package to this	*
 *			package.				*
 *								*
 *  private_packages	A list of the names of the packages	*
 *			that can see private expectations in	*
 *			this package during this import.  This  *
 *			consists of this package, and any 	*
 *			packages that are doing private imports,*
 *			directly or indirectly, of this package.*
 *								*
 *  imported_packages	A list of the names of the packages     *
 *			that this package has imported, either	*
 *			directly or indirectly by importing	*
 *			other packages that imported them.	*
 *								*
 *  import_ids		A list of the ids that are being	*
 *			imported from this package.  For 	*
 *			example, if Import x,y from ...	was	*
 *			stated, the import_ids would be the	*
 *			list ["x", "y"].			*
 *								*
 *  import_dir		This is the current import directory,	*
 *			as set by a directory declaration.	*
 *								*
 *  import_dir_table	This is the directory name table, as	*
 *			set by directory name = dir		*
 *			declarations.  It is a hash table	*
 *			associating directories with names.	*
 *								*
 *  assume_table						*
 *  global_assume_table	These hash tables store information	*
 *			about assume declarations.  Each 	*
 *			associates a type with each name.	*
 *			Table assume_table give local 		*
 *			assumptions, and global_assume_table	*
 *			gives global assumptions.		*
 *								*
 *  assume_role_table						*
 *  global_assume_role_table					*
 *			These tables are similar to assume_table *
 *			and global_assume_table, but store the  *
 * 			role that is part of each assumption.	*
 *								*
 *  patfun_assume_table
 *  global_patfun_assume_table
 *			These tables associate value 1 with 	*
 *			each assumed pattern function.		*
 *			patfun_assume_table is used for local	*
 *			assumptions, and global_patfun_assume_table
 *			is used for global assumptions.		*
 *								*
 *  nat_const_assumption					*
 *  global_nat_const_assumption					*
 *  real_const_assumption					*
 *  global_real_const_assumption				*
 * 			These are types indicating assumptions  *
 *			such as Assume 0: Natural.		*
 *								*
 *  no_tro_table	This is a hash1-table that stores the	*
 *			names of all ids that have been		*
 *			declarared no_tro.			*
 *								*
 *  no_tro_backout	This is a list of the strings that      *
 * 			should be removed from no_tro_table     *
 *			at the end of the current declaration.  *
 *								*
 *  abbrev_id_table						*
 *  global_abbrev_id_table					*
 *			These tables associate types with	*
 *			names, and give type abbreviations.	*
 *								*
 *  ahead_descr_table	This table associates a chain of	*
 *			DESCRIP_CHAIN nodes with each id for	*
 *			which an ahead description has been	*
 *			made.					*
 *								*
 *  local_expect_table	This table associates a list of types	*
 *			with each id that has been expected	*
 *			in this package.  The types		*
 *			in this list should be used as		*
 *			assumptions in type inference.		*
 *								*
 *  other_local_expect_table					*
 *			Similar to local_expect_table, but the	*
 *			types in this list should not be used	*
 *			as assumptions in type inference.	*
 *								*
 *  op_table		This table associates a binary operator *
 *			code with each declared binary operator *
 *			that is visible to this package.	*
 *								*
 *  unary_op_table	This hash1-table stores all names	*
 *			that have been declared unary operators.*
 *								*
 *  default_table	This table associates a default type	*
 *			with each genus or community name that	*
 *			has a default that is visible in this	*
 *			package.				*
 *								*
 *  propagate_mode	The value to restore propagate_mode to  *
 *			when this package is resumed.		*
 *								*
 *  next		The frame for the package that imported *
 *			this one.				*
 ****************************************************************/

IMPORT_STACK* file_info_st = NULL;

/****************************************************************
 *			standard_import_frame			*
 ****************************************************************
 * Standard_import_frame is the frame for the standard package.	*
 ****************************************************************/

IMPORT_STACK* standard_import_frame = NULL;

/****************************************************************
 *			import_archive_list			*
 ****************************************************************
 * When a package is finished, its frame is popped from 	*
 * file_info_st.  We still need some information about that	*
 * package, however, in case it is imported again.  (We do	*
 * not reread the package.)  So the frame that is popped from	*
 * file_info_st is moved to import_archive_list.  Information	*
 * that will not be needed at a later import is deleted from	*
 * the frame.							*
 ****************************************************************/

IMPORT_STACK* import_archive_list = NULL;


/****************************************************************
 *			package_structure_table			*
 ****************************************************************
 * package_structure_table is a hash table that associates,     *
 * with each package name, a list of the packages that are      *
 * beneath it (including itself) in the package hierarchy as    *
 * defined by the isKindOf phrases in the package headings.	*
 ****************************************************************/

HASH2_TABLE* package_structure_table = NULL;

/****************************************************************
 *		      INSTALL_PACKAGE_ISKINDOF			*
 ****************************************************************
 * Package A is declared to be a kind of package B.  Store this *
 * in the package_structure_table.  This involves adding A and  *
 * every package that is a descendant of A to the list of       *
 * descendants for B and for all packages that contain B in	*
 * their lists of decendants.					*
 *								*
 * If A is already in the descendant list for B, do not make    *
 * any changes.							*
 *								*
 * Function iskindof_help installs all members of list		*
 * install_iskindof_A_list into cell h, provided h either has   *
 * key install_iskindof_B or has a value list that contains     *
 * install_iskindof_B.						*
 ****************************************************************/

STR_LIST *install_iskindof_A_list;
char     *install_iskindof_B;

PRIVATE void install_iskindof_help(HASH2_CELLPTR h)
{
  if(h->key.str == install_iskindof_B ||
     str_memq(install_iskindof_B, h->val.list)) {
    SET_LIST(h->val.list, 
	     str_list_union(h->val.list, install_iskindof_A_list));
  }
}

/*-------------------------------------------------------*/

PRIVATE void install_package_iskindof(char *A, char *B)
{
  HASH2_CELLPTR h;
  HASH_KEY u;
  STR_LIST *A_list;

  /*----------------------------------------------------------------*
   * Check whether A is already a descendant of B.  If B is not     *
   * in the table at all, install it with an empty descendant list. *
   *----------------------------------------------------------------*/

  u.str = B;
  h     = insert_loc_hash2(&package_structure_table, u, strhash(u.str), eq);
  if(h->key.num == 0) {
    h->key.str  = B;
    h->val.list = NIL;
  }
  if(!str_memq(A, h->val.list)) {

    /*------------------------------------------------------------------*
     * A is not yet a descendant of B.  Get the list of descendants	*
     * of A.								*
     *------------------------------------------------------------------*/

    u.str  = A;
    h      = locate_hash2(package_structure_table, u, strhash(u.str), eq);
    bump_list(A_list = (h->key.num == 0) ? NIL : h->val.list);
 
    /*-------------------------------------------------------------------*
     * Add A and all members of A_list to the descendant list of B and   *
     * all members of the package_structure_table that contain B in	 *
     * their descendant lists.						 *
     *-------------------------------------------------------------------*/

    SET_LIST(A_list, str_cons(A, A_list));
    install_iskindof_A_list = A_list;
    install_iskindof_B      = B;
    scan_hash2(package_structure_table, install_iskindof_help);
    drop_list(A_list);
  }
}


/****************************************************************
 *			    PACKAGE_IS_BENEATH			*
 ****************************************************************
 * Return true if package A is equal to or beneath packages B   *
 * in the package hierarchy.					*
 ****************************************************************/

PRIVATE Boolean package_is_beneath(char *A, char *B)
{
  HASH2_CELLPTR h;
  HASH_KEY u;

  if(A == B) return TRUE;
  if(A == main_package_imp_name && B == main_package_name) return TRUE;

  u.str = B;
  h = locate_hash2(package_structure_table, u, strhash(u.str), eq);
  if(str_memq(A, h->val.list)) return TRUE;
  if(A == main_package_imp_name && str_memq(main_package_name, h->val.list)) {
    return TRUE;
  }
  return FALSE;
}


/****************************************************************
 *			    PACKAGE_IS_BENEATH_MEM		*
 ****************************************************************
 * Return true if package A is equal to or beneath some package *
 * in list L.							*
 ****************************************************************/

PRIVATE Boolean package_is_beneath_mem(char *A, STR_LIST *B)
{
  STR_LIST *p;

  for(p = B; p != NIL; p = p->tail) {
    if(package_is_beneath(A, p->head.str)) return TRUE;
  }
  return FALSE;
}


/************************************************************************
 *			GET_PROTECTED_PACKAGES				*
 ************************************************************************
 * Return the list of all members of pkgs that are equal to or beneath  *
 * some member of L in the package hierarchy.				*
 ************************************************************************/

PRIVATE STR_LIST* 
get_protected_packages(STR_LIST *pkgs, STR_LIST *L)
{
  if(pkgs == NIL) return NIL;
  else {
    STR_LIST *tl = get_protected_packages(pkgs->tail, L);
    if(package_is_beneath_mem(pkgs->head.str, L)) {
      return str_cons(pkgs->head.str, tl);
    }
    else return tl;
  }
}


/************************************************************************
 *			GET_VISIBLE_IN					*
 ************************************************************************
 * An expectation of identifier name is (or might be) about to be	*
 * issued by the current package.					*
 *									*
 * Return a list that contains the names of all packages that can see   *
 * this expectation, where the expectation is private if 		*
 * has_mode(mode, PRIVATE_MODE) is nonzero, is protected if not         *
 * private and has_mode(mode, PROTECTED_MODE) is nonzero, and		*
 * otherwise is public.	 When protected, list mode->visibleIn tells     *
 * the packages that can see this declaration.  (All packages that are  *
 * declared to be a kind of one of those packages can also see this     *
 * declaration.)							*
 *									*
 * Special case: return NIL if this expectation is being made by the	*
 * standard package.							*
 *									*
 * MODE is a safe pointer.  It only lives as long as this		*
 * function call.							*
 ************************************************************************/

STR_LIST* get_visible_in(MODE_TYPE *mode, char *name)
{
  int package_count;
  STR_LIST *p, *packages, *result;
  IMPORT_STACK *package_imports;

  /*----------------------------------------------*
   * If this is the standard package, return NIL, *
   * as stated above. 				  *
   *----------------------------------------------*/

  if(current_package_name == standard_package_name) {
    return NIL;
  }

  /*-----------------------------*
   * Get the source list of ids. *
   *-----------------------------*/

  packages = has_mode(mode, PRIVATE_MODE)   
                 ? current_private_packages :
             has_mode(mode, PROTECTED_MODE) 
                 ? get_protected_packages(current_public_packages, 
					  get_mode_visible_in(mode))
		 : current_public_packages;
  bump_list(packages);

  /*----------------------------------------------------------*
   * Get the stack held by the lexer that tells ids that are  *
   * being imported. 					      *
   *----------------------------------------------------------*/

  package_imports = file_info_st;

  /*--------------------------------------------------------------------*
   * Break at a package that does not want to import this id.		*
   * package_count is initially 1 to account for the current package,	*
   * which does want to import this id. 				*
   *--------------------------------------------------------------------*/

  package_count = 1;
  p = packages;
  while(package_imports != NULL && p != NULL 
	&& (package_imports->import_ids == (LIST *) 1 
	    || str_member(name, package_imports->import_ids))) {
    package_count++;
    package_imports = package_imports->next;
    p = p->tail;
  }

  /*----------------------------------------------------------------*
   * If p = NIL, then the entire list packages is the visible list. *
   *----------------------------------------------------------------*/

  if(p == NIL) result = packages;

  /*--------------------------------------------------------------------*
   * If p != NIL, then only take package_count names from the front of  *
   * list packages.							*
   *--------------------------------------------------------------------*/

  else {
    int i;
    STR_LIST* q;
    result = NIL;
    for(i = 0, q = packages; i < package_count; i++, q = q->tail) {
      result = str_cons(q->head.str, result);
    }
  }
  bump_list(result);

# ifdef DEBUG
    if(trace_defs) {
       trace_t(340, name);
       print_str_list_nl(result);
       trace_t(341);
       print_str_list_nl(packages);
     }
# endif

  drop_list(packages); 
  if(result != NIL) result->ref_cnt--;
  return result;
} 


/****************************************************************
 *			    CURRENT_OR_STANDARD_FRAME		*
 ****************************************************************
 * This function returns the package frame that should be used  *
 * in the current package.  If we are doing the standard	*
 * package, that is the standard frame.  Otherwise, it is the	*
 * frame on the top of the file_info_st chain.			*
 ****************************************************************/

IMPORT_STACK *current_or_standard_frame(void)
{
  return (current_package_name == standard_package_name)
           ? standard_import_frame
	   : file_info_st;
}


/****************************************************************
 *			CHECK_IMPORTED				*
 ****************************************************************
 * Check that file name has been imported from the standard     *
 * directory by the current package.  If so, then return TRUE.  *
 * If not, then issue a warning if complain is true, and return *
 * FALSE.							*
 ****************************************************************/

/*--------------------------------------------------------------*
 * Array warned_about holds names of files that have already 	*
 * been warned about.  num_warned_about is the number of 	*
 * entries in that array.  					*
 *--------------------------------------------------------------*/

#define WARNED_ABOUT_SIZE 4
PRIVATE char* warned_about[WARNED_ABOUT_SIZE];
PRIVATE int   num_warned_about = 0;

Boolean check_imported(char *name, Boolean complain)
{
  int i;
  char* this_file, *pkg_name;
  Boolean result;

  /*--------------------------------------------------*
   * Get the full name of the file, in internal form. *
   *--------------------------------------------------*/

  char* full_name = (char*) BAREMALLOC(strlen(STD_DIR) + strlen(name) + 2);

  sprintf(full_name, "%s%c%s", STD_DIR, INTERNAL_DIR_SEP_CHAR, name);
  this_file = full_file_name(full_name, 0);

  /*--------------------------------------*
   * Convert to a package name and check. *
   *--------------------------------------*/

  pkg_name = package_name_from_file_name(this_file);
  if(pkg_name == NULL) result = FALSE;
  else result = str_member(pkg_name, file_info_st->imported_packages);

  if(!result && complain) {

    /*------------------------------------------*
     * Check if already warned about this file. *
     * If not, then do the warning.		  *
     *------------------------------------------*/

    for(i = 0; i < num_warned_about; i++) {
      if(strcmp(warned_about[i], this_file) == 0) goto out;
    }

    warn2(EXPECTED_IMPORT_ERR, name, name, 0);
    if(num_warned_about < WARNED_ABOUT_SIZE) {
      warned_about[num_warned_about++] = this_file;
    }
  }

 out:
  FREE(full_name);
  return result;
}


/****************************************************************
 *			NOTE_IMPORTED				*
 ****************************************************************
 * Record package_name in the imported_packages list of each    *
 * package that is listening.					*
 ****************************************************************/

void note_imported(char *package_name)
{
  IMPORT_STACK *p;

  for(p = file_info_st; p != NULL; p = p->next) {
    if(package_name != p->package_name && 
       !str_member(package_name, p->imported_packages)) {
      SET_LIST(p->imported_packages, 
	       str_cons(package_name, p->imported_packages));
    }
  }
}


/****************************************************************
 *			INSTALL_PACKAGE_NAME			*
 ****************************************************************
 * Install in_name as the current package name.  Complain if 	*
 * the name is a duplicate, or if name is symbolic.  		*
 *								*
 * Also install list this package in the iskindof lists (table  *
 * package_structure_table) for all members of list inherits.   *
 *								*
 * Return the name, in the string table.		 	*
 *								*
 * If name is too long, it is shortened.			*
 ****************************************************************/

char* install_package_name(char *in_name, STR_LIST *inherits)
{
  char replace = 0;
  char *name;

  /*-------------------------------------------------*
   * The name of the package should not be symbolic. *
   *-------------------------------------------------*/

  if(!isalpha(in_name[0])) {
    syntax_error(SYM_PACKAGE_NAME_ERR, 0);
  }

  /*------------------------------------------------*
   * If name is too long, shorten it. In any event, *
   * put name in the string table now.              *
   *------------------------------------------------*/

  if(strlen(in_name) > MAX_PACKAGE_NAME_LENGTH) {
    replace = in_name[MAX_PACKAGE_NAME_LENGTH];
    in_name[MAX_PACKAGE_NAME_LENGTH] = 0;
  }
  name = id_tb0(in_name);

  /*------------------------------------------------------------*
   * Record this package in the imported_packages list of all	*
   * packages that are listening.				*
   *------------------------------------------------------------*/

  note_imported(name);

  /*-----------------------------------*
   * Install the package inheritances. *
   *-----------------------------------*/

  {STR_LIST *p;
   for(p = inherits; p != NIL; p = p->tail) {
     install_package_iskindof(name, p->head.str);
   }
  }

  /*----------------------------------------------------------------*
   * Install the package name in the current frame in file_info_st. *
   * Also filter out the list of packages that see protected	    *
   * expectations.						    *
   *----------------------------------------------------------------*/

  current_package_name =
    current_public_packages->head.str =
    current_private_packages->head.str = name;

  /*------------------------------------------------------------*
   * Install this package name in imported_package_names across *
   * from the current file name. 				*
   *------------------------------------------------------------*/

  {STR_LIST *p = imported_package_names, *q = imported_files;
   while(q != NULL && strcmp(q->head.str, current_file_name) != 0) {
     p = p->tail;
     q = q->tail;
   }
   if(p != NULL) p->head.str = name;
  }

  /*----------------------------*
   * Check for duplicate names. *
   *----------------------------*/

  {STR_LIST *p, *q;
   for(p = imported_package_names, q = imported_files;
       p != NULL; p = p->tail, q = q->tail) {
     if(p->head.str != NULL && strcmp(p->head.str, name) == 0
	&& strcmp(q->head.str, current_file_name) != 0 
	&& !compiling_standard_asi) {
       char* ext_name = strdup(nonnull(current_file_name));
       force_external(ext_name);
       warn1(DUPLICATE_PACKAGE_NAMES_ERR, name, 0);
       err_print(PACKAGE_FILE_ERR, ext_name);
       err_print(PACKAGE_FILE_ERR, q->head.str);
       FREE(ext_name);
       break;
     }
   }
  } /* end for(p = ...) */

  /*-----------------------------------*
   * Restore name, if it was shortend. *
   *-----------------------------------*/

  if(replace != 0) {
    in_name[MAX_PACKAGE_NAME_LENGTH] = replace;
  }

# ifdef DEBUG
    if(trace) {
      trace_t(471);
      trace_t(470);
      print_str_list_nl(current_public_packages);
      trace_t(356);
      print_str_list_nl(current_private_packages);
    }
# endif

  return name;
}


/****************************************************************
 *			PUSH_FILE_INFO_FRAME			*
 ****************************************************************
 * Push a new, initialized, frame onto file_info_st.  The	*
 * package_name, private_packages, and public_packages 		*
 * fields must be modified later, when the			*
 * name of the current package is learned.  The modification	*
 * is done in the parser, using install_package_name.		*
 ****************************************************************/

void push_file_info_frame(char *infileName, FILE *file)
{
  IMPORT_STACK* s = allocate_import_stack();

  s->line           = 0;
  s->import_seq_num = ++import_seq_num;
  s->context        = EXPORT_CX;
  s->package_name   = standard_package_name;  /* Modified later */
  s->file_name      = string_const_tb0(infileName);
  s->file           = file;
  s->lexbuf         = yy_create_buffer(file, YY_BUF_SIZE);
  s->imported_packages = NIL;
  bump_list(s->import_ids = new_import_ids);

  {STR_LIST *old_public_packages, *old_private_packages, *private_tail;
   if(file_info_st == NULL) {
     old_public_packages = 
     old_private_packages = NIL;
   }
   else {
     old_public_packages    = current_public_packages;
     old_private_packages   = current_private_packages;
   }
   bump_list(s->public_packages =
	     str_cons(standard_package_name, old_public_packages));
   private_tail = has_mode(&this_mode, PRIVATE_MODE)
		    ? old_private_packages : NIL;
   bump_list(s->private_packages =
	     str_cons(standard_package_name, private_tail));
  }

# ifdef DEBUG
    if(trace_importation) {
      trace_t(467, infileName);
      trace_t(470);
      print_str_list_nl(s->public_packages);
      trace_t(356);
      print_str_list_nl(s->private_packages);
      trace_t(469);
      print_str_list_nl(s->import_ids);
    }
# endif

  s->next = file_info_st;
  file_info_st = s;
}


/****************************************************************
 *			DROP_LOCAL_STACK_FRAME_PARTS		*
 ****************************************************************
 * Drop the tables in frame p that are local to the package	*
 * that p describes.						*
 ****************************************************************/

PRIVATE void drop_local_stack_frame_parts(IMPORT_STACK *p)
{
  SET_LIST(p->shadow_stack, NULL);
  SET_LIST(p->shadow_nums, NULL);
  SET_LIST(p->private_packages, NULL);
  SET_LIST(p->public_packages, NULL);
  SET_LIST(p->imported_packages, NULL);
  SET_LIST(p->import_ids, NULL);
  SET_LIST(p->no_tro_backout, NULL);

  scan_and_clear_hash2(&(p->assume_table), drop_hash_type);
  scan_and_clear_hash2(&(p->assume_role_table), drop_hash_role);
  scan_and_clear_hash2(&(p->abbrev_id_table), drop_hash_ctc);
  scan_and_clear_hash2(&(p->local_expect_table), drop_hash_list);
  scan_and_clear_hash2(&(p->other_local_expect_table), drop_hash_list);
  free_hash2(p->patfun_assume_table);
  p->patfun_assume_table = NULL;
  free_hash2(p->ahead_descr_table);
  p->ahead_descr_table = NULL;
  free_hash1(p->no_tro_table);
  SET_TYPE(p->nat_const_assumption, NULL);
  SET_TYPE(p->real_const_assumption, NULL);
}


/****************************************************************
 *			ARCHIVE_IMPORT_FRAME			*
 ****************************************************************
 * Put frame p in the import_archive list.			*
 ****************************************************************/

void archive_import_frame(IMPORT_STACK *p)
{
# ifdef DEBUG
    if(trace_importation) trace_t(537, p->package_name);
# endif

  drop_local_stack_frame_parts(p);
  p->next = import_archive_list;
  import_archive_list = p;
}
  

/****************************************************************
 *			ARCHIVE_MAIN_FRAME			*
 ****************************************************************
 * Add the top frame of file_info_st to the archive, so that 	*
 * it can be second-imported.  Don't bother that frame though   *
 * -- copy it instead.						*
 ****************************************************************/

void archive_main_frame(void)
{
  IMPORT_STACK* frame = file_info_st;
  IMPORT_STACK* p     = allocate_import_stack();

# ifdef DEBUG
    if(trace_importation) trace_t(536, frame->package_name);
# endif

  *p = *frame;
  p->import_dir_table 		= copy_hash2(p->import_dir_table);
  p->global_assume_table 	= copy_hash2(p->global_assume_table);
  p->global_assume_role_table 	= copy_hash2(p->global_assume_role_table);
  p->global_patfun_assume_table = copy_hash2(p->global_patfun_assume_table);
  p->global_abbrev_id_table 	= copy_hash2(p->global_abbrev_id_table);
  p->op_table 			= copy_hash2(p->op_table);
  p->unary_op_table		= copy_hash1(p->unary_op_table);
  p->default_table 		= copy_hash2(p->default_table);

  scan_hash2(p->global_assume_role_table, 	bump_hash_role);
  scan_hash2(p->global_abbrev_id_table, 	bump_hash_type);
  scan_hash2(p->global_assume_table, 		bump_hash_type);
  scan_hash2(p->default_table, 			bump_hash_type);

  bump_type(p->global_nat_const_assumption);
  bump_type(p->global_real_const_assumption);

  p->next = import_archive_list;
  import_archive_list = p;
}


/****************************************************************
 *			COPY_TABLES_DOWN			*
 ****************************************************************
 * Copy tables from file_info_st to file_info_st->next.		*
 ****************************************************************/

void copy_tables_down(void)
{
  register IMPORT_STACK* fs = file_info_st;
  register IMPORT_STACK* s = fs->next;

  /*-----------------------------------------------------------------*
   * Note: ref counts are inherited, since they will not be dropped. *
   *-----------------------------------------------------------------*/

  s->nat_const_assumption 	= fs->nat_const_assumption;
  s->real_const_assumption	= fs->real_const_assumption;
  s->assume_table 		= fs->assume_table;
  s->assume_role_table 		= fs->assume_role_table;
  s->abbrev_id_table   		= fs->abbrev_id_table;
  s->ahead_descr_table  	= fs->ahead_descr_table;
  s->local_expect_table 	= fs->local_expect_table;
  s->other_local_expect_table 	= fs->other_local_expect_table;
  s->op_table 			= fs->op_table;
  s->import_dir_table		= fs->import_dir_table;
  s->unary_op_table 		= fs->unary_op_table;
  s->default_table		= fs->default_table;
  s->no_tro_table		= fs->no_tro_table;
}


/****************************************************************
 *			PACKAGE_NAME_FROM_FILE_NAME		*
 ****************************************************************
 * Return the package name that corresponds to file name fname, *
 * or NULL if there is none.					*
 ****************************************************************/

char* package_name_from_file_name(char *fname)
{
  LIST *fnames, *pnames;

  for(fnames = imported_files, pnames = imported_package_names;
      fnames != NULL;
      fnames = fnames->tail, pnames = pnames->tail) {
    if(strcmp(fname, fnames->head.str) == 0) return pnames->head.str;
  }
  return NULL;
}


