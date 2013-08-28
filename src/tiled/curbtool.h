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

#ifndef CURBTOOL_H
#define CURBTOOL_H

#include "abstracttiletool.h"

#include "BuildingEditor/singleton.h"

#include <QCoreApplication>
#include <QGraphicsPolygonItem>

namespace Tiled {
class Tile;

namespace Internal {

class Curb
{
public:
    enum Shapes
    {
        FarE,
        FarS,
        FarSE,
        FarJoinSE,
        FarEJoinS,
        FarSJoinE,
        NearE,
        NearS,
        NearSE,
        NearJoinSE,
        NearEJoinS,
        NearSJoinE,
        ShapeCount
    };

    Curb() :
        mTileNames(ShapeCount)
    {}

    QString mLabel;
    QVector<QString> mTileNames;
};

class CurbFile
{
    Q_DECLARE_TR_FUNCTIONS(CurbFile)

public:
    CurbFile();
    ~CurbFile();

    bool read(const QString &fileName);

    QString errorString() const
    { return mError; }

    QList<Curb*> takeCurbs();

private:
    QList<Curb*> mCurbs;
    QString mError;
};

class CurbTool : public AbstractTileTool, public Singleton<CurbTool>
{
public:
    CurbTool(QObject *parent = 0);

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);

    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void languageChanged();

    void setCurb(Curb *Curb);

    void setDefaultLayer(const QString &layer)
    { mDefaultLayer = layer; }
    QString defaultLayer() const
    { return mDefaultLayer; }

    void setSuppressBlendTiles(bool suppress)
    { mSuppressBlendTiles = suppress; }
    bool suppressBlendTiles() const
    { return mSuppressBlendTiles; }

protected:
    void tilePositionChanged(const QPoint &tilePos);

    QVector<Tile*> resolveTiles(Curb *Curb);

private:
    enum Corner
    {
        CornerNW,
        CornerNE,
        CornerSE,
        CornerSW
    };

    void toCorner(const QPointF &scenePos, QPointF &tilePosF, Corner &corner);

    // One map location made of 4 corners
    struct CurbSpot
    {
        bool far;
        bool nw[2];
        bool ne[2];
        bool se[2];
        bool sw[2];
    };

    QVector<CurbSpot> mapToSpots(int x, int y, int w, int h);
    CurbSpot toCurbSpot(Tile *tile, QVector<Tile*> tiles);
    void spotToTile(int x, int y, CurbSpot &spot);

    struct ValidAdjacent
    {
        ValidAdjacent() :
            w(QList<Curb::Shapes>()),
            n(QList<Curb::Shapes>()),
            e(QList<Curb::Shapes>()),
            s(QList<Curb::Shapes>())
        {}
        QList<Curb::Shapes> w;
        QList<Curb::Shapes> n;
        QList<Curb::Shapes> e;
        QList<Curb::Shapes> s;
    };
    QVector<ValidAdjacent> mValidAdjacent;

    enum Edge
    {
        EdgeE,
        EdgeS
    };

    void drawEdge(const QPointF &start, Corner cornerStart, const QPointF &end, Corner cornerEnd, bool far);
    void drawEdgeTile(int x, int y, Edge edge, bool half, bool far);

    bool mInitialClick;
    QPointF mStartTilePosF;
    QPoint mStartTilePos;
    Corner mStartCorner;
    Curb *mCurb;
    QString mDefaultLayer;
    bool mSuppressBlendTiles;
    QGraphicsPolygonItem *mLineItem;
};

} // namespace Internal
} // namespace Tiled

#endif // CURBTOOL_H
