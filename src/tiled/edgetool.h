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

#ifndef EDGETOOL_H
#define EDGETOOL_H

#include "abstracttiletool.h"

#include "BuildingEditor/singleton.h"

#include "tilelayer.h"

#include <QCoreApplication>
#include <QGraphicsPathItem>

class CompositeLayerGroup;
class SimpleFileBlock;

namespace Tiled {
class Tile;

namespace Internal {

class Edges
{
public:
    enum Shapes
    {
        EdgeW,
        EdgeN,
        EdgeE,
        EdgeS,
        InnerNW,
        InnerNE,
        InnerSE,
        InnerSW,
        OuterNW,
        OuterNE,
        OuterSE,
        OuterSW,
        ShapeCount
    };

    Edges() :
        mTileNames(ShapeCount)
    {}

    QString mLabel;
    QString mLayer;
    QVector<QString> mTileNames;
};

class EdgeFile
{
    Q_DECLARE_TR_FUNCTIONS(EdgeFile)

public:
    EdgeFile();
    ~EdgeFile();

    bool read(const QString &fileName);

    QString errorString() const
    { return mError; }

    QList<Edges*> takeEdges();

private:
    bool validKeys(SimpleFileBlock &block, const QStringList &keys);

private:
    QList<Edges*> mEdges;
    QString mFileName;
    QString mError;
};

class EdgeTool : public AbstractTileTool, public Singleton<EdgeTool>
{
public:
    EdgeTool(QObject *parent = 0);

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);

    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void languageChanged();

    void setEdges(Edges *edges);
    void setDash(int len, int gap);

    void setSuppressBlendTiles(bool suppress)
    { mSuppressBlendTiles = suppress; }
    bool suppressBlendTiles() const
    { return mSuppressBlendTiles; }

protected:
    void tilePositionChanged(const QPoint &tilePos);

    QVector<Tile*> resolveEdgeTiles(Edges *edges);

private:
    enum Edge
    {
        EdgeW,
        EdgeN,
        EdgeE,
        EdgeS
    };

    void drawEdge(const QPointF &start, const QPointF &end, Edge edge);
#if 0
    void drawEdgeTile(int x, int y, Edge edge);
    void drawGapTile(int x, int y);
#endif

    void getMapChanges(const QPointF &start, const QPointF &end, Edge edge,
                       TileLayer &stamp, QMap<QString,QRegion> &eraseRgn,
                       QMap<QString, QRegion> &noBlendRgn);
    void drawEdgeTile(const QPoint &origin, int x, int y, Edge edge,
                      TileLayer &stamp, QMap<QString,QRegion> &eraseRgn,
                      QMap<QString,QRegion> &noBlendRgn);
    void drawGapTile(int x, int y, TileLayer &stamp, QMap<QString,QRegion> &eraseRgn,
                     QMap<QString,QRegion> &noBlendRgn);

    MapScene *mScene;
    CompositeLayerGroup *mToolTileLayerGroup;
    TileLayer mToolTiles;
    QRectF mToolTilesRect;
    bool mInitialClick;
    QPointF mStartTilePosF;
    QPoint mStartTilePos;
    Edge mEdge;
    Edges *mEdges;
    int mDashLen;
    int mDashGap;
    bool mSuppressBlendTiles;
    QGraphicsPathItem *mCursorItem;
};

} // namespace Internal
} // namespace Tiled

#endif // EDGETOOL_H
