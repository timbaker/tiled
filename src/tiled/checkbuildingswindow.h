#ifndef CHECKBUILDINGSWINDOW_H
#define CHECKBUILDINGSWINDOW_H

#include "tiledeffile.h"

#include <QDir>
#include <QMainWindow>
#include <QSet>
#include <QTimer>

class CompositeLayerGroup;
class PROGRESS;
class QItemSelection;

namespace BuildingEditor {
class Building;
class BuildingFloor;
class BuildingObject;
class BuildingMap;
class BuildingTileEntry;
class FurnitureGroup;
class FurnitureTiles;
class Room;
}

namespace Tiled {
class Map;
class Tile;
namespace Internal {
class FileSystemWatcher;
}
}

namespace Ui {
class CheckBuildingsWindow;
}

class QTreeWidgetItem;


class CheckBuildingsWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CheckBuildingsWindow(QWidget *parent = 0);
    ~CheckBuildingsWindow();

private slots:
    void browse();
    void browseReplaceTiles();
    void check();
    void checkNextFile();
    void fixSelected();
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void itemActivated(QTreeWidgetItem *item, int column);
    void syncList();
    void fileChanged(const QString &fileName);
    void fileChangedTimeout();
    void pause();
    void stop();
    void selectAll();
    void selectNone();

