## Process this file with automake to produce Makefile.in

CFLAGS+= -Wimplicit -Wstrict-prototypes

SUBDIRS = po intl src plugins

EXTRA_DIST = autogen.sh HACKING INSTALL.w32 faq.html rage.png \
		rage.desktop rage.spec config.h.w32 rage.ico

icon_DATA = rage.png
icondir = $(datadir)/pixmaps
ragelibdir = $(datadir)/plugins

util_DATA = rage.desktop
utildir = $(datadir)/applications
