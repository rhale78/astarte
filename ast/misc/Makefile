SHELL = /bin/csh
ASTC = astc
.SUFFIXES: .ast .aso .asd

.ast.aso:
	$(ASTC) -l -1 $*

.ast.asd:
	$(ASTC) -l -1 $*


all:\
     random.aso\
     trace.aso\
     check.aso

random.aso: random.asi random.ast
