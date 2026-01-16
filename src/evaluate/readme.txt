Directory evaluate
-------------------

This directory constains files that manage declarations for
the compiler.  Files are as follows.

apply.c		This directory contains functions that handle
		function application instructions.

		It also contains process_demon (for use in calling
		demon functions associated with boxes by OnAssign,
		and functions to handle pausing of threads.

coroutin.c	This directory contains functions that manage
		coroutines.

evalsup.c	This file contains some support functions for
		evaluation.  The following are included.

		  Functions to evaluate entities.  Tree representations
		  of lists are handled during evaluation.  If a
		  lazy value is encountered, it is evaluated using
		  functions from lazy.c.

		  Functions to perform occur checks for unification are
		  here, since they must perform evaluations in order
		  to perform occur checks.

		  Functions to prepare activations for evaluation are
		  here.

		  Functions to wrap and unwrap entities are here.

		  Miscellaneous converters to string are here.

evaluate.c	This file contains the main evaluation function,
		called evaluate.  Function evaluate evaluates an
		expression.

fail.c		This file contains functions for dealing with
		failure and timeout.

instinfo.c	This file contains arrays that hold information about
		instructions.

instruc.h	This file defines the instructions of the interpreter.

language.doc	This file describes the instructions that the 
		interpreter understands.

lazy.c		This file contains functions that are used for
		creating and evaluating lazy values, including
		unevaluated globals.

lazyprim.c	This file contains functions that create and
		evaluate primitive lazy values.

typeinst.c	This file contains functions that evaluate type
		instructions, and create types from them.








dclutil.c	This file contains general functions for defining
		automatically constructed things.

defdcls.c	This file contains functions that manage let and
		lazylet declarations.


deferdcl.c	Some declarations that are made inside extensions
		must be deferred until the extension is finished.
		This file contains functions and variables that
		manage these deferred declarations, and that 
		can issue them when the end of an extension is
		reached.

report.c	This file contains functions that manage and perform
		the information reports that are written into the
		listing.  For example, when the compiler writes

			-----> f: Natural -> Natural

		it is one of the functions in report.c that does
		the print.

somedcls.c	This file contains functions that manage miscelleneous
		declarations.  Included are
		   exception 	dcls
		   description 	dcls
		   team 	dcls
		   execute 	dcls
		   assume 	dcls
		   operator 	dcls
		   pattern 	dcls
		   expand 	dcls

		
