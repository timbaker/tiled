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
#include <QSize>

class QAction;
class QGraphicsItem;
class QGraphicsRectItem;
class QGraphicsSceneMouseEvent;

namespace BuildingEditor {

class BuildingObject;
class BuildingTile;
class Door;
class FloorEditor;
class FurnitureTile;
class GraphicsObjectItem;
class GraphicsRoofItem;
class GraphicsRoofCornerItem;
class GraphicsRoofHandleItem;
class RoofCornerObject;
class RoofObject;
class Stairs;
class Window;

/////

class BaseTool : public QObject
{
    Q_OBJECT
public:
    BaseTool();

    virtual void setEditor(FloorEditor *editor);

    void setAction(QAction *action)
    { mAction = action; }

    QAction *action() const
    { return mAction; }

    void setEnabled(bool enabled);

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) = 0;

    QString statusText() const
    { return mStatusText; }

    void setStatusText(const QString &text);

signals:
    void statusTextChanged();

public slots:
    void makeCurrent();
    virtual void documentChanged() {}
    virtual void activate() = 0;
    virtual void deactivate() = 0;

protected:
    FloorEditor *mEditor;
    QAction *mAction;
    QString mStatusText;
};

/////

class ToolManager : public QObject
{
    Q_OBJECT
public:
    static ToolManager *instance();

    ToolManager();

    void addTool(BaseTool *tool);
    void activateTool(BaseTool *tool);

    void toolEnabledChanged(BaseTool *tool, bool enabled);

signals:
    void currentToolChanged(BaseTool *tool);
    void statusTextChanged(BaseTool *tool);

private slots:
    void currentToolStatusTextChanged();

private:
    static ToolManager *mInstance;
    QList<BaseTool*> mTools;
    BaseTool *mCurrentTool;
};

/////

class PencilTool : public BaseTool
{
    Q_OBJECT
public:
    static PencilTool *instance();

    PencilTool();

    void documentChanged();

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

    void documentChanged();

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
    void documentChanged();
    void activate();
    void deactivate();

protected:
    virtual void updateCursorObject() = 0;
    void setCursorObject(BuildingObject *object);
    virtual void placeObject() = 0;

    enum TileEdge {
        Center,
        N,
        S,
        W,
        E
    };

    QPoint mTilePos;
    TileEdge mTileEdge;
    BuildingObject *mCursorObject;
    GraphicsObjectItem *mCursorItem;
};

class DoorTool : public BaseObjectTool
{
    Q_OBJECT
public:
    static DoorTool *instance();

    DoorTool();

    void placeObject();
    void updateCursorObject();

private:
    static DoorTool *mInstance;
};

class WindowTool : public BaseObjectTool
{
    Q_OBJECT
public:
    static WindowTool *instance();

    WindowTool();

    void placeObject();
    void updateCursorObject();

private:
    static WindowTool *mInstance;
};

class StairsTool : public BaseObjectTool
{
    Q_OBJECT
public:
    static StairsTool *instance();

    StairsTool();

    void placeObject();
    void updateCursorObject();

private:
    static StairsTool *mInstance;
};

class FurnitureTool : public BaseObjectTool
{
    Q_OBJECT
public:
    static FurnitureTool *instance();

    FurnitureTool();

    void placeObject();
    void updateCursorObject();

    void setCurrentTile(FurnitureTile *tile);

    FurnitureTile *currentTile() const
    { return mCurrentTile; }

private:
    static FurnitureTool *mInstance;
    FurnitureTile *mCurrentTile;
};

class RoofTool : public BaseTool
{
    Q_OBJECT
public:
    static RoofTool *instance();

    RoofTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void documentChanged();
    void activate();
    void deactivate();

    void setCurrentTile(BuildingTile *btile)
    { mCurrentTile = btile; }

    BuildingTile *currentTile() const
    { return mCurrentTile; }

    void setCurrentCapTile(BuildingTile *btile)
    { mCurrentCapTile = btile; }

    BuildingTile *currentCapTile() const
    { return mCurrentCapTile; }

private slots:
    void objectAboutToBeRemoved(BuildingObject *object);

private:
    RoofObject *topmostRoofAt(const QPointF &scenePos);
    void updateHandle(const QPointF &scenePos);
    void resizeRoof(RoofObject *roof, int length, int thickness);
    void toggleShowWidth1(RoofObject *roof);
    void toggleShowWidth2(RoofObject *roof);
    void toggleCapped(RoofObject *roof);
    void setDepth(RoofObject *roof, int depth);

private:
    static RoofTool *mInstance;
    BuildingTile *mCurrentTile;
    BuildingTile *mCurrentCapTile;

    enum Mode {
        NoMode,
        Create,
        Resize
    };
    Mode mMode;

    QPoint mStartPos;
    QPoint mCurrentPos;
    RoofObject *mObject;
    GraphicsObjectItem *mItem;
    QGraphicsRectItem *mCursorItem;

    GraphicsRoofItem *mObjectItem; // item for roof object mouse is over
    RoofObject *mHandleObject; // roof object mouse is over
    GraphicsRoofHandleItem *mHandleItem;
    bool mMouseOverHandle;
    int mOriginalLength, mOriginalThickness;
};

/////

class RoofCornerTool : public BaseTool
{
    Q_OBJECT
public:
    static RoofCornerTool *instance();

    RoofCornerTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void documentChanged();
    void activate();
    void deactivate();

private slots:
    void objectAboutToBeRemoved(BuildingObject *object);

private:
    static RoofCornerTool *mInstance;

    enum Mode {
        NoMode,
        Create,
        Resize
    };
    Mode mMode;

private:
    RoofCornerObject *topmostRoofCornerAt(const QPointF &scenePos);
    void updateHandle(const QPointF &scenePos);
    void resizeRoof(RoofCornerObject *corner, int width, int height);
    void setDepth(RoofCornerObject *corner, int depth);
    void toggleOrient(RoofCornerObject *corner);

    QPoint mStartPos;
    QPoint mCurrentPos;
    RoofCornerObject *mObject;
    GraphicsObjectItem *mItem;
    QGraphicsRectItem *mCursorItem;

    GraphicsRoofCornerItem *mObjectItem; // item for roof object mouse is over
    RoofCornerObject *mHandleObject; // roof object mouse is over
    GraphicsRoofHandleItem *mHandleItem;
    bool mMouseOverHandle;
    int mOriginalWidth, mOriginalHeight;
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
    void documentChanged();
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

    static SelectMoveObjectTool *mInstance;

    Mode mMode;
    bool mMouseDown;
    QPointF mStartScenePos;
    QPoint mDragOffset;
    BuildingObject *mClickedObject;
    QSet<BuildingObject*> mMovingObjects;
    QGraphicsRectItem *mSelectionRectItem;
};

} // namespace BuildingEditor

#endif // BUILDINGTOOLS_H
