#!/bin/sh -e
host="$1"
if [ "$host" = "" ]; then
  host="`cc -dumpmachine`"
  if [ "$host" = "" ]; then host='i386-pc-linux-gnu'; fi
  echo "No target host specified (argv[1]), defaulting to ${host}"
fi
here="`pwd`"
export PATH="${here}/curlprefix/bin:${here}/ncursesprefix/bin:${PATH}"
if [ ! -e ncursesprefix ]; then
  wget -c http://ftp.gnu.org/gnu/ncurses/ncurses-5.9.tar.gz
  tar -xzf ncurses-5.9.tar.gz
  cd ncurses-5.9
  mkdir build
  cd build
  # Some hackery to build ncursesw5-config, which seems to get disabled along with --disable-database
  ../configure --prefix="${here}/ncursesprefix" --host="${host}" --enable-static --disable-shared --with-termlib --enable-widec --without-cxx ac_cv_header_locale_h=no
  mv Makefile Makefile.tmp
  ../configure --prefix="${here}/ncursesprefix" --host="${host}" --enable-static --disable-shared --with-termlib --enable-widec --without-cxx --disable-database --with-fallbacks=linux ac_cv_header_locale_h=no
  mv Makefile.tmp Makefile
  make
  make install
  cd "$here"
  cp ncursesprefix/lib/libtinfow.a ncursesprefix/lib/libtinfo.a
fi
if [ ! -e readlineprefix ]; then
  wget -c http://ftp.gnu.org/gnu/readline/readline-6.3.tar.gz
  tar -xzf readline-6.3.tar.gz
  cd readline-6.3
  mkdir build
  cd build
  ../configure --prefix="${here}/readlineprefix" --host="${host}" --enable-static --disable-shared bash_cv_wcwidth_broken=no
  make
  make install
  cd "$here"
fi
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
./configure --host="$host" > config.log 2>&1
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
echo "CFLAGS+=-I${here}/iconvprefix/include" >> config.mk
echo "LDFLAGS+=-L${here}/iconvprefix/lib" >> config.mk
echo "READLINE_CFLAGS=-I${here}/readlineprefix/include" >> config.mk
echo "READLINE_LIBS=-L${here}/readlineprefix/lib -lreadline" >> config.mk
make
make utils
