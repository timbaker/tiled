mkdir ../../linuxdeploy-tiled
cd ../../linuxdeploy-tiled

# 1. Force the inclusion of the Wayland platform libraries
export EXTRA_PLATFORM_PLUGINS="libqwayland.so"

# 2. Force the inclusion of the core Wayland protocol engine
export EXTRA_QT_MODULES="waylandcompositor"

# 3. Force the shell and graphics routing sub-folders
export EXTRA_QT_PLUGINS="wayland-shell-integration;wayland-graphics-integration-client"

rm -rf ./AppDir

# copy our plugins before running linuxdeploy
mkdir AppDir
mkdir AppDir/usr
mkdir AppDir/usr/lib
cp -ra ../build-tiled-docker/lib/tilezed AppDir/usr/lib/tilezed

# the plugins copied above have dependencies used by linuxdeploy before it copies our other build files
for file in ../build-tiled-docker/lib/libtiled*; do cp -a "$file" "AppDir/usr/lib/"; done
for file in ../build-tiled-docker/lib/libzlib*; do cp -a "$file" "AppDir/usr/lib/"; done

linuxdeploy \
  --appdir AppDir \
  --desktop-file ../tiled/TileZed.desktop \
  --icon-file ../tiled/src/tiled/images/tiled-icon.svg \
  --executable ../build-tiled-docker/bin/TileZed \
  --plugin qt \
  --output appimage

