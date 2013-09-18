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

#ifndef LUATILETOOL_H
#define LUATILETOOL_H

#include "abstracttiletool.h"
#include "luatooloptions.h"

#include <QCoreApplication>
#include <QMap>

class DistanceIndicator;
class QGraphicsPathItem;
class SimpleFileBlock;

extern "C" {
struct lua_State;
}

namespace Tiled {
class MapNoBlend;
class MapRenderer;
class Tile;

namespace Lua {
class LuaMap;
class LuaTileLayer;

class LuaTileTool : public Tiled::Internal::AbstractTileTool
{
    Q_OBJECT
public:
    LuaTileTool(const QString &scriptFileName,
                const QString &dialogTitle,
                const QString &name,
                const QIcon &icon,
                const QKeySequence &shortcut,
                QObject *parent);
    ~LuaTileTool();

    void loadScript();

    void activate(Tiled::Internal::MapScene *scene);
    void deactivate(Tiled::Internal::MapScene *scene);

    void mouseLeft();

    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);
    void modifiersChanged(Qt::KeyboardModifiers modifiers);

    void languageChanged();
    void tilePositionChanged(const QPoint &tilePos);

    // Callable from Lua scripts
    enum CursorType {
        None,
        CurbTool,
        EdgeTool
    };
    void setCursorType(CursorType type);
    LuaTileLayer *currentLayer() const;
    void applyChanges(const char *undoText);
    int angle(float x1, float y1, float x2, float y2);
    void clearToolTiles();
    void setToolTile(const char *layer, int x, int y, Tile *tile);
    void setToolTile(const char *layer, const QRegion &rgn, Tile *tile);
    void clearToolNoBlends();
    void setToolNoBlend(const char *layer, int x, int y, bool noBlend);
    void setToolNoBlend(const char *layer, const QRegion &rgn, bool noBlend);
    void clearDistanceIndicators();
    void indicateDistance(int x1, int y1, int x2, int y2);
    bool dragged();
    //

protected slots:
    void setOption(LuaToolOption *option, const QVariant &value);
    void mapChanged() { mMapChanged = true; }

protected:
    QPainterPath cursorShape(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void mouseEvent(const char *func, Qt::MouseButtons buttons,
                    const QPointF &scenePos, Qt::KeyboardModifiers modifiers);
    void checkMap();
    void setToolOptions();
    bool parseToolOption(QString &err);

private:
    QString mFileName;
    QString mDialogTitle;
    lua_State *L;
    LuaMap *mMap;
    bool mMapChanged;
    Internal::MapScene *mScene;
    Qt::MouseButtons mButtons;
    QMap<QString,Tiled::TileLayer*> mToolTileLayers;
    QMap<QString,QRegion> mToolTileRegions;
    QMap<QString,Tiled::MapNoBlend*> mToolNoBlends;
    QMap<QString,QRegion> mToolNoBlendRegions;
    QPointF mStartScenePos;
    QPointF mCurrentScenePos;

    enum Edge {
        EdgeW,
        EdgeN,
        EdgeE,
        EdgeS
    };

    QGraphicsPathItem *mCursorItem;
    CursorType mCursorType;

    DistanceIndicator *mDistanceIndicators[4];

    Lua::LuaToolOptions mOptions;
    bool mSaveOptionValue;
};

class LuaToolInfo
{
public:
    QString mLabel;
    QIcon mIcon;
    QString mScript;
    QString mDialogTitle;
};

class LuaToolFile
{
    Q_DECLARE_TR_FUNCTIONS(LuaToolFile)

public:
    LuaToolFile();
    ~LuaToolFile();

    bool read(const QString &fileName);

    QString errorString() const
    { return mError; }

    QList<LuaToolInfo> takeTools();

private:
    bool validKeys(SimpleFileBlock &block, const QStringList &keys);

private:
    QList<LuaToolInfo> mTools;
    QString mFileName;
    QString mError;
};

} // namespace Lua
} // namespace Tiled


#include <QGraphicsItem>

class UnscaledLabelItem : public QGraphicsSimpleTextItem
{
public:
    UnscaledLabelItem(QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    QPainterPath shape() const;
    bool contains(const QPointF &point) const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget);

    void synch();

private:
    QColor mBgColor;
};

class DistanceIndicator : public QGraphicsItem
{
public:
    enum Dir {
        West,
        North,
        East,
        South
    };

    DistanceIndicator(Dir direction, QGraphicsItem *parent = 0);

    void setInfo(const QPoint &tilePos, int dist);
    void setRenderer(Tiled::MapRenderer *renderer);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    Dir mDirection;
    QPoint mTilePos;
    int mDistance;
    Tiled::MapRenderer *mRenderer;
    UnscaledLabelItem *mTextItem;
};

#endif // LUATILETOOL_H
