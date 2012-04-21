/*
 * ztilesetthumbview.h
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

#ifndef ZTILESETTHUMBVIEW_H
#define ZTILESETTHUMBVIEW_H

#include "ztilesetthumbmodel.hpp"

#include <QTableView>

namespace Tiled {
namespace Internal {

class MapDocument;
class Zoomable;

class ZTilesetThumbView : public QTableView
{
    Q_OBJECT

public:
    ZTilesetThumbView(QWidget *parent = 0);

    QSize sizeHint() const;

    ZTilesetThumbModel *model() const
    { return mModel; }

	void setMapDocument(MapDocument *mapDoc);
	int contentWidth() const { return mContentWidth; }

protected:
    void contextMenuEvent(QContextMenuEvent *event);

private:
    MapDocument *mMapDocument;
	ZTilesetThumbModel *mModel;
	int mContentWidth;
};

} // namespace Internal
} // namespace Tiled

Q_DECLARE_METATYPE(Tiled::Internal::ZTilesetThumbView *)

#endif // ZTILESETTHUMBVIEW_H
