VERSION=0.22
CFLAGS=-g3 -Wall $(shell curl-config --cflags)
LIBS=-g3 $(shell curl-config --libs)
ifneq ($(wildcard config.mk),)
  include config.mk
endif
OBJ=client.o amfparser.o rtmp.o numlist.o amfwriter.o idlist.o colors.o endian.o media.o
IRCHACK_OBJ=utilities/irchack/irchack.o
MODBOT_OBJ=utilities/modbot/modbot.o utilities/modbot/list.o utilities/modbot/queue.o
CAMVIEWER_OBJ=utilities/camviewer/camviewer.o
UTILS=irchack modbot
ifdef GTK_LIBS
ifdef AVCODEC_LIBS
ifdef AVUTIL_LIBS
ifdef SWSCALE_LIBS
  UTILS+=camviewer
  CFLAGS+=$(GTK_CFLAGS) $(AVCODEC_CFLAGS) $(AVUTIL_CFLAGS) $(SWSCALE_CFLAGS)
endif
endif
endif
endif

tc_client: $(OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

utils: $(UTILS)

irchack: $(IRCHACK_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

modbot: $(MODBOT_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

camviewer: $(CAMVIEWER_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) $(GTK_LIBS) $(AVCODEC_LIBS) $(AVUTIL_LIBS) $(SWSCALE_LIBS) -o $@

clean:
	rm -f $(OBJ) $(IRCHACK_OBJ) $(MODBOT_OBJ) $(CAMVIEWER_OBJ) tc_client irchack modbot camviewer

tarball:
	tar -cJf tc_client-$(VERSION).tar.xz --transform='s|^|tc_client-$(VERSION)/|' Makefile client.c amfparser.c rtmp.c numlist.c amfwriter.c idlist.c colors.c endian.c media.c amfparser.h rtmp.h numlist.h amfwriter.h idlist.h colors.h endian.h media.h LICENSE README ChangeLog crossbuild.sh utilities/irchack/irchack.c utilities/modbot/modbot.c utilities/modbot/list.c utilities/modbot/list.h utilities/modbot/queue.c utilities/modbot/queue.h utilities/camviewer/camviewer.c configure
