/*
 * objectselectiontool.h
 * Copyright 2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2012, Tim Baker
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

#ifndef SELECTPATHTOOL_H
#define SELECTPATHTOOL_H

#include "abstractpathtool.h"

#include <QSet>

namespace Tiled {
namespace Internal {

class PathItem;
class SelectionRectangle;

class SelectPathTool : public AbstractPathTool
{
    Q_OBJECT

public:
    explicit SelectPathTool(QObject *parent = 0);
    ~SelectPathTool();

    void mouseEntered();
    void mouseMoved(const QPointF &pos,
                    Qt::KeyboardModifiers modifiers);
    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);
    void modifiersChanged(Qt::KeyboardModifiers modifiers);

    void languageChanged();

private:
    enum Mode {
        NoMode,
        Selecting,
        Moving,
        CancelMoving
    };

    void updateSelection(const QPointF &pos,
                         Qt::KeyboardModifiers modifiers);

    void startSelecting();

    void startMoving();
    void updateMovingItems(const QPointF &pos,
                           Qt::KeyboardModifiers modifiers);
    void finishMoving(const QPointF &pos);
    void cancelMoving();

    SelectionRectangle *mSelectionRectangle;
    bool mMousePressed;
    PathItem *mClickedItem;
    QSet<PathItem*> mMovingItems;
    Mode mMode;
    QPointF mStartPos;
    Qt::KeyboardModifiers mModifiers;
};

} // namespace Internal
} // namespace Tiled

#endif // SELECTPATHTOOL_H
