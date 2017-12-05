# contrib/qaudit/Makefile

# This file contains generic rules to build many kinds of simple
# extension modules.  You only need to set a few variables and include
# this file, the rest will be done here.
#
# Use the following layout for your Makefile:
#
#   [variable assignments, see below]
#
#   PG_CONFIG = pg_config
#   PGXS := $(shell $(PG_CONFIG) --pgxs)
#   include $(PGXS)
#
#   [custom rules, rarely necessary]
#
# Set one of these three variables to specify what is built:
#
#   MODULES -- list of shared-library objects to be built from source files
#     with same stem (do not include library suffixes in this list)
#   MODULE_big -- a shared library to build from multiple source files
#     (list object files in OBJS)
#   PROGRAM -- an executable program to build (list object files in OBJS)
#
# The following variables can also be set:
#
#   EXTENSION -- name of extension (there must be a $EXTENSION.control file)
#   MODULEDIR -- subdirectory of $PREFIX/share into which DATA and DOCS files
#     should be installed (if not set, default is "extension" if EXTENSION
#     is set, or "contrib" if not)
#   DATA -- random files to install into $PREFIX/share/$MODULEDIR
#   DATA_built -- random files to install into $PREFIX/share/$MODULEDIR,
#     which need to be built first
#   DATA_TSEARCH -- random files to install into $PREFIX/share/tsearch_data
#   DOCS -- random files to install under $PREFIX/doc/$MODULEDIR
#   SCRIPTS -- script files (not binaries) to install into $PREFIX/bin
#   SCRIPTS_built -- script files (not binaries) to install into $PREFIX/bin,
#     which need to be built first
#   REGRESS -- list of regression test cases (without suffix)
#   REGRESS_OPTS -- additional switches to pass to pg_regress
#   NO_INSTALLCHECK -- don't define an installcheck target, useful e.g. if
#     tests require special configuration, or don't use pg_regress
#   EXTRA_CLEAN -- extra files to remove in 'make clean'
#   PG_CPPFLAGS -- will be added to CPPFLAGS
#   PG_LIBS -- will be added to PROGRAM link line
#   SHLIB_LINK -- will be added to MODULE_big link line
#   PG_CONFIG -- path to pg_config program for the PostgreSQL installation
#     to build against (typically just "pg_config" to use the first one in
#     your PATH)
#
# Better look at some of the existing uses for examples...

MODULE_big = pgsword
OBJS = pgsword.o rule.o tools.o

EXTENSION = pgsword
DATA = pgsword--1.0.sql
PGFILEDESC = "pgsword - Qunar PostgreSQL audit tools"

NO_INSTALLCHECK = 1

ifdef USE_PGXS
PG_CONFIG = /opt/pg101/bin/pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pgsword
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

