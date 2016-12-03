#!/bin/sh
# Note: this script is written for msys2 and is unlikely to work with anything else
getdlls()
{
  objdump -p "$1" | sed -n -e 's/.*DLL Name: //p' | while read name; do
    if ! grep -q -x -F "$name" .handleddeps; then
      echo "$name" >> .handleddeps
      file="`which "$name"`"
      # Skip windows system files
      if echo "$file" | grep -q '^/[a-z]/'; then continue; fi
      if echo "$file" | grep -q '^/cygdrive/'; then continue; fi
      echo "$file"
      getdlls "$file"
    fi
  done
}
finddeps()
{
  rm -f .handleddeps
  touch .handleddeps
  getdlls "$1"
  rm -f .handleddeps
}

rm -rf escapi*
# Find and download the latest version of ESCAPI
curl -s 'http://sol.gfxile.net/zip/?C=M;O=D' | sed -n -e 's|.*<a [^>]*>\(escapi[^<]*\)<.*|http://sol.gfxile.net/zip/\1|p' | head -n 1 | xargs wget
mkdir escapi
(cd escapi; unzip ../escapi*.zip)
version="`sed -n -e 's/^VERSION=//p' Makefile`"
mkdir -p "tc_client-${version}-w32/bin"
here="`pwd`"

# Build the core first
export MSYSTEM='MSYS'
. /etc/profile
cd "$here"
./configure
# make tc_client modbot irchack cursedchat
make tc_client cursedchat CURL_LIBS='-lcurl' || exit 1 # (curl's pkg-config data seems to pull in unnecessary libraries)

# Erase objects used both in msys and mingw32
find . -name '*.o' -delete

# Then the GUI
export MSYSTEM='MINGW32'
. /etc/profile
cd "$here"
./configure
make || exit 1

cp *.exe gtkgui.glade "tc_client-${version}-w32/bin"
(cd "tc_client-${version}-w32/bin"
wget -c 'http://youtube-dl.org/downloads/latest/youtube-dl.exe')
mkdir -p "tc_client-${version}-w32/share/glib-2.0/schemas"
cp /mingw32/share/glib-2.0/schemas/gschemas.compiled "tc_client-${version}-w32/share/glib-2.0/schemas"
. /etc/profile
cd "$here"
(
  finddeps tc_client.exe
  finddeps tc_client-gtk.exe
  finddeps cursedchat.exe
) | while read file; do cp "$file" "tc_client-${version}-w32/bin"; done
cp escapi/bin/Win32/escapi.dll "tc_client-${version}-w32/bin"
echo "cd bin" > "tc_client-${version}-w32/TC Client.cmd"
echo "start tc_client-gtk" >> "tc_client-${version}-w32/TC Client.cmd"
# Some good defaults
echo 'youtubecmd: http://youtube.com/watch?v=%i' > "tc_client-${version}-w32/bin/config.txt"
echo 'youtuberadio_embed: True' >> "tc_client-${version}-w32/bin/config.txt"
zip -r "tc_client-${version}-w32.zip" "tc_client-${version}-w32"
