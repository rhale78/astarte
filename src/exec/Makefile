################################################################
#
# FILE: exec/Makefile
#
# File to make astarte translator and evaluator
################################################################

################################################################
# See src/misc/options.h and src/misc/config.h for 
# compilation options.
################################################################

SHELL = /bin/csh
RM    = /bin/rm
MV    = /bin/mv -f
TOUCH = /bin/touch
CC    = gcc

################################################################
# TOPDIR is the main source directory.  
# This makefile is in TOPDIR/exec.
################################################################

TOPDIR = /usr/local/lib/astarte/src

################################################################
# BINDIR is the directory where the binaries should be placed.
################################################################

BINDIR = /usr/local/bin

###############################################################
# CFLAGS and LDFLAGS options
###############################################################

CFLAGS		= -O
LDFLAGS		= 

####################################################################
#     YOU SHOULD NOT HAVE TO EDIT BELOW THIS LINE
####################################################################

TR	       = -DTRANSLATOR
MA	       = -DMACHINE
VI	       = -DVISUALIZER

####################################################################
# Files for translator (compiler)
####################################################################

TRANS_OFILES =\
  parser.o\
  compiler.o\
  asd.o\
  lexer.o\
  lexvars.o\
  preflex.o\
  lexerr.o\
  lexsup.o\
  lexids.o\
  modes.o\
  parseerr.o\
  parsutil.o\
  parsvars.o\
  ids.o\
  lateids.o\
  open.o\
  allocate.o\
  aloclist.o\
  alocexpr.o\
  t_alocty.o\
  t_alloc.o\
  copytype.o\
  t_prtty.o\
  newrole.o\
  t_type.o\
  typeutil.o\
  t_tyutil.o\
  bring.o\
  debug.o\
  dprtexpr.o\
  dprintty.o\
  t_debug.o\
  prtgic.o\
  copyexpr.o\
  expr.o\
  exprutil.o\
  brngexpr.o\
  choose.o\
  context.o\
  prtexpr.o\
  pretty.o\
  hash.o\
  hash1.o\
  t_hash2.o\
  t_lists.o\
  t_listut.o\
  strops.o\
  t_strops.o\
  t_filename.o\
  t_rdwrt.o\
  stdfuns.o\
  stdtypes.o\
  stdids.o\
  infer.o\
  defer.o\
  defaults.o\
  unify.o\
  overlap.o\
  t_constraint.o\
  t_unify.o\
  generate.o\
  instinfo.o\
  improve.o\
  genexec.o\
  genwrap.o\
  prim.o\
  selmod.o\
  gendcl.o\
  genglob.o\
  genstd.o\
  gentype.o\
  translt.o\
  substit.o\
  mode.o\
  patutils.o\
  pmcompl.o\
  t_clstbl.o\
  classinfo.o\
  stdtytbl.o\
  vartbl.o\
  dflttbl.o\
  cnstrlst.o\
  abbrev.o\
  t_mettbl.o\
  loctbl.o\
  globtbl.o\
  secimp.o\
  idtbl.o\
  missing.o\
  contrib.o\
  fileinfo.o\
  strtbl.o\
  tableman.o\
  descrip.o\
  visible.o\
  assume.o\
  primtbl.o\
  error.o\
  complete.o\
  t_compl.o\
  dcl.o\
  dclutil.o\
  dclclass.o\
  dclcnstr.o\
  defdcls.o\
  deferdcl.o\
  report.o\
  somedcls.o\
  expnddcl.o\
  t_except.o\
  t_misc.o\
  find.o\
  t_stubs.o

