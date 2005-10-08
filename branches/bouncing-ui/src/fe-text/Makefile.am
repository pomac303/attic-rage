CFLAGS+= -Wimplicit -Wstrict-prototypes -Werror

bin_PROGRAMS = rage-text

EXTRA_DIST = README

INCLUDES = $(COMMON_CFLAGS) -I $(top_srcdir)/src/common/

rage_text_LDADD = ../common/libragecommon.a $(COMMON_LIBS)
rage_text_SOURCES = fe-text.c fe-text.h

