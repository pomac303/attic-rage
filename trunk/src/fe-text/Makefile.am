CFLAGS+= -Wimplicit -Wstrict-prototypes -Werror

bin_PROGRAMS = xchat-text

EXTRA_DIST = README 

INCLUDES = $(COMMON_CFLAGS) -I $(top_srcdir)/src/common/

xchat_text_LDADD = ../common/libxchatcommon.a $(COMMON_LIBS)
xchat_text_SOURCES = fe-text.c fe-text.h

