Directory generate
-------------------

This directory contains functions for generating the byte-code 
that is described in evaluate/language.doc.

generate.c	This file contains utilities and initialization
		code.

genexec.c	This file contains the main code generation
		functions.

genglob.c	This file contains functions for generating the
		global environment code, and for keeping track
		of the global environment during code generation.

genstd.c	This file contains functions for generating the
		standard function definition that go along with
		primitives.

genvar.c	This file contains functions for generating code
		that, when run, creates types.

improve.c	When genexec.c creates byte code, it puts the result
		in arrays.  improve.c contains functions for writing
		those arrays out to the .aso file.  It also contains
		functions for performing rudimentary improvements
		on that code.

prim.c		This file contains functions for generating
		code that performs primitive actions.