TRANS_CFILES =\
  parser.c\
  lexer.c\
  ../lexer/lexvars.c\
  ../lexer/preflex.c\
  ../lexer/lexerr.c\
  ../lexer/lexsup.c\
  ../lexer/lexids.c\
  ../lexer/modes.c\
  ../parser/compiler.c\
  ../parser/asd.c\
  ../parser/parseerr.c\
  ../parser/parsutil.c\
  ../parser/parsvars.c\
  ../ids/ids.c\
  ../ids/lateids.c\
  ../ids/open.c\
  ../ids/context.c\
  ../alloc/allocate.c\
  ../alloc/aloclist.c\
  ../alloc/alocexpr.c\
  ../alloc/aloctype.c\
  ../alloc/allocors.c\
  ../classes/copytype.c\
  ../classes/printty.c\
  ../classes/type.c\
  ../classes/typeutil.c\
  ../classes/t_tyutil.c\
  ../classes/bring.c\
  ../debug/debug.c\
  ../debug/dprtent.c\
  ../debug/dprtexpr.c\
  ../debug/dprintty.c\
  ../debug/t_debug.c\
  ../debug/prtgic.c\
  ../exprs/copyexpr.c\
  ../exprs/expr.c\
  ../exprs/exprutil.c\
  ../exprs/brngexpr.c\
  ../exprs/choose.c\
  ../exprs/prtexpr.c\
  ../exprs/pretty.c\
  ../utils/hash.c\
  ../utils/hash1.c\
  ../utils/hash2.c\
  ../utils/lists.c\
  ../utils/listutil.c\
  ../utils/strops.c\
  ../utils/t_strops.c\
  ../utils/filename.c\
  ../utils/rdwrt.c\
  ../standard/stdfuns.c\
  ../standard/stdtypes.c\
  ../standard/stdids.c\
  ../infer/infer.c\
  ../infer/defer.c\
  ../infer/defaults.c\
  ../infer/t_unify.c\
  ../unify/unify.c\
  ../unify/overlap.c\
  ../unify/constraint.c\
  ../unify/complete.c\
  ../evaluate/instinfo.c\
  ../generate/generate.c\
  ../generate/improve.c\
  ../generate/genexec.c\
  ../generate/genwrap.c\
  ../generate/prim.c\
  ../generate/selmod.c\
  ../generate/gendcl.c\
  ../generate/genglob.c\
  ../generate/genstd.c\
  ../generate/gentype.c\
  ../patmatch/translt.c\
  ../patmatch/substit.c\
  ../patmatch/mode.c\
  ../patmatch/patutils.c\
  ../patmatch/pmcompl.c\
  ../clstbl/classtbl.c\
  ../clstbl/classinfo.c\
  ../clstbl/stdtytbl.c\
  ../clstbl/vartbl.c\
  ../clstbl/dflttbl.c\
  ../clstbl/cnstrlst.c\
  ../clstbl/abbrev.c\
  ../clstbl/meettbl.c\
  ../tables/t_compl.c\
  ../tables/loctbl.c\
  ../tables/globtbl.c\
  ../tables/secimp.c\
  ../tables/idtbl.c\
  ../tables/missing.c\
  ../tables/contrib.c\
  ../tables/fileinfo.c\
  ../tables/strtbl.c\
  ../tables/tableman.c\
  ../tables/descrip.c\
  ../tables/visible.c\
  ../tables/assume.c\
  ../tables/primtbl.c\
  ../error/error.c\
  ../error/newrole.c\
  ../misc/t_stubs.c\
  ../misc/misc.c\
  ../misc/find.c\
  ../dcls/dcl.c\
  ../dcls/dclutil.c\
  ../dcls/dclcnstr.c\
  ../dcls/dclclass.c\
  ../dcls/defdcls.c\
  ../dcls/deferdcl.c\
  ../dcls/report.c\
  ../dcls/expnddcl.c\
  ../dcls/somedcls.c\
  ../machdata/except.c

####################################################################
# Files for abstract machine (interpreter)
####################################################################

MACH_OFILES =\
  allocate.o\
  aloclist.o\
  m_alocty.o\
  m_alloc.o\
  gcalloc.o\
  mrcalloc.o\
  tempstr.o\
  copytype.o\
  m_prtty.o\
  m_type.o\
  typeutil.o\
  m_tyutil.o\
  debug.o\
  dprtent.o\
  dprintty.o\
  m_debug.o\
  prtent.o\
  printrts.o\
  profile.o\
  getinfo.o\
  gprint.o\
  hash.o\
  hash1.o\
  m_hash2.o\
  m_lists.o\
  m_listut.o\
  strops.o\
  m_strops.o\
  m_filename.o\
  miscutil.o\
  m_rdwrt.o\
  stdtypes.o\
  stdids.o\
  m_clstbl.o\
  stdtytbl.o\
  m_dflt.o\
  consts.o\
  m_mettbl.o\
  m_glob.o\
  typehash.o\
  gc.o\
  gcstate.o\
  entity.o\
  m_except.o\
  wrap.o\
  control.o\
  environ.o\
  stack.o\
  state.o\
  actvn.o\
  evaluate.o\
  instinfo.o\
  evalsup.o\
  eval.o\
  apply.o\
  coroutin.o\
  fail.o\
  lazy.o\
  lazyprim.o\
  typeinst.o\
  package.o\
  intrprtr.o\
  intvars.o\
  unify.o\
  m_constraint.o\
  bigint.o\
  bitvec.o\
  compare.o\
  integer.o\
  number.o\
  numconv.o\
  prtnum.o\
  tstrutil.o\
  tostring.o\
  product.o\
  append.o\
  array.o\
  rational.o\
  real.o\
  io.o\
  system.o\
  m_error.o\
  m_misc.o\
  m_stubs.o

