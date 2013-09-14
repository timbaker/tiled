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

#include "luatiletool.h"

#include "luaconsole.h"
#include "luatiled.h"
#include "luatooldialog.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "mapscene.h"
#include "painttilelayer.h"
#include "preferences.h"

#include "tolua.h"

#include "maprenderer.h"
#include "tilelayer.h"

#include <qmath.h>
#include <QDebug>
#include <QFileInfo>
#include <QUndoStack>

extern "C" {
#include "lualib.h"
#include "lauxlib.h"
}

TOLUA_API int tolua_tiled_open(lua_State *L);

using namespace Tiled;
using namespace Tiled::Lua;

SINGLETON_IMPL(LuaTileTool)

LuaTileTool::LuaTileTool(const QString &name, const QIcon &icon,
                         const QKeySequence &shortcut, QObject *parent) :
    AbstractTileTool(name, icon, shortcut, parent),
    L(0),
    mMap(0),
    mMapChanged(false),
    mCursorItem(new QGraphicsPathItem),
    mCursorType(None)
{
    mDistanceIndicators[0] = mDistanceIndicators[1] = mDistanceIndicators[2] = mDistanceIndicators[3] = 0;

    mCursorItem->setPen(QPen(QColor(0,255,0,96), 1));
    mCursorItem->setBrush(QColor(0,255,0,64));
}

void LuaTileTool::setScript(const QString &fileName)
{
    Internal::MapScene *scene = mScene;
    if (mScene) deactivate(scene);

    mFileName = fileName;

    if (L)
        lua_close(L);
    L = luaL_newstate();
    luaL_openlibs(L);
    tolua_tiled_open(L);

    tolua_pushusertype(L, this, "LuaTileTool");
    lua_setglobal(L, "self");

    tolua_pushstring(L, Lua::cstring(QFileInfo(fileName).absolutePath()));
    lua_setglobal(L, "scriptDirectory");

    int status = luaL_loadfile(L, cstring(mFileName));
    if (status == LUA_OK) {
        int base = lua_gettop(L);
        lua_pushcfunction(L, traceback);
        lua_insert(L, base);
        status = lua_pcall(L, 0, 0, base);
        lua_remove(L, base);
    }

    QString output;
    if (status != LUA_OK)
        output = QString::fromLatin1(lua_tostring(L, -1));
    LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);

    if (scene) activate(scene);
}

void LuaTileTool::activate(Internal::MapScene *scene)
{
    AbstractTileTool::activate(scene);
    mScene = scene;

    mScene->addItem(mCursorItem);

    Internal::LuaToolDialog::instance()->setVisibleLater(true);

    connect(mapDocument(), SIGNAL(mapChanged()), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(tilesetAdded(int,Tileset*)), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(tilesetRemoved(Tileset*)), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(tilesetNameChanged(Tileset*)), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(layerAdded(int)), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(layerRemoved(int)), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(layerChanged(int)), SLOT(mapChanged())); // layer renamed
    connect(mapDocument(), SIGNAL(regionAltered(QRegion,Layer*)), SLOT(mapChanged()));

//    if (!L) setScript(Internal::Preferences::instance()->luaPath(QLatin1String("tool-street-light.lua")));

    if (!L && !mFileName.isEmpty()) {
        mScene = 0;
        setScript(mFileName);
        mScene = scene;
    }

    if (!L) return;

    checkMap();

    int base = lua_gettop(L);
    lua_pushcfunction(L, traceback);
    lua_insert(L, base);
    lua_getglobal(L, "activate");
    int status = lua_pcall(L, 0, LUA_MULTRET, base);
    lua_remove(L, base);

    QString output;
    if (status != LUA_OK)
        output = QString::fromLatin1(lua_tostring(L, -1));
    LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
}

