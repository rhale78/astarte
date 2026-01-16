Directory exprs
----------------

This directory contains functions for handling expressions, including
basic constructors and utilities such as one for printing expressions.

choose.c	This file holds functions that build expressions
		that are part of choose and loop expressions.

copyexpr.c	Holds function copy_expr, which copies an expression
		by copying all of its nodes and the types to which
		it refers.

expr.c		Basic constructors for expressions are here.

exprutil.c	This file contains assorted utilities for operating
		on expressions.

prtexpr.c	Functions for printing expressions as part of a 
		compiler listing.

