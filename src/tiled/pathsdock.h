/*
 * pathsdock.h
 * Copyright 2012, Tim Baker <treectrl@hotmail.com>
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

#ifndef PATHSDOCK_H
#define PATHSDOCK_H

#include <QDockWidget>
#include <QTreeView>

class QTreeView;

namespace Tiled {

class PathLayer;

namespace Internal {

class MapDocument;
class PathModel;
class PathsView;

class PathsDock : public QDockWidget
{
    Q_OBJECT

public:
    PathsDock(QWidget *parent = 0);

    void setMapDocument(MapDocument *mapDoc);

protected:
    void changeEvent(QEvent *e);

private slots:
    void updateActions();
    void aboutToShowMoveToMenu();
    void triggeredMoveToMenu(QAction *action);
    void duplicatePaths();
    void removePaths();
    void pathProperties();
    void documentAboutToClose(int index, MapDocument *mapDocument);

private:
    void retranslateUi();

    void saveExpandedGroups(MapDocument *mapDoc);
    void restoreExpandedGroups(MapDocument *mapDoc);

    QAction *mActionDuplicatePaths;
    QAction *mActionRemovePaths;
    QAction *mActionPathProperties;
    QAction *mActionMoveToLayer;

    PathsView *mPathsView;
    MapDocument *mMapDocument;
    QMap<MapDocument*, QList<PathLayer*> > mExpandedGroups;
    QMenu *mMoveToMenu;
};

class PathsView : public QTreeView
{
    Q_OBJECT

public:
    PathsView(QWidget *parent = 0);

    QSize sizeHint() const;

    void setMapDocument(MapDocument *mapDoc);
    PathModel *model() const { return mPathModel; }

protected slots:
    virtual void selectionChanged(const QItemSelection &selected,
                                  const QItemSelection &deselected);

private slots:
    void onActivated(const QModelIndex &index);
    void selectedPathsChanged();

private:
    MapDocument *mMapDocument;
    bool mSynching;
    PathModel *mPathModel;
};

} // namespace Internal
} // namespace Tiled

#endif // PATHSDOCK_H
