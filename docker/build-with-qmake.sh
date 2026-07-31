#sudo docker run --rm -v "$(pwd)/../..":/workspace qt-builder \
#    bash -c "ls /opt/qt/6.11.1"

sudo docker run --rm -v "$(pwd)/../..":/workspace qt-builder \
    bash -c "mkdir -p build-tiled-docker && cd build-tiled-docker && qmake ../tiled/tiled.pro INSTALL_ONLY_BUILD=1 && make -j$(nproc) && make install"

sudo chown -R $USER:$USER ../../build-tiled-docker
