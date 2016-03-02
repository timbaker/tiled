#ifndef REARRANGETILES_H
#define REARRANGETILES_H

#include <QAbstractListModel>
#include <QCoreApplication>
#include <QMainWindow>
#include <QTableView>

namespace Ui {
class RearrangeTiles;
}

namespace BuildingEditor {
class BuildingTile;
}

namespace Tiled {

class Tile;
class Tileset;

namespace Internal {

class Zoomable;

class RearrangeIndex
{
public:
    RearrangeIndex() :
        mX(-1), mY(-1)
    {}

    RearrangeIndex(int x, int y) :
        mX(x), mY(y)
    {}

    int x() { return mX; }
    int y() { return mY; }

    int &rx() { return mX; }
    int &ry() { return mY; }

    int index(int columns = 8) const
    {
        return mX + mY * columns;
    }

    friend bool operator<(const RearrangeIndex &lhs, const RearrangeIndex &rhs)
    {
        return lhs.index() < rhs.index();
    }

    bool operator==(const RearrangeIndex &rhs) const
    {
        return (mX == rhs.mX) && (mY == rhs.mY);
    }

    int mX;
    int mY;
};

class RearrangeTileset
{
public:
    QString mName;
    QList<RearrangeIndex> mIndexMap;
};

class RearrangeFile
{
    Q_DECLARE_TR_FUNCTIONS(RearrangeFile)

public:
    ~RearrangeFile()
    {
        qDeleteAll(mTilesets);
    }

    bool read(const QString &fileName);
    bool write(const QString &fileName, const QList<RearrangeTileset *> &tilesets);

    QString errorString() const {
        return mError;
    }

    QList<RearrangeTileset*> takeTilesets() {
        QList<RearrangeTileset*> ret = mTilesets;
        mTilesets.clear();
        return ret;
    }

private:
    bool parse2Ints(const QString &string, int *pa, int *pb);

private:
    QString mError;
    QList<RearrangeTileset*> mTilesets;
};

class RearrangeTilesModel : public QAbstractListModel
{
    Q_OBJECT
public:
    RearrangeTilesModel(QObject *parent = 0);
    ~RearrangeTilesModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(Tile *tile);

    void clear();
    void setTiles(Tileset *ts1x, Tileset *ts2x, const RearrangeTileset *rts);
    void setChanged(const QModelIndex &index, bool changed);
    bool isChanged(const QModelIndex &index) const;

    Tile *tileAt(const QModelIndex &index) const;
    Tile *tile2xAt(const QModelIndex &index) const;

    void scaleChanged(qreal scale);

    void redisplay();
    void redisplay(const QModelIndex &index);

    void setColumnCount(int count);

    Tileset *tileset1x() { return mTileset1x; }
    Tileset *tileset2x() { return mTileset2x; }

private:
    class Item
    {
    public:
        Item() :
            mTileID(-1),
            mChanged(false)
        {
        }

        Item(int tileID, bool changed) :
            mTileID(tileID),
            mChanged(changed)
        {
        }

        bool valid() const
        {
            return mTileID != -1;
        }

        int mTileID;
        bool mChanged;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(Tiled::Tile *tile) const;

    int indexOf(Item *item) { return item->mTileID; }

    Tileset *mTileset1x;
    Tileset *mTileset2x;
    QList<Item*> mItems;
    QMap<int,Item*> mTileItemsByIndex;
    QMap<Tiled::Tile*,Item*> mTileToItem;
    int mColumnCount;
};

class RearrangeTilesView : public QTableView
{
    Q_OBJECT
public:
    explicit RearrangeTilesView(QWidget *parent = 0);
    explicit RearrangeTilesView(Zoomable *zoomable, QWidget *parent = 0);

    QSize sizeHint() const;

    void wheelEvent(QWheelEvent *event);

    RearrangeTilesModel *model() const
    { return mModel; }

    void setZoomable(Zoomable *zoomable);

    Zoomable *zoomable() const
    { return mZoomable; }

    virtual void clear();

    void setTiles(Tileset *ts1x, Tileset *ts2x, const RearrangeTileset *rts);

public slots:
    void scaleChanged(qreal scale);

private:
    void init();

private:
    RearrangeTilesModel *mModel;
    Zoomable *mZoomable;
};

class RearrangeTiles : public QMainWindow
{
    Q_OBJECT

public:
    static RearrangeTiles *instance();

    explicit RearrangeTiles(QWidget *parent = 0);
    ~RearrangeTiles();

    bool isRearranged(BuildingEditor::BuildingTile *btile);
    bool isRearranged(Tile *tile);

    RearrangeIndex rearranged(BuildingEditor::BuildingTile *btile);
    RearrangeIndex rearranged(Tile *tile);

    void readTxtIfNeeded();

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void browse1x();
    void browse2x();
    void currentTilesetChanged(int row);
    void tileClicked(const QModelIndex &index);
    bool fileSave();

private:
    void setTilesetList();
    RearrangeTileset *tileset(const QString &name);
    void rearrange(const QString &tilesetName, const RearrangeIndex &index);
    void updateUI();
    void readTxt();
    void writeTxt();
    bool confirmSave();

private:
    static RearrangeTiles *mInstance;
    Ui::RearrangeTiles *ui;
    Zoomable *mZoomable;
    QList<RearrangeTileset*> mTilesets;
    QString mCurrentTileset;
    bool bDirty;
};

} // namespace Internal
} // namespace Tiled

#endif // REARRANGETILES_H
