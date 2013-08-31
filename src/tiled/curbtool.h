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

class CompositeLayerGroup;
class SimpleFileBlock;

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
        FirstNear,
        NearE = FirstNear,
        NearS,
        NearSE,
        NearJoinSE,
        NearEJoinS,
        NearSJoinE,
        FarSunkenW,
        FarSunkenN,
        FarSunkenJoinW,
        FarSunkenJoinE,
        FarSunkenJoinN,
        FarSunkenJoinS,
        NearSunkenE,
        NearSunkenS,
        NearSunkenJoinW,
        NearSunkenJoinE,
        NearSunkenJoinN,
        NearSunkenJoinS,
        ShapeCount
    };

    Curb() :
        mTileNames(ShapeCount)
    {}

    QString mLabel;
    QString mLayer;
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
    bool validKeys(SimpleFileBlock &block, const QStringList &keys);

private:
    QList<Curb*> mCurbs;
    QString mFileName;
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

    enum Edge
    {
        EdgeE,
        EdgeS
    };

    void drawEdge(const QPointF &start, Corner cornerStart, const QPointF &end, Corner cornerEnd, bool far);
    void drawEdgeTile(const QPoint &origin, int x, int y, Edge edge, bool half, bool far,
                      TileLayer &stamp, QMap<QString,QRegion> &eraseRgn,
                      QMap<QString,QRegion> &noBlendRgn);

    void raiseLower(const QPointF &start, const QPointF &end);
    void raiseLowerChanges(const QPointF &start, const QPointF &end,
                           TileLayer &stamp, QMap<QString,QRegion> &eraseRgn);
    void raiseLowerTile(int sx, int sy, int ex, int ey, int x, int y,
                        TileLayer &stamp, QMap<QString, QRegion> &eraseRgn);

    void getMapChanges(const QPointF &start, Corner cornerStart,
                       const QPointF &end, Corner cornerEnd, bool far,
                       TileLayer &stamp, QMap<QString,QRegion> &eraseRgn,
                       QMap<QString,QRegion> &noBlendRgn);

    MapScene *mScene;
    CompositeLayerGroup *mToolTileLayerGroup;
    QRectF mToolTilesRect;
    bool mInitialClick;
    QPointF mStartTilePosF;
    QPoint mStartTilePos;
    Corner mStartCorner;
    Curb *mCurb;
    bool mSuppressBlendTiles;
    QGraphicsPolygonItem *mCursorItem;
};

} // namespace Internal
} // namespace Tiled

#endif // CURBTOOL_H
