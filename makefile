CC   = gcc
COPT = -g -O2 -DUNIX -I../smapi -I../fidoconfig
INSTBINDIR = /usr/local/bin
INSTMANDIR = /usr/local/man

all : sqpack \
      install

sqpack: sqpack.c
	$(CC) $(COPT) sqpack.c -o sqpack -lsmapilnx -lfidoconfig

sqpack.1.gz : sqpack.1
	gzip -c sqpack.1 > sqpack.1.gz

install: sqpack sqpack.1.gz
	install -s sqpack $(INSTBINDIR)
	install sqpack.1.gz $(INSTMANDIR)/man1