void LuaTileTool::deactivate(Internal::MapScene *scene)
{
    Internal::LuaToolDialog::instance()->setVisibleLater(false);
    clearToolTiles();
    clearDistanceIndicators();
    mapDocument()->disconnect(this, SLOT(mapChanged()));
    scene->removeItem(mCursorItem);
    mCursorType = CursorType::None;

    AbstractTileTool::deactivate(scene);

    if (!L) return;

    int base = lua_gettop(L);
    lua_pushcfunction(L, traceback);
    lua_insert(L, base);
    lua_getglobal(L, "deactivate");
    int status = lua_pcall(L, 0, LUA_MULTRET, base);
    lua_remove(L, base);

    QString output;
    if (status != LUA_OK)
        output = QString::fromLatin1(lua_tostring(L, -1));
    LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);

    lua_close(L);
    L = 0;

    delete mMap;
    mMap = 0;

    mScene = 0;
}

void LuaTileTool::mouseLeft()
{
    clearToolTiles();
    AbstractTileTool::mouseLeft();
}

void LuaTileTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    if (mButtons & Qt::LeftButton) {
        mCursorItem->setVisible(false);
    } else {
        QPainterPath path = cursorShape(pos, modifiers);
        mCursorItem->setPath(path);
        mCursorItem->setVisible(true);
    }
    mouseEvent("mouseMoved", mButtons, pos, modifiers);
}

void LuaTileTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    mButtons = event->buttons();
    mouseEvent("mousePressed", event->button(), event->scenePos(), event->modifiers());
}

void LuaTileTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    mButtons = event->buttons();
    mouseEvent("mouseReleased", event->button(), event->scenePos(), event->modifiers());
}

void LuaTileTool::modifiersChanged(Qt::KeyboardModifiers modifiers)
{
    if (!L) return;

    checkMap();

    int base = lua_gettop(L);
    lua_pushcfunction(L, traceback);
    lua_insert(L, base);

    lua_getglobal(L, "modifiersChanged");
    lua_newtable(L); // arg modifiers
    if (modifiers & Qt::AltModifier) {
        lua_pushstring(L, "alt");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }
    if (modifiers & Qt::ControlModifier) {
        lua_pushstring(L, "control");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }
    if (modifiers & Qt::ShiftModifier) {
        lua_pushstring(L, "shift");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }

    int status = lua_pcall(L, 1, LUA_MULTRET, base);

    lua_remove(L, base);

    QString output;
    if (status != LUA_OK)
        output = QString::fromLatin1(lua_tostring(L, -1));
    LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
}

void LuaTileTool::languageChanged()
{

}

void LuaTileTool::tilePositionChanged(const QPoint &tilePos)
{
    Q_UNUSED(tilePos)
}

void LuaTileTool::setCursorType(LuaTileTool::CursorType type)
{
    mCursorType = type;
}

void LuaTileTool::mouseEvent(const char *func, Qt::MouseButtons buttons,
                             const QPointF &scenePos, Qt::KeyboardModifiers modifiers)
{
    if (!L) return;

    checkMap();

    int base = lua_gettop(L);
    lua_pushcfunction(L, traceback);
    lua_insert(L, base);

    lua_getglobal(L, func); // mouseMoved/mousePressed/mouseReleased

    lua_newtable(L); // arg buttons
    if (buttons & Qt::LeftButton) {
        lua_pushstring(L, "left");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }
    if (buttons & Qt::RightButton) {
        lua_pushstring(L, "right");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }

    Layer *layer = currentTileLayer();
    const QPointF tilePosF = mapDocument()->renderer()->pixelToTileCoords(scenePos, layer->level());
    lua_pushnumber(L, tilePosF.x()); // arg x
    lua_pushnumber(L, tilePosF.y()); // arg y

    lua_newtable(L); // arg modifiers
    if (modifiers & Qt::AltModifier) {
        lua_pushstring(L, "alt");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }
    if (modifiers & Qt::ControlModifier) {
        lua_pushstring(L, "control");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }
    if (modifiers & Qt::ShiftModifier) {
        lua_pushstring(L, "shift");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }

    int status = lua_pcall(L, 4, LUA_MULTRET, base);

    lua_remove(L, base);

    QString output;
    if (status != LUA_OK)
        output = QString::fromLatin1(lua_tostring(L, -1));
    LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
}

