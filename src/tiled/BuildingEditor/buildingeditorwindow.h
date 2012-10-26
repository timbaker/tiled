/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef BUILDINGEDITORWINDOW_H
#define BUILDINGEDITORWINDOW_H

#include <QItemSelection>
#include <QMainWindow>
#include <QMap>
#include <QSettings>
#include <QVector>

class QComboBox;
class QLabel;
class QUndoGroup;

namespace Ui {
class BuildingEditorWindow;
}

namespace Tiled {
class Tile;
class Tileset;
namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {

class BaseTool;
class Building;
class BuildingDocument;
class BuildingFloor;
class BuildingPreviewWindow;
class BuildingTile;
class Door;
class FloorEditor;
class FloorView;
class Room;
class Window;
class Stairs;



class BuildingEditorWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit BuildingEditorWindow(QWidget *parent = 0);
    ~BuildingEditorWindow();

    static BuildingEditorWindow *instance;

    void showEvent(QShowEvent *event);
    void closeEvent(QCloseEvent *event);

    bool confirmAllSave();
    bool closeYerself();

    bool Startup();

    bool LoadBuildingTemplates();
    bool LoadBuildingTiles();
    bool LoadMapBaseXMLLots();
    bool validateTile(BuildingTile *btile, const char *key);

    void setCurrentRoom(Room *mRoomComboBox) const; // TODO: move to BuildingDocument
    Room *currentRoom() const;

    BuildingDocument *currentDocument() const
    { return mCurrentDocument; }

    Building *currentBuilding() const;

private:
    void readSettings();
    void writeSettings();

    void updateRoomComboBox();
    void resizeCoordsLabel();

    void setCategoryLists();

    bool writeBuilding(BuildingDocument *doc, const QString &fileName);

    bool confirmSave();

    void addDocument(BuildingDocument *doc);

private slots:
    void roomIndexChanged(int index);
    void categoryScaleChanged(qreal scale);

    void currentEWallChanged(const QItemSelection &selected);
    void currentIWallChanged(const QItemSelection &selected);
    void currentFloorChanged(const QItemSelection &selected);
    void currentDoorChanged(const QItemSelection &selected);
    void currentDoorFrameChanged(const QItemSelection &selected);
    void currentWindowChanged(const QItemSelection &selected);
    void currentStairsChanged(const QItemSelection &selected);

    void upLevel();
    void downLevel();

    void newBuilding();
    void openBuilding();
    bool saveBuilding();
    bool saveBuildingAs();
    void exportTMX();

    void preferences();

    void roomsDialog();
    void roomAdded(Room *room);
    void roomRemoved(Room *room);
    void roomsReordered();
    void roomChanged(Room *room);

    void rotateRight();
    void rotateLeft();

    void templatesDialog();
    void tilesDialog();
    void templateFromBuilding();

    void mouseCoordinateChanged(const QPoint &tilePos);

    void updateActions();

private:
    Ui::BuildingEditorWindow *ui;
    BuildingDocument *mCurrentDocument;
    FloorEditor *roomEditor;
    FloorView *mView;
    QComboBox *mRoomComboBox;
    QLabel *mFloorLabel;
    QUndoGroup *mUndoGroup;
    QSettings mSettings;
    QString mError;
    BuildingPreviewWindow *mPreviewWin;
    Tiled::Internal::Zoomable *mZoomable;
    Tiled::Internal::Zoomable *mCategoryZoomable;
    bool mSynching;
};

} // namespace BuildingEditor

#endif // BUILDINGEDITORWINDOW_H
