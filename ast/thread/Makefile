SHELL = /bin/csh
.SUFFIXES: .ast .aso .asd
ASTC=astc

.ast.aso:
	$(ASTC) -l -1 $*

.ast.asd:
	$(ASTC) -l -1 $*


all:\
     channel.aso\
     semaphor.aso\
     sync.aso\
     thread.aso

channel.aso: channel.ast channel.asi
semaphor.aso: semaphor.asi semaphor.ast
sync.aso: sync.ast sync.asi
thread.aso: thread.ast thread.asi