MACH_CFILES =\
  ../alloc/allocate.c\
  ../alloc/aloclist.c\
  ../alloc/aloctype.c\
  ../alloc/allocors.c\
  ../alloc/gcalloc.c\
  ../alloc/mrcalloc.c\
  ../alloc/tempstr.c\
  ../classes/copytype.c\
  ../classes/printty.c\
  ../classes/type.c\
  ../classes/typeutil.c\
  ../classes/m_tyutil.c\
  ../debug/debug.c\
  ../debug/dprtent.c\
  ../debug/dprintty.c\
  ../debug/m_debug.c\
  ../show/prtent.c\
  ../show/printrts.c\
  ../show/profile.c\
  ../show/getinfo.c\
  ../show/gprint.c\
  ../utils/hash.c\
  ../utils/hash1.c\
  ../utils/hash2.c\
  ../utils/lists.c\
  ../utils/listutil.c\
  ../utils/strops.c\
  ../utils/m_strops.c\
  ../utils/filename.c\
  ../utils/miscutil.c\
  ../utils/rdwrt.c\
  ../standard/stdtypes.c\
  ../standard/stdids.c\
  ../clstbl/classtbl.c\
  ../clstbl/stdtytbl.c\
  ../clstbl/m_dflt.c\
  ../clstbl/meettbl.c\
  ../clstbl/typehash.c\
  ../tables/consts.c\
  ../tables/m_glob.c\
  ../gc/gc.c\
  ../gc/gcstate.c\
  ../machdata/entity.c\
  ../machdata/except.c\
  ../machdata/wrap.c\
  ../machstrc/control.c\
  ../machstrc/environ.c\
  ../machstrc/stack.c\
  ../machstrc/state.c\
  ../machstrc/actvn.c\
  ../evaluate/evaluate.c\
  ../evaluate/instinfo.c\
  ../evaluate/evalsup.c\
  ../evaluate/eval.c\
  ../evaluate/apply.c\
  ../evaluate/coroutin.c\
  ../evaluate/fail.c\
  ../evaluate/lazy.c\
  ../evaluate/lazyprim.c\
  ../evaluate/typeinst.c\
  ../intrprtr/package.c\
  ../intrprtr/intrprtr.c\
  ../intrprtr/intvars.c\
  ../unify/unify.c\
  ../unify/constraint.c\
  ../rts/bigint.c\
  ../rts/bitvec.c\
  ../rts/compare.c\
  ../rts/integer.c\
  ../rts/number.c\
  ../rts/numconv.c\
  ../rts/prtnum.c\
  ../rts/tstrutil.c\
  ../rts/tostring.c\
  ../rts/product.c\
  ../rts/append.c\
  ../rts/array.c\
  ../rts/rational.c\
  ../rts/real.c\
  ../rts/io.c\
  ../rts/system.c\
  ../error/m_error.c\
  ../misc/misc.c\
  ../misc/m_stubs.c
  
####################################################################
# Main targets
####################################################################

install: astc astr disass asthelp
	$(MV) astc $(BINDIR)
	$(MV) astr $(BINDIR)
	$(MV) disass $(BINDIR)
	$(MV) asthelp $(BINDIR)

all: astc astr disass asthelp;

clean:
	-/bin/rm *.o stamp-*

astc: stamp-astc;

stamp-astc: $(TRANS_OFILES)
	$(CC) -o astc $(LDFLAGS) $(TRANS_OFILES) -lc
	$(TOUCH) stamp-astc

astr: stamp-astr;

stamp-astr: $(MACH_OFILES)
	$(CC) -o astr $(LDFLAGS) $(MACH_OFILES) -ltermcap -lm -lc
	$(TOUCH) stamp-astr

disass: stamp-disass

stamp-disass: ../disassemble.c ../evaluate/instinfo.c ../evaluate/instruc.h
	$(CC) -o disass ../disassemble.c
	$(TOUCH) stamp-disass

asthelp: stamp-asthelp

stamp-asthelp: ../misc/find.c ../misc/options.h
	$(CC) -o asthelp ../misc/find.c ../misc/asthelp.c
	$(TOUCH) stamp-asthelp

################################################################## alloc

allocate.o: ../alloc/allocate.c
	$(CC) -c $(CFLAGS) ../alloc/allocate.c
aloclist.o: ../alloc/aloclist.c
	$(CC) -c $(CFLAGS) ../alloc/aloclist.c
alocexpr.o: ../alloc/alocexpr.c
	$(CC) -c $(CFLAGS) $(TR) ../alloc/alocexpr.c
m_alocty.o: ../alloc/aloctype.c
	$(CC) -c -o m_alocty.o $(CFLAGS) $(MA) ../alloc/aloctype.c
t_alocty.o: ../alloc/aloctype.c
	$(CC) -c -o t_alocty.o $(CFLAGS) $(TR) ../alloc/aloctype.c
t_alloc.o: ../alloc/allocors.c
	$(CC) -c -o t_alloc.o $(CFLAGS) $(TR) ../alloc/allocors.c
m_alloc.o: ../alloc/allocors.c
	$(CC) -c -o m_alloc.o $(CFLAGS) $(MA) ../alloc/allocors.c
gcalloc.o: ../alloc/gcalloc.c
	$(CC) -c $(CFLAGS) $(MA) ../alloc/gcalloc.c
mrcalloc.o: ../alloc/mrcalloc.c
	$(CC) -c $(CFLAGS) $(MA) ../alloc/mrcalloc.c
