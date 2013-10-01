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

#include <QDialog>

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

class VirtualTile;
class VirtualTileset;

class VirtualTilesetDialog : public QDialog
{
    Q_OBJECT

public:
    enum IsoCategory {
        CategoryFloor,
        CategoryRoof,
        CategoryWall
    };

    explicit VirtualTilesetDialog(QWidget *parent = 0);
    ~VirtualTilesetDialog();

    void setVirtualTilesetNamesList();
    void setVirtualTilesetTilesList();
    void setOrthoFilesList();
    void setOrthoTilesList();

    // Undo/Redo callbacks
    void addTileset(VirtualTileset *vts);
    void removeTileset(VirtualTileset *vts);
    QString renameTileset(VirtualTileset *vts, const QString &name);
    void resizeTileset(VirtualTileset *vts, QSize &size, QVector<VirtualTile *> &tiles);
    void changeVTile(VirtualTile *vtile, QString &imageSource, int &srcX, int &srcY, int &isoType);
    //

private slots:
    void addTileset();
    void removeTileset();

    void clearVTiles();
    void showDiskImage(bool show);

    void tilesetAdded(VirtualTileset *vts);
    void tilesetRemoved(VirtualTileset *vts);

    void virtualTilesetNameSelected();
    void editVTileset(const QModelIndex &index);

    void orthoFileSelectionChanged();
    void orthoTileSelectionChanged();

    void isoCategoryChanged(int row);

    void beginDropTiles();
    void endDropTiles();
    void tileDropped(VirtualTile *vtile, const QString &imageSource, int srcX, int srcY, int isoType);

    void editShape(const QModelIndex &index);

    void updateActions();

    void done(int r);

private:
    void saveSplitterSizes(QSplitter *splitter);
    void restoreSplitterSizes(QSplitter *splitter);

private:
    Ui::VirtualTilesetDialog *ui;
    QList<VirtualTileset*> mTilesets;
    VirtualTileset *mCurrentVirtualTileset;
    QList<Tileset*> mOrthoTilesets;
    Tileset *mOrthoTileset;
    VirtualTileset *mIsoTileset;
    IsoCategory mIsoCategory;
    bool mShowDiskImage;

    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
};

} // namespace Internal
} // namespace Tiled


#endif // VIRTUALTILESETDIALOG_H
