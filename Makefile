# include Husky-Makefile-Config
include ../huskymak.cfg

ifeq ($(DEBUG), 1)
  COPT = $(DEBCFLAGS) $(WARNFLAGS) -I$(INCDIR)
  LFLAGS = $(DEBLFLAGS)
else
  COPT = $(OPTCFLAGS) $(WARNFLAGS) -I$(INCDIR)
  LFLAGS = $(OPTLFLAGS)
endif

CDEFS=-D$(OSTYPE) $(ADDCDEFS)

ifeq ($(SHORTNAME), 1)
  LOPT = -L$(LIBDIR) -lsmapi -lfidoconf
else
  LOPT = -L$(LIBDIR) -lsmapi -lfidoconfig
endif

all : sqpack$(EXE) sqpack.1.gz

sqpack$(OBJ): sqpack.c
	$(CC) $(COPT) $(CDEFS) sqpack.c -o sqpack$(OBJ)

sqpack$(EXE): sqpack$(OBJ)
	$(CC) $(LFLAGS) -o sqpack$(EXE) sqpack$(OBJ) $(LOPT)

sqpack.1.gz : sqpack.1
	gzip -c sqpack.1 > sqpack.1.gz

install: sqpack$(EXE) sqpack.1.gz
	$(INSTALL) $(IBOPT) sqpack$(EXE) $(BINDIR)
	$(INSTALL) $(IMOPT) sqpack.1.gz $(MANDIR)/man1

uninstall:
	-$(RM) $(RMOPT) $(BINDIR)$(DIRSEP)sqpack$(EXE)
	-$(RM) $(RMOPT) $(MANDIR)$(DIRSEP)man1$(DIRSEP)sqpack.1.gz

clean: 
	-$(RM) $(RMOPT) *~
	-$(RM) $(RMOPT) *$(OBJ)


distclean: clean
	-$(RM) $(RMOPT) sqpack$(EXE)
	-$(RM) $(RMOPT) sqpack.1.gz

