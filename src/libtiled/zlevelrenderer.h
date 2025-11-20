/*
 * zlevelrenderer.h
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ZLEVELRENDERER_H
#define ZLEVELRENDERER_H

#include "maprenderer.h"

namespace Tiled {

#ifdef ZOMBOID
class Tile;
#endif

/**
 * Modified isometric map renderer for Project Zomboid.
 * Tile layers are arranged into groups, one group per level/story/floor of a map.
 */
class TILEDSHARED_EXPORT ZLevelRenderer : public MapRenderer
{
public:
    ZLevelRenderer(const Map *map) : MapRenderer(map) { set2x(true); }

    QSize mapSize() const override;

    QRect boundingRect(const QRect &rect, int level = 0) const override;

    QRectF boundingRect(const MapObject *object) const override;
    QPainterPath shape(const MapObject *object) const override;

    void drawGrid(QPainter *painter, const QRectF &rect, QColor gridColor,
                  int level = 0, const QRect &tileBounds = QRect()) const override;

    void drawTileLayer(QPainter *painter, const TileLayer *layer,
                       const QRectF &exposed = QRectF()) const override;

   void drawTileLayerGroup(QPainter *painter, ZTileLayerGroup *layerGroup,
                               const QRectF &exposed = QRectF(), ZTileLayerGroupRenderData *renderData = nullptr) const override;

    void drawTileSelection(QPainter *painter,
                           const QRegion &region,
                           const QColor &color,
#ifdef ZOMBOID
                            const QRectF &exposed,
                            int level = 0) const override;
#else
                           const QRectF &exposed) const;
#endif

    void drawMapObject(QPainter *painter,
                       const MapObject *object,
                       const QColor &color) const override;

    void drawImageLayer(QPainter *painter,
                        const ImageLayer *layer,
                        const QRectF &exposed = QRectF()) const override;

    using MapRenderer::pixelToTileCoords;
    QPointF pixelToTileCoords(qreal x, qreal y, int level = 0) const override;

    using MapRenderer::tileToPixelCoords;
    QPointF tileToPixelCoords(qreal x, qreal y, int level = 0) const override;

    void drawFancyRectangle(QPainter *painter,
                            const QRectF &tileBounds,
                            const QColor &color,
                            int level = 0) const override;

private:
    QPolygonF tileRectToPolygon(const QRect &rect, int level = 0) const;
    QPolygonF tileRectToPolygon(const QRectF &rect, int level = 0) const;
    void drawJumboTreeTile_Trunk(Tiled::Tile *tile, QPainter *painter, const QTransform &baseTransform, qreal x, qreal y, qreal m11, qreal m12, qreal m21, qreal m22) const;
    void drawJumboTreeTile_Leaves(Tiled::Tile *tile, QPainter *painter, const QTransform &baseTransform, qreal x, qreal y, qreal m11, qreal m12, qreal m21, qreal m22) const;
};

} // namespace Tiled

#endif // ZLEVELRENDERER_H
