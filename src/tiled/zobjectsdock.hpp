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
	void duplicateObjects();
	void removeObjects();
	void objectProperties();

private:
    void retranslateUi();

	QAction *mActionDuplicateObjects;
	QAction *mActionRemoveObjects;
	QAction *mActionObjectProperties;

	ZObjectsView *mObjectsView;
	MapDocument *mMapDocument;
};

class ZObjectsView : public QTreeView
{
    Q_OBJECT

public:
    ZObjectsView(QWidget *parent = 0);

    QSize sizeHint() const;

    void setMapDocument(MapDocument *mapDoc);
	ZMapObjectModel *model() const { return mMapObjectModel; }

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
