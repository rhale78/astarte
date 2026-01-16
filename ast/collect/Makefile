SHELL = /bin/csh
ASTC = astc
.SUFFIXES: .ast .aso .asd

.ast.aso:
	$(ASTC) -l -1 $*

.ast.asd:
	$(ASTC) -l -1 $*

all:\
     bitvec.aso\
     boxes.aso\
     collect.aso\
     denseset.aso\
     findlist.aso\
     funarr.aso\
     genarray.aso\
     linset.aso\
     lintbl.aso\
     list.asd\
     listfun.aso\
     matrix.aso\
     pair.aso\
     search.aso\
     set.aso\
     sort.aso\
     sparsset.aso\
     sparstbl.aso\
     srchtree.aso\
     string.aso\
     table.aso

bitvec.aso: bitvec.asi bitvec.ast
boxes.aso: boxes.asi boxes.ast
collect.aso: collect.ast collect.asi
denseset.aso: denseset.asi denseset.ast
findlist.aso: findlist.asi findlist.ast
funarr.aso: funarr.ast funarr.asi
linset.aso: linset.asi linset.ast
lintbl.aso: lintbl.asi lintbl.ast
listfun.aso: listfun.asi listfun.ast
listpat.aso: listpat.asi listpat.ast
matrix.aso: matrix.asi matrix.ast
search.aso: search.asi search.ast
set.aso: set.asi set.ast
sort.aso: sort.ast sort.asi
sparsset.aso: sparsset.asi sparsset.ast
sparstbl.aso: sparstbl.asi sparstbl.ast
srchtree.aso: srchtree.asi srchtree.ast
string.aso: string.asi string.ast
table.aso: table.asi table.ast

