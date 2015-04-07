#!/bin/sh -e
host="$1"
if [ "$host" = "" ]; then
  host="`cc -dumpmachine`"
  if [ "$host" = "" ]; then host='i386-pc-linux-gnu'; fi
  echo "No target host specified (argv[1]), defaulting to ${host}"
fi
here="`pwd`"
if false; then
wget -c http://curl.haxx.se/download/curl-7.39.0.tar.bz2
tar -xjf curl-7.39.0.tar.bz2
cd curl-7.39.0
mkdir -p build
cd build
../configure --prefix="${here}/curlprefix" --host="$host" --enable-static --disable-shared --disable-gopher --disable-ftp --disable-tftp --disable-ssh --disable-telnet --disable-dict --disable-file --disable-imap --disable-pop3 --disable-smtp --disable-ldap --without-librtmp --disable-rtsp --without-ssl --disable-sspi --without-nss --without-gnutls --without-libidn
make
make install
cd "$here"
fi
export PATH="${here}/curlprefix/bin:${PATH}"
# Select a crosscompiler (if we can find one)
if which "${host}-gcc" > /dev/null 2> /dev/null && [ "`which "${host}-gcc" 2> /dev/null`" != "" ]; then
  export CC="${host}-gcc"
elif which "${host}-clang" > /dev/null 2> /dev/null && [ "`which "${host}-clang" 2> /dev/null`" != "" ]; then
  export CC="${host}-clang"
elif which "${host}-cc" > /dev/null 2> /dev/null && [ "`which "${host}-cc" 2> /dev/null`" != "" ]; then
  export CC="${host}-cc"
fi
make
make irchack
