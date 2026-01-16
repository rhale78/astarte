SHELL = /bin/csh
ASTC = astc
.SUFFIXES: .ast .aso .asd

.ast.aso:
	$(ASTC) -l -1 $*

.ast.asd:
	$(ASTC) -l -1 $*


all:\
   boolean.asd\
   box.asd\
   bracket.aso\
   char.aso\
   choose.asd\
   compare.asd\
   except.asd\
   expect.asd\
   function.asd\
   hermit.asd\
   hierarch.aso\
   import.asd\
   irreg.asd\
   lazy.asd\
   let.asd\
   loop.asd\
   misc.asd\
   operator.asd\
   optional.asd\
   pattern.asd\
   rank.asd\
   stream.asd

char.aso: char.ast char.asi
