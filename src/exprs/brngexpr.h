/*****************************************************************
 * File:    exprs/brngexpr.h
 * Purpose: Support for bring declarations and expressions
 * Author:  Karl Abrahamson
 *****************************************************************/

EXPR_LIST* bring_exprs	  (LIST *ids, RTYPE tag, EXPR *fro, int line,
			   MODE_TYPE *mode);
EXPR_LIST* bring_by_exprs (LIST *ids, RTYPE tag, EXPR *fun, int line);
