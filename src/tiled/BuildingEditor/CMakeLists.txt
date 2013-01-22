set ( BuildingEd_UIS
    buildingeditorwindow.ui
    buildingfloorsdialog.ui
    buildinglayersdock.ui
    buildingpreferencesdialog.ui
    buildingtemplatesdialog.ui
    buildingtilesdialog.ui
    buildingtilesetdock.ui
    choosebuildingtiledialog.ui
    listofstringsdialog.ui
    newbuildingdialog.ui
    resizebuildingdialog.ui
    roomsdialog.ui
    templatefrombuildingdialog.ui
)

QT4_WRAP_UI ( localUIS ${BuildingEd_UIS} )

set ( BuildingEd_HDRS
    simplefile.h
    building.h
    buildingfloor.h
    buildingundoredo.h
    buildingobjects.h
    buildingwriter.h
    buildingreader.h
    buildingtmx.h
    horizontallinedelegate.h
    listofstringsdialog.h
)

set ( BuildingEd_SRCS
    building.cpp
    buildingdocument.cpp
    buildingeditorwindow.cpp
    buildingfloor.cpp
    buildingfloorsdialog.cpp
    buildingfurnituredock.cpp
    buildinglayersdock.cpp
    buildingmap.cpp
    buildingobjects.cpp
    buildingorthoview.cpp
    buildingpreferences.cpp
    buildingpreferencesdialog.cpp
    buildingreader.cpp
    buildingtemplates.cpp
    buildingtemplatesdialog.cpp
    buildingtilemodeview.cpp
    buildingtiles.cpp
    buildingtilesdialog.cpp
    buildingtilesetdock.cpp
    buildingtiletools.cpp
    buildingtmx.cpp
    buildingtools.cpp
    buildingundoredo.cpp
    buildingwriter.cpp
    choosebuildingtiledialog.cpp
    furnituregroups.cpp
    furnitureview.cpp
    horizontallinedelegate.cpp
    listofstringsdialog.cpp
    mixedtilesetview.cpp
    newbuildingdialog.cpp
    resizebuildingdialog.cpp
    roomsdialog.cpp
    simplefile.cpp
    templatefrombuildingdialog.cpp
    tilecategoryview.cpp
)

set ( BuildingEd_MOCS
    buildingdocument.h
    buildingeditorwindow.h
    buildingfloorsdialog.h
    buildingfurnituredock.h
    buildinglayersdock.h
    buildingmap.h
    buildingorthoview.h
    buildingpreferences.h
    buildingpreferencesdialog.h
    buildingtemplates.h
    buildingtemplatesdialog.h
    buildingtilemodeview.h
    buildingtiletools.h
    buildingtiles.h
    buildingtilesdialog.h
    buildingtilesetdock.h
    buildingtools.h
    choosebuildingtiledialog.h
    furnituregroups.h
    furnitureview.h
    mixedtilesetview.h
    newbuildingdialog.h
    resizebuildingdialog.h
    roomsdialog.h
    templatefrombuildingdialog.h
    tilecategoryview.h
)

QT4_WRAP_CPP ( localMOCS ${BuildingEd_MOCS} )

# Because these resources are part of a static library, the macro 
# Q_INIT_RESOURCES must be placed in the C++ main() function, otherwise the
# resources won't get linked in.
QT4_ADD_RESOURCES ( localRSCS buildingeditor.qrc )

# Find generated ui_xxx.h files in the build directory
include_directories ( ${CMAKE_CURRENT_BINARY_DIR} )

add_library ( BuildingEd STATIC ${BuildingEd_SRCS} ${BuildingEd_HDRS}
              ${BuildingEd_MOCS} ${localUIS} ${localMOCS} ${localRSCS} )

file ( COPY BuildingFurniture.txt BuildingTemplates.txt BuildingTiles.txt
          TMXConfig.txt DESTINATION ${CMAKE_BINARY_DIR}/Debug )

file ( COPY BuildingFurniture.txt BuildingTemplates.txt BuildingTiles.txt
          TMXConfig.txt DESTINATION ${CMAKE_BINARY_DIR}/Release )

install ( FILES BuildingFurniture.txt BuildingTemplates.txt BuildingTiles.txt
          TMXConfig.txt DESTINATION . )

install ( DIRECTORY manual/ DESTINATION docs/BuildingEd )
