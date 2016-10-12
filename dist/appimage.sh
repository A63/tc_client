#!/bin/sh
# This script has been tested on xubuntu 14.04, the resulting appimage has been successfully tested on:
# xubuntu 14.04
# xubuntu 16.04
# ubuntu 14.04
# archbang 100816

# Build our own recent and minimal ffmpeg. Distro-provided ffmpeg is old (because we build appimage on an old distro for backward-compatibility) and buggy, caused segfaults
rootdir="`pwd`"
mkdir -p deps/src
(
  cd deps/src
  wget -c https://www.ffmpeg.org/releases/ffmpeg-3.1.4.tar.xz
  tar -xJf ffmpeg-3.1.4.tar.xz
  cd ffmpeg-3.1.4
  mkdir -p build
  cd build
  ../configure --prefix="${rootdir}/deps" --enable-gpl --disable-programs --disable-doc --disable-htmlpages --disable-manpages --disable-podpages --disable-txtpages --disable-avdevice --disable-avformat --disable-postproc --disable-avfilter --disable-encoders --enable-encoder="flv" --disable-decoders --enable-decoder="flv" --disable-parsers --disable-bsfs --disable-devices --disable-sdl --disable-libxcb --disable-xlib --enable-static --disable-shared
  make
  make install
)
export PKG_CONFIG_PATH="`pwd`/deps/lib/pkgconfig"
# Then build tc_client
./configure --prefix='.'
make utils || exit
make install PREFIX="appdir/usr" || exit
strip appdir/usr/bin/*
cat > appdir/AppRun << 'END'
#!/bin/sh
HERE="`readlink -f "$0"`"
HERE="`dirname "$HERE"`"
# echo "$HERE"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
cd "${HERE}/usr"
bin/tc_client-gtk
END
chmod +x appdir/AppRun
cat > appdir/app.desktop << 'END'
[Desktop Entry]
Name=tc_client
Exec=usr/bin/tc_client-gtk
Icon=tc_client
END
convert -font /usr/share/fonts/truetype/*/DejaVuSans.ttf -size 32x32 'label:tc_c\nlient' appdir/tc_client.png
# Find library dependencies for the executables
mkdir -p appdir/usr/lib
ldd appdir/usr/bin/* | while read dep; do
  if ! echo "$dep" | grep -q ' => '; then continue;fi
  name="`echo "$dep" | sed -e 's/ => .*//; s/^[\t ]*//'`"
  path="`echo "$dep" | sed -e 's/.* => //; s/ (.*//'`"
  # Exclude stable ABI libraries
  case "$name" in
    libc.so.*|\
    libdl.so.*|\
    libm.so.*|\
    libz.so.*|\
    libresolv.so.*|\
    libpthread.so.*|\
    libfontconfig.so.*|\
    libfreetype.so.*|\
    libharfbuzz.so.*|\
    libffi.so.*|\
    libexpat.so.*|\
    libgdk_pixbuf-2.0.so.*|\
    libX*|\
    libxcb*|\
    libSM.so.*|\
    libICE.so.*|\
    libpango-1.0.so.*|\
    libpangocairo-1.0.so.*|\
    libpangoft2-1.0.so.*|\
    libatk-1.0.so.*|\
    libatk-bridge-2.0.so.*|\
    libatspi.so.*|\
    libgdk-3.so.*|\
    libgtk-3.so.*|\
    libgio-2.0.so.*|\
    libgobject-2.0.so.*|\
    libglib-2.0.so.*|\
    libgthread-2.0.so.*|\
    libgmodule-2.0.so.*|\
    libv4l2.so.*|\
    libv4lconvert.so.*|\
    libcairo.so.*|\
    libcairo-gobject.so.*|\
    libssl.so.*|\
    libcrypto.so.*|\
    libxkbcommon.so.*|\
    libgpg-error.so.*) continue;;
  esac
  cp -L "$path" appdir/usr/lib
done
version="`sed -n -e 's/^VERSION=//p' Makefile`"
arch="`uname -m | sed -e 's/^i[3-7]86$/x86/'`"
AppImageAssistant appdir "tc_client-${version}-gnulinux-${arch}.appimage"
