CC = gcc

CFLAGS = -Wall -Wimplicit -Wstrict-prototypes -Werror -O2 -g -I ../../src/common

OBJS= tbh.c

tbh.o:	
	$(CC) $(LDFLAGS) $(CFLAGS) -DSKIPCONFIG -shared -Wl,-soname,tbh.so -o tbh.so tbh.c -lc

clean:
	rm -f *.so

