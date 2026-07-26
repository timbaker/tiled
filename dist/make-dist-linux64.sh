QTVER=5.15.3
QTDIR_LIB=/usr/lib/x86_64-linux-gnu
QTDIR_PLUGINS=/usr/lib/x86_64-linux-gnu/qt5/plugins
SRC=~/Programming/tiled
BUILD=~/Programming/build-tiled-Desktop-Release
DESTROOT=~/Programming/TileZed
DEST=$DESTROOT/TileZed

mkdir $DESTROOT
mkdir $DEST
cp -avr $BUILD/bin/ $DEST
cp -ar $BUILD/share/ $DEST

mkdir $DEST/lib
cp -a $BUILD/lib/libtiled.so.1.0.0 $DEST/lib/libtiled.so.1
cp -a $BUILD/lib/libzlib1.so.1.0.0 $DEST/lib/libzlib1.so.1
#for file in $BUILD/lib/libtiled.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $BUILD/lib/libzlib1.so*; do cp -a "$file" "$DEST/lib/"; done

cp -a $SRC/dist/qt.conf.linux $DEST/bin/qt.conf
cp -a $SRC/dist/TileZed.sh $DEST
chmod +x $DEST/TileZed.sh

cp $QTDIR_LIB/libQt5Core.so.$QTVER $DEST/lib/libQt5Core.so.5
cp $QTDIR_LIB/libQt5DBus.so.$QTVER $DEST/lib/libQt5DBus.so.5
cp $QTDIR_LIB/libQt5Gui.so.$QTVER $DEST/lib/libQt5Gui.so.5
cp $QTDIR_LIB/libQt5Network.so.$QTVER $DEST/lib/libQt5Network.so.5
cp $QTDIR_LIB/libQt5OpenGL.so.$QTVER $DEST/lib/libQt5OpenGL.so.5
cp $QTDIR_LIB/libQt5Widgets.so.$QTVER $DEST/lib/libQt5Widgets.so.5
cp $QTDIR_LIB/libicudata.so.70.1 $DEST/lib/libicudata.so.70
cp $QTDIR_LIB/libicui18n.so.70.1 $DEST/lib/libicui18n.so.70
cp $QTDIR_LIB/libicuuc.so.70.1 $DEST/lib/libicuuc.so.70
cp $QTDIR_LIB/libQt5XcbQpa.so.$QTVER $DEST/lib/libQt5XcbQpa.so.5

#for file in $QTDIR/lib/libQt5Gui.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $QTDIR/lib/libQt5Network.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $QTDIR/lib/libQt5OpenGL.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $QTDIR/lib/libQt5Widgets.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $QTDIR/lib/libicu*.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $QTDIR/lib/libQt5DBus.so*; do cp -a "$file" "$DEST/lib/"; done
#for file in $QTDIR/lib/libQt5XcbQpa.so*; do cp -a "$file" "$DEST/lib/"; done

mkdir $DEST/plugins
mkdir $DEST/plugins/imageformats
cp -a $QTDIR_PLUGINS/imageformats/libqgif.so $DEST/plugins/imageformats/
cp -a $QTDIR_PLUGINS/imageformats/libqjpeg.so $DEST/plugins/imageformats/
mkdir $DEST/plugins/platforms
cp -a $QTDIR_PLUGINS/platforms/libqxcb.so $DEST/plugins/platforms/
#cp -a $QTDIR_PLUGINS/platforms/libqwayland-egl.so $DEST/plugins/platforms/
#cp -a $QTDIR_PLUGINS/platforms/libqwayland-generic.so $DEST/plugins/platforms/
#cp -a $QTDIR_PLUGINS/platforms/libqwayland-xcomposite-egl.so $DEST/plugins/platforms/
#cp -a $QTDIR_PLUGINS/platforms/libqwayland-xcomposite-glx.so $DEST/plugins/platforms/

