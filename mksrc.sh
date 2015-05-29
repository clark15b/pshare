#!/bin/bash

mkdir -p src/
cp -r ipkg/ src/
cp -r debian/ src/
cp -r playlists src/
cp -r www src/
cp mkdeb.sh src/
cp mkipkg.sh src/
cp ChangeList.txt src/
cp LICENSE src/
cp README src/
cp Makefile src/
cp *.cpp src/
cp *.h src/

tar c src | gzip -c > pshare_0.0.2_src.tar.gz
rm -rf src/