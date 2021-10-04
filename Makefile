# sqpack/Makefile
#
# This file is part of sqpack, part of the Husky fidonet software project
# Use with GNU version of make v.3.82 or later
# Requires: husky enviroment
#

# Version
sqpack_g1:=$(GREP) -Po 'define\s+VER_MAJOR\s+\K\d+'
sqpack_g2:=$(GREP) -Po 'define\s+VER_MINOR\s+\K\d+'
sqpack_g3:=$(GREP) -Po 'define\s+VER_PATCH\s+\K\d+'
sqpack_g4:=$(GREP) -Po 'char\s+cvs_date\[\]\s*=\s*"\K\d+-\d+-\d+'
sqpack_VERMAJOR := $(shell $(sqpack_g1) $(sqpack_ROOTDIR)$(sqpack_H_DIR)version.h)
sqpack_VERMINOR := $(shell $(sqpack_g2) $(sqpack_ROOTDIR)$(sqpack_H_DIR)version.h)
sqpack_VERPATCH := $(shell $(sqpack_g3) $(sqpack_ROOTDIR)$(sqpack_H_DIR)version.h)
sqpack_cvsdate  := $(shell $(sqpack_g4) $(sqpack_ROOTDIR)$(sqpack_H_DIR)cvsdate.h)
sqpack_reldate  := $(subst -,,$(sqpack_cvsdate))
sqpack_VER      := $(sqpack_VERMAJOR).$(sqpack_VERMINOR).$(sqpack_reldate)

sqpack_LIBS := $(fidoconf_TARGET_BLD) $(smapi_TARGET_BLD) $(huskylib_TARGET_BLD)

sqpack_CFLAGS = $(CFLAGS)
sqpack_CDEFS := $(CDEFS) -I$(fidoconf_ROOTDIR) -I$(smapi_ROOTDIR) \
                          -I$(huskylib_ROOTDIR) -I$(sqpack_ROOTDIR)$(sqpack_H_DIR)

sqpack_SRC  = $(sqpack_SRCDIR)sqpack.c
sqpack_OBJS = $(addprefix $(sqpack_OBJDIR),$(notdir $(sqpack_SRC:.c=$(_OBJ))))
sqpack_DEPS = $(addprefix $(sqpack_DEPDIR),$(notdir $(sqpack_SRC:.c=$(_DEP))))

sqpack_TARGET     = sqpack$(_EXE)
sqpack_TARGET_BLD = $(sqpack_BUILDDIR)$(sqpack_TARGET)
sqpack_TARGET_DST = $(BINDIR_DST)$(sqpack_TARGET)

ifdef MAN1DIR
    sqpack_MAN1PAGES := sqpack.1
    sqpack_MAN1BLD := $(sqpack_BUILDDIR)$(sqpack_MAN1PAGES).gz
    sqpack_MAN1DST := $(DESTDIR)$(MAN1DIR)$(DIRSEP)$(sqpack_MAN1PAGES).gz
endif

.PHONY: sqpack_all sqpack_install sqpack_uninstall sqpack_clean \
        sqpack_distclean sqpack_depend sqpack_rmdir_DEP sqpack_rm_DEPS \
        sqpack_clean_OBJ sqpack_main_distclean

sqpack_all: $(sqpack_TARGET_BLD) $(sqpack_MAN1BLD)

ifneq ($(MAKECMDGOALS), depend)
    ifneq ($(MAKECMDGOALS), distclean)
        ifneq ($(MAKECMDGOALS), uninstall)
            include $(sqpack_DEPS)
        endif
    endif
endif


# Build application
$(sqpack_TARGET_BLD): $(sqpack_OBJS) $(sqpack_LIBS) | do_not_run_make_as_root
	$(CC) $(LFLAGS) $(EXENAMEFLAG) $@ $^

# Compile .c files
$(sqpack_OBJS): $(sqpack_SRC) | $(sqpack_OBJDIR) do_not_run_make_as_root
	$(CC) $(sqpack_CFLAGS) $(sqpack_CDEFS) -o $@ $<

$(sqpack_OBJDIR): | do_not_run_make_as_root $(sqpack_BUILDDIR)
	[ -d $@ ] || $(MKDIR) $(MKDIROPT) $@


# Build man pages
ifdef MAN1DIR
    $(sqpack_MAN1BLD): $(sqpack_MANDIR)$(sqpack_MAN1PAGES) | do_not_run_make_as_root
	gzip -c $< > $@
else
    $(sqpack_MAN1BLD): ;
endif

# Install
ifneq ($(MAKECMDGOALS), install)
    sqpack_install: ;
else
    sqpack_install: $(sqpack_TARGET_DST) sqpack_install_man ;
endif

$(sqpack_TARGET_DST): $(sqpack_TARGET_BLD) | $(DESTDIR)$(BINDIR)
	$(INSTALL) $(IBOPT) $< $(DESTDIR)$(BINDIR); \
	$(TOUCH) "$@"

ifndef MAN1DIR
    sqpack_install_man: ;
else
    sqpack_install_man: $(sqpack_MAN1DST)

    $(sqpack_MAN1DST): $(sqpack_MAN1BLD) | $(DESTDIR)$(MAN1DIR)
	$(INSTALL) $(IMOPT) $< $(DESTDIR)$(MAN1DIR); $(TOUCH) "$@"
endif


# Clean
sqpack_clean: sqpack_clean_OBJ
	-[ -d "$(sqpack_OBJDIR)" ] && $(RMDIR) $(sqpack_OBJDIR) || true

sqpack_clean_OBJ:
	-$(RM) $(RMOPT) $(sqpack_OBJDIR)*

# Distclean
sqpack_distclean: sqpack_main_distclean sqpack_rmdir_DEP
	-[ -d "$(sqpack_BUILDDIR)" ] && $(RMDIR) $(sqpack_BUILDDIR) || true

sqpack_rmdir_DEP: sqpack_rm_DEPS
	-[ -d "$(sqpack_DEPDIR)" ] && $(RMDIR) $(sqpack_DEPDIR) || true

sqpack_rm_DEPS:
	-$(RM) $(RMOPT) $(sqpack_DEPDIR)*

sqpack_main_distclean: sqpack_clean
	-$(RM) $(RMOPT) $(sqpack_TARGET_BLD)
ifdef MAN1DIR
	-$(RM) $(RMOPT) $(sqpack_MAN1BLD)
endif


# Uninstall
sqpack_uninstall:
	-$(RM) $(RMOPT) $(sqpack_TARGET_DST)
ifdef MAN1DIR
	-$(RM) $(RMOPT) $(sqpack_MAN1DST)
endif


# Depend
ifeq ($(MAKECMDGOALS),depend)
sqpack_depend: $(sqpack_DEPS) ;

# Build a dependency makefile for the source file
$(sqpack_DEPS): $(sqpack_DEPDIR)%$(_DEP): $(sqpack_SRCDIR)%.c | $(sqpack_DEPDIR)
	@set -e; rm -f $@; \
	$(CC) -MM $(sqpack_CFLAGS) $(sqpack_CDEFS) $< > $@.$$$$; \
	sed 's,\($*\)$(_OBJ)[ :]*,$(sqpack_OBJDIR)\1$(_OBJ) $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(sqpack_DEPDIR): | do_not_run_depend_as_root $(sqpack_BUILDDIR)
	[ -d $@ ] || $(MKDIR) $(MKDIROPT) $@
endif

$(sqpack_BUILDDIR):
	[ -d $@ ] || $(MKDIR) $(MKDIROPT) $@