void LuaTileTool::checkMap()
{
    if (mMap && !mMapChanged)
        return;

    delete mMap;

    mMap = new LuaMap(mapDocument()->map());
    tolua_pushusertype(L, mMap, "LuaMap");
    lua_setglobal(L, "map");

    mMapChanged = false;

    qDebug() << "LuaTileTool::checkMap(): recreated map";
}

void LuaTileTool::applyChanges(const char *undoText)
{
    QUndoStack *us = mapDocument()->undoStack();
    QList<QUndoCommand*> cmds;

    foreach (Lua::LuaLayer *ll, mMap->mLayers) {
        // Apply changes to tile layers.
        if (Lua::LuaTileLayer *tl = ll->asTileLayer()) {
            if (tl->mOrig == 0)
                continue; // Ignore new layers.
            if (!tl->mCloneTileLayer || tl->mAltered.isEmpty())
                continue; // No changes.
            TileLayer *source = tl->mCloneTileLayer->copy(tl->mAltered);
            QRect r = tl->mAltered.boundingRect();
            cmds += new ::Internal::PaintTileLayer(mapDocument(), tl->mOrig->asTileLayer(),
                                                   r.x(), r.y(), source, tl->mAltered, true);
            delete source;
        }
    }

    if (cmds.size()) {
        us->beginMacro(QLatin1String(undoText));
        foreach (QUndoCommand *cmd, cmds)
            us->push(cmd);
        us->endMacro();
    }

    mMapChanged = true;
}

int LuaTileTool::angle(float x1, float y1, float x2, float y2)
{
    QLineF line(x1, y1, x2, y2);
    return line.angle();
}

void LuaTileTool::clearToolTiles()
{
    foreach (QString layerName, mToolTileLayers.keys()) {
        if (!mToolTileLayers[layerName]->isEmpty()) {
            mToolTileLayers[layerName]->erase();
            int n = mapDocument()->map()->indexOfLayer(layerName, Layer::TileLayerType);
            if (n >= 0) {
                TileLayer *tl = mapDocument()->map()->layerAt(n)->asTileLayer();
                mapDocument()->mapComposite()->layerGroupForLayer(tl)->clearToolTiles();
                QRect r = mapDocument()->renderer()->boundingRect(mToolTileRegions[layerName].boundingRect(),
                                                                  tl->level()).adjusted(-3, -(128-32) - 3, 3, 3);
                mScene->update(r);
            }
            mToolTileRegions[layerName] = QRegion();
        }
     }
}

void LuaTileTool::setToolTile(const char *layer, int x, int y, Tile *tile)
{
    QString layerName = QLatin1String(layer);
    Map *map = mapDocument()->map();
    if (!mToolTileLayers.keys().contains(layerName))
        mToolTileLayers[layerName] = new Tiled::TileLayer(layerName, 0, 0,
                                                          map->width(),
                                                          map->height());

    TileLayer *tlTool = mToolTileLayers[layerName];
    if (tlTool->width() != map->width() || tlTool->height() != map->height()) {
        tlTool->resize(map->size(), QPoint());
        mToolTileRegions[layerName] &= QRect(QPoint(), map->size());
    }

    if (tlTool->contains(x, y)) {
        tlTool->setCell(x, y, Cell(tile));
        mToolTileRegions[layerName] += QRect(x, y, 1, 1);
        int n = map->indexOfLayer(layerName, Layer::TileLayerType);
        if (n >= 0) {
            TileLayer *tl = map->layerAt(n)->asTileLayer();
            mapDocument()->mapComposite()->layerGroupForLayer(tl)->setToolTiles(
                        tlTool, QPoint(), mToolTileRegions[layerName], tl);
            QRect r = mapDocument()->renderer()->boundingRect(
                        mToolTileRegions[layerName].boundingRect(),
                        tl->level()).adjusted(-3, -(128-32) - 3, 3, 3);
            mScene->update(r);
        }
    }
}

