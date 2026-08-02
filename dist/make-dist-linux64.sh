SRC=$(pwd)/..
BUILD=$SRC/../build-tiled-docker
APPIMAGE=$SRC/../linuxdeploy-tiled/TileZed-x86_64.AppImage
DESTROOT=$SRC/../dist-tiled-docker
DEST=$DESTROOT/TileZed

mkdir $DESTROOT
mkdir $DEST
cp -a $APPIMAGE $DEST
cp -ar $BUILD/share/ $DEST

cp -a $SRC/dist/TileZed-x86_64.AppImage.sh $DEST
chmod +x $DEST/TileZed-x86_64.AppImage.sh

cp -a $SRC/LICENSE.AUTHORS.txt $DEST
cp -a $SRC/COPYING.txt $DEST
cp -a $SRC/LICENSE.APACHE.txt $DEST
cp -a $SRC/LICENSE.BSD.txt $DEST
cp -a $SRC/LICENSE.GPL.txt $DEST
cp -a $SRC/LICENSE.QT6 $DEST

