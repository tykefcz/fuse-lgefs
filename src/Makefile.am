MAINTAINERCLEANFILES = Makefile.in
AM_CFLAGS	= -Wall -I../include $(FUSE_CFLAGS)
bin_PROGRAMS	= lgefuse
noinst_PROGRAMS = lstmmdb tsthash

lgefuse_SOURCES	= \
	lgefuse.c \
	hashtab.c \
	lglib.c

lgefuse_LDADD = $(FUSE_LIBS)

lstmmdb_SOURCES = \
	lstmmdb.c \
	hashtab.c \
	lglib.c

tsthash_SOURCES = \
	tsthash.c \
	hashtab.c

