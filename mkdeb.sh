#/bin/bash

mkdir -p foo/DEBIAN
mkdir -p foo/usr/bin
mkdir -p foo/opt/share/pshare/www
mkdir -p foo/opt/share/pshare/playlists

cp debian/control foo/DEBIAN/
cp pshare foo/usr/bin/
cp -r www/* foo/opt/share/pshare/www/
cp -r playlists/* foo/opt/share/pshare/playlists/

fakeroot -- dpkg -b foo debian-pkg.deb
dpkg-name -o debian-pkg.deb

rm -rf foo/
