VERSION=0.9
CFLAGS=-g3 -Wall $(shell curl-config --cflags)
LIBS=-g3 $(shell curl-config --libs)

tc_client: client.o amfparser.o rtmp.o numlist.o amfwriter.o idlist.o colors.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

clean:
	rm -f client.o amfparser.o rtmp.o numlist.o amfwriter.o idlist.o colors.o tc_client

tarball:
	tar -cJf tc_client-$(VERSION).tar.xz --transform='s|^|tc_client-$(VERSION)/|' Makefile client.c amfparser.c rtmp.c numlist.c amfwriter.c idlist.c colors.c amfparser.h rtmp.h numlist.h amfwriter.h idlist.h colors.h LICENSE README ChangeLog crossbuild.sh irchack.c
