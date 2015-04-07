#!/bin/sh
# Utility to make sure that changes did not break the project in certain configurations
make clean > /dev/null 2> /dev/null
printf "Without mic, with gtk+-2.x, with streaming, without RTMP_DEBUG: "
res="[31mbroken[0m"
while true; do
  ./configure > /dev/null 2> /dev/null || break
  sed -i -e '/^GTK_/d' config.mk
  echo "GTK_LIBS=`pkg-config --libs gtk+-2.0 2> /dev/null`" >> config.mk
  echo "GTK_CFLAGS=`pkg-config --cflags gtk+-2.0`" >> config.mk
  if ! grep -q 'GTK_LIBS=.*-lgtk-[^- ]*-2' config.mk; then res="gtk+-2.x not found, can't test"; break; fi
  if ! grep -q '^AVCODEC_LIBS' config.mk; then res="libavcodec not found, can't test"; break; fi
  if ! grep -q '^SWSCALE_LIBS' config.mk; then res="libswscale not found, can't test"; break; fi
  if ! grep -q '^LIBV4L2_LIBS' config.mk; then res="libv4l2 not found, can't test"; break; fi
  echo 'CFLAGS+=-Werror' >> config.mk
  make utils > /dev/null 2> /dev/null || break
  res="[32mworks[0m"
  break
done
echo "$res"

make clean > /dev/null 2> /dev/null
printf "With mic, with gtk+-3.x, without streaming, with RTMP_DEBUG: "
res="[31mbroken[0m"
while true; do
  ENABLE_MIC=1 ./configure > /dev/null 2> /dev/null || break
  if ! grep -q 'GTK_LIBS=.*-lgtk-3' config.mk; then res="gtk+-3.x not found, can't test"; break; fi
  if ! grep -q '^AO_LIBS' config.mk; then res="libao not found, can't test"; break; fi
  if ! grep -q 'RESAMPLE_LIBS' config.mk; then res="lib(av|sw)resample not found, can't test"; break; fi
  if ! grep -q '^AVCODEC_LIBS' config.mk; then res="libavcodec not found, can't test"; break; fi
  if ! grep -q '^SWSCALE_LIBS' config.mk; then res="libswscale not found, can't test"; break; fi
  sed -i -e '/^LIBV4L2_LIBS/d' config.mk
  echo 'CFLAGS+=-DRTMP_DEBUG=1 -Werror' >> config.mk
  make utils > /dev/null 2> /dev/null || break
  res="[32mworks[0m"
  break
done
echo "$res"
