#!/bin/bash

VERSION="0.0.2rc2"

if test -z $1; then
    echo "$0 mipsel|ar71xx|..."
    exit 0
fi
    

mkdir -p foo/data/opt/bin
mkdir -p foo/data/opt/share/pshare/www
mkdir -p foo/data/opt/share/pshare/playlists

cp pshare foo/data/opt/bin/
cp -r www/* foo/data/opt/share/pshare/www/
cp -r playlists/* foo/data/opt/share/pshare/playlists/

awk '{gsub("ARCH", "'"$1"'", $0); print }' ipkg/control_tmpl | awk '{gsub("VER", "'"${VERSION}"'", $0); print }' > ipkg/control

tar -C ipkg -czf foo/control.tar.gz ./control
tar -C foo/data -czf foo/data.tar.gz ./opt
echo "2.0" > foo/debian-binary

rm -f ipkg/control
rm -rf foo/data

tar -C foo -cz ./debian-binary ./data.tar.gz ./control.tar.gz > pshare_${VERSION}_$1.ipk

rm -rf foo/
