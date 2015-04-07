VERSION=0.18
CFLAGS=-g3 -Wall $(shell curl-config --cflags)
LIBS=-g3 $(shell curl-config --libs)
ifneq ($(wildcard config.mk),)
  include config.mk
endif

tc_client: client.o amfparser.o rtmp.o numlist.o amfwriter.o idlist.o colors.o endian.o
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

utils: irchack modbot

clean:
	rm -f client.o amfparser.o rtmp.o numlist.o amfwriter.o idlist.o colors.o endian.o tc_client irchack modbot

tarball:
	tar -cJf tc_client-$(VERSION).tar.xz --transform='s|^|tc_client-$(VERSION)/|' Makefile client.c amfparser.c rtmp.c numlist.c amfwriter.c idlist.c colors.c endian.c amfparser.h rtmp.h numlist.h amfwriter.h idlist.h colors.h endian.h LICENSE README ChangeLog crossbuild.sh irchack.c modbot.c configure
