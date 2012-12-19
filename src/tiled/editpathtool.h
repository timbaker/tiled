/*
 * editpolygontool.h
 * Copyright 2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#ifndef EDITPATHTOOL_H
#define EDITPATHTOOL_H

#include "abstractpathtool.h"

#include <QMap>
#include <QSet>

class QGraphicsItem;

namespace Tiled {
namespace Internal {

class BetterSelectionRectangle;
class PathItem;
class PathPointHandle;

/**
 * A tool that allows dragging around the points of a path.
 */
class EditPathTool : public AbstractPathTool
{
    Q_OBJECT

public:
    explicit EditPathTool(QObject *parent = 0);
    ~EditPathTool();

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);

    void mouseEntered();
    void mouseMoved(const QPointF &pos,
                    Qt::KeyboardModifiers modifiers);
    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);
    void modifiersChanged(Qt::KeyboardModifiers modifiers);

    void languageChanged();

private slots:
    void updateHandles();
    void pathsRemoved(const QList<Path*> &paths);

    void deleteNodes();
    void joinNodes();
    void splitSegments();

private:
    enum Mode {
        NoMode,
        Selecting,
        Moving
    };

    void setSelectedHandles(const QSet<PathPointHandle*> &handles);
    void setSelectedHandle(PathPointHandle *handle)
    { setSelectedHandles(QSet<PathPointHandle*>() << handle); }

    void updateSelection(const QPointF &pos,
                         Qt::KeyboardModifiers modifiers);

    void startSelecting();

    void startMoving();
    void updateMovingItems(const QPointF &pos,
                           Qt::KeyboardModifiers modifiers);
    void finishMoving(const QPointF &pos);

    void showHandleContextMenu(PathPointHandle *clickedHandle, QPoint screenPos);

    BetterSelectionRectangle *mSelectionRectangle;
    bool mMousePressed;
    PathPointHandle *mClickedHandle;
    PathItem *mClickedPathItem;
    QMap<Path*, QPolygon> mOldPolygons;
    Mode mMode;
    QPointF mStart;
    Qt::KeyboardModifiers mModifiers;

    /// The list of handles associated with each selected path
    QMap<PathItem*, QList<PathPointHandle*> > mHandles;
    QSet<PathPointHandle*> mSelectedHandles;
};

} // namespace Internal
} // namespace Tiled

#endif // EDITPATHTOOL_H
