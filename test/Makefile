SHELL = /bin/csh
ASTC = astc
ASTR = astr

#.badcomp: files with compile errors
#.badrun:  files with run-time errors (want to dump configuration)
#.run:     files that compile/run smoothly, but want behavior checking 
#	   suppressed on compiles
#.showcomp:like .run, but show compiler listing.
#.failrun: files that compile fine (with behavior testing suppressed)
#	   but that are expected to fail when run)
#.behrun:  like .run, but compile with behavior checking.

#DTO = -DTO
DTO = 

.SUFFIXES: .ast .aso .badcomp .showcomp .comp .run .failrun .behrun .badrun


.ast.run:
	cd $(@D); $(ASTC) -l $(*F)
	cd $(@D); $(ASTR) $(DTO) $(*F)
	/bin/touch $*.run
	@echo "=========================================================="

.ast.comp:
	cd $(@D); $(ASTC) -l $(*F)
	/bin/touch $*.comp
	@echo "=========================================================="

.ast.showcomp:
	cd $(@D); $(ASTC) $(*F)
	cd $(@D); $(ASTR) $(DTO) $(*F)
	/bin/touch $*.showcomp
	@echo "=========================================================="

.ast.failrun:
	cd $(@D); $(ASTC) -l $(*F)
	-cd $(@D); $(ASTR) $(DTO) $(*F)
	/bin/touch $*.failrun
	@echo "=========================================================="

.ast.behrun:
	cd $(@D); $(ASTC) -l +b $(*F)
	cd $(@D); $(ASTR) $(DTO) $(*F)
	/bin/touch $*.behrun
	@echo "=========================================================="

.ast.badrun:
	cd $(@D); $(ASTC) -l $(*F)
	-cd $(@D); $(ASTR) -t $(*F)
	cat $(@D)/ast.rts
	/bin/touch $*.badrun
	@echo "=========================================================="

.ast.badcomp:
	-cd $(@D); $(ASTC) $(*F)
	/bin/touch $*.badcomp
	@echo "=========================================================="

.ast.aso:
	$(ASTC) $*

all: run showcomp badcomp

good: run showcomp

run: exprrun dclrun librun packagecomp packagerun miscrun

#showcomp: exprshowcomp dclshowcomp
showcomp: dclshowcomp

badcomp: exprbadcomp dclbadcomp syntax

clean:
	find . \( -name \*.run -o -name \*.comp -o -name \*.badrun -o -name \*.badcomp -o -name \*.failrun -o -name \*.behrun -o -name \*.showcomp \) -print -exec /bin/rm -f \{\} \;

cleanshow:
	find . \( -name \*.run -o -name \*.comp -o -name \*.badrun -o -name \*.badcomp -o -name \*.failrun -o -name \*.behrun -o -name \*.showcomp \) -print

veryclean:
	find . \( -name \*.run -o -name \*.comp -o -name \*.badrun -o -name \*.badcomp -o -name \*.failrun -o -name \*.behrun -o -name \*.showcomp -o -name \*.aso -o -name \*.asd -o -name index.asx -o -name mon.out -o -name gmon.out -o -name core -o -name ast.rts \) -print -exec /bin/rm -f \{\} \;

verycleanshow:
	find . \( -name \*.run -o -name \*.comp -o -name \*.badrun -o -name \*.badcomp -o -name \*.failrun -o -name \*.behrun -o -name \*.showcomp -o -name \*.aso -o -name mon.out -o -name core -o -name ast.rts \) -print



###############################################################
#			Misc
###############################################################

miscrun: misc/pid.run

###############################################################
#			Expressions
###############################################################

exprrun:     expr/constant.run

exprrun:     expr/appl/curry.run
exprrun:     expr/appl/function.run
exprrun:     expr/appl/ops.run
exprrun:     expr/appl/operator.run
exprrun:     expr/appl/opover.run
exprrun:     expr/appl/function2.run
exprrun:     expr/appl/fixp.run
exprrun:     expr/appl/higher.run
exprbadcomp: expr/appl/operr.badcomp
exprbadcomp: expr/appl/openproc.badcomp

