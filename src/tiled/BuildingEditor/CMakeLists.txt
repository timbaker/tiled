set ( BuildingEd_UIS
    buildingeditorwindow.ui
    buildingpreferencesdialog.ui
    buildingpreviewwindow.ui
    buildingtemplatesdialog.ui
    buildingtilesdialog.ui
    choosebuildingtiledialog.ui
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
)

set ( BuildingEd_SRCS
    buildingeditorwindow.cpp
    simplefile.cpp
    buildingtools.cpp
    FloorEditor.cpp
    buildingdocument.cpp
    building.cpp
    buildingfloor.cpp
    buildingundoredo.cpp
    buildingpreviewwindow.cpp
    mixedtilesetview.cpp
    newbuildingdialog.cpp
    buildingpreferencesdialog.cpp
    buildingobjects.cpp
    buildingtemplates.cpp
    buildingtemplatesdialog.cpp
    choosebuildingtiledialog.cpp
    roomsdialog.cpp
    buildingtilesdialog.cpp
    buildingtiles.cpp
    templatefrombuildingdialog.cpp
    buildingwriter.cpp
    buildingreader.cpp
    resizebuildingdialog.cpp
    furnitureview.cpp
    furnituregroups.cpp
    buildingpreferences.cpp
    buildingtmx.cpp
    tilecategoryview.cpp
)

set ( BuildingEd_MOCS
    buildingdocument.h
    buildingeditorwindow.h
    buildingpreferences.h
    buildingpreferencesdialog.h
    buildingpreviewwindow.h
    buildingtemplates.h
    buildingtemplatesdialog.h
    buildingtiles.h
    buildingtilesdialog.h
    buildingtools.h
    choosebuildingtiledialog.h
    FloorEditor.h
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

install ( FILES BuildingFurniture.txt BuildingTemplates.txt BuildingTiles.txt
          TMXConfig.txt DESTINATION . )
          
install ( DIRECTORY manual DESTINATION docs )