tempstr.o: ../alloc/tempstr.c
	$(CC) -c $(CFLAGS) $(MA) ../alloc/tempstr.c


################################################################## classes

copytype.o: ../classes/copytype.c
	$(CC) -c $(CFLAGS) ../classes/copytype.c
t_prtty.o: ../classes/printty.c
	$(CC) -c -o t_prtty.o $(CFLAGS) $(TR) ../classes/printty.c
m_prtty.o: ../classes/printty.c
	$(CC) -c -o m_prtty.o $(CFLAGS) $(MA) ../classes/printty.c
t_type.o: ../classes/type.c
	$(CC) -c -o t_type.o $(CFLAGS) $(TR) ../classes/type.c
m_type.o: ../classes/type.c
	$(CC) -c -o m_type.o $(CFLAGS) $(MA) ../classes/type.c
typeutil.o: ../classes/typeutil.c
	$(CC) -c $(CFLAGS) ../classes/typeutil.c
t_tyutil.o: ../classes/t_tyutil.c
	$(CC) -c $(CFLAGS) $(TR) ../classes/t_tyutil.c
m_tyutil.o: ../classes/m_tyutil.c
	$(CC) -c $(CFLAGS) $(MA) ../classes/m_tyutil.c
bring.o: ../classes/bring.c
	$(CC) -c $(CFLAGS) $(TR) ../classes/bring.c

################################################################## debug

debug.o: ../debug/debug.c
	$(CC) -c $(CFLAGS) ../debug/debug.c
m_debug.o: ../debug/m_debug.c
	$(CC) -c $(CFLAGS) $(MA) ../debug/m_debug.c
t_debug.o: ../debug/t_debug.c
	$(CC) -c $(CFLAGS) $(TR) ../debug/t_debug.c
dprtent.o: ../debug/dprtent.c
	$(CC) -c $(CFLAGS) $(MA) ../debug/dprtent.c
dprintty.o: ../debug/dprintty.c
	$(CC) -c $(CFLAGS) ../debug/dprintty.c
dprtexpr.o: ../debug/dprtexpr.c
	$(CC) -c $(CFLAGS) $(TR) ../debug/dprtexpr.c
prtgic.o: ../debug/prtgic.c
	$(CC) -c $(CFLAGS) $(TR) ../debug/prtgic.c

################################################################## show

prtent.o: ../show/prtent.c
	$(CC) -c $(CFLAGS) $(MA) ../show/prtent.c
printrts.o: ../show/printrts.c
	$(CC) -c $(CFLAGS) $(MA) ../show/printrts.c
profile.o: ../show/profile.c
	$(CC) -c $(CFLAGS) $(MA) ../show/profile.c
getinfo.o: ../show/getinfo.c
	$(CC) -c $(CFLAGS) $(MA) ../show/getinfo.c
gprint.o: ../show/gprint.c
	$(CC) -c $(CFLAGS) $(MA) ../show/gprint.c

################################################################## exprs


expr.o: ../exprs/expr.c
	$(CC) -c $(CFLAGS) $(TR) ../exprs/expr.c
copyexpr.o: ../exprs/copyexpr.c
	$(CC) -c $(CFLAGS) $(TR) ../exprs/copyexpr.c
exprutil.o: ../exprs/exprutil.c
	$(CC) -c $(CFLAGS) $(TR) ../exprs/exprutil.c
brngexpr.o: ../exprs/brngexpr.c
	$(CC) -c $(CFLAGS) $(TR) ../exprs/brngexpr.c
context.o: ../exprs/context.c
	$(CC) -c $(CFLAGS) $(TR) ../exprs/context.c
choose.o: ../exprs/choose.c
	$(CC) -c $(CFLAGS) $(TR) ../exprs/choose.c
prtexpr.o: ../exprs/prtexpr.c
	$(CC) -c $(CFLAGS) $(TR) ../exprs/prtexpr.c
pretty.o: ../exprs/pretty.c
	$(CC) -c $(CFLAGS) $(TR) ../exprs/pretty.c

################################################################## misc

t_misc.o: ../misc/misc.c
	$(CC) -c -o t_misc.o $(CFLAGS) $(TR) ../misc/misc.c
m_misc.o: ../misc/misc.c
	$(CC) -c -o m_misc.o $(CFLAGS) $(MA) ../misc/misc.c
find.o: ../misc/find.c
	$(CC) -c $(CFLAGS) ../misc/find.c

################################################################## utils

hash.o: ../utils/hash.c
	$(CC) -c $(CFLAGS) ../utils/hash.c
hash1.o: ../utils/hash1.c
	$(CC) -c $(CFLAGS) ../utils/hash1.c
m_hash2.o: ../utils/hash2.c
	$(CC) -c -o m_hash2.o $(CFLAGS) $(MA) ../utils/hash2.c
t_hash2.o: ../utils/hash2.c
	$(CC) -c -o t_hash2.o $(CFLAGS) $(TR) ../utils/hash2.c
