# See the README file for instructions about setting the install prefix.
isEmpty(PREFIX):PREFIX = /usr/local
isEmpty(LIBDIR):LIBDIR = $${PREFIX}/lib

macx {
    # Do a universal build when possible
    contains(QT_CONFIG, ppc):CONFIG += x86 ppc
}

# Only "make install" newer files
!isEmpty(INSTALL_ONLY_BUILD):win32-msvc* {
    QMAKE_INSTALL_FILE=xcopy /d /y
    QMAKE_INSTALL_DIR=xcopy /d /s /q /y /i
}

DEFINES += ZOMBOID
