# temp. solution while mailcheck & xdcc arn't in SUBDIRS

CFLAGS+= -Wimplicit -Wstrict-prototypes

EXTRA_DIST = plugin20.html Make.plugin plugin-conf.in \
				mailcheck/mailcheck.c \
				mailcheck/mailcheck-config.h.in \
				mailcheck/Makefile.am \
				xdcc/xdcc.c \
				xdcc/Makefile.am

if DO_PYTHON
pythondir = python
endif

if DO_PERL
perldir = perl
endif

if DO_TCL
tcldir = tcl
endif

if DO_RUBY
rubydir = ruby
endif

SUBDIRS = . $(pythondir) $(perldir) $(tcldir) $(rubydir)