void LuaTileTool::clearDistanceIndicators()
{
    for (int i = 0; i < 4; i++) {
        if (mDistanceIndicators[i] && mDistanceIndicators[i]->isVisible()) {
            mScene->removeItem(mDistanceIndicators[i]);
            mDistanceIndicators[i]->hide();
        }
    }
}

void LuaTileTool::indicateDistance(int x1, int y1, int x2, int y2)
{
    int d = -1, dist = -1;
    DistanceIndicator::Dir dir;
    if (x1 > x2 && y1 == y2) d = 0, dir = DistanceIndicator::West, dist = x1 - x2;
    else if (x1 == x2 && y1 > y2) d = 1, dir = DistanceIndicator::North, dist = y1 - y2;
    else if (x1 < x2 && y1 == y2) d = 2, dir = DistanceIndicator::East, dist = x2 - x1;
    else if (x1 == x2 && y1 < y2) d = 3, dir = DistanceIndicator::South, dist = y2 - y1;
    if (d != -1) {
        if (!mDistanceIndicators[d]) {
            mDistanceIndicators[d] = new DistanceIndicator(dir);
            mDistanceIndicators[d]->setVisible(false);
        }
        mDistanceIndicators[d]->setRenderer(mapDocument()->renderer());
        mDistanceIndicators[d]->setInfo(QPoint(x1, y1), dist);
        if (!mDistanceIndicators[d]->isVisible()) {
            mScene->addItem(mDistanceIndicators[d]);
            mDistanceIndicators[d]->show();
        }
    }
}

QPainterPath LuaTileTool::cursorShape(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    QPainterPath path;
    if (mCursorType == CursorType::EdgeTool) {
        const MapRenderer *renderer = mapDocument()->renderer();
        int level = 0;
        QPointF tilePosF = renderer->pixelToTileCoords(pos, level);
        QPoint tilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
        QPointF m(tilePosF.x() - tilePos.x(), tilePosF.y() - tilePos.y());
        qreal dW = m.x(), dN = m.y(), dE = 1.0 - dW, dS = 1.0 - dN;

        Edge edge;
        if (dW < dE) {
            edge = (dW < dN && dW < dS) ? EdgeW : ((dN < dS) ? EdgeN : EdgeS);
        } else {
            edge = (dE < dN && dE < dS) ? EdgeE : ((dN < dS) ? EdgeN : EdgeS);
        }

        QPolygonF polyN = renderer->tileToPixelCoords(QRectF(tilePos.x(), tilePos.y(), 1, 0.25), level);
        QPolygonF polyS = renderer->tileToPixelCoords(QRectF(tilePos.x(), tilePos.y() + 0.75, 1, 0.25), level);
        QPolygonF polyW = renderer->tileToPixelCoords(QRectF(tilePos.x(), tilePos.y(), 0.25, 1), level);
        QPolygonF polyE = renderer->tileToPixelCoords(QRectF(tilePos.x() + 0.75, tilePos.y(), 0.25, 1), level);
        polyN += polyN.first(), polyS += polyS.first(), polyW += polyW.first(), polyE += polyE.first();
        if ((edge == EdgeN && dW < 0.5) || (edge == EdgeW && dN < 0.5))
            path.addPolygon(polyN), path.addPolygon(polyW);
        if ((edge == EdgeS && dW < 0.5) || (edge == EdgeW && dS < 0.5))
            path.addPolygon(polyS), path.addPolygon(polyW);
        if ((edge == EdgeN && dE < 0.5) || (edge == EdgeE && dN < 0.5))
            path.addPolygon(polyN), path.addPolygon(polyE);
        if ((edge == EdgeS && dE < 0.5) || (edge == EdgeE && dS < 0.5))
            path.addPolygon(polyS), path.addPolygon(polyE);
        path.setFillRule(Qt::WindingFill);
        path = path.simplified();
    }
    return path;
}

/////

