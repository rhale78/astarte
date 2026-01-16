/*******************************************************************
 * File: parser/open.h
 * Purpose: handle open conditionals
 * Author: Karl Abrahamson
 *******************************************************************/

extern Boolean seen_open_expr;

void      process_new_open      (EXPR *e);
void      mark_alts_used	(EXPR *e);
EXPR_LIST* get_id_list		(EXPR *e, Boolean pattern);
void      set_open_lists	(EXPR *e);
EXPR* 	  open_if		(EXPR *e);
EXPR* 	  open_choose		(EXPR *e);
EXPR*     open_loop             (EXPR *e);
EXPR* 	  open_stream		(EXPR *e);
void      patch_open            (EXPR *e);
