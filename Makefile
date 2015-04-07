VERSION=0.1
CFLAGS=-g3 -Wall $(shell curl-config --cflags)
LIBS=-g3 $(shell curl-config --libs)

tc_client: client.o amfparser.o rtmp.o numlist.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

clean:
	rm -f client.o amfparser.o rtmp.o numlist.o tc_client

tarball:
	tar -cJf tc_client-$(VERSION).tar.xz --transform='s|^|tc_client-$(VERSION)/|' Makefile client.c amfparser.c rtmp.c numlist.c amfparser.h rtmp.h numlist.h LICENSE README
