# include Husky-Makefile-Config
ifeq ($(DEBIAN), 1)
# Every Debian-Source-Paket has one included.
include /usr/share/husky/huskymak.cfg
else
include ../huskymak.cfg
endif

ifeq ($(DEBUG), 1)
  COPT = $(DEBCFLAGS) $(WARNFLAGS) -I$(INCDIR)
  LFLAGS = $(DEBLFLAGS)
else
  COPT = $(OPTCFLAGS) $(WARNFLAGS) -I$(INCDIR)
  LFLAGS = $(OPTLFLAGS)
endif

CDEFS=-D$(OSTYPE) $(ADDCDEFS)

ifeq ($(SHORTNAME), 1)
  LIBS=-L$(LIBDIR) -lfidoconf -lsmapi -lhusky
else
  LIBS=-L$(LIBDIR) -lfidoconfig -lsmapi -lhusky
endif

all : sqpack$(_EXE) sqpack.1.gz

sqpack$(_OBJ): sqpack.c
	$(CC) $(COPT) $(CDEFS) sqpack.c -o sqpack$(_OBJ)

sqpack$(_EXE): sqpack$(_OBJ)
	$(CC) $(LFLAGS) -o sqpack$(_EXE) sqpack$(_OBJ) $(LIBS)

sqpack.1.gz : sqpack.1
	gzip -9c sqpack.1 > sqpack.1.gz

install: sqpack$(_EXE) sqpack.1.gz
	$(INSTALL) $(IBOPT) sqpack$(_EXE) $(BINDIR)
	$(INSTALL) $(IMOPT) sqpack.1.gz $(MANDIR)/man1

uninstall:
	-$(RM) $(RMOPT) $(BINDIR)$(DIRSEP)sqpack$(_EXE)
	-$(RM) $(RMOPT) $(MANDIR)$(DIRSEP)man1$(DIRSEP)sqpack.1.gz

clean:
	-$(RM) $(RMOPT) *~
	-$(RM) $(RMOPT) *$(_OBJ)


distclean: clean
	-$(RM) $(RMOPT) sqpack$(_EXE)
	-$(RM) $(RMOPT) sqpack.1.gz

