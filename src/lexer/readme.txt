Directory lexer
----------------

This directory contains the lexical analyzer for the compiler.
The files are as follows.

lexer.lex	This file contains a flex lexer.  This lexer reads
		lexemes from the current input file and returns
		tokens.  This lexer is the core of the lexer, but
		is insulated from the raw input by functions provided
		in preflex.c, and from the parser by function yylexx,
		also in preflex.c.

preflex.c	This filne contains support routines for the lexer.  
		These functions are concerned with raw input, including
		pushing and popping of files to read at imports, and 
		with handling pushbacks of tokens.  Function yylexx, 
		which is the lexer that the parser calls, is defined here.

lexvars.c	This file contains the global variables used by the
		lexer.

lexsup.c	This file contains assorted support routines for the
		lexer, as well as data about the tokens.  It should
		be consulted when modifying the lexer.

lexids.c	This file contains functions for handling identifiers
		in the lexer.

lexerr.c	This file contains functions for handling and detecting
		errors in the lexer.  This file also maintains a stack
		that is a shadow of the parser's stack, so that it can
		know which constructs are currently being parsed, and
		what end-words or right brackets are expected.

lexer.h		This file contains prototypes and some types for the
		lexer.

attr.h		This file defines modes for declarations as bit
		fields.
