Directory rts
---------------

This directory contains assorted library functions.

File			Contents

bitvec.c		Functions for bit vectors.

bigint.c		Functions for handling large integers,
			including the basic addition, subtraction,
			multiplication and division functions.

compare.c		Functions for comparing primitive things,
			such as numbers and strings.

integer.c		A few operations on integers.  These operations
			manage small integers.  Large integers are
			in bigint.c.  Most of the small integer
			arithmetic is actually inlined in number.c.

io.c			Functions for managing input and output files.

number.c		Numeric functions.  These functions form a
			top level to bigint.c, rational.c and real.c, and are
			the ones called by the interpreter.

numconv.c		Functions for converting among numeric types.

product.c		Functions for handling ordered pairs and
			lists.  For example, head, tail, length and
			subscript operations are here.

prtnum.c		Functions for converting numbers to strings.

rational.c		Functions for computing basic operations on
			rational numbers.

real.c			Functions for computing basic operations on
			real numbers, as well as additional functions
			such as sqrt and sin.

system.c		Functions for accessing system functions such
			as getenv and mkdir.  Also, management of the
			'system' function is here.

tstrutil.c		Functions for managing concatenable strings.
			These functions are used in prtnum.c.
