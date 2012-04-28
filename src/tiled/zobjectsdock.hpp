/*
 * zobjectsdock.h
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

#ifndef ZOBJECTSDOCK_H
#define ZOBJECTSDOCK_H

#include <QDockWidget>
#include <QTreeView>

class QTreeView;

namespace Tiled {

class ObjectGroup;

namespace Internal {

class MapDocument;
class ZMapObjectModel;
class ZObjectsView;

class ZObjectsDock : public QDockWidget
{
    Q_OBJECT

public:
    ZObjectsDock(QWidget *parent = 0);

    void setMapDocument(MapDocument *mapDoc);

protected:
	void changeEvent(QEvent *e);

private slots:
	void updateActions();
	void aboutToShowMoveToMenu();
	void triggeredMoveToMenu(QAction *action);
	void duplicateObjects();
	void removeObjects();
	void objectProperties();
	void documentCloseRequested(int index);

private:
    void retranslateUi();

	void saveExpandedGroups(MapDocument *mapDoc);
	void restoreExpandedGroups(MapDocument *mapDoc);

	QAction *mActionDuplicateObjects;
	QAction *mActionRemoveObjects;
	QAction *mActionObjectProperties;
	QAction *mActionMoveToLayer;

	ZObjectsView *mObjectsView;
	MapDocument *mMapDocument;
    QMap<MapDocument*,QList<ObjectGroup*> > mExpandedGroups;
	QMenu *mMoveToMenu;
};

class ZObjectsView : public QTreeView
{
    Q_OBJECT

public:
    ZObjectsView(QWidget *parent = 0);

    QSize sizeHint() const;

    void setMapDocument(MapDocument *mapDoc);
	ZMapObjectModel *model() const { return mMapObjectModel; }

protected slots:
	virtual void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

private slots:
	void currentRowChanged(const QModelIndex &index);
	void currentLayerIndexChanged(int index);
	void onActivated(const QModelIndex &index);
	void selectedObjectsChanged();

private:
	MapDocument *mMapDocument;
	bool mSynching;
	ZMapObjectModel *mMapObjectModel;
};

} // namespace Internal
} // namespace Tiled

#endif // ZOBJECTSDOCK_H
