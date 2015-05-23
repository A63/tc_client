VERSION=0.32pre
CFLAGS=-g3 -Wall $(shell curl-config --cflags)
LIBS=-g3 $(shell curl-config --libs)
PREFIX=/usr/local
ifneq ($(wildcard config.mk),)
  include config.mk
endif
OBJ=client.o amfparser.o rtmp.o numlist.o amfwriter.o idlist.o colors.o endian.o media.o
IRCHACK_OBJ=utilities/irchack/irchack.o utilities/compat.o
MODBOT_OBJ=utilities/modbot/modbot.o utilities/list.o utilities/modbot/queue.o
CAMVIEWER_OBJ=utilities/camviewer/camviewer.o
CURSEDCHAT_OBJ=utilities/cursedchat/cursedchat.o utilities/cursedchat/buffer.o utilities/compat.o utilities/list.o
TC_CLIENT_GTK_OBJ=utilities/gtk/camviewer.o utilities/gtk/userlist.o utilities/gtk/media.o utilities/gtk/compat.o utilities/gtk/config.o utilities/gtk/gui.o utilities/stringutils.o utilities/gtk/logging.o
UTILS=irchack modbot
ifdef GTK_LIBS
ifdef AVCODEC_LIBS
ifdef AVUTIL_LIBS
ifdef SWSCALE_LIBS
  UTILS+=camviewer tc_client-gtk
  CFLAGS+=$(GTK_CFLAGS) $(AVCODEC_CFLAGS) $(AVUTIL_CFLAGS) $(SWSCALE_CFLAGS)
  INSTALLDEPS+=tc_client-gtk gtkgui.glade
  ifdef AO_LIBS
    ifdef AVRESAMPLE_LIBS
      CFLAGS+=-DHAVE_AVRESAMPLE=1 $(AVRESAMPLE_CFLAGS) $(AO_CFLAGS)
    endif
    ifdef SWRESAMPLE_LIBS
      CFLAGS+=-DHAVE_SWRESAMPLE=1 $(SWRESAMPLE_CFLAGS) $(AO_CFLAGS)
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
  INSTALLDEPS+=cursedchat
endif
endif
CFLAGS+=-DPREFIX=\"$(PREFIX)\"
INSTALLDEPS=tc_client

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

tc_client-gtk: $(TC_CLIENT_GTK_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) $(GTK_LIBS) $(AVCODEC_LIBS) $(AVUTIL_LIBS) $(SWSCALE_LIBS) $(AVRESAMPLE_LIBS) $(SWRESAMPLE_LIBS) $(AO_LIBS) $(LIBV4L2_LIBS) -o $@

clean:
	rm -f $(OBJ) $(IRCHACK_OBJ) $(MODBOT_OBJ) $(CAMVIEWER_OBJ) $(CURSEDCHAT_OBJ) $(TC_CLIENT_GTK_OBJ) tc_client irchack modbot camviewer cursedchat tc_client-gtk

SOURCES=Makefile client.c amfparser.c rtmp.c numlist.c amfwriter.c idlist.c colors.c endian.c media.c amfparser.h rtmp.h numlist.h amfwriter.h idlist.h colors.h endian.h media.h LICENSE README ChangeLog crossbuild.sh testbuilds.sh configure
SOURCES+=utilities/irchack/irchack.c
SOURCES+=utilities/modbot/modbot.c utilities/modbot/queue.c utilities/modbot/queue.h utilities/modbot/commands.html
SOURCES+=utilities/camviewer/camviewer.c
SOURCES+=utilities/cursedchat/cursedchat.c utilities/cursedchat/buffer.c utilities/cursedchat/buffer.h
SOURCES+=utilities/gtk/camviewer.c utilities/gtk/userlist.c utilities/gtk/media.c utilities/gtk/compat.c utilities/gtk/config.c utilities/gtk/gui.c utilities/gtk/logging.c utilities/gtk/userlist.h utilities/gtk/media.h utilities/gtk/compat.h utilities/gtk/config.h utilities/gtk/gui.h utilities/gtk/logging.h gtkgui.glade
SOURCES+=utilities/compat.c utilities/compat.h utilities/list.c utilities/list.h utilities/stringutils.c utilities/stringutils.h
tarball:
	tar -cJf tc_client-$(VERSION).tar.xz --transform='s|^|tc_client-$(VERSION)/|' $(SOURCES)

install: $(INSTALLDEPS)
	install -m 755 -D tc_client "$(PREFIX)/bin/tc_client"
ifdef GTK_LIBS
ifdef AVCODEC_LIBS
ifdef AVUTIL_LIBS
ifdef SWSCALE_LIBS
	install -m 755 -D tc_client-gtk "$(PREFIX)/bin/tc_client-gtk"
	install -D gtkgui.glade "$(PREFIX)/share/tc_client/gtkgui.glade"
endif
endif
endif
endif
ifdef CURSES_LIBS
ifdef READLINE_LIBS
	install -m 755 -D cursedchat "$(PREFIX)/bin/tc_client-cursedchat"
endif
endif
