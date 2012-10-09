/*
 * editpathtool.cpp
 * Copyright 2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#include "editpathtool.h"

#include "addremovepath.h"
#include "changepathpolygon.h"
#include "layer.h"
#include "map.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "maprenderer.h"
#include "mapscene.h"
#include "pathitem.h"
#include "pathlayer.h"
#include "preferences.h"
#include "rangeset.h"
#include "selectionrectangle.h"
#include "utils.h"

#include <QApplication>
#include <QGraphicsItem>
#include <QMenu>
#include <QPainter>
#include <QPalette>
#include <QUndoStack>

using namespace Tiled;
using namespace Tiled::Internal;

namespace Tiled {
namespace Internal {

/**
 * A handle that allows moving around a point of a polygon.
 */
class PathPointHandle : public QGraphicsItem
{
public:
    PathPointHandle(PathItem *pathItem, int pointIndex)
        : QGraphicsItem()
        , mPathItem(pathItem)
        , mPointIndex(pointIndex)
        , mSelected(false)
        , mDragging(false)
    {
        setFlags(QGraphicsItem::ItemIgnoresTransformations |
                 QGraphicsItem::ItemIgnoresParentOpacity);
        setZValue(10000);
        setCursor(Qt::SizeAllCursor);
    }

    Path *path() const { return mPathItem->path(); }

    int pointIndex() const { return mPointIndex; }

    QPoint pointPosition() const;
    void setPointPosition(const QPoint &pos);

    // These hide the QGraphicsItem members
    void setSelected(bool selected) { mSelected = selected; update(); }
    bool isSelected() const { return mSelected; }

    void setDragging(bool dragging)
    {
        if (dragging != mDragging) {
            mDragging = dragging;
            if (mDragging)
                mStartPos = pointPosition();
        }
    }

    void setDragOffset(const QPoint &offset)
    {
        mDragOffset = offset;
        update();
    }
    QPoint dragOffset() const
    { return mDragOffset; }

    QPoint startPosition() const
    { return mStartPos; }

    QRectF boundingRect() const;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

private:
    PathItem *mPathItem;
    int mPointIndex;
    bool mSelected;
    bool mDragging;
    QPoint mDragOffset;
    QPoint mStartPos;
};

} // namespace Internal
} // namespace Tiled

QPoint PathPointHandle::pointPosition() const
{
    Path *path = mPathItem->path();
    return path->polygon().at(mPointIndex);
}

void PathPointHandle::setPointPosition(const QPoint &pos)
{
    Path *path = mPathItem->path();
    path->setPoint(mPointIndex, PathPoint(pos.x(), pos.y()));
    mPathItem->syncWithPath();
}

QRectF PathPointHandle::boundingRect() const
{
    return QRectF(-5, -5, 10 + 1, 10 + 1);
}

void PathPointHandle::paint(QPainter *painter,
                        const QStyleOptionGraphicsItem *,
                        QWidget *)
{
    painter->setPen(Qt::black);
    if (mSelected) {
        painter->setBrush(QApplication::palette().highlight());
        painter->drawRect(QRectF(-4, -4, 8, 8));
    } else {
        painter->setBrush(Qt::lightGray);
        painter->drawRect(QRectF(-3, -3, 6, 6));
    }
}

/////

EditPathTool::EditPathTool(QObject *parent)
    : AbstractPathTool(tr("Edit Paths"),
          QIcon(QLatin1String(":images/24x24/tool-edit-polygons.png")),
          QKeySequence(tr("E")),
          parent)
    , mSelectionRectangle(new SelectionRectangle)
    , mMousePressed(false)
    , mClickedHandle(0)
    , mClickedPathItem(0)
    , mMode(NoMode)
{
}

EditPathTool::~EditPathTool()
{
    delete mSelectionRectangle;
}

