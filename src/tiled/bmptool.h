/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#ifndef BMPTOOL_H
#define BMPTOOL_H

#include "abstracttool.h"

#include "map.h" // for MapRands

#include <QUndoCommand>

namespace Tiled {
class Layer;

namespace Internal {
class BmpToolDialog;
class BrushItem;

// Base class for all BMP-editing tools, shamelessly ripped from AbstractTileTool.
class AbstractBmpTool : public AbstractTool
{
    Q_OBJECT

public:
    /**
     * Constructs an abstract tile tool with the given \a name and \a icon.
     */
    AbstractBmpTool(const QString &name,
                     const QIcon &icon,
                     const QKeySequence &shortcut,
                     QObject *parent = 0);

    ~AbstractBmpTool();

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);

    void mouseEntered();
    void mouseLeft();
    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);

protected:
    void mapDocumentChanged(MapDocument *oldDocument,
                            MapDocument *newDocument);

    void updateEnabledState();

    virtual void tilePositionChanged(const QPoint &tilePos) = 0;

    virtual void updateStatusInfo();

    bool isBrushVisible() const { return mBrushVisible; }

    QPoint tilePosition() const { return QPoint(mTileX, mTileY); }

    BrushItem *brushItem() const { return mBrushItem; }

    Layer *currentLayer() const;

    MapScene *scene() const
    { return mScene; }

private:
    void setBrushVisible(bool visible);
    void updateBrushVisibility();

    MapScene *mScene;
    BrushItem *mBrushItem;
    int mTileX, mTileY;
    bool mBrushVisible;
};

// This tool is for painting (and erasing) pixels in a map's BMP images.
class BmpPainterTool : public AbstractBmpTool
{
    Q_OBJECT
public:
    static BmpPainterTool *instance();

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);

    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void setColor(int index, QRgb color)
    { mBmpIndex = index; mColor = color; }

    int bmpIndex() const
    { return mBmpIndex; }

    void setBrushSize(int size);

protected:
    void mapDocumentChanged(MapDocument *oldDocument,
                            MapDocument *newDocument);

    void languageChanged();

protected:
    void tilePositionChanged(const QPoint &tilePos);

    void paint(bool mergeable);

private:
    Q_DISABLE_COPY(BmpPainterTool)
    static BmpPainterTool *mInstance;
    BmpPainterTool(QObject *parent = 0);
    ~BmpPainterTool();

    bool mPainting;
    QPoint mStampPos;
    bool mErasing;
    int mBmpIndex;
    QRgb mColor;
    int mBrushSize;
    BmpToolDialog *mDialog;
};

// This tool is for selecting and moving pixels in a map's BMP images.
class BmpSelectionTool : public AbstractBmpTool
{
    Q_OBJECT

public:
    static BmpSelectionTool *instance();

    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);

    void languageChanged();

protected:
    void tilePositionChanged(const QPoint &tilePos);

    void updateStatusInfo();

private:
    Q_DISABLE_COPY(BmpSelectionTool)
    static BmpSelectionTool *mInstance;
    BmpSelectionTool(QObject *parent = 0);

    enum Mode {
        NoMode,
        Selecting,
        Dragging
    };

    enum SelectionMode {
        Replace,
        Add,
        Subtract,
        Intersect
    };

    QRect selectedArea() const;

    Mode mMode;
    QPoint mDragStart;

    QPointF mStartScenePos;
    bool mMouseDown;
    bool mMouseMoved;

    QPoint mSelectionStart;
    SelectionMode mSelectionMode;
    bool mSelecting;
};

/////

class ChangeBmpSelection : public QUndoCommand
{
public:
    ChangeBmpSelection(MapDocument *mapDocument, const QRegion &selection);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    MapDocument *mMapDocument;
    QRegion mSelection;
};

class ResizeBmpImage : public QUndoCommand
{
public:
    ResizeBmpImage(MapDocument *mapDocument, int bmpIndex, const QSize &size,
              const QPoint &offset);

    void undo();
    void redo();

private:
    MapDocument *mMapDocument;
    int mBmpIndex;
    QImage mOriginal;
    QImage mResized;
};

class ResizeBmpRands : public QUndoCommand
{
public:
    ResizeBmpRands(MapDocument *mapDocument, int bmpIndex, const QSize &size);

    void undo();
    void redo();

private:
    MapDocument *mMapDocument;
    int mBmpIndex;
    MapRands mOriginal;
    MapRands mResized;
};

/////

} // namespace Internal
} // namespace Tiled

#endif // BMPTOOL_H
