# See the README file for instructions about setting the install prefix.
isEmpty(PREFIX):PREFIX = /usr/local
isEmpty(LIBDIR):LIBDIR = $${PREFIX}/lib

macx {
    # Do a universal build when possible
    contains(QT_CONFIG, ppc):CONFIG += x86 ppc
}

# Only "make install" newer files when copying files from the source
# directory to the shadow-build directory.
!isEmpty(INSTALL_ONLY_BUILD) {
    win32-msvc* {
        QMAKE_INSTALL_FILE=xcopy /d /y
        QMAKE_INSTALL_DIR=xcopy /d /s /q /y /i
    }
}

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050F00

DEFINES += ZOMBOID
