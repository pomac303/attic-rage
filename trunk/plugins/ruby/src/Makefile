RUBY=ruby
CC=gcc
CFLAGS=-g -Wall -Wimplicit -Werror -shared -O2 -fpic -I../../../src/common/

ARCHDIR=`$(RUBY) -rrbconfig -e "puts Config::CONFIG['archdir']"`
LIBARG=`$(RUBY) -rrbconfig -e "puts Config::CONFIG['LIBRUBYARG_SHARED']"`

.SUFFIXES: .c .o .rb .h
.PHONY: all clean

all: rage-ruby.so

.rb.h:
	$(RUBY) ../scripts/embedify.rb $*.rb $*.h

.c.o:
	$(CC) $(CFLAGS) -c -I$(ARCHDIR) $*.c -o $*.o

rage-ruby.so: rage-ruby.o rage-ruby-plugin.h
	$(CC) -L$(ARCHDIR) $(CFLAGS) -shared -o rage-ruby.so rage-ruby.o $(LIBARG)

install: rage-ruby.so
	mkdir -p ~/.rage
	cp rage-ruby.so ~/.rage
	$(RUBY) -e 'puts $$LOAD_PATH.join("\n")' > ~/.rage/rubyenv

clean:
	rm -f *.so *.o rage-ruby-plugin.h

distclean: clean

rage-ruby.o: rage-ruby-plugin.h
