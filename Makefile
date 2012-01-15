CC=g++

################ Nothing to configure below #################
CONF_LIBNAME	= focfile
CONF_VERSION	= alpha-02
UNIX_DISTDIR	= $(CONF_LIBNAME)-$(CONF_VERSION)

RCS=debug.h focfile.h focfile.cpp mas2h rdfocfdt.cpp smdate.h smdate.cpp \
	progman.sgml README 

DOC_DISTFILES=doc/Focus.txt doc/LGPL
PROG_DISTFILES=debug.h focfile.h focfile.cpp mas2h rdfocfdt.cpp \
	smdate.h smdate.cpp progman.txt README \
	Makefile testcar.cpp car.h

all:	testcar

printdates	: printdates.o smdate.o
	$(CC) -o $@ $< smdate.o

printdates.o	: printdates.cpp smdate.h
	$(CC) -c $<


rdfocfdt	: rdfocfdt.o focfile.a
	$(CC) -o $@ $< focfile.a

rdfocfdt.o	: rdfocfdt.cpp focfile.h
	$(CC) -c rdfocfdt.cpp



testjoin	: testjoin.o focfile.a
	$(CC) -o testjoin testjoin.o focfile.a

testjoin.o	:	testjoin.cpp abstract.h organ.h
	$(CC) -c testjoin.cpp



testcar	: testcar.o focfile.a
	$(CC) -o testcar testcar.o focfile.a

testcar.o	:	testcar.cpp car.h
	$(CC) -c testcar.cpp

focfile.a	:	focfile.o smdate.o
	ar r focfile.a focfile.o smdate.o

focfile.o	:	focfile.cpp focfile.h
	$(CC) -c focfile.cpp

smdate.o	:	smdate.cpp smdate.h
	$(CC) -c smdate.cpp

%.h	: data/%.foc data/%.mas mas2h
	mas2h data/$*.foc data/$*.mas > $@

.PHONY:	checkin checkout backup clean

clean	:
	rm -f *.o *.a

backup	:
	tar cvf /dev/fd0 *.h *.cpp doc/*
		tests/Makefile tests/*.cpp tests/*.ref\
		RCS/* tests/RCS/*

checkin_name :
	ci -n$(CONF_VERSION) $(RCS)

checkin :
	ci $(RCS)

checkout :
	co -l $(RCS)

dist: unix_dist 

unix_dist: $(DOC_DISTFILES) $(PROG_DISTFILES)
	@rm -rf $(UNIX_DISTDIR)
	@mkdir $(UNIX_DISTDIR)
	@for file in $(DOC_DISTFILES); do \
		ln $$file $(UNIX_DISTDIR) || { \
			cp -p $$file $(UNIX_DISTDIR); \
		}; \
	done
	@for file in $(PROG_DISTFILES); do \
		ln $$file $(UNIX_DISTDIR) || { \
			cp -p $$file $(UNIX_DISTDIR); \
		}; \
	done
	tar -chvf - $(UNIX_DISTDIR) | gzip -9 > dist/$(UNIX_DISTDIR).tar.gz
	@rm -rf $(UNIX_DISTDIR)


FORMATS=progman.html progman.dvi progman.lj progman.ps progman.txt

zdocs : $(FORMATS)
	gzip $(FORMATS)

%.html	: %.sgml
	sgml2html $<

%.ps	: %.sgml
	sgml2latex -p $<

%.lj	: %.sgml %.dvi
	dvilj2p $*.dvi

%.dvi	: %.sgml
	sgml2latex -d $<

%.txt	: %.sgml
	sgml2txt $<

