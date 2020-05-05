# Makefile for building Pelagia
# See ../doc/readme.html for installation and customization instructions.

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

# Your platform. See PLATS for possible values.
PLAT= none

CC= gcc -std=gnu99
CFLAGS= -O2 -fPIC -Wall -Wextra $(SYSCFLAGS) $(MYCFLAGS)
LDFLAGS= $(SYSLDFLAGS) $(MYLDFLAGS)
LIBS= -lm $(SYSLIBS) $(MYLIBS)

AR= ar rcu
RANLIB= ranlib
RM= rm -f

SYSCFLAGS=
SYSLDFLAGS=
SYSLIBS=

MYCFLAGS=
MYLDFLAGS=
MYLIBS=
MYOBJS=

# == END OF USER SETTINGS -- NO NEED TO CHANGE ANYTHING BELOW THIS LINE =======

PLATS= aix bsd c89 freebsd generic linux macosx mingw posix solaris

PLG_A=	libpelagia.a

CORE_O=	padlist.o pbase64.o pbaseall.o pbitarray.o pcache.o pcmp.o pcrc16.o pcrc64.o pdict.o \
	pdictexten.o pdictset.o pdisk.o pelog.o pequeue.o pevent.o pfile.o \
	pfilesys.o pjob.o pjson.o plapi.o\
	plibsys.o plistdict.o plocks.o plvm.o pmanage.o pmemorylist.o \
	pmemorypool.o pquicksort.o prfesa.o psds.o psha1.o psimple.o psiphash.o \
	pskiplist.o pstart.o pstringmatch.o ptable.o ptimesys.o prandomlevel.o \
	psemaphore.o pconio.o

BASE_O= $(CORE_O) $(MYOBJS)

PLG_T=	pelagia
PLG_O=	pelagia.o

ALL_O= $(BASE_O) $(PLG_O)
ALL_T= $(PLG_A) $(PLG_T)
ALL_A= $(PLG_A)

PLG_SO=	libpelagia.so
ALL_T= $(PLG_A) $(PLG_T) $(PLG_SO)
$(PLG_SO): $(CORE_O)
	$(CC) -o $@ -shared $? -ldl -lm

# Targets start here.
default: $(PLAT)

all:	$(ALL_T)

o:	$(ALL_O)

a:	$(ALL_A)

$(PLG_A): $(BASE_O)
	$(AR) $@ $(BASE_O)
	$(RANLIB) $@

$(PLG_T): $(PLG_O) $(PLG_A)
	$(CC) -o $@ $(LDFLAGS) $(PLG_O) $(PLG_A) $(LIBS)

clean:
	$(RM) $(ALL_T) $(ALL_O)

depend:
	@$(CC) $(CFLAGS) -MM l*.c

echo:
	@echo "PLAT= $(PLAT)"
	@echo "CC= $(CC)"
	@echo "CFLAGS= $(CFLAGS)"
	@echo "LDFLAGS= $(SYSLDFLAGS)"
	@echo "LIBS= $(LIBS)"
	@echo "AR= $(AR)"
	@echo "RANLIB= $(RANLIB)"
	@echo "RM= $(RM)"

# Convenience targets for popular platforms
ALL= all

none:
	@echo "Please do 'make PLATFORM' where PLATFORM is one of these:"
	@echo "   $(PLATS)"

generic: $(ALL)

linux:
	$(MAKE) $(ALL) SYSCFLAGS="" SYSLIBS="-Wl,-E -ldl -lpthread -lreadline"

macosx:
	$(MAKE) $(ALL) SYSCFLAGS="" SYSLIBS="-lpthread -lreadline"


# list targets that do not create files (but not all makes understand .PHONY)
.PHONY: all $(PLATS) default o a clean depend echo none

# DO NOT DELETE

padlist.o: padlist.c pmemorypool.h plateform.h padlist.h
pbase64.o: pbase64.c plateform.h pbase64.h
pbaseall.o: pbaseall.c pbaseall.h pelagia.h ptimesys.h
pbitarray.o: pbitarray.c plateform.h pbitarray.h
pcache.o: pcache.c plateform.h pinterface.h pelog.h psds.h padlist.h pbitarray.h \
 pcrc16.h pdict.h plocks.h pmanage.h pcache.h pquicksort.h prandomlevel.h pinterface.h \
 pequeue.h pdisk.h pfile.h plistdict.h ptable.h pmemorylist.h pdictexten.h ptimesys.h pjson.h