m_lists.o: ../utils/lists.c
	$(CC) -c -o m_lists.o $(CFLAGS) $(MA) ../utils/lists.c
t_lists.o: ../utils/lists.c
	$(CC) -c -o t_lists.o $(CFLAGS) $(TR) ../utils/lists.c
t_listut.o: ../utils/listutil.c
	$(CC) -c -o t_listut.o $(CFLAGS) $(TR) ../utils/listutil.c
m_listut.o: ../utils/listutil.c
	$(CC) -c -o m_listut.o $(CFLAGS) $(MA) ../utils/listutil.c
strops.o: ../utils/strops.c
	$(CC) -c $(CFLAGS) ../utils/strops.c
m_strops.o: ../utils/m_strops.c
	$(CC) -c $(CFLAGS) $(MA) ../utils/m_strops.c
t_strops.o: ../utils/t_strops.c
	$(CC) -c $(CFLAGS) $(TR) ../utils/t_strops.c
t_filename.o: ../utils/filename.c
	$(CC) -c -o t_filename.o $(CFLAGS) $(TR) ../utils/filename.c
m_filename.o: ../utils/filename.c
	$(CC) -c -o m_filename.o $(CFLAGS) $(MA) ../utils/filename.c
t_rdwrt.o: ../utils/rdwrt.c
	$(CC) -c -o t_rdwrt.o $(CFLAGS) $(TR) ../utils/rdwrt.c
m_rdwrt.o: ../utils/rdwrt.c
	$(CC) -c -o m_rdwrt.o $(CFLAGS) $(MA) ../utils/rdwrt.c
miscutil.o: ../utils/miscutil.c
	$(CC) -c $(CFLAGS) ../utils/miscutil.c


################################################################## standard

stdfuns.o: ../standard/stdfuns.c
	$(CC) -c $(CFLAGS) $(TR) ../standard/stdfuns.c
stdtypes.o: ../standard/stdtypes.c
	$(CC) -c $(CFLAGS) ../standard/stdtypes.c
stdids.o: ../standard/stdids.c
	$(CC) -c $(CFLAGS) ../standard/stdids.c

################################################################## lexer

lexer.c: ../lexer/lexer.lex ../exec/lexerpre.c
	flex ../lexer/lexer.lex
	-/bin/rm lexer.c
	sed -f lexer.scr lex.yy.c >lexer1.c
	/bin/rm lex.yy.c
	cat lexerpre.c lexer1.c >lexer.c
	/bin/rm lexer1.c

lexer.o: lexer.c
	$(CC) -c $(CFLAGS) $(TR) lexer.c
lexvars.o: ../lexer/lexvars.c
	$(CC) -c $(CFLAGS) $(TR) ../lexer/lexvars.c
preflex.o: ../lexer/preflex.c
	$(CC) -c $(CFLAGS) $(TR) ../lexer/preflex.c
lexerr.o: ../lexer/lexerr.c
	$(CC) -c $(CFLAGS) $(TR) ../lexer/lexerr.c
lexsup.o: ../lexer/lexsup.c
	$(CC) -c $(CFLAGS) $(TR) ../lexer/lexsup.c
lexids.o: ../lexer/lexids.c
	$(CC) -c $(CFLAGS) $(TR) ../lexer/lexids.c
modes.o: ../lexer/modes.c
	$(CC) -c $(CFLAGS) $(TR) ../lexer/modes.c

################################################################## parser

parser.c: ../parser/parser.y ../parser/tokmod.h
	bison -y -v -d ../parser/parser.y
	-/bin/rm parser.c
	sed -f parser.scr y.tab.c >parser.c
	/bin/rm y.tab.c
	cat y.tab.h ../parser/tokmod.h >! ../parser/tokens.h
	/bin/rm y.tab.h

parser.o: parser.c
	$(CC) -c $(CFLAGS) $(TR) parser.c
compiler.o: ../parser/compiler.c
	$(CC) -c $(CFLAGS) $(TR) ../parser/compiler.c
asd.o: ../parser/asd.c
	$(CC) -c $(CFLAGS) $(TR) ../parser/asd.c
parseerr.o: ../parser/parseerr.c
	$(CC) -c $(CFLAGS) $(TR) ../parser/parseerr.c
parsutil.o: ../parser/parsutil.c
	$(CC) -c $(CFLAGS) $(TR) ../parser/parsutil.c
parsvars.o: ../parser/parsvars.c
	$(CC) -c $(CFLAGS) $(TR) ../parser/parsvars.c


################################################################## ids

ids.o: ../ids/ids.c
	$(CC) -c $(CFLAGS) $(TR) ../ids/ids.c
lateids.o: ../ids/lateids.c
	$(CC) -c $(CFLAGS) $(TR) ../ids/lateids.c
open.o: ../ids/open.c
	$(CC) -c $(CFLAGS) $(TR) ../ids/open.c

################################################################## generate

genexec.o: ../generate/genexec.c
	$(CC) -c $(CFLAGS) $(TR) ../generate/genexec.c
