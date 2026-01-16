SHELL = /bin/csh
ASTC = astc
.SUFFIXES: .ast .aso .asd

.ast.aso:
	$(ASTC) -l -1 $*

.ast.asd:
	$(ASTC) -l -1 $*

all:\
     listpat.aso\
     pattern.aso\
     stringp.aso

listpat.aso: listpat.asi listpat.ast
pattern.aso: pattern.asi pattern.ast
stringp.aso: stringp.asi stringp.ast
