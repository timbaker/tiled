SRC=$(pwd)/..
BUILD=$SRC/../build-tiled-docker
APPIMAGE=$SRC/../linuxdeploy-tiled/TileZed-x86_64.AppImage
DESTROOT=$SRC/../dist-tiled-docker
DEST=$DESTROOT/TileZed

mkdir -p $DESTROOT
mkdir -p $DEST
cp -a $APPIMAGE $DEST
cp -ar $BUILD/share/ $DEST

cp -a $SRC/dist/TileZed-x86_64.AppImage.sh $DEST
chmod +x $DEST/TileZed-x86_64.AppImage.sh

cp -a $SRC/AUTHORS $DEST
cp -a $SRC/COPYING $DEST
cp -a $SRC/LICENSE.APACHE $DEST
cp -a $SRC/LICENSE.BSD $DEST
cp -a $SRC/LICENSE.GPL $DEST
cp -a $SRC/LICENSE.QT6 $DEST

