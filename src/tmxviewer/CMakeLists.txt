project( tmxviewer )
cmake_minimum_required( VERSION 2.6 )
set ( CMAKE_BUILD_TYPE Release )
#add_definitions ( -Wall )
find_package ( Qt4 REQUIRED )
include ( ${QT_USE_FILE} )
include_directories (
	${CMAKE_CURRENT_SOURCE_DIR}/../libtiled
	${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR}
	)
#set ( EXECUTABLE_OUTPUT_PATH ../../bin )
set ( tmxviewer_HDRS
	
	)

set ( tmxviewer_SRCS
	main.cpp
	tmxviewer.cpp
	)

set ( tmxviewer_MOCS
	tmxviewer.h
	)
QT4_WRAP_CPP(MOCS ${tmxviewer_MOCS})

add_executable ( tmxviewer ${tmxviewer_SRCS} ${UIS} ${RSCS} ${TRS} ${MOCS} )
target_link_libraries ( tmxviewer libtiled ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} )

INSTALL(TARGETS tmxviewer DESTINATION ".")