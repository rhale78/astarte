SHELL = /bin/csh
.SUFFIXES: .ast .aso .asd

.ast.aso:
	$(ASTC) -l -1 $*

.ast.asd:
	$(ASTC) -l -1 $*


all:\
   io.asd\
   system.aso

system.aso: system.asi system.ast
