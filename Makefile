VERSION=0.41pre
CFLAGS=-g3 -Wall -I. $(shell curl-config --cflags)
LDFLAGS=-g3
PREFIX=/usr/local
CURL_LIBS=$(shell curl-config --libs)
ifneq ($(wildcard config.mk),)
  include config.mk
else
  ifneq ($(findstring MINGW,$(shell uname -s)),)
    err:
	@echo 'Run ./configure first, make sure tc_client-gtk is enabled.'
  endif
endif
OBJ=client.o amfparser.o rtmp.o numlist.o amfwriter.o idlist.o colors.o endianutils.o media.o utilities/compat.o
IRCHACK_OBJ=utilities/irchack/irchack.o utilities/compat.o
MODBOT_OBJ=utilities/modbot/modbot.o utilities/list.o utilities/modbot/queue.o utilities/compat.o
CAMVIEWER_OBJ=utilities/camviewer/camviewer.o utilities/compat.o utilities/compat_av.o libcamera.a
CURSEDCHAT_OBJ=utilities/cursedchat/cursedchat.o utilities/cursedchat/buffer.o utilities/compat.o utilities/list.o
TC_CLIENT_GTK_OBJ=utilities/gtk/camviewer.o utilities/gtk/userlist.o utilities/gtk/media.o utilities/gtk/compat.o utilities/gtk/configfile.o utilities/gtk/gui.o utilities/stringutils.o utilities/gtk/logging.o utilities/gtk/postproc.o utilities/compat.o utilities/compat_av.o utilities/gtk/inputhistory.o utilities/gtk/playmedia.o libcamera.a
LIBCAMERA_OBJ=utilities/libcamera/camera.o utilities/libcamera/camera_img.o
UTILS=irchack modbot
CONFINFO=|Will enable the IRC utility irchack|Will enable the bot utility modbot
ifdef GTK_LIBS
ifdef AVCODEC_LIBS
ifdef AVUTIL_LIBS
ifdef SWSCALE_LIBS
  CONFINFO+=|Will enable the graphical utilities tc_client-gtk and camviewer
  UTILS+=camviewer tc_client-gtk
  CFLAGS+=$(GTK_CFLAGS) $(AVCODEC_CFLAGS) $(AVUTIL_CFLAGS) $(SWSCALE_CFLAGS)
  INSTALLDEPS+=tc_client-gtk gtkgui.glade
  ifdef AO_LIBS
    ifdef AVRESAMPLE_LIBS
      CONFINFO+=|  Will enable incoming mic support
      CFLAGS+=-DHAVE_LIBAO=1 -DHAVE_AVRESAMPLE=1 $(AVRESAMPLE_CFLAGS) $(AO_CFLAGS)
    endif
    ifdef SWRESAMPLE_LIBS
      CONFINFO+=|  Will enable incoming mic support
      CFLAGS+=-DHAVE_LIBAO=1 -DHAVE_SWRESAMPLE=1 $(SWRESAMPLE_CFLAGS) $(AO_CFLAGS)
    endif
    ifndef AVRESAMPLE_LIBS
    ifndef SWRESAMPLE_LIBS
      CONFINFO+=|  Incoming mic support will not be enabled
    endif
    endif
  endif
  ifdef AVFORMAT_LIBS
    CFLAGS+=-DHAVE_AVFORMAT=1
    CONFINFO+=|  Will enable integrated youtube videos
  endif
  ifdef PULSE_LIBS
    CONFINFO+=|  Will enable outgoing mic support
    CFLAGS+=-DHAVE_PULSEAUDIO=1 $(PULSE_CFLAGS)
  else
    CONFINFO+=|  Outgoing mic support will not be enabled
  endif
  ifdef LIBV4L2_LIBS
    CONFINFO+=|  Will enable v4l2 camera support
    CFLAGS+=-DHAVE_V4L2 $(LIBV4L2_CFLAGS)
    LIBCAMERA_OBJ+=utilities/libcamera/camera_v4l2.o
  else
    CONFINFO+=|  v4l2 camera support will not be enabled
  endif
  ifdef LIBX11_LIBS
    CONFINFO+=|  Will enable X11 virtual camera support
    CFLAGS+=-DHAVE_X11 $(LIBX11_CFLAGS)
    LIBCAMERA_OBJ+=utilities/libcamera/camera_x11.o
  endif
  ifneq ($(findstring MINGW,$(shell uname -s)),)
    ifeq ($(findstring mingw,$(shell $(CC) -v 2>&1 | grep Target)),)
    err:
	@echo "Error, you're in a mingw shell but $(CC) doesn't seem to be a mingw compiler"
    endif
    LDFLAGS+=-mwindows
    # Using ESCAPI for cam support on windows, http://sol.gfxile.net/escapi/
    ifneq ($(wildcard escapi),)
      CONFINFO+=|  Will enable escapi windows camera support
      CFLAGS+=-DHAVE_ESCAPI
      LIBCAMERA_OBJ+=utilities/libcamera/camera_escapi.o escapi/escapi.o
    else
      CONFINFO+=|  escapi windows camera support will not be enabled
    endif
    windowstargets: camviewer tc_client-gtk
	@echo
	@echo 'To build the core (tc_client.exe), enter this directory from cygwin (or MSYS2 non-MinGW shell) and type make'
  endif
  ifneq ($(findstring MSYS,$(shell uname -s)),)
    ifeq ($(findstring msys,$(shell $(CC) -v 2>&1 | grep Target)),)
    err:
	@echo "Error, you're in an msys shell but $(CC) doesn't seem to be an msys compiler"
    endif
    msystargets: tc_client
	@echo
	@echo 'To build the gtk+ GUI, enter this directory from a MinGW shell and type make'
  endif
