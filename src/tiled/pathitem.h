/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef PATHITEM_H
#define PATHITEM_H

#include <QGraphicsItem>

namespace Tiled {
class Path;

namespace Internal {
class MapDocument;
class PathLayerItem;

class PathItem : public QGraphicsItem
{
public:
    PathItem(Path *path, MapDocument *mapDocument, PathLayerItem *parent = 0);

    // QGraphicsItem
    QRectF boundingRect() const;
    QPainterPath shape() const;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    Path *path() const
    { return mPath; }

    void setEditable(bool editable);

    bool isEditable() const
    { return mEditable; }

    void setDragging(bool dragging);

    void setDragOffset(const QPoint &offset);

    QPoint dragOffset() const
    { return mDragOffset; }

    void syncWithPath();

private:
    Path *mPath;
    MapDocument *mMapDocument;
    QRectF mBoundingRect;
    QColor mColor;
    bool mEditable;
    bool mDragging;
    QPoint mDragOffset;
    QPolygon mPolygon;
};

} // namespace Internal
} // namespace Tiled

#endif // PATHITEM_H
