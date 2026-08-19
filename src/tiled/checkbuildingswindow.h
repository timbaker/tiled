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

        QString toString()
        {
            if (type == Type::ReplaceRoomInternalName) {

                return QStringLiteral("%1 : %2 -> %3 @ %4,%5,%6").arg(detail).arg(roomNameOld).arg(roomNameNew).arg(x).arg(y).arg(z);
            }
            return QString::fromLatin1("%1 @ %2,%3,%4").arg(detail).arg(x).arg(y).arg(z);
        }

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

    void check(const QString &filePath);
    void check(BuildingEditor::BuildingMap *bmap, BuildingEditor::Building *building, Tiled::Map *map, const QString &fileName);
    void issue(Issue::Type type, const QString &detail, int x, int y, int z);
    void issue(Issue::Type type, const char *detail, int x, int y, int z);
    void issue(Issue::Type type, const char *detail, BuildingEditor::BuildingObject *object);
    void issue(Issue::Type type, const QRegion &roomRegion, int z);
    void issue(Issue::Type type, const QString &roomNameOld, const QString &roomNameNew, const QRegion &roomRegion, int z);
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
};

#endif // CHECKBUILDINGSWINDOW_H
