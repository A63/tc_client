VERSION=0.28
CFLAGS=-g3 -Wall $(shell curl-config --cflags)
LIBS=-g3 $(shell curl-config --libs)
ifneq ($(wildcard config.mk),)
  include config.mk
endif
OBJ=client.o amfparser.o rtmp.o numlist.o amfwriter.o idlist.o colors.o endian.o media.o
IRCHACK_OBJ=utilities/irchack/irchack.o utilities/compat.o
MODBOT_OBJ=utilities/modbot/modbot.o utilities/list.o utilities/modbot/queue.o
CAMVIEWER_OBJ=utilities/camviewer/camviewer.o
CURSEDCHAT_OBJ=utilities/cursedchat/cursedchat.o utilities/cursedchat/buffer.o utilities/compat.o utilities/list.o
UTILS=irchack modbot
ifdef GTK_LIBS
ifdef AVCODEC_LIBS
ifdef AVUTIL_LIBS
ifdef SWSCALE_LIBS
  UTILS+=camviewer
  CFLAGS+=$(GTK_CFLAGS) $(AVCODEC_CFLAGS) $(AVUTIL_CFLAGS) $(SWSCALE_CFLAGS)
  ifdef AO_LIBS
    ifdef AVRESAMPLE_LIBS
      CFLAGS+=-DHAVE_SOUND=avresample $(AVRESAMPLE_CFLAGS) $(AO_CFLAGS)
    endif
    ifdef SWRESAMPLE_LIBS
      CFLAGS+=-DHAVE_SOUND=swresample $(SWRESAMPLE_CFLAGS) $(AO_CFLAGS)
    endif
  endif
  ifdef LIBV4L2_LIBS
    CFLAGS+=-DHAVE_V4L2 $(LIBV4L2_CFLAGS)
  endif
endif
endif
endif
endif
ifdef CURSES_LIBS
ifdef READLINE_LIBS
  UTILS+=cursedchat
  CFLAGS+=$(CURSES_CFLAGS) $(READLINE_CFLAGS)
endif
endif

tc_client: $(OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

utils: $(UTILS) tc_client

irchack: $(IRCHACK_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

modbot: $(MODBOT_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

camviewer: $(CAMVIEWER_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) $(GTK_LIBS) $(AVCODEC_LIBS) $(AVUTIL_LIBS) $(SWSCALE_LIBS) $(AVRESAMPLE_LIBS) $(SWRESAMPLE_LIBS) $(AO_LIBS) $(LIBV4L2_LIBS) -o $@

cursedchat: $(CURSEDCHAT_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) $(READLINE_LIBS) $(CURSES_LIBS) -o $@

clean:
	rm -f $(OBJ) $(IRCHACK_OBJ) $(MODBOT_OBJ) $(CAMVIEWER_OBJ) $(CURSEDCHAT_OBJ) tc_client irchack modbot camviewer cursedchat

tarball:
	tar -cJf tc_client-$(VERSION).tar.xz --transform='s|^|tc_client-$(VERSION)/|' Makefile client.c amfparser.c rtmp.c numlist.c amfwriter.c idlist.c colors.c endian.c media.c amfparser.h rtmp.h numlist.h amfwriter.h idlist.h colors.h endian.h media.h LICENSE README ChangeLog crossbuild.sh utilities/irchack/irchack.c utilities/modbot/modbot.c utilities/modbot/queue.c utilities/modbot/queue.h utilities/modbot/commands.html utilities/camviewer/camviewer.c utilities/cursedchat/cursedchat.c utilities/cursedchat/buffer.c utilities/cursedchat/buffer.h utilities/compat.c utilities/compat.h utilities/list.c utilities/list.h configure
