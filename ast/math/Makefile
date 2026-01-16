SHELL = /bin/csh
.SUFFIXES: .ast .aso .asd

.ast.aso:
	astc -l -1 $*

.ast.asd:
	astc -l -1 $*
all:\
     complex.aso\
     math.aso\
     modular.aso\
     prime.aso\
     stdnum.asd

complex.aso: complex.asi complex.ast
number.aso: number.asi number.ast
modular.aso: modular.asi modular.ast
math.aso: math.ast math.asi
prime.aso: prime.ast prime.asi