exprrun: expr/env/env.run
exprrun: expr/env/let.run
exprrun: expr/env/relet.run
exprrun: expr/env/letpat.run
exprrun: expr/env/team.run
exprrun: expr/env/team2.run
exprrun: expr/env/open.run
exprrun: expr/env/openfail.run
exprrun: expr/env/locglob.run
exprrun: expr/env/funby.run
exprrun: expr/env/opendist.run
exprrun: expr/env/openpair.run
exprrun: expr/env/shadglob.run
exprrun: expr/env/infdup.run

exprbadcomp: expr/env/badlet.badcomp
exprbadcomp: expr/env/scope.badcomp
exprbadcomp: expr/env/scope2.badcomp
exprbadcomp: expr/env/openscop.badcomp
exprbadcomp: expr/env/use.badcomp

exprrun: expr/cond/if.run
exprrun: expr/cond/ifscope.run
exprrun: expr/cond/chsfrst.run
exprrun: expr/cond/chsone.run
exprrun: expr/cond/chsperh.run
exprrun: expr/cond/multicas.run
exprrun: expr/cond/do1.run
exprrun: expr/cond/do.run
exprrun: expr/cond/dosubcases.run
exprrun: expr/cond/dothen.run
exprrun: expr/cond/letdo.run
exprrun: expr/cond/funcases.run
exprrun: expr/cond/chsmat.run
exprrun: expr/cond/chserr2.failrun
exprrun: expr/cond/chserr3.failrun
exprrun: expr/cond/defcases.run
exprrun: expr/cond/defcase.run
exprrun: expr/cond/exh.run
exprrun: expr/cond/openchoose.run
exprrun: expr/cond/whenmat.run
exprbadcomp: expr/cond/chserr1.badcomp
exprbadcomp: expr/cond/chserr4.badcomp
exprbadcomp: expr/cond/openchoose2.badcomp

exprrun: expr/loop/loop.run
exprrun: expr/loop/nest.run
exprrun: expr/loop/for.run
exprrun: expr/loop/openloop.run
exprrun: expr/loop/forfail.run
exprrun: expr/loop/multicas.run
exprrun: expr/loop/while.run
exprbadcomp: expr/loop/loopelse.badcomp

exprrun: expr/var/var.run
exprrun: expr/var/var1.run
exprrun: expr/var/var2.run
exprrun: expr/var/var3.run
exprrun: expr/var/vard1.run
exprrun: expr/var/vare1.run
exprrun: expr/var/vard2.run
exprrun: expr/var/vare2.run
exprrun: expr/var/vard3.run
exprrun: expr/var/vare3.run
exprrun: expr/var/vard4.run
exprrun: expr/var/vare4.run
exprrun: expr/var/vard5.run
exprrun: expr/var/vare5.run
exprrun: expr/var/state.run
exprrun: expr/var/rank.run
exprrun: expr/var/printbox.run
exprrun: expr/var/init.run
exprrun: expr/var/inf.run
exprrun: expr/var/arr.run
exprrun: expr/var/trybox.run
exprrun: expr/var/onassgn.run

exprrun: expr/lazy/lazy.run
exprrun: expr/lazy/lazyleft.run
exprrun: expr/lazy/layers.run
exprrun: expr/lazy/dollar.run
exprrun: expr/lazy/recomput.run
exprrun: expr/lazy/await.run
exprrun: expr/lazy/lazylst.run
exprrun: expr/lazy/lazylst2.run
exprrun: expr/lazy/fail.failrun
exprrun: expr/lazy/fail2.failrun
exprrun: expr/lazy/show1.run

exprrun: expr/fail/exc1.failrun
exprrun: expr/fail/exc2.failrun
exprrun: expr/fail/excinfo1.run
exprrun: expr/fail/excinfo2.failrun
exprrun: expr/fail/excinfo.failrun
exprrun: expr/fail/excname.run
exprrun: expr/fail/except2.comp
exprrun: expr/fail/except.run
exprrun: expr/fail/try.run
exprrun: expr/fail/quicktry.run

