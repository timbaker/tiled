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

#ifndef BUILDINGTOOLS_H
#define BUILDINGTOOLS_H

#include <QObject>
#include <QPointF>
#include <QSet>

class QAction;
class QGraphicsRectItem;
class QGraphicsSceneMouseEvent;

namespace BuildingEditor {

class BaseMapObject;
class Door;
class FloorEditor;
class GraphicsObjectItem;
class Window;

/////

class BaseTool : public QObject
{
    Q_OBJECT
public:
    BaseTool() :
        QObject(0),
        mEditor(0)
    {}

    virtual void setEditor(FloorEditor *editor)
    { mEditor = editor; }

    void setAction(QAction *action)
    { mAction = action; }

    QAction *action() const
    { return mAction; }

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) = 0;

public slots:
    virtual void activate() = 0;
    virtual void deactivate() = 0;

protected:
    FloorEditor *mEditor;
    QAction *mAction;
};

class PencilTool : public BaseTool
{
    Q_OBJECT
public:
    static PencilTool *instance();

    PencilTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

public slots:
    void activate();
    void deactivate();

private:
    void updateCursor(const QPointF &scenePos);

private:
    static PencilTool *mInstance;
    bool mMouseDown;
    bool mInitialPaint;
    QGraphicsRectItem *mCursor;
};

class EraserTool : public BaseTool
{
    Q_OBJECT
public:
    static EraserTool *instance();

    EraserTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

private:
    void updateCursor(const QPointF &scenePos);

public slots:
    void activate();
    void deactivate();

private:
    static EraserTool *mInstance;
    bool mMouseDown;
    bool mInitialPaint;
    QGraphicsRectItem *mCursor;
};

/////

class BaseObjectTool : public BaseTool
{
    Q_OBJECT
public:
    BaseObjectTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

public slots:
    void activate();
    void deactivate();

protected:
    void setCursorObject(BaseMapObject *object);

    enum TileEdge {
        Center,
        N,
        S,
        W,
        E
    };

    QPoint mTilePos;
    TileEdge mTileEdge;
    GraphicsObjectItem *mCursorItem;
};

class DoorTool : public BaseObjectTool
{
    Q_OBJECT
public:
    static DoorTool *instance();

    DoorTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);

private:
    static DoorTool *mInstance;
    Door *mCursorObject;
};

class WindowTool : public BaseObjectTool
{
    Q_OBJECT
public:
    static WindowTool *instance();

    WindowTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);

private:
    static WindowTool *mInstance;
    Window *mCursorObject;
};

/////

class SelectMoveObjectTool : public BaseTool
{
    Q_OBJECT
public:
    static SelectMoveObjectTool *instance();

    SelectMoveObjectTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

public slots:
    void activate();
    void deactivate();

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

    BaseMapObject *topmostObjectAt(const QPointF &scenePos);

    static SelectMoveObjectTool *mInstance;

    Mode mMode;
    bool mMouseDown;
    QPointF mStartScenePos;
    QPoint mDragOffset;
    BaseMapObject *mClickedObject;
    QSet<BaseMapObject*> mMovingObjects;
    QGraphicsRectItem *mSelectionRectItem;
};

} // namespace BuildingEditor

#endif // BUILDINGTOOLS_H
