/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#ifndef TILEDEFDIALOG_H
#define TILEDEFDIALOG_H

#include <QMainWindow>

#include <QMap>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QToolButton;
class QUndoGroup;
class QUndoStack;

namespace Ui {
class TileDefDialog;
}

namespace Tiled {

class Map;
class Tile;
class Tileset;

namespace Internal {

class TileDefFile;
class TileDefProperties;
class Zoomable;

class TileDefDialog : public QMainWindow
{
    Q_OBJECT
    
public:
    static TileDefDialog *instance();
    static void deleteInstance();

    // UNDO/REDO
    void addTileset(Tileset *ts);
    void removeTileset(Tileset *ts);
    //

private slots:
    void fileNew();
    void fileOpen();
    void fileSave();

    void addTileset();
    void removeTileset();

    void currentTilesetChanged(int row);
    void tileSelectionChanged();

    void comboBoxActivated(int index);
    void checkboxToggled(bool value);
    void spinBoxValueChanged(int value);

    void undoTextChanged(const QString &text);
    void redoTextChanged(const QString &text);

    void updateTilesetList();

    void updateUI();

    void accept();
    void reject() { accept(); }

private:
    bool eventFilter(QObject *object, QEvent *event);
    void fileOpen(const QString &fileName);
    void updateTilesetListLater();
    void setTilesetList();
    void setTilesList();
    void setToolTipEtc(int tileID);
    void setPropertiesPage();
    void setBold(QWidget *w, bool bold);

private:
    static TileDefDialog *mInstance;
    explicit TileDefDialog(QWidget *parent = 0);
    ~TileDefDialog();

    QStringList tilesetNames() const
    { return mTilesets.keys(); }
    Tileset *tileset(const QString &name) const;
    Tileset *tileset(int index) const;
    int indexOf(const QString &name) const;

    void loadTilesets();
    Tileset *loadTileset(const QString &source);
    bool loadTilesetImage(Tileset *ts, const QString &source);

private:
    Ui::TileDefDialog *ui;
    Tileset *mCurrentTileset;
    QList<Tile*> mSelectedTiles;
    Zoomable *mZoomable;
    bool mSynching;
    bool mUpdatePending;
    QString mError;

    TileDefFile *mTileDefFile;
    QMap<QString,Tileset*> mTilesets;
    QList<Tileset*> mRemovedTilesets;

    TileDefProperties *mTileDefProperties;
    QMap<QString,QCheckBox*> mCheckBoxes;
    QMap<QString,QComboBox*> mComboBoxes;
    QMap<QString,QSpinBox*> mSpinBoxes;
    QFont mLabelFont;
    QFont mBoldLabelFont;

    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
    QToolButton *mUndoButton;
    QToolButton *mRedoButton;
};

} // namespace Internal
} // namespace Tiled

#endif // TILEDEFDIALOG_H