exprrun: expr/stream/stream1.run
exprrun: expr/stream/stream2.run
exprrun: expr/stream/chsall1.run
exprrun: expr/stream/loopall.run
exprrun: expr/stream/strmex.run
exprrun: expr/stream/strmrlet.run
exprrun: expr/stream/letbyall.run

exprrun: expr/mix/mix2.run
exprrun: expr/mix/strmix.run
#exprrun: expr/mix/chsonemx.run
exprrun: expr/mix/deadlock.failrun
exprrun: expr/mix/execthread.run
exprrun: expr/mix/freeze.run
exprrun: expr/mix/mixes.run

exprrun: expr/irreg/unwrap.run
##exprrun: expr/irreg/dispatch.failrun
exprrun: expr/irreg/rtv.run
exprrun: expr/irreg/rtv3.failrun
exprrun: expr/irreg/forget.run
exprrun: expr/irreg/forget1.failrun
#exprrun: expr/irreg/irrfun.run
#exprrun: expr/irreg/irrfun2.run
exprrun: expr/irreg/uw.run
exprrun: expr/irreg/rtv1.run
#exprrun: expr/irreg/rtv2.run
exprbadcomp: expr/irreg/ovl1.badcomp
exprbadcomp: expr/irreg/ovl2.badcomp

exprrun: expr/patmat/orelse.run
exprrun: expr/patmat/basic.run
exprrun: expr/patmat/factoria.run
exprrun: expr/patmat/cat.run
exprrun: expr/patmat/stringma.run
exprrun: expr/patmat/orelse.run
exprrun: expr/patmat/lazy.run
exprrun: expr/patmat/rolesel.run
exprrun: expr/patmat/rolesel2.run
exprrun: expr/patmat/rolemod.run
exprrun: expr/patmat/rolemod2.run
exprrun: expr/patmat/currypat.run
#exprrun: expr/patmat/markpat.run
exprrun: expr/patmat/patconst.run
exprbadcomp: expr/patmat/general.badcomp
exprbadcomp: expr/patmat/where.badcomp
exprbadcomp: expr/patmat/scope.badcomp


################################################################
#			Library
################################################################

librun: lib/char/charcode.run
librun: lib/char/charrank.run

lobrun: lib/bool/misc.run

librun: lib/binary/alloc1.run
librun: lib/binary/bigarr.run
librun: lib/binary/cmpstr.run
librun: lib/binary/denseset.run
librun: lib/binary/length.run
librun: lib/binary/list2.run
librun: lib/binary/num.run
librun: lib/binary/split.run
librun: lib/binary/sublist.run
librun: lib/binary/subscript.run
librun: lib/binary/test.run
librun: lib/binary/tostring.run


librun: lib/io/copyfile.run
librun: lib/io/dollar.run
librun: lib/io/read.run
librun: lib/io/forms.run
librun: lib/io/seek1.run
librun: lib/io/fprint.run
librun: lib/io/print.run
librun: lib/io/nofile.run
librun: lib/io/show.run
librun: lib/io/pipedollar.run
librun: lib/io/read2.run
librun: lib/io/strtonum.failrun

librun: lib/list/product.run
librun: lib/list/list.run
librun: lib/list/append.run
librun: lib/list/list1.run
librun: lib/list/list2.run
librun: lib/list/listcmp.run
librun: lib/list/bitvec.run
librun: lib/list/bitvec2.run
librun: lib/list/fcnal.run
librun: lib/list/fcnal1.run
librun: lib/list/fcnal2.run
librun: lib/list/pat.run
librun: lib/list/scanfor.run
librun: lib/list/string.run
librun: lib/list/pull.run
librun: lib/list/pulltest.run
librun: lib/list/pulltest3.run
librun: lib/list/sublist.run
librun: lib/list/sublist2.run
librun: lib/list/usedpref.run
librun: lib/list/pack.run
#librun: lib/list/findlst.run
librun: lib/list/findlast.run
#librun: lib/list/rev.run
librun: lib/list/srt.run
librun: lib/list/unfold.run
librun: lib/list/sort.run
librun: lib/list/listt.run

