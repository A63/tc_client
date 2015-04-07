#!/bin/sh -e
host="$1"
if [ "$host" = "" ]; then
  host="`cc -dumpmachine`"
  if [ "$host" = "" ]; then host='i386-pc-linux-gnu'; fi
  echo "No target host specified (argv[1]), defaulting to ${host}"
fi
./configure --host="$host"
here="`pwd`"
if [ ! -e curlprefix ]; then
  wget -c http://curl.haxx.se/download/curl-7.40.0.tar.bz2
  tar -xjf curl-7.40.0.tar.bz2
  cd curl-7.40.0
  mkdir -p build
  cd build
  ../configure --prefix="${here}/curlprefix" --host="$host" --enable-static --disable-shared --disable-gopher --disable-ftp --disable-tftp --disable-ssh --disable-telnet --disable-dict --disable-file --disable-imap --disable-pop3 --disable-smtp --disable-ldap --without-librtmp --disable-rtsp --without-ssl --disable-sspi --without-nss --without-gnutls --without-libidn
  make
  make install
  cd "$here"
fi
if grep -q 'LIBS+=-liconv' config.mk && [ ! -e iconvprefix ]; then
  wget -c http://ftp.gnu.org/gnu/libiconv/libiconv-1.14.tar.gz
  tar -xzf libiconv-1.14.tar.gz
  cd libiconv-1.14
  mkdir -p build
  cd build
  ../configure --prefix="${here}/iconvprefix" --host="`echo "$host" | sed -e 's/android/gnu/'`" --enable-static --disable-shared CC="${host}-gcc" # libiconv does not handle android well, so we pretend it's GNU and specify the compiler
  make
  make install
  cd "$here"
fi
export PATH="${here}/curlprefix/bin:${PATH}"
echo "CFLAGS+=-I${here}/iconvprefix/include" >> config.mk
echo "LDFLAGS+=-L${here}/iconvprefix/lib" >> config.mk
make
make utils
