## Process this file with automake to produce Makefile.in

CFLAGS+= -Wimplicit -Wstrict-prototypes -Werror

noinst_LIBRARIES = libragecommon.a

INCLUDES = $(COMMON_CFLAGS)

BUILT_SOURCES = rage-version.c

EXTRA_DIST = makefile.msc \
	cfgfiles.h \
	ctcp.h \
	dcc.h \
	fe.h \
	history.h \
	identd.c \
	ignore.h \
	inbound.h \
	inet.h \
	make-te.c \
	modes.h \
	network.h \
	notify.h \
	dict.h \
	set.h \
	outbound.h \
	parse.h \
	plugin.h \
	plugin-timer.h \
	proto-irc.h \
	server.h \
	servlist.h \
	ssl.h \
	ssl.c	\
	text.h \
	textenums.h \
	textevents.h \
	textevents.in \
	tree.h \
	url.h \
	userlist.h \
	util.h \
	rage.h \
	rage_data.h \
	ragec.h \
	rage-plugin.h

if USE_OPENSSL
ssl_c = ssl.c
endif

.PHONY: checkversion
checkversion:
	@SVN_VERSION=`svn info $(top_srcdir) | grep "^Revision: " | cut -d" " -f2 2>/dev/null`; \
	[ "z$$SVN_VERSION" != "z" ] || exit 0; \
	TMPFILE=`mktemp rage-version.c.XXXXXX` || exit 1; \
	echo "const char rage_svn_version[] = \"$$SVN_VERSION\";" >> $$TMPFILE; \
	if diff -q rage-version.c $$TMPFILE >/dev/null 2>&1; then \
		rm $$TMPFILE; \
	else \
		rm -f rage-version.c; \
		mv $$TMPFILE rage-version.c; \
	fi

rage-version.c: checkversion

libragecommon_a_SOURCES = cfgfiles.c ctcp.c dcc.c history.c ignore.c \
	inbound.c modes.c network.c notify.c dict.c set.c parse.c outbound.c \
	plugin.c plugin-timer.c proto-irc.c server.c servlist.c $(ssl_c) \
	text.c tree.c url.c userlist.c util.c rage.c rage-version.c

textevents.h: textevents
textenums.h: textevents

textevents: make-te textevents.in
	./make-te < textevents.in > textevents.h 2> textenums.h

plugin.o: $(srcdir)/plugin.c $(srcdir)/rage-plugin.h $(srcdir)/cfgfiles.h
	$(COMPILE) -DRAGELIBDIR=\"$(ragelibdir)\" -c $(srcdir)/plugin.c