private:
    class IssueFile;

    class ReplaceTileInfo
    {
    public:
        enum class Type
        {
            None,
            BuildingTileEntry,
            RoomTile,
            FurnitureTile,
            ObjectTile,
            GrimeTile,
        };

        ReplaceTileInfo() :
            type(Type::None)
        {

        }

        ReplaceTileInfo(const Type type, const QString &tileNameOld, const QString &tileNameNew) :
            type(type),
            tileNameOld(tileNameOld),
            tileNameNew(tileNameNew)
        {

        }

        Type type;
        QString tileNameOld;
        QString tileNameNew;
    };


    class Issue
    {
    public:
        enum Type
        {
            LightSwitch,
            InteriorOutside,
            RoomLight,
            Grime,
            Sinks,
            Rearranged,
            RearrangeGrid,
            MultipleContainers,
            DoorInWall,
            KidsBedroom,
            ReplaceRoomInternalName,
            ReplaceTile,
        };

        Issue(IssueFile *file, Type type, const QString &detail, int x, int y, int z) :
            file(file),
            type(type),
            detail(detail),
            x(x),
            y(y),
            z(z),
            objectIndex(-1)
        {

        }

        Issue(IssueFile *file, Type type, const QRegion &roomRegion, int z) :
            file(file),
            type(type),
            detail(QStringLiteral("bedroom -> kidsbedroom")),
            x(roomRegion.cbegin()->left()),
            y(roomRegion.cbegin()->top()),
            z(z),
            objectIndex(-1),
            roomRegion(roomRegion)
        {

        }

        Issue(IssueFile *file, Type type, const QString &detail, BuildingEditor::BuildingObject *object);

        Issue(IssueFile *file, Type type, const QString &roomNameOld, const QString &roomNameNew, const QRegion &roomRegion, int z) :
            file(file),
            type(type),
            detail(QStringLiteral("room name")),
            x(roomRegion.cbegin()->x()),
            y(roomRegion.cbegin()->y()),
            z(z),
            objectIndex(-1),
            roomRegion(roomRegion),
            roomNameOld(roomNameOld),
            roomNameNew(roomNameNew)
        {

        }

        Issue(IssueFile *file, int x, int y, int z, const ReplaceTileInfo &replaceTileInfo) :
            file(file),
            type(Type::ReplaceTile),
            detail(QStringLiteral("replace")),
            x(x),
            y(y),
            z(z),
            objectIndex(-1),
            replaceTileInfo(replaceTileInfo)
        {

        }

        Issue(IssueFile *file, const ReplaceTileInfo &replaceTileInfo) :
            file(file),
            type(Type::ReplaceTile),
            detail(QStringLiteral("replace")),
            x(-1),
            y(-1),
            z(-1),
            objectIndex(-1),
            replaceTileInfo(replaceTileInfo)
        {

        }

        QString toString();

        IssueFile *file;
        Type type;
        QString detail;
        int x;
        int y;
        int z;
        int objectIndex;
        QRegion roomRegion;
        QString roomNameOld;
        QString roomNameNew;
        ReplaceTileInfo replaceTileInfo;
    };

    class IssueFile
    {
    public:
        IssueFile(const QString &path) :
            path(path)
        {

        }

        QString path;
        QList<Issue> issues;
    };

    struct FixSelected
    {
        CheckBuildingsWindow::Issue issue;

        FixSelected(const CheckBuildingsWindow::Issue &issue) :
            issue(issue)
        {

        }
    };

    bool readReplaceTilesTxt();
    void check(const QString &filePath);
    void check(BuildingEditor::BuildingMap *bmap, BuildingEditor::Building *building, Tiled::Map *map, const QString &fileName);
    void issue(Issue::Type type, const QString &detail, int x, int y, int z);
    void issue(Issue::Type type, const char *detail, int x, int y, int z);
    void issue(Issue::Type type, const char *detail, BuildingEditor::BuildingObject *object);
    void issue(Issue::Type type, const QRegion &roomRegion, int z);
    void issue(Issue::Type type, const QString &roomNameOld, const QString &roomNameNew, const QRegion &roomRegion, int z);
    void issue(int x, int y, int z, const ReplaceTileInfo &replaceTileInfo);
    void issue(const ReplaceTileInfo &replaceTileInfo);
    void updateList(IssueFile *file);
    void syncList(const IssueFile *file);

    void checkKidsBedroom(BuildingEditor::BuildingFloor *floor, CompositeLayerGroup *layers, BuildingEditor::Room *room);
    bool isKidsBedroomRegion(CompositeLayerGroup *layers, const QRegion &roomRegion);
    bool isKidsBedroomRect(CompositeLayerGroup *layers, const QRect &roomRect);
    bool isKidsBedroomTile(Tiled::Tile *tile);
    void fixKidsBedroom(const QString &tbxPath, const QRegion &roomRegion, int z);
    BuildingEditor::Room *findExistingKidsBedroom(BuildingEditor::Building *building, BuildingEditor::Room *roomOld);
    QString kidsBedroomName(BuildingEditor::Room *roomOld);

    void checkRoomInternalName(BuildingEditor::BuildingFloor *floor, BuildingEditor::Room *room);
    void fixRoomInternalName(const Issue &issue);

    void fixReplaceTile(const Issue &issue);
    BuildingEditor::BuildingTileEntry *replaceTileInEntry(const BuildingEditor::BuildingTileEntry *bte, const QString &tileOld, const QString &tileNew);
    BuildingEditor::FurnitureTiles *replaceTileInFurniture(const BuildingEditor::FurnitureTiles *ftiles, const QString &tileOld, const QString &tileNew, QMap<BuildingEditor::FurnitureGroup*,BuildingEditor::FurnitureGroup*> &doneFurnitureGroups);
    void replaceFurnitureTilesInObjects(BuildingEditor::Building *building, BuildingEditor::FurnitureTiles *ftilesOld, BuildingEditor::FurnitureTiles *ftilesNew);

private:
    Ui::CheckBuildingsWindow *ui;
    QDir mDirectory;
    QStringList mFileNames;
    QTimer mCheckNextFileTimer;
    QList<IssueFile*> mFiles;
    IssueFile *mCurrentIssueFile;
    bool mPaused = false;

    Tiled::Internal::FileSystemWatcher *mFileSystemWatcher;
    QList<QString> mWatchedFiles;
    QSet<QString> mChangedFiles;
    QTimer mChangedFilesTimer;
    QStringList mKidsBedroomTiles;
    QMap<QString, QString> mReplaceTileLookup;
};

#endif // CHECKBUILDINGSWINDOW_H
