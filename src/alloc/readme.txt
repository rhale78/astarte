Directory alloc
----------------

This directory contains functions for memory management.  Files
are as follows.

allocate.c     Allocate blocks of memory.  This implementation avoids
               using system allocation facilities such as malloc on a small
               scale.  Rather, large blocks are allocated, and parcelled
               out as needed.  That work is done in allocate.c.
               Before allocating any memory, init_alloc must be called.
               See allocate.c.

               There are two options for allocate.c, set in misc/options.h.
               Option USE_SBRK indicates that the sbrk system call should
               be used to get more memory.  Option USE_MALLOC indicates that
               the malloc system call should be used to get more memory.
               Exactly one of those preprocessor variables should
               be set.

allocors.c     This file contains specific allocation and deallocation
               functions for:

                  strings
                  hash tables
                  class-union cells
                  class-table cells
                  file information frames for compiler
                  table cells (for compiler)
                  report records (for compiler)
                  deferred declarations (for compiler)
		  roles (for compiler)
                  global id table (interpreter)
                  packages (interpreter)

               This file contains flags to select functions
               for the compiler or for the interpreter.


Note: The allocators that work with referece counts do not deallocate
when option GCTEST has been selected.  They just set to reference
count to -100, so that bad reference count management will show
up.  See misc/options.h for GCTEST.

alocexpr.c     Allocators for type EXPR.  These use reference counts.
               Used only by compiler.

aloclist.c     Allocators for type LIST.  These use reference counts.

aloctype.c     Allocators for type TYPE.  These use reference counts.

mrcalloc.c     Allocators for interpreter structures that are managed
               by reference counts.  These are
                  states
                  trap vectors
                  controls
                  continuations
                  activations
                  environments
               It also handles emptying out the type stack and type
               storage used by evaluate/typeinstrs.c.
               Used only by interpreter.

gcalloc.c      Allocators and deallocators for garbage collected
               memory.  Interacts with gc/gc.c.  Kinds of things allocated:
                  large reals  (high precision)
                  small reals  (doubles)
                  entities
                  strings
                  integers
                  file records
               Used only by interpreter.

tempstr.c      Allocators and deallocators for temporary string buffers.
	       These buffers are used by some functions that build
	       strings.