endif
endif
endif
endif
ifdef CURSES_LIBS
ifdef READLINE_LIBS
  CONFINFO+=|Will enable the curses utility cursedchat
  UTILS+=cursedchat
  CFLAGS+=$(CURSES_CFLAGS) $(READLINE_CFLAGS)
  INSTALLDEPS+=cursedchat
endif
endif
ifeq ($(AR),)
  AR=ar
endif
ifeq ($(RANLIB),)
  RANLIB=ranlib
endif
CFLAGS+=-DPREFIX=\"$(PREFIX)\" -DVERSION=\"$(VERSION)\"
INSTALLDEPS=tc_client

tc_client: $(OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) $(CURL_LIBS) -o $@
# Make sure client.o gets rebuilt if we change the version number in the Makefile
client.o: Makefile

utils: $(UTILS) tc_client

irchack: $(IRCHACK_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

modbot: $(MODBOT_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) $(CURL_LIBS) -o $@

camviewer: $(CAMVIEWER_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) $(GTK_LIBS) $(AVCODEC_LIBS) $(AVUTIL_LIBS) $(SWSCALE_LIBS) $(AVRESAMPLE_LIBS) $(SWRESAMPLE_LIBS) $(AO_LIBS) $(LIBV4L2_LIBS) $(LIBX11_LIBS) -o $@

cursedchat: $(CURSEDCHAT_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) $(READLINE_LIBS) $(CURSES_LIBS) -o $@

tc_client-gtk: $(TC_CLIENT_GTK_OBJ) camplaceholder.gif modicon.png
	$(CC) $(LDFLAGS) $(TC_CLIENT_GTK_OBJ) $(LIBS) $(GTK_LIBS) $(AVCODEC_LIBS) $(AVUTIL_LIBS) $(SWSCALE_LIBS) $(AVRESAMPLE_LIBS) $(SWRESAMPLE_LIBS) $(AVFORMAT_LIBS) $(AO_LIBS) $(LIBV4L2_LIBS) $(LIBX11_LIBS) $(PULSE_LIBS) -o $@

camplaceholder.gif: utilities/gtk/gencamplaceholder.sh utilities/gtk/camplaceholder.xcf utilities/gtk/spinnerdot.xcf
	utilities/gtk/gencamplaceholder.sh

modicon.png: $(SRCDIR)utilities/gtk/modicon.xcf
	convert -background none $< -layers Merge -scale x20 $@

libcamera.a: $(LIBCAMERA_OBJ)
	$(AR) cru $@ $^
	$(RANLIB) $@

clean:
	rm -f $(OBJ) $(IRCHACK_OBJ) $(MODBOT_OBJ) $(CAMVIEWER_OBJ) $(CURSEDCHAT_OBJ) $(TC_CLIENT_GTK_OBJ) $(LIBCAMERA_OBJ) tc_client irchack modbot camviewer cursedchat tc_client-gtk camplaceholder.gif

SOURCES=Makefile client.c amfparser.c rtmp.c numlist.c amfwriter.c idlist.c colors.c endianutils.c media.c amfparser.h rtmp.h numlist.h amfwriter.h idlist.h colors.h endianutils.h media.h LICENSE README ChangeLog crossbuild.sh testbuilds.sh configure
SOURCES+=utilities/irchack/irchack.c
SOURCES+=utilities/modbot/modbot.c utilities/modbot/queue.c utilities/modbot/queue.h utilities/modbot/commands.html
SOURCES+=utilities/camviewer/camviewer.c
SOURCES+=utilities/cursedchat/cursedchat.c utilities/cursedchat/buffer.c utilities/cursedchat/buffer.h
SOURCES+=utilities/gtk/camviewer.c utilities/gtk/userlist.c utilities/gtk/media.c utilities/gtk/compat.c utilities/gtk/configfile.c utilities/gtk/gui.c utilities/gtk/logging.c utilities/gtk/postproc.c utilities/gtk/inputhistory.c utilities/gtk/playmedia.c utilities/gtk/main.h utilities/gtk/userlist.h utilities/gtk/media.h utilities/gtk/compat.h utilities/gtk/configfile.h utilities/gtk/gui.h utilities/gtk/logging.h utilities/gtk/postproc.h utilities/gtk/inputhistory.h utilities/gtk/playmedia.h gtkgui.glade
SOURCES+=utilities/gtk/gencamplaceholder.sh utilities/gtk/camplaceholder.xcf utilities/gtk/spinnerdot.xcf utilities/gtk/modicon.xcf
SOURCES+=utilities/compat.c utilities/compat.h utilities/list.c utilities/list.h utilities/stringutils.c utilities/stringutils.h utilities/compat_av.c utilities/compat_av.h
SOURCES+=utilities/libcamera/camera.c utilities/libcamera/camera.h utilities/libcamera/camera_v4l2.c utilities/libcamera/camera_v4l2.h utilities/libcamera/camera_img.c utilities/libcamera/camera_img.h utilities/libcamera/camera_escapi.cpp utilities/libcamera/camera_escapi.h utilities/libcamera/camera_x11.c utilities/libcamera/camera_x11.h
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
	install -D camplaceholder.gif "$(PREFIX)/share/tc_client/camplaceholder.gif"
	install -D modicon.png "$(PREFIX)/share/tc_client/modicon.png"
endif
endif
endif
endif
ifdef CURSES_LIBS
ifdef READLINE_LIBS
	install -m 755 -D cursedchat "$(PREFIX)/bin/tc_client-cursedchat"
endif
endif

CONFINFO+=|
ifeq ($(findstring tc_client-gtk,$(UTILS)),)
  CONFINFO+=|The graphical utilities tc_client-gtk and camviewer will not be enabled
endif
ifeq ($(findstring cursedchat,$(UTILS)),)
  CONFINFO+=|The curses utility cursedchat will not be enabled
endif
confinfo:
	@echo "$(CONFINFO)" | tr '|' '\n'
