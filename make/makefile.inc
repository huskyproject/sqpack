# $Id$
#
# Common makefiles part (include-file) for sqpack

# Use defines before including this file:
# _OBJ  object files suffix begins with point (".o", ".obj", ... )
# _LIB  library files suffix begins with point (".a", ".lib", ... )
# _DLL  dinamic linking library files suffix begins with point (".dll", ".so", ...)
# LIBPREFIX  library filename prefix ("lib", "", ...)
# LIBSUFFIX  library filename suffix (compiler ID, three characters, somewhere empty)
# DLLPREFIX  dinamic linking library prefix (usually empty)
# DLLSUFFIX  dinamic linking library suffix (compiler ID or empty)
# O, B, L, H prefixes to use with names of objects, binaries, libraries and headers respectively

# Module name
LIBNAME = sqpack

# Version
VERMAJOR= 1
VERMINOR= 9
VERPATCH= 0
VERH    = $(VERMAJOR).$(VERMINOR)
VER     = $(VERH).$(VERPATCH)

# Sources directory
_SRC_DIR= ../
# Headers directory
H_DIR   = ../

# Programm name
SQPACK	= $(B)sqpack$(_EXE)

# Object files (please sort list to easy check by human)
OBJS    = $(O)sqpack$(_OBJ)

# Header files
HEADERS = $(H)cvsdate.h $(H)version.h


# Binary file(s) to build from sources
PROGRAMS= $(SQPACK)

# List of libraries required to build binary file(s)
LIBS    =