genwrap.o: ../generate/genwrap.c
	$(CC) -c $(CFLAGS) $(TR) ../generate/genwrap.c
prim.o: ../generate/prim.c
	$(CC) -c $(CFLAGS) $(TR) ../generate/prim.c
selmod.o: ../generate/selmod.c
	$(CC) -c $(CFLAGS) $(TR) ../generate/selmod.c
gendcl.o: ../generate/gendcl.c
	$(CC) -c $(CFLAGS) $(TR) ../generate/gendcl.c
generate.o: ../generate/generate.c
	$(CC) -c $(CFLAGS) $(TR) ../generate/generate.c
improve.o: ../generate/improve.c
	$(CC) -c $(CFLAGS) $(TR) ../generate/improve.c
genglob.o: ../generate/genglob.c
	$(CC) -c $(CFLAGS) $(TR) ../generate/genglob.c
genstd.o: ../generate/genstd.c
	$(CC) -c $(CFLAGS) $(TR) ../generate/genstd.c
gentype.o: ../generate/gentype.c
	$(CC) -c $(CFLAGS) $(TR) ../generate/gentype.c

################################################################## patmatch

translt.o: ../patmatch/translt.c
	$(CC) -c $(CFLAGS) $(TR) ../patmatch/translt.c
substit.o: ../patmatch/substit.c
	$(CC) -c $(CFLAGS) $(TR) ../patmatch/substit.c
mode.o: ../patmatch/mode.c
	$(CC) -c $(CFLAGS) $(TR) ../patmatch/mode.c
patutils.o: ../patmatch/patutils.c
	$(CC) -c $(CFLAGS) $(TR) ../patmatch/patutils.c
pmcompl.o: ../patmatch/pmcompl.c
	$(CC) -c $(CFLAGS) $(TR) ../patmatch/pmcompl.c


################################################################## unify

unify.o: ../unify/unify.c
	$(CC) -c $(CFLAGS) ../unify/unify.c
overlap.o: ../unify/overlap.c
	$(CC) -c $(CFLAGS) $(TR) ../unify/overlap.c
m_constraint.o: ../unify/constraint.c
	$(CC) -c -o m_constraint.o $(CFLAGS) $(MA) ../unify/constraint.c
t_constraint.o: ../unify/constraint.c
	$(CC) -c -o t_constraint.o $(CFLAGS) $(TR) ../unify/constraint.c
complete.o: ../unify/complete.c
	$(CC) -c $(CFLAGS) $(TR) ../unify/complete.c

################################################################## infer

infer.o: ../infer/infer.c
	$(CC) -c $(CFLAGS) $(TR) ../infer/infer.c
defer.o: ../infer/defer.c
	$(CC) -c $(CFLAGS) $(TR) ../infer/defer.c
defaults.o: ../infer/defaults.c
	$(CC) -c $(CFLAGS) $(TR) ../infer/defaults.c
t_unify.o: ../infer/t_unify.c
	$(CC) -c $(CFLAGS) $(TR) ../infer/t_unify.c

################################################################## clstbl

t_clstbl.o: ../clstbl/classtbl.c
	$(CC) -c -o t_clstbl.o $(CFLAGS) $(TR) ../clstbl/classtbl.c
m_clstbl.o: ../clstbl/classtbl.c
	$(CC) -c -o m_clstbl.o $(CFLAGS) $(MA) ../clstbl/classtbl.c
classinfo.o: ../clstbl/classinfo.c
	$(CC) -c $(CFLAGS) $(TR) ../clstbl/classinfo.c
vartbl.o: ../clstbl/vartbl.c
	$(CC) -c $(CFLAGS) $(TR) ../clstbl/vartbl.c
cnstrlst.o: ../clstbl/cnstrlst.c
	$(CC) -c $(CFLAGS) $(TR) ../clstbl/cnstrlst.c
dflttbl.o: ../clstbl/dflttbl.c
	$(CC) -c $(CFLAGS) $(TR) ../clstbl/dflttbl.c
m_dflt.o: ../clstbl/m_dflt.c
	$(CC) -c $(CFLAGS) $(MA) ../clstbl/m_dflt.c
stdtytbl.o: ../clstbl/stdtytbl.c
	$(CC) -c $(CFLAGS) ../clstbl/stdtytbl.c
abbrev.o: ../clstbl/abbrev.c
	$(CC) -c $(CFLAGS) $(TR) ../clstbl/abbrev.c
t_mettbl.o: ../clstbl/meettbl.c
	$(CC) -c -o t_mettbl.o $(CFLAGS) $(TR) ../clstbl/meettbl.c
m_mettbl.o: ../clstbl/meettbl.c
	$(CC) -c -o m_mettbl.o $(CFLAGS) $(MA) ../clstbl/meettbl.c
typehash.o: ../clstbl/typehash.c
	$(CC) -c $(CFLAGS) $(MA) ../clstbl/typehash.c

################################################################## tables

consts.o: ../tables/consts.c
	$(CC) -c $(CFLAGS) $(MA) ../tables/consts.c