void EditPathTool::activate(MapScene *scene)
{
    AbstractPathTool::activate(scene);

    updateHandles();

    // TODO: Could be more optimal by separating the updating of handles from
    // the creation and removal of handles depending on changes in the
    // selection, and by only updating the handles of the paths that changed.
    connect(mapDocument(), SIGNAL(pathsChanged(QList<Path*>)),
            this, SLOT(updateHandles()));
    connect(scene, SIGNAL(selectedPathItemsChanged()),
            this, SLOT(updateHandles()));

    connect(mapDocument(), SIGNAL(pathsRemoved(QList<Path*>)),
            this, SLOT(pathsRemoved(QList<Path*>)));
}

void EditPathTool::deactivate(MapScene *scene)
{
    disconnect(mapDocument(), SIGNAL(pathsChanged(QList<Path*>)),
               this, SLOT(updateHandles()));
    disconnect(scene, SIGNAL(selectedPathItemsChanged()),
               this, SLOT(updateHandles()));

    // Delete all handles
    QMapIterator<PathItem*, QList<PathPointHandle*> > i(mHandles);
    while (i.hasNext())
        qDeleteAll(i.next().value());

    mHandles.clear();
    mSelectedHandles.clear();
    mClickedHandle = 0;

    AbstractPathTool::deactivate(scene);
}

void EditPathTool::mouseEntered()
{
}

void EditPathTool::mouseMoved(const QPointF &pos,
                                 Qt::KeyboardModifiers modifiers)
{
    AbstractPathTool::mouseMoved(pos, modifiers);

    if (mMode == NoMode && mMousePressed) {
        const int dragDistance = (mStart - pos).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedHandle)
                startMoving();
            else
                startSelecting();
        }
    }

    switch (mMode) {
    case Selecting:
        mSelectionRectangle->setRectangle(QRectF(mStart, pos).normalized());
        break;
    case Moving:
        updateMovingItems(pos, modifiers);
        break;
    case NoMode:
        break;
    }
}

template <class T>
static T *first(const QList<QGraphicsItem *> &items)
{
    foreach (QGraphicsItem *item, items) {
        if (T *t = dynamic_cast<T*>(item))
            return t;
    }
    return 0;
}

void EditPathTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (mMode != NoMode) // Ignore additional presses during select/move
        return;

    switch (event->button()) {
    case Qt::LeftButton: {
        mMousePressed = true;
        mStart = event->scenePos();

        const QList<QGraphicsItem *> items = mapScene()->items(mStart);
        mClickedPathItem = first<PathItem>(items);
        mClickedHandle = first<PathPointHandle>(items);
        break;
    }
    case Qt::RightButton: {
        QList<QGraphicsItem *> items = mapScene()->items(event->scenePos());
        PathPointHandle *clickedHandle = first<PathPointHandle>(items);
        if (clickedHandle || !mSelectedHandles.isEmpty()) {
            showHandleContextMenu(clickedHandle,
                                  event->screenPos());
        } else {
            AbstractPathTool::mousePressed(event);
        }
        break;
    }
    default:
        AbstractPathTool::mousePressed(event);
        break;
    }
}

void EditPathTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode:
        if (mClickedHandle) {
            QSet<PathPointHandle*> selection = mSelectedHandles;
            const Qt::KeyboardModifiers modifiers = event->modifiers();
            if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
                if (selection.contains(mClickedHandle))
                    selection.remove(mClickedHandle);
                else
                    selection.insert(mClickedHandle);
            } else {
                selection.clear();
                selection.insert(mClickedHandle);
            }
            setSelectedHandles(selection);
        } else if (mClickedPathItem) {
            QSet<PathItem*> selection = mapScene()->selectedPathItems();
            const Qt::KeyboardModifiers modifiers = event->modifiers();
            if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
                if (selection.contains(mClickedPathItem))
                    selection.remove(mClickedPathItem);
                else
                    selection.insert(mClickedPathItem);
            } else {
                selection.clear();
                selection.insert(mClickedPathItem);
            }
            mapScene()->setSelectedPathItems(selection);
            updateHandles();
        } else if (!mSelectedHandles.isEmpty()) {
            // First clear the handle selection
            setSelectedHandles(QSet<PathPointHandle*>());
        } else {
            // If there is no handle selection, clear the path selection
            mapScene()->setSelectedPathItems(QSet<PathItem*>());
            updateHandles();
        }
        break;
    case Selecting:
        updateSelection(event->scenePos(), event->modifiers());
        mapScene()->removeItem(mSelectionRectangle);
        mMode = NoMode;
        break;
    case Moving:
        finishMoving(event->scenePos());
        break;
    }

    mMousePressed = false;
    mClickedHandle = 0;
}

