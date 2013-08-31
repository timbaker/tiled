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

#ifndef FENCETOOL_H
#define FENCETOOL_H

#include "abstracttiletool.h"

#include "BuildingEditor/singleton.h"

#include <QCoreApplication>
#include <QGraphicsPathItem>

class SimpleFileBlock;

class CompositeLayerGroup;

namespace Tiled {
class Tile;

namespace Internal {

class Fence
{
public:
    enum Shapes
    {
        West1,
        West2,
        North1,
        North2,
        NorthWest,
        GateSpaceW,
        GateSpaceN,
        GateDoorW,
        GateDoorN,
        Post,
        ShapeCount
    };

    Fence() :
        mTileNames(ShapeCount)
    {}

    QString mLabel;
    QString mLayer;
    QVector<QString> mTileNames;
};

class FenceFile
{
    Q_DECLARE_TR_FUNCTIONS(FenceFile)

public:
    FenceFile();
    ~FenceFile();

    bool read(const QString &fileName);

    QString errorString() const
    { return mError; }

    QList<Fence*> takeFences();

private:
    bool validKeys(SimpleFileBlock &block, const QStringList &keys);

private:
    QList<Fence*> mFences;
    QString mFileName;
    QString mError;
};

class FenceTool : public AbstractTileTool, public Singleton<FenceTool>
{
public:
    FenceTool(QObject *parent = 0);

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);

    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void languageChanged();

    void setFence(Fence *fence);
    void setPostGap(int gap);

protected:
    void tilePositionChanged(const QPoint &tilePos);

    QVector<Tile*> resolveTiles(Fence *fence);

private:
    void drawWestEdge(int sx, int sy, int ey);
    void drawNorthEdge(int sx, int sy, int ex);
    void drawPost(int x, int y);
    void drawGate(int x, int y, bool west);
    Tile *fenceTile(int x, int y, bool west);
    void getWestEdgeTiles(int sx, int sy, int ey, TileLayer &stamp);
    void getNorthEdgeTiles(int sx, int sy, int ex, TileLayer &stamp);
    Tile *gateTile(int x, int y, bool west);

    MapScene *mScene;
    CompositeLayerGroup *mToolTileLayerGroup;
    bool mInitialClick;
    QPointF mStartTilePosF;
    QPoint mStartTilePos;
    Fence *mFence;
    int mPostGap;
    QGraphicsPathItem *mCursorItem;
    QRectF mToolTilesRect;
};

} // namespace Internal
} // namespace Tiled

#endif // FENCETOOL_H