librun: lib/system/cmdline.run
librun: lib/system/date.run
librun: lib/system/system.run
librun: lib/system/dirlist.run
librun: lib/system/fileexists.run
librun: lib/system/sys2.run
librun: lib/system/sysenv.run

librun: lib/number/commas.run
librun: lib/number/int1.run
librun: lib/number/int2.run
librun: lib/number/int3.run
librun: lib/number/int4.failrun
librun: lib/number/int5.run
librun: lib/number/hex.run
librun: lib/number/complex1.run
librun: lib/number/complex2.run
librun: lib/number/modular.run
#librun: lib/number/pi.run
librun: lib/number/polycons.run
librun: lib/number/real1.run
librun: lib/number/real2.run
librun: lib/number/real3.run
librun: lib/number/real4.run
librun: lib/number/real5.run
librun: lib/number/round.run
librun: lib/number/wnum.run
librun: lib/number/conv.run

librun: lib/logic/append.failrun
librun: lib/logic/grammar.run
librun: lib/logic/grammar2.run
librun: lib/logic/sym.run
librun: lib/logic/forunk.run
librun: lib/logic/unkkey.run

librun: lib/set/coll.run
librun: lib/set/set.run
librun: lib/set/linset.run
librun: lib/set/denseset.run
librun: lib/set/lintbl.run
librun: lib/set/lintbl2.run
librun: lib/set/table.run
librun: lib/set/table2.run
librun: lib/set/stress1.run

librun: lib/misc/rand.run
#librun: lib/misc/random.run

librun: lib/wrapped/eqq.run
librun: lib/wrapped/weq.run


#################################################################
#			Declarations
#################################################################

dclrun: dcl/execute/execute.run
dclrun: dcl/execute/exec2.run
dclrun: dcl/execute/exec1.run
dclbadcomp: dcl/execute/err.badcomp

dclrun: dcl/misc/descr1.run
dclrun: dcl/misc/hat.run
dclrun: dcl/misc/locassm.run
dclrun: dcl/misc/advise2.run
dclbadcomp: dcl/misc/advise1.badcomp
dclbadcomp: dcl/misc/empty.badcomp
dclshowcomp: dcl/misc/defassume.showcomp
dclshowcomp: dcl/misc/showloc.showcomp

dclrun: dcl/defexp/def1.run
dclrun: dcl/defexp/def2.run
dclrun: dcl/defexp/def3.run
dclrun: dcl/defexp/copy.run
dclrun: dcl/defexp/team1.run
dclrun: dcl/defexp/nodef.failrun
dclrun: dcl/defexp/defelse.run
dclrun: dcl/defexp/defthen.run
dclrun: dcl/defexp/letop.run
dclrun: dcl/defexp/ant1.run
dclrun: dcl/defexp/ant2.run
dclrun: dcl/defexp/ovld.run
dclrun: dcl/defexp/ovld2.run
dclrun: dcl/defexp/overundr.run
dclrun: dcl/defexp/dispatch1.run

dclshowcomp: dcl/defexp/id.showcomp
dclshowcomp: dcl/defexp/assume.showcomp

dclbadcomp: dcl/defexp/expect.badcomp
dclbadcomp: dcl/defexp/leterr.badcomp
dclbadcomp: dcl/defexp/expect2.badcomp
dclbadcomp: dcl/defexp/strngmis.badcomp
dclbadcomp: dcl/defexp/defall.badcomp
dclbadcomp: dcl/defexp/underover.badcomp

dclrun:     dcl/bring/bring2.run

dclrun: dcl/infer/default.run
dclrun: dcl/infer/dflt.run
dclrun: dcl/infer/hermit.run
dclrun: dcl/infer/assm.run
dclshowcomp: dcl/infer/assume.showcomp
dclbadcomp: dcl/infer/locexp.badcomp
dclbadcomp: dcl/infer/default2.badcomp

