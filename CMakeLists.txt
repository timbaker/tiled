project( TileZed )
cmake_minimum_required( VERSION 2.6 )
set ( CMAKE_BUILD_TYPE Release )
#add_definitions ( -Wall )
find_package ( Qt4 REQUIRED )
include ( ${QT_USE_FILE} )
include_directories (
	${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR}
	)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/MinSizeRel)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/RelWithDebInfo)

add_definitions( -DZOMBOID )

add_subdirectory( src )
add_subdirectory( translations )

install(FILES AUTHORS DESTINATION . RENAME AUTHORS.txt)
install(FILES COPYING DESTINATION . RENAME COPYING.txt)
install(FILES LICENSE.BSD DESTINATION . RENAME LICENSE.BSD.txt)
install(FILES LICENSE.GPL DESTINATION . RENAME LICENSE.GPL.txt)
install(FILES LICENSE.LGPL DESTINATION . RENAME LICENSE.LGPL.txt)
install(FILES NEWS DESTINATION . RENAME NEWS.txt)
install(FILES README.md DESTINATION . RENAME README.txt)

#--------------------------------------------------------------------------------
# install Qt runtime

install(FILES ${QT_BINARY_DIR}/QtCore4.dll DESTINATION . CONFIGURATIONS MinSizeRel Release RelWithDebInfo)
install(FILES ${QT_BINARY_DIR}/QtGui4.dll DESTINATION . CONFIGURATIONS MinSizeRel Release RelWithDebInfo)
install(FILES ${QT_BINARY_DIR}/QtOpenGL4.dll DESTINATION . CONFIGURATIONS MinSizeRel Release RelWithDebInfo)

install(FILES ${QT_BINARY_DIR}/QtCore4d.dll DESTINATION . CONFIGURATIONS Debug)
install(FILES ${QT_BINARY_DIR}/QtGui4d.dll DESTINATION . CONFIGURATIONS Debug)
install(FILES ${QT_BINARY_DIR}/QtOpenGL4d.dll DESTINATION . CONFIGURATIONS Debug)

#--------------------------------------------------------------------------------
# install Qt imageformats plugins

install(
	FILES
		${QT_PLUGINS_DIR}/imageformats/qgif4.dll
		${QT_PLUGINS_DIR}/imageformats/qjpeg4.dll
		${QT_PLUGINS_DIR}/imageformats/qtiff4.dll
	DESTINATION
		plugins/imageformats
	CONFIGURATIONS
		MinSizeRel Release RelWithDebInfo
)

install(
	FILES
		${QT_PLUGINS_DIR}/imageformats/qgifd4.dll
		${QT_PLUGINS_DIR}/imageformats/qjpegd4.dll
		${QT_PLUGINS_DIR}/imageformats/qtiffd4.dll
	DESTINATION
		plugins/imageformats
	CONFIGURATIONS
		Debug
)

#--------------------------------------------------------------------------------
# install Qt codecs plugins

install(
	FILES
		${QT_PLUGINS_DIR}/codecs/qcncodecs4.dll
		${QT_PLUGINS_DIR}/codecs/qjpcodecs4.dll
		${QT_PLUGINS_DIR}/codecs/qkrcodecs4.dll
		${QT_PLUGINS_DIR}/codecs/qtwcodecs4.dll
	DESTINATION
		plugins/codecs
	CONFIGURATIONS
		MinSizeRel Release RelWithDebInfo
)

install(
	FILES
		${QT_PLUGINS_DIR}/codecs/qcncodecsd4.dll
		${QT_PLUGINS_DIR}/codecs/qjpcodecsd4.dll
		${QT_PLUGINS_DIR}/codecs/qkrcodecsd4.dll
		${QT_PLUGINS_DIR}/codecs/qtwcodecsd4.dll
	DESTINATION
		plugins/codecs
	CONFIGURATIONS
		Debug
)

install(FILES docs/map.dtd docs/map.xsd DESTINATION docs)
install(DIRECTORY examples DESTINATION .)

#--------------------------------------------------------------------------------
# install a qt.conf file
# this inserts some cmake code into the install script to write the file
INSTALL(CODE "
    file(WRITE \"\${CMAKE_INSTALL_PREFIX}/${qtconf_dest_dir}/qt.conf\" \"[Paths]
Plugins = plugins
Translations = translations\")
    " COMPONENT Runtime)