void EditPathTool::modifiersChanged(Qt::KeyboardModifiers modifiers)
{
    mModifiers = modifiers;
}

void EditPathTool::languageChanged()
{
    setName(tr("Edit Polygons"));
    setShortcut(QKeySequence(tr("E")));
}

void EditPathTool::setSelectedHandles(const QSet<PathPointHandle *> &handles)
{
    foreach (PathPointHandle *handle, mSelectedHandles)
        if (!handles.contains(handle))
            handle->setSelected(false);

    foreach (PathPointHandle *handle, handles)
        if (!mSelectedHandles.contains(handle))
            handle->setSelected(true);

    mSelectedHandles = handles;
}

/**
 * Creates and removes handle instances as necessary to adapt to a new path
 * selection.
 */
void EditPathTool::updateHandles()
{
    const QSet<PathItem*> &selection = mapScene()->selectedPathItems();

    // First destroy the handles for paths that are no longer selected
    QMutableMapIterator<PathItem*, QList<PathPointHandle*> > i(mHandles);
    while (i.hasNext()) {
        i.next();
        if (!selection.contains(i.key())) {
            foreach (PathPointHandle *handle, i.value()) {
                if (handle->isSelected())
                    mSelectedHandles.remove(handle);
                delete handle;
            }

            i.remove();
        }
    }

    MapRenderer *renderer = mapDocument()->renderer();

    foreach (PathItem *item, selection) {
        const Path *path = item->path();

        QPolygonF polygon = path->polygonf();

        QList<PathPointHandle*> pointHandles = mHandles.value(item);

        // Create missing handles
        while (pointHandles.size() < polygon.size()) {
            PathPointHandle *handle = new PathPointHandle(item, pointHandles.size());
            pointHandles.append(handle);
            mapScene()->addItem(handle);
        }

        // Remove superfluous handles
        while (pointHandles.size() > polygon.size()) {
            PathPointHandle *handle = pointHandles.takeLast();
            if (handle->isSelected())
                mSelectedHandles.remove(handle);
            delete handle;
        }

        // Update the position of all handles
        for (int i = 0; i < pointHandles.size(); ++i) {
            const QPointF &point = polygon.at(i);
#ifdef ZOMBOID
            Layer *layer = pointHandles[i]->path()->pathLayer();
            const QPointF handlePos = renderer->tileToPixelCoords(point, layer->level());
#else
            const QPointF handlePos = renderer->tileToPixelCoords(point);
#endif
            pointHandles.at(i)->setPos(handlePos);
        }

        mHandles.insert(item, pointHandles);
    }
}

void EditPathTool::pathsRemoved(const QList<Path *> &paths)
{
    if (mMode == Moving) {
        // Make sure we're not going to try to still change these paths when
        // finishing the move operation.
        // TODO: In addition to avoiding crashes, it would also be good to
        // disallow other actions while moving.
        foreach (Path *path, paths)
            mOldPolygons.remove(path);
    }
}