UnscaledLabelItem::UnscaledLabelItem(QGraphicsItem *parent)
    : QGraphicsSimpleTextItem(parent)
{
    setFlag(ItemIgnoresTransformations);

    synch();
}

QRectF UnscaledLabelItem::boundingRect() const
{
    QRectF r = QGraphicsSimpleTextItem::boundingRect().adjusted(-3, -3, 2, 2);
    return r.translated(-r.center());
}

QPainterPath UnscaledLabelItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

bool UnscaledLabelItem::contains(const QPointF &point) const
{
    return boundingRect().contains(point);
}

void UnscaledLabelItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                            QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    mBgColor = Qt::white;
    mBgColor.setAlphaF(0.75);

    QRectF r = boundingRect();
    painter->fillRect(r, mBgColor);
    painter->drawText(r, Qt::AlignCenter, text());
}

void UnscaledLabelItem::synch()
{
}

/////

DistanceIndicator::DistanceIndicator(DistanceIndicator::Dir direction, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mDirection(direction),
    mRenderer(0),
    mTextItem(new UnscaledLabelItem(this))
{
}

void DistanceIndicator::setInfo(const QPoint &tilePos, int dist)
{
    prepareGeometryChange();
    mTilePos = tilePos;
    mDistance = dist;

    mTextItem->setText(QString::number(dist));
    int level = 0;
    QPointF centerW = mRenderer->tileToPixelCoords(mTilePos.x() - 0.5, mTilePos.y() + 0.5, level);
    QPointF centerN = mRenderer->tileToPixelCoords(mTilePos.x() + 0.5, mTilePos.y() - 0.5, level);
    QPointF centerE = mRenderer->tileToPixelCoords(mTilePos.x() + 1.5, mTilePos.y() + 0.5, level);
    QPointF centerS = mRenderer->tileToPixelCoords(mTilePos.x() + 0.5, mTilePos.y() + 1.5, level);
    QPointF pos;
    switch (mDirection) {
    case West: pos = centerW; break;
    case North: pos = centerN; break;
    case East: pos = centerE; break;
    case South: pos = centerS; break;
    }
    mTextItem->setPos(pos);
}

void DistanceIndicator::setRenderer(MapRenderer *renderer)
{
    mRenderer = renderer;
}

QRectF DistanceIndicator::boundingRect() const
{
    return mRenderer ? mRenderer->boundingRect(QRect(mTilePos, QSize(1, 1))) : QRectF();
}

void DistanceIndicator::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (!mRenderer) return;

    int level = 0;
    QPointF topLeft = mRenderer->tileToPixelCoords(mTilePos.x(), mTilePos.y(), level);
    QPointF topRight = mRenderer->tileToPixelCoords(mTilePos.x() + 1, mTilePos.y(), level);
    QPointF bottomRight = mRenderer->tileToPixelCoords(mTilePos.x() + 1, mTilePos.y() + 1, level);
    QPointF bottomLeft = mRenderer->tileToPixelCoords(mTilePos.x(), mTilePos.y() + 1, level);
    QPointF centerW = mRenderer->tileToPixelCoords(mTilePos.x() - 0.5, mTilePos.y() + 0.5, level);
    QPointF centerN = mRenderer->tileToPixelCoords(mTilePos.x() + 0.5, mTilePos.y() - 0.5, level);
    QPointF centerE = mRenderer->tileToPixelCoords(mTilePos.x() + 1.5, mTilePos.y() + 0.5, level);
    QPointF centerS = mRenderer->tileToPixelCoords(mTilePos.x() + 0.5, mTilePos.y() + 1.5, level);

    QPolygonF poly;
    switch (mDirection) {
    case West: poly << centerW << topLeft << bottomLeft; break;
    case North: poly << centerN << topLeft << topRight; break;
    case East: poly << centerE << topRight << bottomRight; break;
    case South: poly << centerS << bottomLeft << bottomRight; break;
    }
    poly << poly.first();

    painter->setPen(QPen(Qt::lightGray, 2));
    painter->setBrush(Qt::gray);
    painter->drawPolygon(poly);
}
