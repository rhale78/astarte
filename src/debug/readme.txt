Directory debug
---------------

This directory contains functions for debugging the compiler
and the interpreter.

debug.c		Functions that are used for both the interpreter
		and the compiler.

m_debug.c	Functions that are only used by the interpreter.

t_debug.c	Functions that are only used by the compiler.

dprintty.c	Functions for printing types in long(debug) form.
		Used by both compiler and interpreter.

dprtexpr.c	Functions for printing expressions in long form.
		Used only by the compiler.

dprtent.c	Functions for printing entities in long form.	
		Used only by the interpreter.
