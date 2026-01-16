SHELL = /bin/csh
ASTC = astc
.SUFFIXES: .ast .aso .asd

.ast.aso:
	$(ASTC) -l -1 $*

.ast.asd:
	$(ASTC) -l -1 $*


all:\
   logic.aso\
   symbol.aso\
   unify.aso\
   unknown.asd\
   utils.aso

utils.aso: utils.asi utils.ast
unify.aso: unify.asi unify.ast
