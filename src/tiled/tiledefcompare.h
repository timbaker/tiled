#ifndef TILEDEFCOMPARE_H
#define TILEDEFCOMPARE_H

#include "tiledeffile.h"
#include "tiledeftextfile.h"
#include <QMainWindow>

class QComboBox;
class QListWidgetItem;

namespace Ui {
class TileDefCompare;
}

class TileDefCompare : public QMainWindow
{
    Q_OBJECT

public:
    explicit TileDefCompare(QWidget *parent = 0);
    ~TileDefCompare();

private slots:
    void browse1();
    void browse2();
    void swapPaths();
    void compare();
    void use1();
    void use2();
    void saveMerged();
    void currentRowChanged(int row);

private:
    QString propertiesString(int fileIndex, Tiled::Internal::TileDefTile *tdt);
    QString listString(int use, Tiled::Internal::TileDefTile *tdt, Tiled::Internal::TileDefTile *tdt2);
    void readSettings();
    void writeSettings();
    QImage getTileImage(Tiled::Internal::TileDefTile *tdt);
    void addRecentFile1(const QString& fileName);
    void addRecentFile2(const QString& fileName);
    QStringList recentFiles1() const;
    QStringList recentFiles2() const;
    void setRecentFilesCombo1();
    void setRecentFilesCombo2();

private:
    Ui::TileDefCompare *ui;
    Tiled::Internal::TileDefFile mTileDefFile1;
    Tiled::Internal::TileDefFile mTileDefFile2;
    Tiled::Internal::TileDefFile mMergedFile;
    QMap<QListWidgetItem*,Tiled::Internal::TileDefTile*> mTileMap1;
    QMap<QListWidgetItem*,Tiled::Internal::TileDefTile*> mTileMap2;
    QMap<QListWidgetItem*,int> mUseMap;
    static const int MaxRecentFiles = 10;
};

#endif // TILEDEFCOMPARE_H
