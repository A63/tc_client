#!/bin/sh
# Utility to make sure that changes did not break the project in certain configurations
rm -rf testbuilds
mkdir -p testbuilds
cd testbuilds

printf "Without mic support, with gtk+-2.x, without RTMP_DEBUG, without youtube support, with glib: "
res="[31mbroken[0m"
while true; do
  ../configure > /dev/null 2> /dev/null || break
  sed -i -e '/^GTK_/d' config.mk
  echo "GTK_LIBS=`pkg-config --libs gtk+-2.0 2> /dev/null`" >> config.mk
  echo "GTK_CFLAGS=`pkg-config --cflags gtk+-2.0`" >> config.mk
  if ! grep -q 'GTK_LIBS=.*-lgtk-[^- ]*-2' config.mk; then res="gtk+-2.x not found, can't test"; break; fi
  if ! grep -q '^AVCODEC_LIBS' config.mk; then res="libavcodec not found, can't test"; break; fi
  if ! grep -q '^SWSCALE_LIBS' config.mk; then res="libswscale not found, can't test"; break; fi
  if ! grep -q '^GLIB_LIBS' config.mk; then res="glib not found, can't test"; break; fi
  sed -i -e '/^AO_LIBS/d' config.mk
  sed -i -e '/^PULSE_LIBS/d' config.mk
  sed -i -e '/^HAVE_AO/d' config.mk
  sed -i -e '/^HAVE_PULSE/d' config.mk
  sed -i -e '/^AVFORMAT_LIBS/d' config.mk
  echo 'CFLAGS+=-Werror' >> config.mk
  if ! make utils > /dev/null 2> /dev/null; then
    sed -i -e 's/-Werror[^ ]*//g' config.mk
    make utils > /dev/null 2> /dev/null || break
    res="[33mworks but with warnings[0m"
    break
  fi
  res="[32mworks[0m"
  break
done
echo "$res"
mv camviewer camviewer.gtk2 > /dev/null 2> /dev/null
mv tc_client-gtk tc_client-gtk.gtk2 > /dev/null 2> /dev/null

make clean > /dev/null 2> /dev/null
printf "With mic support, with gtk+-3.x, with RTMP_DEBUG, with youtube support, without glib: "
res="[31mbroken[0m"
while true; do
  ../configure > /dev/null 2> /dev/null || break
  if ! grep -q 'GTK_LIBS=.*-lgtk-3' config.mk; then res="gtk+-3.x not found, can't test"; break; fi
  if ! grep -q '^AO_LIBS' config.mk; then res="libao not found, can't test"; break; fi
  if ! grep -q 'RESAMPLE_LIBS' config.mk; then res="lib(av|sw)resample not found, can't test"; break; fi
  if ! grep -q '^AVCODEC_LIBS' config.mk; then res="libavcodec not found, can't test"; break; fi
  if ! grep -q '^SWSCALE_LIBS' config.mk; then res="libswscale not found, can't test"; break; fi
  if ! grep -q '^AVFORMAT_LIBS' config.mk; then res="libavformat not found, can't test"; break; fi
  sed -i -e '/GLIB_/d' config.mk
  echo 'CFLAGS+=-DRTMP_DEBUG=1 -Werror' >> config.mk
  if ! make utils > /dev/null 2> /dev/null; then
    sed -i -e 's/-Werror[^ ]*//g' config.mk
    make utils > /dev/null 2> /dev/null || break
    res="[33mworks but with warnings[0m"
    break
  fi
  res="[32mworks[0m"
  break
done
echo "$res"
