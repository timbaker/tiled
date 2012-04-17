/*
 * zmapsdock.h
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

#ifndef ZMAPSDOCK_H
#define ZMAPSDOCK_H

#include <QDockWidget>
#include <QTreeView>

class QLabel;
class QLineEdit;
class QModelIndex;
class QTreeView;

namespace Tiled {
namespace Internal {

class ZMapsView;

class ZMapsDock : public QDockWidget
{
    Q_OBJECT

public:
    ZMapsDock(QWidget *parent = 0);

private slots:
	void browse();
	void editedLotDirectory();
	void onLotDirectoryChanged();

protected:
    void changeEvent(QEvent *e);

private:
    void retranslateUi();

	QLineEdit *mDirectoryEdit;
	ZMapsView *mMapsView;
};

/**
 * This view makes sure the size hint makes sense and implements the context
 * menu.
 */
class ZMapsView : public QTreeView
{
    Q_OBJECT

public:
    ZMapsView(QWidget *parent = 0);

    QSize sizeHint() const;

protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);

    void contextMenuEvent(QContextMenuEvent *event);
    void keyPressEvent(QKeyEvent *event);

private slots:
    void currentRowChanged(const QModelIndex &index);
    void currentLayerIndexChanged(int index);
	void onLotDirectoryChanged();

private:
	QPoint mDragStartPosition;
};

} // namespace Internal
} // namespace Tiled

#endif // ZMAPSDOCK_H
