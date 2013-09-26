#ifndef VIRTUALTILESETDIALOG_H
#define VIRTUALTILESETDIALOG_H

#include <QDialog>

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
    explicit VirtualTilesetDialog(QWidget *parent = 0);
    ~VirtualTilesetDialog();

    void setVirtualTilesetNamesList();
    void setVirtualTilesetTilesList();
    void setOrthoFilesList();
    void setOrthoTilesList();

    // Undo/Redo callbacks
    void addTileset(VirtualTileset *vts);
    void removeTileset(VirtualTileset *vts);
    void changeVTile(VirtualTile *vtile, QString &imageSource, int &srcX, int &srcY, int &isoType);
    //

private slots:
    void addTileset();
    void removeTileset();

    void clearVTiles();

    void tilesetAdded(VirtualTileset *vts);
    void tilesetRemoved(VirtualTileset *vts);

    void virtualTilesetNameSelected();
    void orthoFileSelectionChanged();
    void orthoTileSelectionChanged();

    void beginDropTiles();
    void endDropTiles();
    void tileDropped(VirtualTile *vtile, const QString &imageSource, int srcX, int srcY, int isoType);

    void updateActions();

private:
    Ui::VirtualTilesetDialog *ui;
    QList<VirtualTileset*> mTilesets;
    VirtualTileset *mCurrentVirtualTileset;
    QStringList mOrthoFiles;
    Tileset *mOrthoTileset;
    VirtualTileset *mIsoTileset;

    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
};

} // namespace Internal
} // namespace Tiled


#endif // VIRTUALTILESETDIALOG_H
