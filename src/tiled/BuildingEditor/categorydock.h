/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CATEGORYDOCK_H
#define CATEGORYDOCK_H

#include <QDockWidget>

#include <QModelIndex>

class QComboBox;
class QListWidget;
class QMenu;
class QSettings;
class QSplitter;
class QStackedWidget;

namespace Tiled {
class Tile;
class Tileset;
namespace Internal {
class Zoomable;
}
}

template<class T>
class Singleton
{
public:
    static T &instance()
    {
        Q_ASSERT(mInstance);
        return *mInstance;
    }

    static void deleteInstance()
    {
        delete T;
    }

    Singleton()
    {
        Q_ASSERT(!mInstance);
        mInstance = static_cast< T* >( this );
    }

private:
    Singleton(const Singleton<T> &other);
    Singleton<T> &operator =(const Singleton<T> &other);

protected:
    static T *mInstance;
};

#define SINGLETON_IMPL(T) T *Singleton<T>::mInstance = 0;

namespace BuildingEditor {

class Building;
class BuildingDocument;
class BuildingFloor;
class BuildingObject;
class BuildingTileCategory;
class BuildingTileEntry;
class BuildingTileEntryView;
class FurnitureGroup;
class FurnitureTile;
class FurnitureView;
class Room;

class CategoryDock : public QDockWidget
{
    Q_OBJECT
public:
    CategoryDock(QWidget *parent = 0);
    
    Building *currentBuilding() const;
    Room *currentRoom() const;

    void readSettings(QSettings &settings);
    void writeSettings(QSettings &settings);

signals:
    
private slots:
    void currentDocumentChanged(BuildingDocument *doc);

    void categoryScaleChanged(qreal scale);
    void categoryViewMousePressed();
    void categoryActivated(const QModelIndex &index);

    void categorySelectionChanged();
    void tileSelectionChanged();
    void furnitureSelectionChanged();

    void scrollToNow(int which, const QModelIndex &index);

    void usedTilesChanged();
    void usedFurnitureChanged();
    void resetUsedTiles();
    void resetUsedFurniture();

    void currentRoomChanged();

    void tilesDialogEdited();

    void currentToolChanged();

    void objectPicked(BuildingObject *object);

private:
    void setCategoryList();

    void currentEWallChanged(BuildingTileEntry *entry, bool mergeable);
    void currentIWallChanged(BuildingTileEntry *entry, bool mergeable);
    void currentFloorChanged(BuildingTileEntry *entry, bool mergeable);
    void currentDoorChanged(BuildingTileEntry *entry, bool mergeable);
    void currentDoorFrameChanged(BuildingTileEntry *entry, bool mergeable);
    void currentWindowChanged(BuildingTileEntry *entry, bool mergeable);
    void currentCurtainsChanged(BuildingTileEntry *entry, bool mergeable);
    void currentStairsChanged(BuildingTileEntry *entry, bool mergeable);
    void currentRoomTileChanged(int entryEnum, BuildingTileEntry *entry, bool mergeable);
    void currentRoofTileChanged(BuildingTileEntry *entry, int which, bool mergeable);

    void selectCurrentCategoryTile();

    BuildingTileCategory *categoryAt(int row);
    FurnitureGroup *furnitureGroupAt(int row);

    void selectAndDisplay(BuildingTileEntry *entry);
    void selectAndDisplay(FurnitureTile *ftile);

private:
    BuildingDocument *mCurrentDocument;
    BuildingTileCategory *mCategory;
    FurnitureGroup *mFurnitureGroup;
    int mRowOfFirstCategory;
    int mRowOfFirstFurnitureGroup;
    bool mInitialCategoryViewSelectionEvent;
    Tiled::Internal::Zoomable *mCategoryZoomable;
    QMenu *mUsedContextMenu;
    QAction *mActionClearUsed;
    bool mSynching;

    struct
    {
        QSplitter *categorySplitter;
        QListWidget *categoryList;
        QStackedWidget *categoryStack;
        BuildingTileEntryView *tilesetView;
        FurnitureView *furnitureView;
        QComboBox *scaleComboBox;
    } _ui, *ui;
};

} // namespace BuildingEditor

#endif // CATEGORYDOCK_H
