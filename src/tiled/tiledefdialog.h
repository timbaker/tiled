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
class QSplitter;
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
class TileDefTile;
class TilePropertyClipboard;
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

    QVariant changePropertyValue(TileDefTile *defTile, const QString &name,
                                 const QVariant &value);
    //

private slots:
    void fileNew();
    void fileOpen();
    bool fileSave();
    bool fileSaveAs();

    void addTileset();
    void removeTileset();

    void currentTilesetChanged(int row);
    void tileSelectionChanged();

    void comboBoxActivated(int index);
    void checkboxToggled(bool value);
    void spinBoxValueChanged(int value);
    void stringEdited();

    void undoTextChanged(const QString &text);
    void redoTextChanged(const QString &text);

    void updateTilesetList();
    void updatePropertyPage();

    void copyProperties();
    void pasteProperties();
    void resetDefaults();

    void tileEntered(Tile *tile);
    void tileLeft(Tile *tile);

    void updateUI();

private:
    bool eventFilter(QObject *object, QEvent *event);
    void closeEvent(QCloseEvent *event);

    bool confirmSave();
    void fileOpen(const QString &fileName);
    bool fileSave(const QString &fileName);

    void changePropertyValues(const QList<TileDefTile*> &defTiles,
                              const QString &name, const QVariant &value);

    void updateTilesetListLater();
    void updatePropertyPageLater();

    void setTilesetList();
    void setTilesList();
    void setToolTipEtc(int tileID);
    void resetDefaults(TileDefTile *defTile);
    void setPropertiesPage();
    void setBold(QWidget *w, bool bold);

    void initStringComboBoxValues();

    void saveSplitterSizes(QSplitter *splitter);
    void restoreSplitterSizes(QSplitter *splitter);

    void updateWindowTitle();

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
    QList<TileDefTile*> mSelectedTiles;
    Zoomable *mZoomable;
    bool mSynching;
    bool mTilesetsUpdatePending;
    bool mPropertyPageUpdatePending;
    QString mError;

    TileDefFile *mTileDefFile;
    QMap<QString,Tileset*> mTilesets;
    QList<Tileset*> mRemovedTilesets;

    TileDefProperties *mTileDefProperties;

    TilePropertyClipboard *mClipboard;

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
