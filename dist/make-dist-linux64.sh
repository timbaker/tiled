SRC=$(pwd)/..
BUILD=$SRC/../build-tiled-docker
APPIMAGE=$SRC/../linuxdeploy/TileZed-x86_64.AppImage
DESTROOT=$SRC/../dist-tiled-docker
DEST=$DESTROOT/TileZed

mkdir $DESTROOT
mkdir $DEST
cp -a $APPIMAGE $DEST
cp -ar $BUILD/share/ $DEST

cp -a $SRC/dist/TileZed-x86_64.AppImage.sh $DEST
chmod +x $DEST/TileZed-x86_64.AppImage.sh