void EditPathTool::updateSelection(const QPointF &pos,
                                      Qt::KeyboardModifiers modifiers)
{
    QRectF rect = QRectF(mStart, pos).normalized();

    // Make sure the rect has some contents, otherwise intersects returns false
    rect.setWidth(qMax(qreal(1), rect.width()));
    rect.setHeight(qMax(qreal(1), rect.height()));

    const QSet<PathItem*> oldSelection = mapScene()->selectedPathItems();

    if (oldSelection.isEmpty()) {
        // Allow selecting some map paths only when there aren't any selected
        QSet<PathItem*> selectedItems;

        foreach (QGraphicsItem *item, mapScene()->items(rect)) {
            PathItem *pathItem = dynamic_cast<PathItem*>(item);
            if (pathItem)
                selectedItems.insert(pathItem);
        }


        QSet<PathItem*> newSelection;

        if (modifiers & (Qt::ControlModifier | Qt::ShiftModifier)) {
            newSelection = oldSelection | selectedItems;
        } else {
            newSelection = selectedItems;
        }

        mapScene()->setSelectedPathItems(newSelection);
        updateHandles();
    } else {
        // Update the selected handles
        QSet<PathPointHandle*> selectedHandles;

        foreach (QGraphicsItem *item, mapScene()->items(rect)) {
            if (PathPointHandle *handle = dynamic_cast<PathPointHandle*>(item))
                selectedHandles.insert(handle);
        }

        if (modifiers & (Qt::ControlModifier | Qt::ShiftModifier))
            setSelectedHandles(mSelectedHandles | selectedHandles);
        else
            setSelectedHandles(selectedHandles);
    }
}

void EditPathTool::startSelecting()
{
    mMode = Selecting;
    mapScene()->addItem(mSelectionRectangle);
}

void EditPathTool::startMoving()
{
    // Move only the clicked handle, if it was not part of the selection
    if (!mSelectedHandles.contains(mClickedHandle))
        setSelectedHandle(mClickedHandle);

    mOldPolygons.clear();
    foreach (PathPointHandle *handle, mSelectedHandles)
        mOldPolygons.insert(handle->path(), handle->path()->polygon());

    mMode = Moving;
}

void EditPathTool::updateMovingItems(const QPointF &pos,
                                     Qt::KeyboardModifiers modifiers)
{
    MapRenderer *renderer = mapDocument()->renderer();

    // Don't use toPoint, it rounds up
    QPoint startPos = renderer->pixelToTileCoordsInt(mStart, currentPathLayer()->level());
    QPoint curPos = renderer->pixelToTileCoordsInt(pos, currentPathLayer()->level());
    QPoint diff = curPos - startPos;

    int i = 0;
    foreach (PathPointHandle *handle, mSelectedHandles) {
        handle->setDragging(true);
        handle->setDragOffset(diff);
        QPointF newScenePos = renderer->tileToPixelCoords(handle->startPosition() + diff + QPointF(0.5, 0.5),
                                                          handle->path()->pathLayer()->level());
        handle->setPos(newScenePos);
        handle->setPointPosition(handle->startPosition() + diff);
        ++i;
    }
}

void EditPathTool::finishMoving(const QPointF &pos)
{
    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    foreach (PathPointHandle *handle, mSelectedHandles)
        handle->setDragging(false);

    if (mClickedHandle->dragOffset().isNull()) // Move is a no-op
        return;

    QUndoStack *undoStack = mapDocument()->undoStack();
    undoStack->beginMacro(tr("Move %n Point(s)", "", mSelectedHandles.size()));

    QMapIterator<Path*, QPolygon> i(mOldPolygons);
    while (i.hasNext()) {
        i.next();
        undoStack->push(new ChangePathPolygon(mapDocument(), i.key(), i.value()));
    }

    undoStack->endMacro();

    mOldPolygons.clear();
}

void EditPathTool::showHandleContextMenu(PathPointHandle *clickedHandle,
                                            QPoint screenPos)
{
    if (clickedHandle && !mSelectedHandles.contains(clickedHandle))
        setSelectedHandle(clickedHandle);

    const int n = mSelectedHandles.size();
    Q_ASSERT(n > 0);

    QIcon delIcon(QLatin1String(":images/16x16/edit-delete.png"));
    QString delText = tr("Delete %n Node(s)", "", n);

    QMenu menu;

    QAction *deleteNodesAction = menu.addAction(delIcon, delText);
    QAction *joinNodesAction = menu.addAction(tr("Join Nodes"));
    QAction *splitSegmentsAction = menu.addAction(tr("Split Segments"));

    Utils::setThemeIcon(deleteNodesAction, "edit-delete");

    joinNodesAction->setEnabled(n > 1);
    splitSegmentsAction->setEnabled(n > 1);

    connect(deleteNodesAction, SIGNAL(triggered()), SLOT(deleteNodes()));
    connect(joinNodesAction, SIGNAL(triggered()), SLOT(joinNodes()));
    connect(splitSegmentsAction, SIGNAL(triggered()), SLOT(splitSegments()));

    menu.exec(screenPos);
}

