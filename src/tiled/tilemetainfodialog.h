/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
 *
 * This file is part of Tiled.
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

#ifndef TILEMETAINFODIALOG_H
#define TILEMETAINFODIALOG_H

#include <QDialog>
#include <QMap>

class QToolButton;
class QUndoGroup;
class QUndoStack;

namespace Ui {
class TileMetaInfoDialog;
}

namespace Tiled {

class Tile;
class Tileset;

namespace Internal {

class Zoomable;

class TileMetaInfoDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit TileMetaInfoDialog(QWidget *parent = 0);
    ~TileMetaInfoDialog();
    
    QString setTileEnum(Tile *tile, const QString &enumName);
    void addTileset(Tileset *ts);
    void removeTileset(Tileset *ts);

private slots:

    void addTileset();
    void removeTileset();

    void currentTilesetChanged(int row);
    void tileSelectionChanged();

    void enumChanged(int index);

    void undoTextChanged(const QString &text);
    void redoTextChanged(const QString &text);

    void updateUI();

    void accept();
    void reject();

private:
    void setTilesetList();
    void setTilesList();

private:
    Ui::TileMetaInfoDialog *ui;
    Tileset *mCurrentTileset;
    QList<Tile*> mSelectedTiles;
    Zoomable *mZoomable;
    bool mSynching;
    bool mClosing;

    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
    QToolButton *mUndoButton;
    QToolButton *mRedoButton;
};

} // namespace Internal
} // namespace Tiled

#endif // TILEMETAINFODIALOG_H