loctbl.o: ../tables/loctbl.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/loctbl.c
globtbl.o: ../tables/globtbl.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/globtbl.c
secimp.o: ../tables/secimp.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/secimp.c
idtbl.o: ../tables/idtbl.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/idtbl.c
missing.o: ../tables/missing.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/missing.c
contrib.o: ../tables/contrib.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/contrib.c
fileinfo.o: ../tables/fileinfo.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/fileinfo.c
m_glob.o: ../tables/m_glob.c
	$(CC) -c $(CFLAGS) $(MA) ../tables/m_glob.c
strtbl.o: ../tables/strtbl.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/strtbl.c
tableman.o: ../tables/tableman.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/tableman.c
descrip.o: ../tables/descrip.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/descrip.c
visible.o: ../tables/visible.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/visible.c
assume.o: ../tables/assume.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/assume.c
primtbl.o: ../tables/primtbl.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/primtbl.c
t_compl.o: ../tables/t_compl.c
	$(CC) -c $(CFLAGS) $(TR) ../tables/t_compl.c


################################################################## dcls

dcl.o: ../dcls/dcl.c
	$(CC) -c $(CFLAGS) $(TR) ../dcls/dcl.c
dclutil.o: ../dcls/dclutil.c
	$(CC) -c $(CFLAGS) $(TR) ../dcls/dclutil.c
dclclass.o: ../dcls/dclclass.c
	$(CC) -c $(CFLAGS) $(TR) ../dcls/dclclass.c
dclcnstr.o: ../dcls/dclcnstr.c
	$(CC) -c $(CFLAGS) $(TR) ../dcls/dclcnstr.c
defdcls.o: ../dcls/defdcls.c
	$(CC) -c $(CFLAGS) $(TR) ../dcls/defdcls.c
deferdcl.o: ../dcls/deferdcl.c
	$(CC) -c $(CFLAGS) $(TR) ../dcls/deferdcl.c
report.o: ../dcls/report.c
	$(CC) -c $(CFLAGS) $(TR) ../dcls/report.c
somedcls.o: ../dcls/somedcls.c
	$(CC) -c $(CFLAGS) $(TR) ../dcls/somedcls.c
expnddcl.o: ../dcls/expnddcl.c
	$(CC) -c $(CFLAGS) $(TR) ../dcls/expnddcl.c


################################################################## gc

gc.o: ../gc/gc.c
	$(CC) -c $(CFLAGS) $(MA) ../gc/gc.c
gcstate.o: ../gc/gcstate.c
	$(CC) -c $(CFLAGS) $(MA) ../gc/gcstate.c


################################################################## error

error.o: ../error/error.c
	$(CC) -c $(CFLAGS) $(TR) ../error/error.c
m_error.o: ../error/m_error.c
	$(CC) -c $(CFLAGS) $(MA) ../error/m_error.c
newrole.o: ../error/newrole.c
	$(CC) -c $(CFLAGS) $(TR) ../error/newrole.c


################################################################## machdata


entity.o: ../machdata/entity.c
	$(CC) -c $(CFLAGS) $(MA) ../machdata/entity.c
m_except.o: ../machdata/except.c
	$(CC) -o m_except.o -c $(CFLAGS) $(MA) ../machdata/except.c
t_except.o: ../machdata/except.c
	$(CC) -o t_except.o -c $(CFLAGS) $(TR) ../machdata/except.c
wrap.o: ../machdata/wrap.c
	$(CC) -c $(CFLAGS) $(MA) ../machdata/wrap.c

############################################################### machstrc

control.o: ../machstrc/control.c
	$(CC) -c $(CFLAGS) $(MA) ../machstrc/control.c
environ.o: ../machstrc/environ.c
	$(CC) -c $(CFLAGS) $(MA) ../machstrc/environ.c
stack.o: ../machstrc/stack.c
	$(CC) -c $(CFLAGS) $(MA) ../machstrc/stack.c
state.o: ../machstrc/state.c
	$(CC) -c $(CFLAGS) $(MA) ../machstrc/state.c
actvn.o: ../machstrc/actvn.c
	$(CC) -c $(CFLAGS) $(MA) ../machstrc/actvn.c


############################################################### evaluate

evaluate.o: ../evaluate/evaluate.c
	$(CC) -c $(CFLAGS) $(MA) ../evaluate/evaluate.c
instinfo.o: ../evaluate/instinfo.c
	$(CC) -c $(CFLAGS) ../evaluate/instinfo.c
evalsup.o: ../evaluate/evalsup.c
	$(CC) -c $(CFLAGS) $(MA) ../evaluate/evalsup.c
eval.o: ../evaluate/eval.c
	$(CC) -c $(CFLAGS) $(MA) ../evaluate/eval.c
apply.o: ../evaluate/apply.c
	$(CC) -c $(CFLAGS) $(MA) ../evaluate/apply.c