dclrun: dcl/expand/expand1.run
dclrun: dcl/expand/patmat1.run
dclrun: dcl/expand/pat.run
dclrun: dcl/expand/pat2.run
dclrun: dcl/expand/pat3.run
dclrun: dcl/expand/patwcon.run
dclrun: dcl/expand/const.run
dclrun: dcl/expand/copy.failrun
dclrun: dcl/expand/nolocals.run
dclrun: dcl/expand/patimp4.comp
dclrun: dcl/expand/patimp3.comp
dclrun: dcl/expand/patimp2.comp
dclrun: dcl/expand/patimp1.run
dclrun: dcl/expand/assume.run

dclbadcomp: dcl/expand/pat1.badcomp
dclbadcomp: dcl/expand/test5.badcomp

dclrun: dcl/context/context1.run
dclrun: dcl/context/context2.run
dclrun: dcl/context/context3.run
dclrun: dcl/context/context4.run
dclrun: dcl/context/reveal.run

dclrun: dcl/type/selector.run
dclrun: dcl/type/role1.run
dclrun: dcl/type/type8.run
dclrun: dcl/type/cnstr1.run
dclrun: dcl/type/enum.run
dclrun: dcl/type/type5.run
dclrun: dcl/type/default3.run
dclrun: dcl/type/tyexpdsc.run
dclrun: dcl/type/varname.run
dclrun: dcl/type/anyvar.run

dclrun: dcl/type/circconstraint.run

dclrun: dcl/type/type.run
dclrun: dcl/type/type1.run
dclrun: dcl/type/type2.run
dclrun: dcl/type/type3.run
dclrun: dcl/type/currycon.run
dclrun: dcl/type/exp1.run
dclrun: dcl/type/exp2.run
dclrun: dcl/type/abbrev3a.run
dclrun: dcl/type/abbrev3.run
dclrun: dcl/type/abbrevvar.run

dclrun: dcl/class/class1.run
dclrun: dcl/class/class1a.run
dclrun: dcl/class/class2.run
dclrun: dcl/class/class3.run
dclrun: dcl/class/class4.run

dclshowcomp: dcl/type/fammacro.showcomp
#dclshowcomp: dcl/type/with.showcomp
dclshowcomp: dcl/type/default2.showcomp

dclbadcomp: dcl/type/exp3.badcomp
dclbadcomp: dcl/type/eq1.badcomp
dclbadcomp: dcl/type/assoc1.badcomp
dclbadcomp: dcl/type/badvar.badcomp
dclbadcomp: dcl/type/abbrev1.badcomp
dclbadcomp: dcl/type/abbrev2.badcomp
dclbadcomp: dcl/type/tydeferr.badcomp
dclbadcomp: dcl/type/type4.badcomp
dclbadcomp: dcl/type/ubvar.badcomp
dclbadcomp: dcl/type/classes.badcomp
dclbadcomp: dcl/type/badextend.badcomp

dclrun: dcl/misc/descrip1.run

dclbadcomp: dcl/misc/assume3.badcomp	# No errors, but don't run.
dclbadcomp: dcl/misc/assume1.badcomp

###############################################################
#			Packages
###############################################################

packagecomp: packages/dir5.comp
packagecomp: packages/subdir/dir3.comp
packagecomp: packages/subdir/dir4.comp
packagerun:  packages/dir2.run
packagecomp: packages/imp2.comp
packagecomp: packages/imp01.comp
packagerun:  packages/imp0.run
packagecomp: packages/impA2.comp
packagecomp: packages/impA1.comp
packagerun:  packages/impA0.run
packagecomp: packages/impB1.comp
packagerun:  packages/impB0.run
packagecomp: packages/impE1.comp
packagerun:  packages/impE0.run
packagerun: packages/secimp0.run
packagecomp: packages/secimp1.comp
packagecomp: packages/secimp2.comp
packagecomp: packages/secimp3.comp
packagecomp: packages/secimp4.comp
packagecomp: packages/secimp5.comp
packagerun: packages/secimpa0.run
packagecomp: packages/secimpa1.comp
packagecomp: packages/secimpa2.comp
packagecomp: packages/secimpa3.comp
packagecomp: packages/secimpa4.comp
packagecomp: packages/secimpa5.comp
packagecomp: packages/prot1.comp
packagecomp: packages/prot2.comp
packagecomp: packages/prot4.comp
packagecomp: packages/prot5.comp
packagecomp: packages/prot6.comp
packagecomp: packages/prot7.comp
packagecomp: packages/impall2.comp

