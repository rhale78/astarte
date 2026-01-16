SHELL = /bin/csh
ASTC = astc
.SUFFIXES: .ast .aso .asd

.ast.aso:
	$(ASTC) -l -1 $*

.ast.asd:
	$(ASTC) -l -1 $*


all:\
   object.aso\
   standard.aso\
   collectt\
   langg\
   logicc\
   mathh\
   miscc\
   patternn\
   systemm\
   threadd

collectt:
	cd collect; make ASTC=astc all

patternn:
	cd pattern; make ASTC=astc all

logicc:
	cd logic; make ASTC=astc all

miscc:
	cd misc; make ASTC=astc all
   
mathh:
	cd math; make ASTC=astc all

systemm:
	cd system; make ASTC=astc all

threadd:
	cd thread; make ASTC=astc all

langg:
	cd lang; make ASTC=astc all

clean: 
	-find . \( -name \*.aso -o -name \*.asd -o -name \*.asx \) -print -exec mv -f \{\} ~/.remove \;

standard.aso: standard.asi standard.ast
object.asi: object.asi object.ast

