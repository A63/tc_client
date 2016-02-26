VERSION=0.38pre
CFLAGS=-g3 -Wall $(shell curl-config --cflags)
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
OBJ=client.o amfparser.o rtmp.o numlist.o amfwriter.o idlist.o colors.o endian.o media.o
IRCHACK_OBJ=utilities/irchack/irchack.o utilities/compat.o
MODBOT_OBJ=utilities/modbot/modbot.o utilities/list.o utilities/modbot/queue.o
CAMVIEWER_OBJ=utilities/camviewer/camviewer.o utilities/compat.o libcamera.a
CURSEDCHAT_OBJ=utilities/cursedchat/cursedchat.o utilities/cursedchat/buffer.o utilities/compat.o utilities/list.o
TC_CLIENT_GTK_OBJ=utilities/gtk/camviewer.o utilities/gtk/userlist.o utilities/gtk/media.o utilities/gtk/compat.o utilities/gtk/config.o utilities/gtk/gui.o utilities/stringutils.o utilities/gtk/logging.o utilities/compat.o utilities/gtk/inputhistory.o libcamera.a
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
      CONFINFO+=|  Will enable experimental mic support
      CFLAGS+=-DHAVE_AVRESAMPLE=1 $(AVRESAMPLE_CFLAGS) $(AO_CFLAGS)
    endif
    ifdef SWRESAMPLE_LIBS
      CONFINFO+=|  Will enable experimental mic support
      CFLAGS+=-DHAVE_SWRESAMPLE=1 $(SWRESAMPLE_CFLAGS) $(AO_CFLAGS)
    endif
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
    windowstargets: camviewer tc_client-gtk tc_client-gtk-camthread
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
  ifdef LIBV4L2_LIBS
    CONFINFO+=|  Will enable v4l2 camera support
    CFLAGS+=-DHAVE_V4L2 $(LIBV4L2_CFLAGS)
    LIBCAMERA_OBJ+=utilities/libcamera/camera_v4l2.o
  else
    CONFINFO+=|  v4l2 camera support will not be enabled
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
	$(CC) $(LDFLAGS) $^ $(LIBS) $(GTK_LIBS) $(AVCODEC_LIBS) $(AVUTIL_LIBS) $(SWSCALE_LIBS) $(AVRESAMPLE_LIBS) $(SWRESAMPLE_LIBS) $(AO_LIBS) $(LIBV4L2_LIBS) -o $@

cursedchat: $(CURSEDCHAT_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) $(READLINE_LIBS) $(CURSES_LIBS) -o $@

tc_client-gtk: $(TC_CLIENT_GTK_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) $(GTK_LIBS) $(AVCODEC_LIBS) $(AVUTIL_LIBS) $(SWSCALE_LIBS) $(AVRESAMPLE_LIBS) $(SWRESAMPLE_LIBS) $(AO_LIBS) $(LIBV4L2_LIBS) -o $@

# Workaround for windows' lack of fork() and inflexibility of having or not having a terminal
utilities/gtk/camthread.gen.c: utilities/gtk/media.c
	sed -n -e '/^ *#/p' utilities/gtk/media.c > $@
	echo 'int main(int argc, char** argv){' >> $@
	echo 'avcodec_register_all();' >> $@
	echo 'AVCodec* vencoder=avcodec_find_encoder(AV_CODEC_ID_FLV1);' >> $@
	echo 'cam_img_filepicker=camselect_file;' >> $@
	echo 'gtk_init(&argc, &argv);' >> $@
	echo 'int campipe[]={0,1};' >> $@
	echo 'setmode(1, O_BINARY);' >> $@
	echo 'CAM* cam=cam_open(argv[1]);' >> $@
	echo 'unsigned int delay=atoi(argv[2]);' >> $@
	sed -n -e '/if(!camproc)$$/,/^  }/p' utilities/gtk/media.c | sed -e '1,3d' >> $@
	sed -n -e '/ camselect_file/,/^}/p' utilities/gtk/media.c >> $@
tc_client-gtk-camthread: utilities/gtk/camthread.gen.o utilities/compat.o libcamera.a
	$(CC) $^ $(LIBS) $(GTK_LIBS) $(AVCODEC_LIBS) $(AVUTIL_LIBS) $(SWSCALE_LIBS) $(AVRESAMPLE_LIBS) -o $@

libcamera.a: $(LIBCAMERA_OBJ)
	$(AR) cru $@ $^
	$(RANLIB) $@

clean:
	rm -f $(OBJ) $(IRCHACK_OBJ) $(MODBOT_OBJ) $(CAMVIEWER_OBJ) $(CURSEDCHAT_OBJ) $(TC_CLIENT_GTK_OBJ) $(LIBCAMERA_OBJ) tc_client irchack modbot camviewer cursedchat tc_client-gtk

SOURCES=Makefile client.c amfparser.c rtmp.c numlist.c amfwriter.c idlist.c colors.c endian.c media.c amfparser.h rtmp.h numlist.h amfwriter.h idlist.h colors.h endian.h media.h LICENSE README ChangeLog crossbuild.sh testbuilds.sh configure
SOURCES+=utilities/irchack/irchack.c
SOURCES+=utilities/modbot/modbot.c utilities/modbot/queue.c utilities/modbot/queue.h utilities/modbot/commands.html
SOURCES+=utilities/camviewer/camviewer.c
SOURCES+=utilities/cursedchat/cursedchat.c utilities/cursedchat/buffer.c utilities/cursedchat/buffer.h
SOURCES+=utilities/gtk/camviewer.c utilities/gtk/userlist.c utilities/gtk/media.c utilities/gtk/compat.c utilities/gtk/config.c utilities/gtk/gui.c utilities/gtk/logging.c utilities/gtk/inputhistory.c utilities/gtk/userlist.h utilities/gtk/media.h utilities/gtk/compat.h utilities/gtk/config.h utilities/gtk/gui.h utilities/gtk/logging.h utilities/gtk/inputhistory.h gtkgui.glade
SOURCES+=utilities/compat.c utilities/compat.h utilities/list.c utilities/list.h utilities/stringutils.c utilities/stringutils.h
SOURCES+=utilities/libcamera/camera.c utilities/libcamera/camera.h utilities/libcamera/camera_v4l2.c utilities/libcamera/camera_v4l2.h utilities/libcamera/camera_img.c utilities/libcamera/camera_img.h utilities/libcamera/camera_escapi.cpp utilities/libcamera/camera_escapi.h
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

CONFINFO+=|
ifeq ($(findstring tc_client-gtk,$(UTILS)),)
  CONFINFO+=|The graphical utilities tc_client-gtk and camviewer will not be enabled
endif
ifeq ($(findstring cursedchat,$(UTILS)),)
  CONFINFO+=|The curses utility cursedchat will not be enabled
endif
confinfo:
	@echo "$(CONFINFO)" | tr '|' '\n'
