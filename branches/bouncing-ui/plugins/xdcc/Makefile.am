CFLAGS+= -Wimplicit -Wstrict-prototypes -Werror -shared

libdir = $(ragedir)/plugins

lib_LTLIBRARIES = xdcc.la
xdcc_la_SOURCES = xdcc.c
xdcc_la_LDFLAGS = -avoid-version -module 
INCLUDES = $(COMMON_CFLAGS) -I$(srcdir)/..

