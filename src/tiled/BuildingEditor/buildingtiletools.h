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

#ifndef BUILDINGTILETOOLS_H
#define BUILDINGTILETOOLS_H

#include <QObject>

#include <QGraphicsPolygonItem>
#include <QRectF>

class QAction;
class QGraphicsSceneMouseEvent;
class QUndoStack;

namespace BuildingEditor {

class BuildingDocument;
class BuildingFloor;
class BuildingTileModeScene;
class FloorTileGrid;

/////

class BaseTileTool : public QObject
{
    Q_OBJECT
public:
    BaseTileTool();

    virtual void setEditor(BuildingTileModeScene *editor);

    void setAction(QAction *action)
    { mAction = action; }

    QAction *action() const
    { return mAction; }

    void setEnabled(bool enabled);

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) = 0;

    virtual void currentModifiersChanged(Qt::KeyboardModifiers modifiers)
    { Q_UNUSED(modifiers) }

    Qt::KeyboardModifiers keyboardModifiers() const;

    bool controlModifier() const;
    bool shiftModifier() const;

    QString statusText() const
    { return mStatusText; }

    void setStatusText(const QString &text);

    BuildingDocument *document() const;
    BuildingFloor *floor() const;
    QUndoStack *undoStack() const;

    QString layerName() const;

    bool isCurrent();

signals:
    void statusTextChanged();

public slots:
    void makeCurrent();
    virtual void documentChanged() {}
    virtual void activate() = 0;
    virtual void deactivate() = 0;

protected:
    BuildingTileModeScene *mEditor;
    QAction *mAction;
    QString mStatusText;
};

/////

class TileToolManager : public QObject
{
    Q_OBJECT
public:
    static TileToolManager *instance();

    TileToolManager();

    void addTool(BaseTileTool *tool);
    void activateTool(BaseTileTool *tool);

    void toolEnabledChanged(BaseTileTool *tool, bool enabled);

    BaseTileTool *currentTool() const
    { return mCurrentTool; }

    void checkKeyboardModifiers(Qt::KeyboardModifiers modifiers);

    Qt::KeyboardModifiers keyboardModifiers()
    { return mCurrentModifiers; }

signals:
    void currentToolChanged(BaseTileTool *tool);
    void statusTextChanged(BaseTileTool *tool);

private slots:
    void currentToolStatusTextChanged();

private:
    static TileToolManager *mInstance;
    QList<BaseTileTool*> mTools;
    BaseTileTool *mCurrentTool;
    Qt::KeyboardModifiers mCurrentModifiers;
};

/////

class DrawTileToolCursor : public QGraphicsPolygonItem
{
public:
    DrawTileToolCursor(BuildingTileModeScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget);

    void setPolygonFromTileRect(const QRect &tileRect);

private:
    BuildingTileModeScene *mScene;
    QRectF mBoundingRect;
};

class DrawTileTool : public BaseTileTool
{
    Q_OBJECT
public:
    static DrawTileTool *instance();

    DrawTileTool();

    void documentChanged();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void currentModifiersChanged(Qt::KeyboardModifiers modifiers);

    void setTile(const QString &tileName);

    QString currentTile() const
    { return mTileName; }

public slots:
    void activate();
    void deactivate();

private:
    void beginCapture();
    void endCapture();
    void clearCaptureTiles();

    void updateCursor(const QPointF &scenePos);
    void updateStatusText();

private:
    static DrawTileTool *mInstance;
    bool mMouseDown;
    bool mErasing;
    QPointF mMouseScenePos;
    QPoint mStartTilePos;
    QPoint mCursorTilePos;
    QRect mCursorTileBounds;
    DrawTileToolCursor *mCursor;
    QRectF mCursorViewRect;

    bool mCapturing;
    FloorTileGrid *mCaptureTiles;

    QString mTileName;
};

class SelectTileTool : public BaseTileTool
{
    Q_OBJECT
public:
    static SelectTileTool *instance();

    SelectTileTool();

    void documentChanged();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void currentModifiersChanged(Qt::KeyboardModifiers modifiers);

public slots:
    void activate();
    void deactivate();

private:
    void updateCursor(const QPointF &scenePos, bool force = true);
    void updateStatusText();

private:
    static SelectTileTool *mInstance;

    enum SelectionMode {
        Replace,
        Add,
        Subtract,
        Intersect
    };

    SelectionMode mSelectionMode;
    bool mMouseDown;
    QPointF mMouseScenePos;
    QPoint mStartTilePos;
    QPoint mCursorTilePos;
    QRect mCursorTileBounds;
    DrawTileToolCursor *mCursor;
    QRectF mCursorViewRect;
    QRegion mSelectedRegion;
};

} // namespace BuildingEditor

#endif // BUILDINGTILETOOLS_H
