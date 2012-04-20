/*
 * zlevelsdock.h
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

#ifndef ZLEVELSDOCK_H
#define ZLEVELSDOCK_H

#include <QDockWidget>
#include <QTreeView>

class QTreeView;

namespace Tiled {

class ZTileLayerGroup;

namespace Internal {

class MapDocument;
class ZLevelsModel;
class ZLevelsView;

class ZLevelsDock : public QDockWidget
{
    Q_OBJECT

public:
    ZLevelsDock(QWidget *parent = 0);

    void setMapDocument(MapDocument *mapDoc);

protected:
	void changeEvent(QEvent *e);

private slots:
	void updateActions();
	void documentCloseRequested(int index);

private:
    void retranslateUi();

	void saveExpandedLevels(MapDocument *mapDoc);
	void restoreExpandedLevels(MapDocument *mapDoc);

	ZLevelsView *mView;
	MapDocument *mMapDocument;
	QMap<MapDocument*,QList<ZTileLayerGroup*>> mExpandedLevels;
};

class ZLevelsView : public QTreeView
{
    Q_OBJECT

public:
    ZLevelsView(QWidget *parent = 0);

    QSize sizeHint() const;

    void setMapDocument(MapDocument *mapDoc);
	ZLevelsModel *model() const { return mModel; }

protected slots:
	virtual void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

private slots:
	void currentLayerIndexChanged(int index);
	void onActivated(const QModelIndex &index);

private:
	MapDocument *mMapDocument;
	bool mSynching;
	ZLevelsModel *mModel;
};

} // namespace Internal
} // namespace Tiled

#endif // ZLEVELSDOCK_H
