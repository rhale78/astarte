Directory dcls
---------------

This directory constains files that manage declarations for
the compiler.  Files are as follows.

dcl.c          This file contains the top-level handler for
               declarations such as define, let, execute, pattern,
               etc, and support for it.  It also contains functions
	       to manage Advise show declarations.

dclclass.c	This file contains functions that manage species,
		genus and community declarations.

dclcnstr.c	This file contains functions for declaring automatic
		functions, etc, that are defined with species
		declarations and bring declarations.

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

		
