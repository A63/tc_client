VERSION=0.5
CFLAGS=-g3 -Wall $(shell curl-config --cflags)
LIBS=-g3 $(shell curl-config --libs)

tc_client: client.o amfparser.o rtmp.o numlist.o amfwriter.o idlist.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

clean:
	rm -f client.o amfparser.o rtmp.o numlist.o amfwriter.o idlist.o tc_client

tarball:
	tar -cJf tc_client-$(VERSION).tar.xz --transform='s|^|tc_client-$(VERSION)/|' Makefile client.c amfparser.c rtmp.c numlist.c amfwriter.c idlist.c amfparser.h rtmp.h numlist.h amfwriter.h idlist.h LICENSE README ChangeLog irchack.c