typedef QMap<Path*, RangeSet<int> > PointIndexesByPath;
static PointIndexesByPath
groupIndexesByObject(const QSet<PathPointHandle*> &handles)
{
    PointIndexesByPath result;

    // Build the list of point indexes for each map path
    foreach (PathPointHandle *handle, handles) {
        RangeSet<int> &pointIndexes = result[handle->path()];
        pointIndexes.insert(handle->pointIndex());
    }

    return result;
}

void EditPathTool::deleteNodes()
{
    if (mSelectedHandles.isEmpty())
        return;

    PointIndexesByPath p = groupIndexesByObject(mSelectedHandles);
    QMapIterator<Path*, RangeSet<int> > i(p);

    QUndoStack *undoStack = mapDocument()->undoStack();

    QString delText = tr("Delete %n Node(s)", "", mSelectedHandles.size());
    undoStack->beginMacro(delText);

    while (i.hasNext()) {
        Path *path = i.next().key();
        const RangeSet<int> &indexRanges = i.value();

        QPolygon oldPolygon = path->polygon();
        QPolygon newPolygon = oldPolygon;

        // Remove points, back to front to keep the indexes valid
        RangeSet<int>::Range it = indexRanges.end();
        RangeSet<int>::Range begin = indexRanges.begin();
        // assert: end != begin, since there is at least one entry
        do {
            --it;
            newPolygon.remove(it.first(), it.length());
        } while (it != begin);

        if (newPolygon.size() < 2) {
            // We've removed the entire path
            undoStack->push(new RemovePath(mapDocument(), path));
        } else {
            path->setPolygon(newPolygon);
            undoStack->push(new ChangePathPolygon(mapDocument(), path,
                                                 oldPolygon));
        }
    }

    undoStack->endMacro();
}

/**
 * Joins the nodes at the given \a indexRanges. Each consecutive sequence
 * of nodes will be joined into a single node at the average location.
 *
 * This method can deal with both polygons as well as polylines. For polygons,
 * pass <code>true</code> for \a closed.
 */
static QPolygon joinPolygonNodes(const QPolygon &polygon,
                                  const RangeSet<int> &indexRanges,
                                  bool closed)
{
    if (indexRanges.isEmpty())
        return polygon;

    // Do nothing when dealing with a polygon with less than 3 points
    // (we'd no longer have a polygon)
    const int n = polygon.size();
    if (n < 3)
        return polygon;

    RangeSet<int>::Range firstRange = indexRanges.begin();
    RangeSet<int>::Range it = indexRanges.end();

    RangeSet<int>::Range lastRange = it;
    --lastRange; // We know there is at least one range

    QPolygon result = polygon;

    // Indexes need to be offset when first and last range are joined.
    int indexOffset = 0;

    // Check whether the first and last ranges connect
    if (firstRange.first() == 0 && lastRange.last() == n - 1) {
        // Do nothing when the selection spans the whole polygon
        if (firstRange == lastRange)
            return polygon;

        // Join points of the first and last range when the polygon is closed
        if (closed) {
            QPoint averagePoint;
            for (int i = firstRange.first(); i <= firstRange.last(); i++)
                averagePoint += polygon.at(i);
            for (int i = lastRange.first(); i <= lastRange.last(); i++)
                averagePoint += polygon.at(i);
            averagePoint /= firstRange.length() + lastRange.length();

            result.remove(lastRange.first(), lastRange.length());
            result.remove(1, firstRange.length() - 1);
            result.replace(0, averagePoint);

            indexOffset = firstRange.length() - 1;

            // We have dealt with these ranges now
            // assert: firstRange != lastRange
            ++firstRange;
            --it;
        }
    }

    while (it != firstRange) {
        --it;

        // Merge the consecutive nodes into a single average point
        QPoint averagePoint;
        for (int i = it.first(); i <= it.last(); i++)
            averagePoint += polygon.at(i - indexOffset);
        averagePoint /= it.length();

        result.remove(it.first() + 1 - indexOffset, it.length() - 1);
        result.replace(it.first() - indexOffset, averagePoint);
    }

    return result;
}

