/*
 * Copyright 2016, Tim Baker <treectrl@users.sf.net>
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

#ifndef CHECKMAPSWINDOW_H
#define CHECKMAPSWINDOW_H

#include <QMainWindow>
#include <QSet>
#include <QTimer>

class QTreeWidgetItem;

namespace Ui {
class CheckMapsWindow;
}

namespace Tiled {
class Map;
namespace Internal {
class FileSystemWatcher;
class MapDocument;
}
}

class CheckMapsWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CheckMapsWindow(QWidget *parent = 0);
    ~CheckMapsWindow();

private slots:
    void browse();
    void check();
    void checkCurrent();
    void itemActivated(QTreeWidgetItem *item, int column);
    void fileChanged(const QString &fileName);
    void fileChangedTimeout();

private:
    class IssueFile;

    class Issue
    {
    public:
        enum Type
        {
            Bogus
        };

        Issue(IssueFile *file, Type type, const QString &detail, int x, int y, int z) :
            file(file),
            type(type),
            detail(detail),
            x(x),
            y(y),
            z(z)
        {

        }

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

    void check(const QString &fileName);
    void check(Tiled::Internal::MapDocument *doc);
    void issue(Issue::Type type, const QString &detail, int x, int y, int z);
    void updateList(CheckMapsWindow::IssueFile *file);
    void syncList();
    void syncList(IssueFile *file);

private:
    Ui::CheckMapsWindow *ui;
    QList<IssueFile*> mFiles;
    IssueFile *mCurrentIssueFile;

    Tiled::Internal::FileSystemWatcher *mFileSystemWatcher;
    QList<QString> mWatchedFiles;
    QSet<QString> mChangedFiles;
    QTimer mChangedFilesTimer;
};

#endif // CHECKMAPSWINDOW_H