packagebadcomp: packages/prot3.badcomp
packagecomp: packages/private2.comp
packagebadcomp: packages/private1.badcomp
packagebadcomp:  packages/priv1a.badcomp
packagebadcomp: packages/imp1.badcomp

#######################################################################
#			Error Reporting: Compiler

syntax: misc/longcom2.run
syntax: misc/longcom4.run
syntax: misc/small.run
syntax: err/syn1.badcomp
syntax: err/sympkg.badcomp
syntax: err/badinden.badcomp
syntax: err/badpack.badcomp
syntax: err/badpack2.badcomp
syntax: err/noname.badcomp
syntax: err/indent.badcomp
syntax: err/indent1.badcomp
syntax: misc/longcomm.badcomp
syntax: misc/longcom3.badcomp

#syntax: synerr/syn1.badcomp
#syntax: synerr/syn2.badcomp
#syntax: synerr/syn3.badcomp
#syntax: synerr/syn4.badcomp
#syntax: synerr/syn5.badcomp
#syntax: synerr/syn6.badcomp
#syntax: synerr/syn7.badcomp
#syntax: synerr/syn8.badcomp
#syntax: synerr/syn9.badcomp
#syntax: synerr/syn10.badcomp
#syntax: synerr/syn11.badcomp
#syntax: synerr/syn12.badcomp
#syntax: synerr/syn13.badcomp
#syntax: synerr/imp2.badcomp
#syntax: synerr/imp4.badcomp
#syntax: synerr/missingcomma.badcomp
#syntax: synerr/choose.badcomp
#syntax: synerr/dotdotdot.badcomp

# Type, role checking

#typecheck: err/missing.badcomp
#typecheck: err/missing2.badcomp
#typecheck: err/usedpref.badcomp (not a bad compile!)
#typecheck: err/role1.badcomp
#typecheck: err/looprole.badcomp


# Id handling

#idhandle: err/relet.badcomp
#idhandle: class/tydeferr1.badcomp
#idhandle: err/ovlderr.badcomp
#idhandle: err/used.badcomp
#idhandle: err/open1.badcomp
#idhandle: err/if2.badcomp
#idhandle: err/try2.badcomp
#idhandle: err/try3.badcomp
#idhandle: patmat/where.badcomp
#idhandle: err/expect1.badcomp
#idhandle: err/expect2.badcomp
#idhandle: patmat/pat1.badcomp
#idhandle: patmat/patvar.badcomp
#idhandle: defexp/leterr.badcomp
#idhandle: err/team.badcomp
#idhandle: err/team3.badcomp
#idhandle: err/team4.badcomp
#idhandle: defexp/special.badcomp
#idhandle: defexp/special2.badcomp
#idhandle: rtbv/consolidate.badcomp

# conditionals

#conderr: cond/chooseErr1.badcomp

# misc

#miscbadcomps: err/pmcompl.badcomp
#miscbadcomps: patmat/pat1.badcomp
#miscbadcomps: patmat/where.badcomp
#miscbadcomps: patmat/scope.badcomp
#miscbadcomps: err/descr.badcomp
#miscbadcomps: err/bad1.badcomp
#miscbadcomps: class/classes.badcomp
#miscbadcomps: class/type4.badcomp
#miscbadcomps: class/tabvar.badcomp
#miscbadcomps: class/ubvars.badcomp



#######################################################################
#			Error Reporting: Interpreter

#badruns: cond/chooseErr2.badrun
#badruns: cond/chooseErr3.badrun
#badruns: err/state.badrun
#badruns: err/machov.badrun
#badruns: err/machov1.badrun
#badruns: err/trap.badrun
#badruns: err/trap2.badrun
#badruns: err/trap3.badrun
#badruns: err/trap4.badrun
#badruns: err/trap5.badrun
#badruns: lib/number/int4.badrun

lis.run: lis.ast
	$(ASTC) lis
	$(ASTR) -idata lis
	/bin/touch lis.run
	echo "=========================================================="
