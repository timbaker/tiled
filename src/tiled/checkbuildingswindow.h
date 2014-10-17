#ifndef CHECKBUILDINGSWINDOW_H
#define CHECKBUILDINGSWINDOW_H

#include <QMainWindow>
#include <QSet>
#include <QTimer>

namespace BuildingEditor {
class Building;
class BuildingObject;
class BuildingMap;
}

namespace Tiled {
class Map;
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
    void itemActivated(QTreeWidgetItem *item, int column);
    void syncList();
    void fileChanged(const QString &fileName);
    void fileChangedTimeout();

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
            Sinks
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

        Issue(IssueFile *file, Type type, const QString &detail, BuildingEditor::BuildingObject *object);

        QString toString()
        {
            return QString::fromLatin1("%1 @ %2,%3,%4").arg(detail).arg(x).arg(y).arg(z);
        }

        IssueFile *file;
        Type type;
        QString detail;
        int x;
        int y;
        int z;
        int objectIndex;
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

    void check(const QString &filePath);
    void check(BuildingEditor::BuildingMap *bmap, BuildingEditor::Building *building, Tiled::Map *map, const QString &fileName);
    void issue(Issue::Type type, const QString &detail, int x, int y, int z);
    void issue(Issue::Type type, char *detail, int x, int y, int z);
    void issue(Issue::Type type, char *detail, BuildingEditor::BuildingObject *object);
    void updateList(IssueFile *file);
    void syncList(IssueFile *file);

private:
    Ui::CheckBuildingsWindow *ui;
    QList<IssueFile*> mFiles;
    IssueFile *mCurrentIssueFile;

    Tiled::Internal::FileSystemWatcher *mFileSystemWatcher;
    QList<QString> mWatchedFiles;
    QSet<QString> mChangedFiles;
    QTimer mChangedFilesTimer;
};

#endif // CHECKBUILDINGSWINDOW_H