/**
 * Splits the selected segments by inserting new nodes in the middle. The
 * selected segments are defined by each pair of consecutive \a indexRanges.
 *
 * This method can deal with both polygons as well as polylines. For polygons,
 * pass <code>true</code> for \a closed.
 */
static QPolygon splitPolygonSegments(const QPolygon &polygon,
                                     const RangeSet<int> &indexRanges,
                                     bool closed)
{
    if (indexRanges.isEmpty())
        return polygon;

    const int n = polygon.size();

    QPolygon result = polygon;

    RangeSet<int>::Range firstRange = indexRanges.begin();
    RangeSet<int>::Range it = indexRanges.end();
    // assert: firstRange != it

    if (closed) {
        RangeSet<int>::Range lastRange = it;
        --lastRange; // We know there is at least one range

        // Handle the case where the first and last nodes are selected
        if (firstRange.first() == 0 && lastRange.last() == n - 1) {
            const QPoint splitPoint = (result.first() + result.last()) / 2;
            result.append(splitPoint);
        }
    }

    do {
        --it;

        for (int i = it.last(); i > it.first(); --i) {
            const QPoint splitPoint = (result.at(i) + result.at(i - 1)) / 2;
            result.insert(i, splitPoint);
        }
    } while (it != firstRange);

    return result;
}


void EditPathTool::joinNodes()
{
    if (mSelectedHandles.size() < 2)
        return;

    const PointIndexesByPath p = groupIndexesByObject(mSelectedHandles);
    QMapIterator<Path*, RangeSet<int> > i(p);

    QUndoStack *undoStack = mapDocument()->undoStack();
    bool macroStarted = false;

    while (i.hasNext()) {
        Path *path = i.next().key();
        const RangeSet<int> &indexRanges = i.value();

        const bool closed = path->isClosed();
        QPolygon oldPolygon = path->polygon();
        QPolygon newPolygon = joinPolygonNodes(oldPolygon, indexRanges,
                                               closed);

        if (newPolygon.size() < oldPolygon.size()) {
            if (!macroStarted) {
                undoStack->beginMacro(tr("Join Nodes"));
                macroStarted = true;
            }

            path->setPolygon(newPolygon);
            undoStack->push(new ChangePathPolygon(mapDocument(), path,
                                                 oldPolygon));
        }
    }

    if (macroStarted)
        undoStack->endMacro();
}

void EditPathTool::splitSegments()
{
    if (mSelectedHandles.size() < 2)
        return;

    const PointIndexesByPath p = groupIndexesByObject(mSelectedHandles);
    QMapIterator<Path*, RangeSet<int> > i(p);

    QUndoStack *undoStack = mapDocument()->undoStack();
    bool macroStarted = false;

    while (i.hasNext()) {
        Path *path = i.next().key();
        const RangeSet<int> &indexRanges = i.value();

        const bool closed = path->isClosed();
        QPolygon oldPolygon = path->polygon();
        QPolygon newPolygon = splitPolygonSegments(oldPolygon, indexRanges,
                                                   closed);

        if (newPolygon.size() > oldPolygon.size()) {
            if (!macroStarted) {
                undoStack->beginMacro(tr("Split Segments"));
                macroStarted = true;
            }

            path->setPolygon(newPolygon);
            undoStack->push(new ChangePathPolygon(mapDocument(), path,
                                                 oldPolygon));
        }
    }

    if (macroStarted)
        undoStack->endMacro();
}
