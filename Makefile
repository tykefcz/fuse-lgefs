CC=gcc
LD=gcc
FUSELIBS=`pkg-config fuse --libs`
#-pg
CFLAGS=-Wall `pkg-config fuse --cflags`
#-ggdb -pg
#-DDEBUG

all: lgefuse lstmmdb

%o: %c
	$(CC) $(CFLAGS) -c $< -o $@

lglib.o: lglib.c lglib.h endian.h Makefile

lgefuse.o: lgefuse.c lglib.h hashtab.h

lgefuse: lgefuse.o lglib.o hashtab.o
	$(LD) $(LDFLAGS) $^ -o $@ $(FUSELIBS)

lstmmdb: lstmmdb.o lglib.o hashtab.o

lstmmdb.o: lstmmdb.c lgmme.h hashtab.h 

hashtab.o: hashtab.c hashtab.h

lstmmdb: lstmmdb.o lglib.o hashtab.o
	$(LD) $(LDFLAGS) $^ -o $@

tsthash: tsthash.o hashtab.o

clean:
	rm -f *.o lgefuse lstmmdb tsthash
