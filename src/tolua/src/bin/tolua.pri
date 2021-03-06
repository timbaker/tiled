win32:tolua_exe = $$top_builddir/tolua.exe
unix:tolua_exe = $$top_builddir/bin/tolua

tolua.name = tolua generator
tolua.input = TOLUA_PKG
tolua.depends = $$TOLUA_DEPS $${tolua_exe}
tolua.output = ${QMAKE_FILE_BASE}.tolua.cpp
#tolua.commands = $$top_builddir/tolua -n $${TOLUA_PKGNAME} -o $${OUT_PWD}/${QMAKE_FILE_BASE}.tolua.cpp ${QMAKE_FILE_NAME}
tolua.commands = $${tolua_exe} -n $${TOLUA_PKGNAME} -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_NAME}
tolua.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += tolua
