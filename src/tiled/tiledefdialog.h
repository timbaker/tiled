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

#include <QFormLayout>
#include <QMap>
#include <QSet>
#include <QModelIndex>

class QCheckBox;
class QComboBox;
class QLineEdit;
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
class TileDefTileset;
class TilePropertyClipboard;
class Zoomable;

class TileDefDialog : public QMainWindow
{
    Q_OBJECT
    
public:
    static TileDefDialog *instance();
    static void deleteInstance();

    static bool closeYerself();

    void displayTile(const QString &tileName);

    // UNDO/REDO
    void insertTileset(int index, Tileset *ts, TileDefTileset *defTileset);
    void removeTileset(int index, Tileset **tsPtr = 0,
                       TileDefTileset **defTilesetPtr = 0);

    QVariant changePropertyValue(TileDefTile *defTile, const QString &name,
                                 const QVariant &value);

    const QMap<QString,int> assignTilesetIDs(const QMap<QString,int>& mapping);
    //

private slots:
    void fileNew();
    void fileOpen();
    void openRecentFile();
    void clearRecentFiles();
    bool fileSave();
    bool fileSaveAs();

    void addTileset();
    void removeTileset();

#ifdef TDEF_TILES_DIR
    void chooseTilesDirectory();
#endif

    void currentTilesetChanged(int row);
    void goBack();
    void goForward();

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
    void pasteFilteredProperties();
    void resetDefaults();

    void tileEntered(const QModelIndex &index);
    void tileLeft(const QModelIndex &index);

    void tilesetChanged(Tiled::Tileset *tileset);

    void rightPropertyFilterEdited(const QString &text);

    void tilesetFilterEdited(const QString &text);
    void propertyFilterEdited(const QString &text);
    void valueFilterEdited(const QString &text);

    void tilesetBackgroundColorChanged(const QColor& color);

    void reassignTilesetIDs();

    void updateUI();

    void help();

private:
    bool eventFilter(QObject *object, QEvent *event);
    void closeEvent(QCloseEvent *event);

    bool confirmSave();
    QString getSaveLocation();
    void fileOpen(const QString &fileName);
    bool fileSave(const QString &fileName);

    void clearDocument();

    void changePropertyValues(const QList<TileDefTile*> &defTiles,
                              const QString &name, const QVariant &value);

    void updateTilesetListLater();
    void updatePropertyPageLater();

    void setTilesetList();
    void setTilesList();
    void setToolTipEtc(int tileID);
    void highlightTilesWithMatchingProperties(TileDefTile *defTile);
    void resetDefaults(TileDefTile *defTile);
    void setPropertiesPage();
    void setBold(QWidget *w, bool bold);

    void initStringComboBoxValues();

    void saveSplitterSizes(QSplitter *splitter);
    void restoreSplitterSizes(QSplitter *splitter);

    void updateWindowTitle();

    void selectCurrentVisibleTileset();

    void applyPropertyFilters();
    QStringList filteredPropertyNames() const;
    bool propertyPassesFilter(const QString& propertyName, const QStringList& filters) const;

    QStringList recentFiles() const;
    void addRecentFile(const QString &fileName);
    void setRecentFilesMenu();

private:
    static TileDefDialog *mInstance;
    explicit TileDefDialog(QWidget *parent = 0);
    ~TileDefDialog();

    QStringList tilesetNames() const
    { return mTilesetByName.keys(); }
    Tileset *tileset(const QString &name) const;
    Tileset *tileset(int row) const;
    int indexOf(const QString &name) const;
    int rowOf(const QString &name) const;

    void loadTilesets();
    Tileset *loadTileset(const QString &source);
#if 0
    bool loadTilesetImage(Tileset *ts, const QString &source);
#endif
    void tilesDirChanged();
    void checkProperties();

    QString tilesDir();
#ifdef TDEF_TILES_DIR
    void setTilesDir(const QString &path);
    void getTilesDirKeyValues(QMap<QString,QString> &map);
#endif

    int uniqueTilesetID();

private:
    struct LabelField
    {
        QWidget *label;
        QWidget *field;

        LabelField(const QFormLayout::TakeRowResult &trr)
        {
            label = trr.labelItem ? trr.labelItem->widget() : nullptr;
            field = trr.fieldItem->widget();
            delete trr.labelItem;
            delete trr.fieldItem;
        }
    };

    Ui::TileDefDialog *ui;
    QFormLayout *mPropertySheetFormLayout;
    QList<LabelField> mPropertySheetWidgets;
    Tileset *mCurrentTileset;
    TileDefTileset *mCurrentDefTileset;
    QString mCurrentTilesetName;
    QList<TileDefTile*> mSelectedTiles;
    Zoomable *mZoomable;
    bool mSynching;
    bool mTilesetsUpdatePending;
    bool mPropertyPageUpdatePending;
    QString mError;

    TileDefFile *mTileDefFile;
#ifdef TDEF_TILES_DIR
    QString mTilesDirectory;
#endif
    QList<Tileset*> mTilesets;
    QMap<QString,Tileset*> mTilesetByName;
    QList<Tileset*> mRemovedTilesets;
    QList<TileDefTileset*> mRemovedDefTilesets;

    const TileDefProperties *mTileDefProperties;

    QSet<TileDefTile*> mTilesWithMatchingProperties;

    TilePropertyClipboard *mClipboard;

    QStringList mTilesetHistory;
    int mTilesetHistoryIndex;

    QMap<QString,QCheckBox*> mCheckBoxes;
    QMap<QString,QComboBox*> mComboBoxes;
    QMap<QString,QSpinBox*> mSpinBoxes;
    QFont mLabelFont;
    QFont mBoldLabelFont;

    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
    QToolButton *mUndoButton;
    QToolButton *mRedoButton;

    static const int MaxRecentFiles = 10;
    QAction* mRecentFiles[MaxRecentFiles];
};

} // namespace Internal
} // namespace Tiled

#endif // TILEDEFDIALOG_H
