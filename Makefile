# pg_cgroup/Makefile

MODULE_big = pg_cgroup
OBJS = pg_cgroup.o
DATA = pg_cgroup--1.0.sql

EXTENSION = pg_cgroup

SHLIB_LINK += -lcgroup

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_cgroup
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
