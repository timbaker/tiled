#!/bin/bash
SRC=$(pwd)/..
BUILD=$(realpath $SRC/../Qt_6_11_1_for_macOS-Release/bin)
DESTROOT=$(realpath $SRC/../../ProjectZomboid)
DEST=$DESTROOT/TileZed

mkdir -p $DESTROOT
mkdir -p $DEST

rm -rf $DEST/TileZed.app
cp -a $BUILD/TileZed.app $DEST

# Add all necessary Qt libraries
~/Qt/6.11.1/macOS/bin/macdeployqt $DEST/TileZed.app

# Remove all symlinks, they won't work on Steam
find $DEST/TileZed.app/Contents -type l -delete

mv $DEST/TileZed.app/Contents/Frameworks/libtiled.1.0.0.dylib $DEST/TileZed.app/Contents/Frameworks/libtiled.1.dylib
mv $DEST/TileZed.app/Contents/Frameworks/libzlib1.1.0.0.dylib $DEST/TileZed.app/Contents/Frameworks/libzlib1.1.dylib

cp -a $SRC/AUTHORS $DEST
cp -a $SRC/COPYING $DEST
cp -a $SRC/LICENSE.APACHE $DEST
cp -a $SRC/LICENSE.BSD $DEST
cp -a $SRC/LICENSE.GPL $DEST
cp -a $SRC/LICENSE.QT6 $DEST


