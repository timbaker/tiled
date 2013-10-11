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

#ifndef VIRTUALTILESETDIALOG_H
#define VIRTUALTILESETDIALOG_H

#include <QMainWindow>

#include "virtualtileset.h" // for TileShapeFace

#include "BuildingEditor/singleton.h"

class QModelIndex;
class QSplitter;
class QUndoGroup;
class QUndoStack;

namespace Ui {
class VirtualTilesetDialog;
}

namespace Tiled {
class Tileset;

namespace Internal {

class TextureInfo;
class TileShape;
class TileShapeGroup;
class VirtualTile;
class VirtualTileset;
class VirtualTilesetsFile;

class VirtualTilesetDialog : public QMainWindow, public Singleton<VirtualTilesetDialog>
{
    Q_OBJECT

public:
    explicit VirtualTilesetDialog(QWidget *parent = 0);
    ~VirtualTilesetDialog();

    static bool closeYerself()
    {
        return mInstance ? mInstance->close() : true;
    }

    void setVirtualTilesetNamesList();
    void setVirtualTilesetTilesList();
    void setTextureNamesList();
    void setTextureTilesList();

    // Undo/Redo callbacks
    void addTileset(VirtualTileset *vts);
    void removeTileset(VirtualTileset *vts);
    QString renameTileset(VirtualTileset *vts, const QString &name);
    void resizeTileset(VirtualTileset *vts, QSize &size, QVector<VirtualTile *> &tiles);
    void changeVTile(VirtualTile *vtile, QString &imageSource, int &srcX, int &srcY, TileShape *&isoType);
    void addTexture(TextureInfo *tex);
    void removeTexture(TextureInfo *tex);
    void changeTexture(TextureInfo *tex, int &tileWidth, int &tileHeight);
    void editShape(TileShape *shape, TileShape &other);
    //

    void closeEvent(QCloseEvent *event);

private slots:
    void addTileset();
    void removeTileset();

    void vTileActivated(const QModelIndex &index);
    void clearVTiles();
    void showDiskImage(bool show);

    void tilesetAdded(VirtualTileset *vts);
    void tilesetRemoved(VirtualTileset *vts);
    void tilesetChanged(VirtualTileset *vts);

    void virtualTilesetNameSelected();
    void editVTileset(const QModelIndex &index);

    void textureNameSelectionChanged();
    void textureTileSelectionChanged();

    void addTexture();
    void removeTexture();
    void textureProperties();

    void textureAdded(TextureInfo *tex);
    void textureRemoved(TextureInfo *tex);
    void textureChanged(TextureInfo *tex);

    void shapeGroupChanged(int row);
    void editGroups();

    void shapeGroupAdded(int index, TileShapeGroup *g);
    void shapeGroupRemoved(int index, TileShapeGroup *g);
    void shapeGroupChanged(TileShapeGroup *group);
    void shapeAssigned(TileShapeGroup *g, int col, int row);
    void shapeChanged(TileShape *shape);

    void beginDropTiles();
    void endDropTiles();
    void tileDropped(VirtualTile *vtile, const QString &imageSource, int srcX, int srcY, TileShape *shape);

    void editShape(const QModelIndex &index);

    void updateActions();

    void fileOpen();
    bool fileSave();
    bool fileSaveAs();

private:
    TileShape *shape(const char *name);
    void writeShapes();
    void fileOpen(const QString &fileName);
    bool fileSave(const QString &fileName);
    void clearDocument();

private:
    void saveSplitterSizes(QSplitter *splitter);
    void restoreSplitterSizes(QSplitter *splitter);
    bool confirmSave();
    QString getSaveLocation();

private:
    Ui::VirtualTilesetDialog *ui;
    QList<VirtualTileset*> mTilesets;
    VirtualTileset *mCurrentVirtualTileset;
    TextureInfo *mCurrentTexture;
    QList<Tileset*> mTextureTilesets;
    Tileset *mTextureTileset;
    QImage mTextureTileImage;
    VirtualTileset *mIsoTileset;
    TileShapeGroup *mShapeGroup;
    TileShapeGroup *mUngroupedGroup;
    bool mShowDiskImage;
    VirtualTilesetsFile *mFile;
    bool mShapesEdited;

    QUndoStack *mUndoStack;
};

} // namespace Internal
} // namespace Tiled


#endif // VIRTUALTILESETDIALOG_H
