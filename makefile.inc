# $Id$
#
# Common makefiles part (include-file) for sqpack

# Use defines before including this file:
# _OBJ	object files suffix begins with point (".o", ".obj", ... )
# _LIB	library files suffix begins with point (".a", ".lib", ... )
# _DLL	dinamic linking library files suffix begins with point (".dll", ".so", ...)
# LIBPREFIX  library filename prefix ("lib", "", ...)
# LIBSUFFIX  library filename suffix (compiler ID, three characters, somewhere empty)
# DLLPREFIX  dinamic linking library prefix (usually empty)
# DLLSUFFIX  dinamic linking library suffix (compiler ID or empty)
# _SRC_DIR  sources dir

LIBNAME = sqpack

# Version
VERMAJOR= 1
VERMINOR= 9
VERPATCH= 0
VERH	= $(VERMAJOR).$(VERMINOR)
VER	= $(VERH).$(VERPATCH)

# Object files (please sort list to easy check by human)
OBJS	= sqpack$(_OBJ)

# Header files
HEADERS	= cvsdate.h version.h

# Sources directory
_SRC_DIR= ./

# Headers directory
H_DIR	= ./

# Binary file(s) to build from sources
PROGRAMS= $(LIBNAME)$(_EXE)

# List of libraries required to build binary file(s)
LIBS	=
