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

#ifndef TILESHAPESDIALOG_H
#define TILESHAPESDIALOG_H

#include <QDialog>

namespace Ui {
class TileShapeGroupDialog;
}

class QUndoGroup;
class QUndoStack;

namespace Tiled {
namespace Internal {

class TileShape;
class TileShapeGroup;
class VirtualTile;
class VirtualTileset;

class TileShapeGroupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TileShapeGroupDialog(QWidget *parent = 0);
    ~TileShapeGroupDialog();

    // Undo/Redo
    void insertGroup(int index, TileShapeGroup *g);
    TileShapeGroup *removeGroup(int index);
    void editGroup(TileShapeGroup *g, QString &label, int &columnCount, int &rowCount);
    TileShape *assignShape(TileShapeGroup *g, int col, int row, TileShape *shape);
    //

private slots:
    void addGroup();
    void removeGroup();
    void editGroup();

    void selectedGroupChanged();

    void shapeGroupAdded(int index, TileShapeGroup *g);
    void shapeGroupRemoved(int index, TileShapeGroup *g);
    void shapeGroupChanged(TileShapeGroup *g);
    void shapeAssigned(TileShapeGroup *g, int col, int row);

    void addShape();
    void clearShape();

    void beginDropTiles();
    void endDropTiles();
    void tileDropped(VirtualTile *vtile, const QString &imageSource, int x, int y, TileShape *shape);

    void srcGroupSelected(int index);

    void updateActions();

private:
    void setGroupList();
    void setShapeList();

    void setShapeSrcList();

private:
    Ui::TileShapeGroupDialog *ui;
    QUndoStack *mUndoStack;
    TileShapeGroup *mUngroupedGroup;
    TileShapeGroup *mCurrentGroup;
    VirtualTileset *mCurrentGroupTileset;
    TileShapeGroup *mCurrentSrcGroup;
    VirtualTileset *mCurrentSrcGroupTileset;
};

} // namespace Internal
} // namespace Tiled

#endif // TILESHAPESDIALOG_H