pcmp.o: pcmp.c pcmp.h
pcrc16.o: pcrc16.c pcrc16.h
pcrc64.o: pcrc64.c pcrc64.h
pdict.o: pdict.c pdict.h plateform.h pmemorypool.h
pdictexten.o: pdictexten.c plateform.h padlist.h pdict.h pdictexten.h pquicksort.h
pdictset.o: pdictset.c plateform.h pdict.h pdictset.h
pdisk.o: pdisk.c pelog.h psds.h padlist.h pbitarray.h pcrc16.h pdict.h \
 plocks.h pmanage.h pdisk.h pquicksort.h prandomlevel.h pinterface.h \
 pfile.h  ptable.h  ptimesys.h pbase64.h pstart.h
pelagia.o: pelagia.c plateform.h pelagia.h pelog.h psds.h pdisk.h pmanage.h \
 pstart.h pcmd.h pbaseall.h psimple.h prfesa.h pbase64.h
pelog.o: pelog.c plateform.h pelog.h psds.h
pequeue.o: pequeue.c plateform.h padlist.h pelog.h pequeue.h psds.h plocks.h
pevent.o: pevent.c plateform.h pjob.h pequeue.h psds.h
pfile.o: pfile.c plateform.h psds.h pelog.h pfile.h plocks.h pjob.h pmemorylist.h \
 pfilesys.h
pfilesys.o: pfilesys.c plateform.h pfilesys.h
pjob.o: pjob.c plateform.h psds.h pdict.h pjob.h pequeue.h \
 padlist.h pcache.h pinterface.h pmanage.h plocks.h pelog.h pdictexten.h ptimesys.h \
 plibsys.h plvm.h pquicksort.h
pjson.o: pjson.c plateform.h pjson.h
plapi.o: plapi.c plateform.h plapi.h plua.h plauxlib.h plvm.h pjson.h pelagia.h \
 pelog.h psds.h
plibsys.o: plibsys.c plateform.h plibsys.h
plistdict.o: plistdict.c plateform.h padlist.h pdict.h plistdict.h pquicksort.h
plocks.o: plocks.c psds.h padlist.h pelog.h plocks.h
plvm.o: plvm.c plateform.h plvm.h plauxlib.h pelog.h plibsys.h \
 plualib.h plua.h
pmanage.o: pmanage.c plateform.h pequeue.h psds.h pdict.h padlist.h pdisk.h \
 pdictset.h pelog.h pjob.h pfile.h pinterface.h pmanage.h plocks.h pfilesys.h \
 ptimesys.h pelagia.h pjson.h pjob.h pbase64.h
pmemorylist.o: pmemorylist.c plateform.h pmemorylist.h plateform.h plocks.h pelog.h psds.h \
 pdict.h ptimesys.h
pmemorypool.o: pmemorypool.c plateform.h pmemorypool.h pbitarray.h
pquicksort.o: pquicksort.c plateform.h padlist.h psds.h pquicksort.h
prfesa.o: prfesa.c prfesa.h pelagia.h ptimesys.h
psds.o: psds.c plateform.h psds.h
psha1.o: psha1.c plateform.h psha1.h
psimple.o: psimple.c pelagia.h psimple.h ptimesys.h
psiphash.o: psiphash.c
pskiplist.o: pskiplist.c pskiplist.h
pstart.o: pstart.c pelog.h plateform.h pjson.h pelagia.h
pstringmatch.o: pstringmatch.c pstringmatch.h
ptable.o: ptable.c plateform.h pelog.h pinterface.h psds.h prandomlevel.h pquicksort.h \
 ptable.h pdictexten.h pcrc16.h pstringmatch.h ptimesys.h pjson.h pbase64.h
ptimesys.o: ptimesys.c ptimesys.h
prandomlevel.o: prandomlevel.c prandomlevel.h pinterface.h
psemaphore.o: psemaphore.c psemaphore.h plateform.h
pconio.o: pconio.c pconio.h plateform.h
# (end of Makefile)