coroutin.o: ../evaluate/coroutin.c
	$(CC) -c $(CFLAGS) $(MA) ../evaluate/coroutin.c
fail.o: ../evaluate/fail.c
	$(CC) -c $(CFLAGS) $(MA) ../evaluate/fail.c
lazy.o: ../evaluate/lazy.c
	$(CC) -c $(CFLAGS) $(MA) ../evaluate/lazy.c
lazyprim.o: ../evaluate/lazyprim.c
	$(CC) -c $(CFLAGS) $(MA) ../evaluate/lazyprim.c
typeinst.o: ../evaluate/typeinst.c
	$(CC) -c $(CFLAGS) $(MA) ../evaluate/typeinst.c


############################################################### intrprtr

intrprtr.o: ../intrprtr/intrprtr.c
	$(CC) -c $(CFLAGS) $(MA) ../intrprtr/intrprtr.c
package.o: ../intrprtr/package.c
	$(CC) -c $(CFLAGS) $(MA) ../intrprtr/package.c
intvars.o: ../intrprtr/intvars.c
	$(CC) -c $(CFLAGS) $(MA) ../intrprtr/intvars.c


################################################################## rts

bigint.o: ../rts/bigint.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/bigint.c
bitvec.o: ../rts/bitvec.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/bitvec.c
compare.o: ../rts/compare.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/compare.c
integer.o: ../rts/integer.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/integer.c
number.o: ../rts/number.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/number.c
numconv.o: ../rts/numconv.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/numconv.c
prtnum.o: ../rts/prtnum.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/prtnum.c
tstrutil.o: ../rts/tstrutil.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/tstrutil.c
tostring.o: ../rts/tostring.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/tostring.c
product.o: ../rts/product.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/product.c
append.o: ../rts/append.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/append.c
array.o: ../rts/array.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/array.c
rational.o: ../rts/rational.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/rational.c
real.o: ../rts/real.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/real.c
qreal.o: ../rts/qreal.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/qreal.c
io.o: ../rts/io.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/io.c
system.o: ../rts/system.c
	$(CC) -c $(CFLAGS) $(MA) ../rts/system.c


################################################################## misc


t_stubs.o: ../misc/t_stubs.c
	$(CC) -c $(CFLAGS) $(TR) ../misc/t_stubs.c
m_stubs.o: ../misc/m_stubs.c
	$(CC) -c $(CFLAGS) $(MA) ../misc/m_stubs.c
.c.o:
	$(CC) -c $(CFLAGS) $<

########################################## Other dependencies

generate.o: ../evaluate/instruc.h
package.o:  ../evaluate/instruc.h

allocors.o: ../parser/stmptok

t_tyutil.o: ../parser/stmptok
type.o:     ../parser/stmptok
printty.o:  ../parser/stmptok
complete.o: ../parser/stmptok

t_clstbl.o: ../parser/stmptok
m_clstbl.o: ../parser/stmptok
t_mettbl.o: ../parser/stmptok
m_mettbl.o: ../parser/stmptok
abbrev.o:   ../parser/stmptok
cnstrlst.o: ../parser/stmptok
stdtytbl.o: ../parser/stmptok
dflttbl.o:  ../parser/stmptok

dcl.o:      ../parser/stmptok
dclclass.o: ../parser/stmptok
dclutil.o:  ../parser/stmptok
dclcnstr.o: ../parser/stmptok
somedcls.o: ../parser/stmptok

debug.o:    ../parser/stmptok
t_debug.o:  ../parser/stmptok
m_debug.o:  ../parser/stmptok

typeinst.o: ../parser/stmptok

error.o:    ../error/error.h
error.o:    ../parser/stmptok

choose.o:   ../parser/stmptok

gendcl.o:   ../parser/stmptok
generate.o: ../parser/stmptok
genstd.o:   ../parser/stmptok
improve.o:  ../parser/stmptok
gentype.o:   ../parser/stmptok

defer.o:    ../parser/stmptok
defaults.o: ../parser/stmptok

package.o:  ../parser/stmptok

pmcompl.o:  ../parser/stmptok

lexer.o:    ../parser/stmptok
lexids.o:   ../parser/stmptok
lexsup.o:   ../parser/stmptok
preflex.o:  ../parser/stmptok
lexerr.o:   ../parser/stmptok

parseerr.o: ../parser/stmptok
parsutil.o: ../parser/stmptok

numconv.o:  ../parser/stmptok

stdfuns.o:  ../parser/stmptok

fileinfo.o: ../parser/stmptok
primtbl.o:  ../parser/stmptok
tableman.o: ../parser/stmptok
assume.o:   ../parser/stmptok
secimp.o:   ../parser/stmptok
t_compl.o:  ../parser/stmptok

unify.o:    ../parser/stmptok

t_strops.o: ../parser/stmptok
m_strops.o: ../parser/stmptok
filename.o: ../parser/stmptok

t_debug.o:  ../debug/debug.h
m_debug.o:  ../debug/debug.h

printrts.o:   ../show/printrts.h

