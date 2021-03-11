/*
 * tilerotationwindow.cpp
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

#include "tilerotationwindow.h"
#include "ui_tilerotationwindow.h"

#include "tilemetainfodialog.h"
#include "tilemetainfomgr.h"
#include "tilerotation.h"
#include "tilerotationfile.h"

#include "tileset.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/furnituregroups.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QUndoGroup>
#include <QUndoStack>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

// // // // //

#include <QPainter>

//namespace  {

// Copied from MixedTilesetView, changed to draw runtime-created rotated tilesets.

class TileRotateDelegate : public QAbstractItemDelegate
{
public:
    TileRotateDelegate(MixedTilesetView *view, QObject *parent, TileRotationWindow* window)
        : QAbstractItemDelegate(parent)
        , mView(view)
        , mWindow(window)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

    QPoint dropCoords(const QPoint &dragPos, const QModelIndex &index);

    qreal scale() const
    { return mView->zoomable()->scale(); }

    void itemResized(const QModelIndex &index);

private:
    MixedTilesetView *mView;
    TileRotationWindow *mWindow;
};

void TileRotateDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());

    QBrush brush = qvariant_cast<QBrush>(m->data(index, Qt::BackgroundRole));
    painter->fillRect(option.rect, brush);

    QString tilesetName = m->headerAt(index);
    if (!tilesetName.isEmpty()) {
        if (index.row() > 0) {
            painter->setPen(Qt::darkGray);
            painter->drawLine(option.rect.topLeft(), option.rect.topRight());
            painter->setPen(Qt::black);
        }
        // One slice of the tileset name is drawn in each column.
        if (index.column() == 0)
            painter->drawText(option.rect.adjusted(2, 2, 0, 0), Qt::AlignLeft,
                              tilesetName);
        else {
            QRect r = option.rect.adjusted(-index.column() * option.rect.width(),
                                           0, 0, 0);
            painter->save();
            painter->setClipRect(option.rect);
            painter->drawText(r.adjusted(2, 2, 0, 0), Qt::AlignLeft, tilesetName);
            painter->restore();
        }
        return;
    }

    Tile *tile;
    if (!(tile = m->tileAt(index))) {
#if 0
        painter->drawLine(option.rect.topLeft(), option.rect.bottomRight());
        painter->drawLine(option.rect.topRight(), option.rect.bottomLeft());
#endif
        return;
    }
    if (m->showEmptyTilesAsMissing() && tile->image().isNull())
        tile = TilesetManager::instance()->missingTile();

    const int extra = 2;

    QString label = index.data(Qt::DecorationRole).toString();

    QRect r = m->categoryBounds(index);
    if (m->showLabels() && m->highlightLabelledItems() && label.length())
        r = QRect(index.column(),index.row(),1,1);
    if (r.isValid() && !(option.state & QStyle::State_Selected)) {
        int left = option.rect.left();
        int right = option.rect.right();
        int top = option.rect.top();
        int bottom = option.rect.bottom();
        if (index.column() == r.left())
            left += extra;
        if (index.column() == r.right())
            right -= extra;
        if (index.row() == r.top())
            top += extra;
        if (index.row() == r.bottom())
            bottom -= extra;

        QBrush brush = qvariant_cast<QBrush>(index.data(MixedTilesetModel::CategoryBgRole));
        painter->fillRect(left, top, right-left+1, bottom-top+1, brush);

        painter->setPen(Qt::darkGray);
        if (index.column() == r.left())
            painter->drawLine(left, top, left, bottom);
        if (index.column() == r.right())
            painter->drawLine(right, top, right, bottom);
        if (index.row() == r.top())
            painter->drawLine(left, top, right, top);
        if (index.row() == r.bottom())
            painter->drawLine(left, bottom, right, bottom);
        painter->setPen(Qt::black);
    }

    // Draw the tile image
//    const QVariant display = index.model()->data(index, Qt::DisplayRole);
//    const QPixmap tileImage = display.value<QPixmap>();
    qreal scale = mView->zoomable()->scale();
    const int tileWidth = tile->tileset()->tileWidth() * scale;
    const int tileHeight = tile->tileset()->tileHeight() * scale;

    if (mView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

    const QFontMetrics fm = painter->fontMetrics();
    const int labelHeight = m->showLabels() ? fm.lineSpacing() : 0;
    const int dw = option.rect.width() - tileWidth;
    const QMargins margins = tile->drawMargins(mView->zoomable()->scale());
#if 1
    if (TileRotated *tileR = mWindow->rotatedTileFor(tile)) {
        int m = (int(tileR->mTileset->mRotation) + int(tileR->mRotation)) % MAP_ROTATION_COUNT;
        TileRotatedVisualData& direction = tileR->mVisual->mData[m];
        for (int i = 0; i < direction.mTileNames.size(); i++) {
            const QString& tileName = direction.mTileNames[i];
            const QPoint tileOffset = direction.pixelOffset(i);
            QRect r = option.rect.adjusted(extra, extra, -extra, -extra);
            if (Tile *tile = BuildingTilesMgr::instance()->tileFor(tileName)) { // FIXME: calc this elsewhere
                QRect r1(r.topLeft() + tileOffset * scale, QSize(tileWidth, tileHeight));
                if (tile->image().isNull())
                    tile = TilesetManager::instance()->missingTile();
                const QMargins margins = tile->drawMargins(scale);
                painter->drawImage(r1.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom()), tile->image());
            }
        }
    }
#else
    painter->drawImage(option.rect.adjusted(dw/2 + margins.left(), extra + margins.top(), -(dw - dw/2) - margins.right(), -extra - labelHeight - margins.bottom()), tile->image());
#endif

    // Draw the "floor"
    if (true) {
        qreal floorHeight = 32 * scale;
        QRect r = option.rect.adjusted(extra, extra, -extra, -extra);
        QPointF p1(r.left() + r.width() / 2, r.bottom() - floorHeight);
        QPointF p2(r.right(), r.bottom() - floorHeight / 2);
        QPointF p3(r.left() + r.width() / 2, r.bottom());
        QPointF p4(r.left(), r.bottom() - floorHeight / 2);
        painter->drawLine(p1, p2); painter->drawLine(p2, p3);
        painter->drawLine(p3, p4); painter->drawLine(p4, p1);
    }

    if (m->showLabels()) {
        QString name = fm.elidedText(label, Qt::ElideRight, option.rect.width());
        painter->drawText(option.rect.left(), option.rect.bottom() - labelHeight,
                          option.rect.width(), labelHeight, Qt::AlignHCenter, name);
    }

    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(option.rect.adjusted(extra, extra, -extra, -extra),
                          option.palette.highlight());
        painter->setOpacity(opacity);
    }

    // Focus rect around 'current' item
    if (option.state & QStyle::State_HasFocus) {
        QStyleOptionFocusRect o;
        o.QStyleOption::operator=(option);
        o.rect = option.rect.adjusted(1,1,-1,-1);
        o.state |= QStyle::State_KeyboardFocusChange;
        o.state |= QStyle::State_Item;
        QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled)
                                  ? QPalette::Normal : QPalette::Disabled;
        o.backgroundColor = option.palette.color(cg, (option.state & QStyle::State_Selected)
                                                 ? QPalette::Highlight : QPalette::Window);
        const QWidget *widget = nullptr/*d->widget(option)*/;
        QStyle *style = /*widget ? widget->style() :*/ QApplication::style();
        style->drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter, widget);
    }
}

QSize TileRotateDelegate::sizeHint(const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
    const qreal zoom = mView->zoomable()->scale();
    const int extra = 2 * 2;
    if (m->headerAt(index).length()) {
        if (m->columnCount() == 1)
            return QSize(mView->maxHeaderWidth() + extra,
                         option.fontMetrics.lineSpacing() + 2);
        return QSize(64 * zoom + extra, option.fontMetrics.lineSpacing() + 2);
    }
    if (!m->tileAt(index))
        return QSize(64 * zoom + extra, 128 * zoom + extra);
    const Tileset *tileset = m->tileAt(index)->tileset();
    const int tileWidth = tileset->tileWidth() + (m->showLabels() ? 16 : 0);
    const QFontMetrics &fm = option.fontMetrics;
    const int labelHeight = m->showLabels() ? fm.lineSpacing() : 0;
    return QSize(tileWidth * zoom + extra,
                 tileset->tileHeight() * zoom + extra + labelHeight);
}

// namespace {}
//}

class AddTile : public QUndoCommand
{
public:
    AddTile(TileRotationWindow *d, TileRotated *tile, int index, const QString& tileName) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Tile")),
        mDialog(d),
        mTile(tile),
        mIndex(index),
        mTileName(tileName)
    {
    }

    void undo() override
    {
        mTileName = mDialog->removeTile(mTile, mIndex);
    }

    void redo() override
    {
        mDialog->addTile(mTile, mIndex, mTileName);
    }

    TileRotationWindow *mDialog;
    TileRotated *mTile;
    int mIndex;
    QString mTileName;
};

class ChangeTiles : public QUndoCommand
{
public:
    ChangeTiles(TileRotationWindow *d, TileRotated *tile, int x, const QStringList& tileNames) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Tiles")),
        mDialog(d),
        mTile(tile),
        mX(x),
        mTileNames(tileNames)
    {
    }

    void undo() override
    {
        mTileNames = mDialog->changeTiles(mTile, mX, mTileNames);
    }

    void redo() override
    {
        mTileNames = mDialog->changeTiles(mTile, mX, mTileNames);
    }

    TileRotationWindow *mDialog;
    TileRotated *mTile;
    int mX;
    QStringList mTileNames;
};

// TODO: NR / 90 / 180 / 270 tabs over the tileset view

#if 1

// This is temporary code to bootstrap creation of TileRotation.txt from the existing BuildingEd information.
class InitFromBuildingTiles
{
public:
    void initFromBuildingTiles(const QList<BuildingTileEntry*> &entries, int n, int w, TileRotateType rotateType)
    {
        for (BuildingTileEntry* bte : entries) {
            initFromBuildingTiles(bte, n, w, rotateType);
        }
    }

    void initFromBuildingTiles(BuildingTileEntry* bte, int n, int w, int nw, TileRotateType rotateType)
    {
        BuildingTile *btileN = bte->tile(n);
        BuildingTile *btileW = bte->tile(w);
        if (btileN->isNone() || btileW->isNone())
            return;

        QSharedPointer<TileRotatedVisual> visual = allocVisual();
        visual->mData[0].addTile(btileN->name());
        visual->mData[1].addTileDX(btileW->name());
        visual->mData[2].addTileDY(btileN->name());
        visual->mData[3].addTile(btileW->name());

        initVisual(btileN, visual, MapRotation::NotRotated);
        initVisual(btileW, visual, MapRotation::Clockwise270);

//        mMapping[btileN->name()] = tileRotatedN->name();
//        mMapping[btileW->name()] = tileRotatedW->name();

        if (nw != -1) {
            BuildingTile *btileNW = bte->tile(nw);
            if (!btileNW->isNone()) {
                visual = allocVisual();
                visual->mData[0].addTile(btileNW->name());

                visual->mData[1].addTile(btileN->name());
                visual->mData[1].addTileDX(btileW->name());

                visual->mData[2].addTileDX(btileW->name());
                visual->mData[2].addTileDY(btileN->name());

                visual->mData[3].addTile(btileW->name());
                visual->mData[3].addTileDY(btileN->name());

                initVisual(btileNW, visual, MapRotation::NotRotated);

//                mMapping[btileNW->name()] = tileRotatedNW->name();
            }
        }
    }

    void initFromBuildingTiles(BuildingTileEntry* bte, int n, int w, TileRotateType rotateType)
    {
        int nw = (rotateType == TileRotateType::Wall) ? BTC_Walls::NorthWest : -1;
        initFromBuildingTiles(bte, n, w, nw, rotateType);
    }

    void initGrime()
    {
#if 0
        West,
        North,
        NorthWest,
        SouthEast,
        WestWindow,
        NorthWindow,
        WestDoor,
        NorthDoor,
        WestTrim,
        NorthTrim,
        NorthWestTrim,
        SouthEastTrim,
        WestDoubleLeft,
        WestDoubleRight,
        NorthDoubleLeft,
        NorthDoubleRight,
#endif
        BuildingTileCategory *slopes = BuildingTilesMgr::instance()->catGrimeWall();
        for (BuildingTileEntry *entry : slopes->entries()) {
            initFromBuildingTiles(entry, BTC_GrimeWall::North, BTC_GrimeWall::West, BTC_GrimeWall::NorthWest, TileRotateType::Wall);
            initFromBuildingTiles(entry, BTC_GrimeWall::NorthDoor, BTC_GrimeWall::WestDoor, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_GrimeWall::NorthWindow, BTC_GrimeWall::WestWindow, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_GrimeWall::NorthTrim, BTC_GrimeWall::WestTrim, BTC_GrimeWall::NorthWestTrim, TileRotateType::Wall);
            // DoubleLeft/DoubleRight should be single FurnitureTile
        }
    }

    void initRoofCaps(BuildingTileEntry *entry, int n, int e, int s, int w)
    {
        BuildingTile *btileN = entry->tile(n);
        BuildingTile *btileE = entry->tile(e);
        BuildingTile *btileS = entry->tile(s);
        BuildingTile *btileW = entry->tile(w);
        if (btileN->isNone() || btileE->isNone() || btileS->isNone() || btileW->isNone())
            return;

        QSharedPointer<TileRotatedVisual> visual = allocVisual();
        visual->mData[0].addTile(btileN->name());
        visual->mData[1].addTileDX(btileE->name());
        visual->mData[2].addTileDY(btileS->name());
        visual->mData[3].addTile(btileW->name());

        initVisual(btileN, visual, MapRotation::NotRotated);
        initVisual(btileW, visual, MapRotation::Clockwise270);
    }

    void initRoofCaps()
    {
#if 0
        // Sloped cap tiles go left-to-right or bottom-to-top
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,

        // Cap tiles with peaked tops
        PeakPt5S, PeakPt5E,
        PeakOnePt5S, PeakOnePt5E,
        PeakTwoPt5S, PeakTwoPt5E,

        // Cap tiles with flat tops
        CapGapS1, CapGapS2, CapGapS3,
        CapGapE1, CapGapE2, CapGapE3,

        // Cap tiles for shallow (garage, trailer, etc) roofs
        CapShallowRiseS1, CapShallowRiseS2, CapShallowFallS1, CapShallowFallS2,
        CapShallowRiseE1, CapShallowRiseE2, CapShallowFallE1, CapShallowFallE2,
#endif
        BuildingTileCategory *category = BuildingTilesMgr::instance()->catRoofCaps();
        for (BuildingTileEntry *entry : category->entries()) {
            initRoofCaps(entry, BTC_RoofCaps::CapRiseS1, BTC_RoofCaps::CapFallE1, BTC_RoofCaps::CapFallS1, BTC_RoofCaps::CapRiseE1);
            initRoofCaps(entry, BTC_RoofCaps::CapRiseS2, BTC_RoofCaps::CapFallE2, BTC_RoofCaps::CapFallS2, BTC_RoofCaps::CapRiseE2);
            initRoofCaps(entry, BTC_RoofCaps::CapRiseS3, BTC_RoofCaps::CapFallE3, BTC_RoofCaps::CapFallS3, BTC_RoofCaps::CapRiseE3);

            initRoofCaps(entry, BTC_RoofCaps::CapFallS1, BTC_RoofCaps::CapRiseE1, BTC_RoofCaps::CapRiseS1, BTC_RoofCaps::CapFallE1);
            initRoofCaps(entry, BTC_RoofCaps::CapFallS2, BTC_RoofCaps::CapRiseE2, BTC_RoofCaps::CapRiseS2, BTC_RoofCaps::CapFallE2);
            initRoofCaps(entry, BTC_RoofCaps::CapFallS3, BTC_RoofCaps::CapRiseE3, BTC_RoofCaps::CapRiseS3, BTC_RoofCaps::CapFallE3);

            initFromBuildingTiles(entry, BTC_RoofCaps::PeakPt5S, BTC_RoofCaps::PeakPt5E, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::PeakOnePt5S, BTC_RoofCaps::PeakOnePt5E, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::PeakTwoPt5S, BTC_RoofCaps::PeakTwoPt5E, TileRotateType::WallExtra);

            initFromBuildingTiles(entry, BTC_RoofCaps::CapGapS1, BTC_RoofCaps::CapGapE1, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapGapS2, BTC_RoofCaps::CapGapE2, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapGapS3, BTC_RoofCaps::CapGapE3, TileRotateType::WallExtra);

            initRoofCaps(entry, BTC_RoofCaps::CapShallowRiseS1, BTC_RoofCaps::CapShallowFallE1, BTC_RoofCaps::CapShallowFallS1, BTC_RoofCaps::CapShallowRiseE1);
            initRoofCaps(entry, BTC_RoofCaps::CapShallowRiseS2, BTC_RoofCaps::CapShallowFallE2, BTC_RoofCaps::CapShallowFallS2, BTC_RoofCaps::CapShallowRiseE2);

            initRoofCaps(entry, BTC_RoofCaps::CapShallowFallS1, BTC_RoofCaps::CapShallowRiseE1, BTC_RoofCaps::CapShallowRiseS1, BTC_RoofCaps::CapShallowFallE1);
            initRoofCaps(entry, BTC_RoofCaps::CapShallowFallS2, BTC_RoofCaps::CapShallowRiseE2, BTC_RoofCaps::CapShallowRiseS2, BTC_RoofCaps::CapShallowFallE2);
        }
    }

    void initRoofSlope(BuildingTileEntry *entry, int north, int west)
    {
        BuildingTile *btileN = entry->tile(north);
        BuildingTile *btileW = entry->tile(west);
        if (btileN->isNone() && btileW->isNone())
            return;

        QSharedPointer<TileRotatedVisual> visual = allocVisual();
        visual->mData[0].addTile(btileN->name());
        visual->mData[3].addTile(btileW->name());

        initVisual(btileN, visual, MapRotation::NotRotated);
        initVisual(btileW, visual, MapRotation::Clockwise270);

//        mMapping[btileN->name()] = tileRotatedN->name();
//        mMapping[btileW->name()] = tileRotatedW->name();
    }

    // Corners
    void initRoofSlope(BuildingTileEntry *bte, int se, int sw, int ne)
    {
        BuildingTile *btileSE = bte->tile(se);
        BuildingTile *btileSW = bte->tile(sw);
        BuildingTile *btileNE = bte->tile(ne);
         if (btileSE->isNone() || btileNE->isNone() || btileSW->isNone())
            return;

        QSharedPointer<TileRotatedVisual> visual = allocVisual();
        visual->mData[1].addTile(btileNE->name());
        visual->mData[2].addTile(btileSE->name());
        visual->mData[3].addTile(btileSW->name());

        // Missing NW = R0

        initVisual(btileNE, visual, MapRotation::Clockwise90);
        initVisual(btileSE, visual, MapRotation::Clockwise180);
        initVisual(btileSW, visual, MapRotation::Clockwise270);

//        mMapping[btileNE->name()] = tileRotatedNE->name();
//        mMapping[btileSE->name()] = tileRotatedSE->name();
//        mMapping[btileSW->name()] = tileRotatedSW->name();
    }

    void initRoofSlope(BuildingTile *btileN, BuildingTile *btileE, BuildingTile *btileS, BuildingTile *btileW)
    {
        QSharedPointer<TileRotatedVisual> visual = allocVisual();
        visual->mData[0].addTile(btileN->name());
        visual->mData[1].addTile(btileE->name());
        visual->mData[2].addTile(btileS->name());
        visual->mData[3].addTile(btileW->name());

        initVisual(btileN, visual, MapRotation::NotRotated);
        initVisual(btileE, visual, MapRotation::Clockwise90);
        initVisual(btileS, visual, MapRotation::Clockwise180);
        initVisual(btileW, visual, MapRotation::Clockwise270);

//        mMapping[btileN->name()] = tileRotatedN->name();
//        mMapping[btileE->name()] = tileRotatedE->name();
//        mMapping[btileS->name()] = tileRotatedS->name();
//        mMapping[btileW->name()] = tileRotatedW->name();
    }

    void initRoofSlopes()
    {
#if 0
        // Sloped sides
        SlopeS1, SlopeS2, SlopeS3,
        SlopeE1, SlopeE2, SlopeE3,
        SlopePt5S, SlopePt5E,
        SlopeOnePt5S, SlopeOnePt5E,
        SlopeTwoPt5S, SlopeTwoPt5E,

        // Shallow sides
        ShallowSlopeW1, ShallowSlopeW2,
        ShallowSlopeE1, ShallowSlopeE2,
        ShallowSlopeN1, ShallowSlopeN2,
        ShallowSlopeS1, ShallowSlopeS2,

        // Sloped corners
        Inner1, Inner2, Inner3,
        Outer1, Outer2, Outer3,
        InnerPt5, InnerOnePt5, InnerTwoPt5,
        OuterPt5, OuterOnePt5, OuterTwoPt5,
        CornerSW1, CornerSW2, CornerSW3,
        CornerNE1, CornerNE2, CornerNE3,
#endif
        BuildingTileCategory *category = BuildingTilesMgr::instance()->catRoofSlopes();
        for (BuildingTileEntry *entry : category->entries()) {
            initRoofSlope(entry, BTC_RoofSlopes::SlopeS1, BTC_RoofSlopes::SlopeE1);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeS2, BTC_RoofSlopes::SlopeE2);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeS3, BTC_RoofSlopes::SlopeE3);

            initRoofSlope(entry, BTC_RoofSlopes::SlopePt5S, BTC_RoofSlopes::SlopePt5E);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeOnePt5S, BTC_RoofSlopes::SlopeOnePt5E);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeTwoPt5S, BTC_RoofSlopes::SlopeTwoPt5E);

            {
                BuildingTile *tileN = entry->tile(BTC_RoofSlopes::ShallowSlopeN1);
                BuildingTile *tileE = entry->tile(BTC_RoofSlopes::ShallowSlopeE1);
                BuildingTile *tileS = entry->tile(BTC_RoofSlopes::ShallowSlopeS1);
                BuildingTile *tileW = entry->tile(BTC_RoofSlopes::ShallowSlopeW1);
                if (!tileN->isNone() && !tileE->isNone() && !tileS->isNone() && !tileW->isNone()) {
                    initRoofSlope(tileN, tileE, tileS, tileW);
                    initRoofSlope(tileE, tileS, tileW, tileN);
                    initRoofSlope(tileS, tileW, tileN, tileE);
                    initRoofSlope(tileW, tileN, tileE, tileS);
                }
            }

            {
                BuildingTile *tileN = entry->tile(BTC_RoofSlopes::ShallowSlopeN2);
                BuildingTile *tileE = entry->tile(BTC_RoofSlopes::ShallowSlopeE2);
                BuildingTile *tileS = entry->tile(BTC_RoofSlopes::ShallowSlopeS2);
                BuildingTile *tileW = entry->tile(BTC_RoofSlopes::ShallowSlopeW2);
                if (!tileN->isNone() && !tileE->isNone() && !tileS->isNone() && !tileW->isNone()) {
                    initRoofSlope(tileN, tileE, tileS, tileW);
                    initRoofSlope(tileE, tileS, tileW, tileN);
                    initRoofSlope(tileS, tileW, tileN, tileE);
                    initRoofSlope(tileW, tileN, tileE, tileS);
                }
            }

            initRoofSlope(entry, BTC_RoofSlopes::Outer1, BTC_RoofSlopes::CornerSW1, BTC_RoofSlopes::CornerNE1);
            initRoofSlope(entry, BTC_RoofSlopes::Outer2, BTC_RoofSlopes::CornerSW2, BTC_RoofSlopes::CornerNE2);
            initRoofSlope(entry, BTC_RoofSlopes::Outer3, BTC_RoofSlopes::CornerSW3, BTC_RoofSlopes::CornerNE3);
        }
    }

    QPoint rotatePoint(int width, int height, MapRotation rotation, const QPoint &pos) const
    {
        switch (rotation)
        {
        case MapRotation::NotRotated: // w,h=3,2 x,y=2,1 -> 2,1
            return pos;
        case MapRotation::Clockwise90:
            return QPoint(height - pos.y() - 1, pos.x()); // w,h=3,2 x,y=2,1 -> 0,2
        case MapRotation::Clockwise180:
            return QPoint(width - pos.x() - 1, height - pos.y() - 1); // w,h=3,2 x,y=2,1 -> 0,0
        case MapRotation::Clockwise270:
            return QPoint(pos.y(), width - pos.x() - 1); // w,h=3,2 x,y=2,1 -> 1,0
        }
    }

    MapRotation ROTATION[MAP_ROTATION_COUNT] = {
        MapRotation::NotRotated,
        MapRotation::Clockwise90,
        MapRotation::Clockwise180,
        MapRotation::Clockwise270
    };

    bool isFurnitureOK(FurnitureTile* ft[4])
    {
        if (ft[0]->isEmpty() && ft[1]->isEmpty() && ft[2]->isEmpty() && ft[3]->isEmpty())
            return false;
        return true;
    }

    bool initFurniture(FurnitureTile* ft[4])
    {
        int width = ft[0]->width();
        int height = ft[0]->height();
        for (int dy = 0; dy < height; dy++) {
            for (int dx = 0; dx < width; dx++) {
                BuildingTile *tiles[MAP_ROTATION_COUNT];
                BuildingTile *masterTile = nullptr; // normally this is the North/NotRotated tile
                for (int i = 0; i < MAP_ROTATION_COUNT; i++) {
                    QPoint p = rotatePoint(width, height, ROTATION[i], QPoint(dx, dy));
                    tiles[i] = ft[i]->tile(p.x(), p.y());
                    if (masterTile == nullptr && tiles[i] != nullptr) {
                        masterTile = tiles[i];
                    }
                }
                if (masterTile == nullptr)
                    continue;
                QSharedPointer<TileRotatedVisual> visual = allocVisual();
                for (int i = 0; i < MAP_ROTATION_COUNT; i++) {
                    BuildingTile *btile = tiles[i];
                    if (btile == nullptr)
                        continue;
                    visual->mData[i].addTile(btile->name());
                    initVisual(btile, visual, ROTATION[i]);
//                    mMapping[btile->name()] = tileR->name();
                }
            }
        }
        return false;
    }

    void initFurniture()
    {
        const QList<FurnitureGroup*> furnitureGroups = FurnitureGroups::instance()->groups();
        for (FurnitureGroup* furnitureGroup : furnitureGroups) {
            for (FurnitureTiles* furnitureTiles : furnitureGroup->mTiles) {
                FurnitureTile* ft[4];
                ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureN);
                ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureE);
                ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureS);
                ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureW);
                if (isFurnitureOK(ft)) {
                    initFurniture(ft);
                }

                if (furnitureTiles->hasCorners()) {
                    ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureNW);
                    ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureNE);
                    ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureSE);
                    ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureSW);
                    if (isFurnitureOK(ft)) {
                        initFurniture(ft);
                    }
                }
            }
        }
    }

    QString getTilesetNameR(const QString& tilesetName, int degrees)
    {
       return QString(QLatin1Literal("%1_R%2")).arg(tilesetName).arg(degrees);
    }

    QString getTileName0(BuildingTile *buildingTile)
    {
        QString tilesetName = getTilesetNameR(buildingTile->mTilesetName, 0);
        return BuildingTilesMgr::instance()->nameForTile(tilesetName, buildingTile->mIndex);
    }

    QString getTileName90(BuildingTile *buildingTile)
    {
        QString tilesetName = getTilesetNameR(buildingTile->mTilesetName, 90);
        return BuildingTilesMgr::instance()->nameForTile(tilesetName, buildingTile->mIndex);
    }

    QString getTileName180(BuildingTile *buildingTile)
    {
        QString tilesetName = getTilesetNameR(buildingTile->mTilesetName, 180);
        return BuildingTilesMgr::instance()->nameForTile(tilesetName, buildingTile->mIndex);
    }

    QString getTileName270(BuildingTile *buildingTile)
    {
        QString tilesetName = getTilesetNameR(buildingTile->mTilesetName, 270);
        return BuildingTilesMgr::instance()->nameForTile(tilesetName, buildingTile->mIndex);
    }

    QMap<QString,TilesetRotated*>* tilesetLookup(int degrees)
    {
        switch (degrees) {
        case 0: return &mTilesetR0;
        case 90: return &mTilesetR90;
        case 180: return &mTilesetR180;
        case 270: return &mTilesetR270;
        }
        return nullptr;
    }

    TilesetRotated* getTilesetRotated(const QString &tilesetName, int degrees, MapRotation mapRotation)
    {
        QMap<QString, TilesetRotated*>& lookup = *tilesetLookup(degrees);
        TilesetRotated *tileset = lookup[tilesetName];
        if (tileset == nullptr) {
            tileset = new TilesetRotated();
            tileset->mNameUnrotated = tilesetName;
            tileset->mNameRotated = getTilesetNameR(tilesetName, degrees);
            if (Tileset *tileset1 = TileMetaInfoMgr::instance()->tileset(tilesetName)) {
                tileset->mColumnCount = tileset1->columnCount();
            } else {
                tileset->mColumnCount = 8; // FIXME
            }
            tileset->mRotation = mapRotation;
            lookup[tilesetName] = tileset;
            mTilesets += tileset;
        }
        return tileset;
    }

    TileRotated* getTileRotated(BuildingTile *buildingTile, int degrees, MapRotation mapRotation)
    {
        TilesetRotated *tileset = getTilesetRotated(buildingTile->mTilesetName, degrees, mapRotation);
        return getTileRotated(tileset, buildingTile->mIndex);
    }

    TileRotated* getTileRotatedR0(BuildingTile *buildingTile)
    {
        return getTileRotated(buildingTile, 0, MapRotation::NotRotated);
    }

    TileRotated* getTileRotatedR90(BuildingTile *buildingTile)
    {
        return getTileRotated(buildingTile, 90, MapRotation::Clockwise90);
    }

    TileRotated* getTileRotatedR180(BuildingTile *buildingTile)
    {
        return getTileRotated(buildingTile, 180, MapRotation::Clockwise180);
    }

    TileRotated* getTileRotatedR270(BuildingTile *buildingTile)
    {
        return getTileRotated(buildingTile, 270, MapRotation::Clockwise270);
    }

    TileRotated* getTileRotated(TilesetRotated *tileset, int index)
    {
        if (index >= tileset->mTileByID.size() || tileset->mTileByID[index] == nullptr) {
            TileRotated* tile = new TileRotated();
            tile->mTileset = tileset;
            tile->mID = index;
            tile->mXY = QPoint(index % tileset->mColumnCount, index / tileset->mColumnCount);
            tileset->mTiles += tile;
            if (index >= tileset->mTileByID.size()) {
                tileset->mTileByID.resize(index + 1);
            }
            tileset->mTileByID[index] = tile;
        }
        return tileset->mTileByID[index];
    }

    void initVisual(BuildingTile *buildingTile, QSharedPointer<TileRotatedVisual> &visual, MapRotation mapRotation)
    {
        TileRotated* tileRotatedN = getTileRotatedR0(buildingTile);
        tileRotatedN->mVisual = visual;
        tileRotatedN->mRotation = mapRotation;

        TileRotated* tileRotatedE = getTileRotatedR90(buildingTile);
        tileRotatedE->mVisual = visual;
        tileRotatedE->mRotation = mapRotation;

        TileRotated* tileRotatedS = getTileRotatedR180(buildingTile);
        tileRotatedS->mVisual = visual;
        tileRotatedS->mRotation = mapRotation;

        TileRotated* tileRotatedW = getTileRotatedR270(buildingTile);
        tileRotatedW->mVisual = visual;
        tileRotatedW->mRotation = mapRotation;
    }

    void init()
    {
        BuildingTileCategory *category = BuildingTilesMgr::instance()->catFloors();
        for (BuildingTileEntry *entry : category->entries()) {
            BuildingTile *btile = entry->tile(BTC_Floors::Floor);
            if (btile->isNone())
                continue;
            QSharedPointer<TileRotatedVisual> visual = allocVisual();
            visual->mData[0].addTile(btile->name());
            visual->mData[1].addTile(btile->name());
            visual->mData[2].addTile(btile->name());
            visual->mData[3].addTile(btile->name());
            initVisual(btile, visual, MapRotation::NotRotated);
        }

        initFromBuildingTiles(BuildingTilesMgr::instance()->catDoors()->entries(), BTC_Doors::North, BTC_Doors::West, TileRotateType::Door);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catDoorFrames()->entries(), BTC_DoorFrames::North, BTC_DoorFrames::West, TileRotateType::DoorFrame);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::Wall);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::NorthDoor, BTC_Walls::WestDoor, TileRotateType::DoorFrame);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::NorthWindow, BTC_Walls::WestWindow, TileRotateType::WindowFrame);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::Wall);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::NorthDoor, BTC_Walls::WestDoor, TileRotateType::DoorFrame);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::NorthWindow, BTC_Walls::WestWindow, TileRotateType::WindowFrame);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWallTrim()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::WallExtra);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWallTrim()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::WallExtra);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catWindows()->entries(), BTC_Windows::North, BTC_Windows::West, TileRotateType::Window);

        initGrime();
        initRoofCaps();
        initRoofSlopes();

        initFurniture();
    }

    QSharedPointer<TileRotatedVisual> allocVisual()
    {
        QSharedPointer<TileRotatedVisual> visual(new TileRotatedVisual());
        visual->mUuid = QUuid::createUuid();
//        mVisualLookup[visual->mUuid] = visual;
        mVisuals += visual;
        return visual;
    }

    QList<TilesetRotated*> mTilesets;
    QList<QSharedPointer<TileRotatedVisual>> mVisuals;
//    QMap<QString, QString> mMapping;
    QMap<QString, TilesetRotated*> mTilesetR0;
    QMap<QString, TilesetRotated*> mTilesetR90;
    QMap<QString, TilesetRotated*> mTilesetR180;
    QMap<QString, TilesetRotated*> mTilesetR270;
};

#else

// This is temporary code to bootstrap creation of TileRotation.txt from the existing BuildingEd furniture information.
class InitFromBuildingTiles
{
public:
    QPoint rotatePoint(int width, int height, MapRotation rotation, const QPoint &pos) const
    {
        switch (rotation)
        {
        case MapRotation::NotRotated: // w,h=3,2 x,y=2,1 -> 2,1
            return pos;
        case MapRotation::Clockwise90:
            return QPoint(height - pos.y() - 1, pos.x()); // w,h=3,2 x,y=2,1 -> 0,2
        case MapRotation::Clockwise180:
            return QPoint(width - pos.x() - 1, height - pos.y() - 1); // w,h=3,2 x,y=2,1 -> 0,0
        case MapRotation::Clockwise270:
            return QPoint(pos.y(), width - pos.x() - 1); // w,h=3,2 x,y=2,1 -> 1,0
        }
    }

    void initFromBuildingTiles(const QList<BuildingTileEntry*> &entries, int n, int w, TileRotateType rotateType)
    {
        for (BuildingTileEntry* bte : entries) {
            initFromBuildingTiles(bte, n, w, rotateType);
        }
    }

    void initFromBuildingTiles(BuildingTileEntry* bte, int n, int w, int nw, TileRotateType rotateType)
    {
        BuildingTile *tileN = bte->tile(n);
        BuildingTile *tileW = bte->tile(w);
        if (tileN->isNone() && tileW->isNone())
            return;
        QString tileNames[MAP_ROTATION_COUNT];
        tileNames[0] = tileN->name();
        tileNames[3] = tileW->name();
        if (nw != -1) {
            BuildingTile *tileNW = bte->tile(nw);
            if (!tileNW->isNone()) {
                tileNames[1] = tileNW->name();
            }
        }
        addFurnitureTilesForRotateInfo(tileNames, rotateType);
    }

    void initFromBuildingTiles(BuildingTileEntry* bte, int n, int w, TileRotateType rotateType)
    {
        int nw = (rotateType == TileRotateType::Wall) ? BTC_Walls::NorthWest : -1;
        initFromBuildingTiles(bte, n, w, nw, rotateType);
    }

    FurnitureTile::FurnitureOrientation orient[MAP_ROTATION_COUNT] = {
        FurnitureTile::FurnitureN,
        FurnitureTile::FurnitureE,
        FurnitureTile::FurnitureS,
        FurnitureTile::FurnitureW
    };

    void addFurnitureTilesForRotateInfo(QString tileNames[MAP_ROTATION_COUNT], TileRotateType rotateType)
    {
        TRWFurnitureTiles* furnitureTiles1 = new TRWFurnitureTiles();
        furnitureTiles1->mType = rotateType;
        for (int j = 0; j < 4; j++) {
            FurnitureTile* furnitureTile1 = new FurnitureTile(furnitureTiles1, orient[j]);
            BuildingTile* buildingTile = BuildingTilesMgr::instance()->get(tileNames[j]);
            if (!buildingTile->isNone()) {
                furnitureTile1->setTile(0, 0, buildingTile);
            }
            furnitureTiles1->setTile(furnitureTile1);
        }
        mFurnitureTiles += furnitureTiles1;
    }

    MapRotation rotation[MAP_ROTATION_COUNT] = {
        MapRotation::NotRotated,
        MapRotation::Clockwise90,
        MapRotation::Clockwise180,
        MapRotation::Clockwise270
    };

    bool initFromBuildingTiles(FurnitureTile* ft[4])
    {
        if (ft[0]->isEmpty() && ft[1]->isEmpty() && ft[2]->isEmpty() && ft[3]->isEmpty())
            return false;
        int width = ft[0]->width();
        int height = ft[0]->height();
        for (int dy = 0; dy < height; dy++) {
            for (int dx = 0; dx < width; dx++) {
                for (int i = 0; i < 4; i++) {
                    QPoint p = rotatePoint(width, height, rotation[i], QPoint(dx, dy));
                    BuildingTile* buildingTile = ft[i]->tile(p.x(), p.y());
                    if (buildingTile != nullptr) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void initGrime()
    {
#if 0
        West,
        North,
        NorthWest,
        SouthEast,
        WestWindow,
        NorthWindow,
        WestDoor,
        NorthDoor,
        WestTrim,
        NorthTrim,
        NorthWestTrim,
        SouthEastTrim,
        WestDoubleLeft,
        WestDoubleRight,
        NorthDoubleLeft,
        NorthDoubleRight,
#endif
        BuildingTileCategory *slopes = BuildingTilesMgr::instance()->catGrimeWall();
        for (BuildingTileEntry *entry : slopes->entries()) {
            initFromBuildingTiles(entry, BTC_GrimeWall::North, BTC_GrimeWall::West, BTC_GrimeWall::NorthWest, TileRotateType::Wall);
            initFromBuildingTiles(entry, BTC_GrimeWall::NorthDoor, BTC_GrimeWall::WestDoor, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_GrimeWall::NorthWindow, BTC_GrimeWall::WestWindow, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_GrimeWall::NorthTrim, BTC_GrimeWall::WestTrim, BTC_GrimeWall::NorthWestTrim, TileRotateType::Wall);
            // DoubleLeft/DoubleRight should be single FurnitureTile
        }
    }

    void initRoofCaps()
    {
#if 0
        // Sloped cap tiles go left-to-right or bottom-to-top
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,

        // Cap tiles with peaked tops
        PeakPt5S, PeakPt5E,
        PeakOnePt5S, PeakOnePt5E,
        PeakTwoPt5S, PeakTwoPt5E,

        // Cap tiles with flat tops
        CapGapS1, CapGapS2, CapGapS3,
        CapGapE1, CapGapE2, CapGapE3,

        // Cap tiles for shallow (garage, trailer, etc) roofs
        CapShallowRiseS1, CapShallowRiseS2, CapShallowFallS1, CapShallowFallS2,
        CapShallowRiseE1, CapShallowRiseE2, CapShallowFallE1, CapShallowFallE2,
#endif
        BuildingTileCategory *category = BuildingTilesMgr::instance()->catRoofCaps();
        for (BuildingTileEntry *entry : category->entries()) {
            initFromBuildingTiles(entry, BTC_RoofCaps::CapRiseS1, BTC_RoofCaps::CapRiseE1, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapRiseS2, BTC_RoofCaps::CapRiseE2, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapRiseS3, BTC_RoofCaps::CapRiseE3, TileRotateType::WallExtra);

            initFromBuildingTiles(entry, BTC_RoofCaps::CapFallS1, BTC_RoofCaps::CapFallE1, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapFallS2, BTC_RoofCaps::CapFallE2, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapFallS3, BTC_RoofCaps::CapFallE3, TileRotateType::WallExtra);

            initFromBuildingTiles(entry, BTC_RoofCaps::PeakPt5S, BTC_RoofCaps::PeakPt5E, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::PeakOnePt5S, BTC_RoofCaps::PeakOnePt5E, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::PeakTwoPt5S, BTC_RoofCaps::PeakTwoPt5E, TileRotateType::WallExtra);

            initFromBuildingTiles(entry, BTC_RoofCaps::CapGapS1, BTC_RoofCaps::CapGapE1, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapGapS2, BTC_RoofCaps::CapGapE2, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapGapS3, BTC_RoofCaps::CapGapE3, TileRotateType::WallExtra);

            initFromBuildingTiles(entry, BTC_RoofCaps::CapShallowRiseS1, BTC_RoofCaps::CapShallowRiseE1, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapShallowRiseS2, BTC_RoofCaps::CapShallowRiseE2, TileRotateType::WallExtra);

            initFromBuildingTiles(entry, BTC_RoofCaps::CapShallowFallS1, BTC_RoofCaps::CapShallowFallE1, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapShallowFallS2, BTC_RoofCaps::CapShallowFallE2, TileRotateType::WallExtra);
        }
    }

    void initRoofSlope(BuildingTileEntry *entry, int north, int west)
    {
        initFromBuildingTiles(entry, north, west, TileRotateType::None);
    }

    void initRoofSlope(BuildingTileEntry *bte, int north, int east, int west)
    {
        BuildingTile *tileN = bte->tile(north);
        BuildingTile *tileE = bte->tile(east);
        BuildingTile *tileW = bte->tile(west);
        if (tileN->isNone() || tileE->isNone() || tileW->isNone())
            return;
        QString tileNames[MAP_ROTATION_COUNT];
        tileNames[TileRotateInfo::NORTH] = tileN->name();
        tileNames[TileRotateInfo::EAST] = tileE->name();
        tileNames[TileRotateInfo::WEST] = tileW->name();
        addFurnitureTilesForRotateInfo(tileNames, TileRotateType::None);
    }

    void initRoofSlopes()
    {
#if 0
        // Sloped sides
        SlopeS1, SlopeS2, SlopeS3,
        SlopeE1, SlopeE2, SlopeE3,
        SlopePt5S, SlopePt5E,
        SlopeOnePt5S, SlopeOnePt5E,
        SlopeTwoPt5S, SlopeTwoPt5E,

        // Shallow sides
        ShallowSlopeW1, ShallowSlopeW2,
        ShallowSlopeE1, ShallowSlopeE2,
        ShallowSlopeN1, ShallowSlopeN2,
        ShallowSlopeS1, ShallowSlopeS2,

        // Sloped corners
        Inner1, Inner2, Inner3,
        Outer1, Outer2, Outer3,
        InnerPt5, InnerOnePt5, InnerTwoPt5,
        OuterPt5, OuterOnePt5, OuterTwoPt5,
        CornerSW1, CornerSW2, CornerSW3,
        CornerNE1, CornerNE2, CornerNE3,
#endif
        BuildingTileCategory *category = BuildingTilesMgr::instance()->catRoofSlopes();
        for (BuildingTileEntry *entry : category->entries()) {
            initRoofSlope(entry, BTC_RoofSlopes::SlopeS1, BTC_RoofSlopes::SlopeE1);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeS2, BTC_RoofSlopes::SlopeE2);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeS3, BTC_RoofSlopes::SlopeE3);

            initRoofSlope(entry, BTC_RoofSlopes::SlopePt5S, BTC_RoofSlopes::SlopePt5E);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeOnePt5S, BTC_RoofSlopes::SlopeOnePt5E);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeTwoPt5S, BTC_RoofSlopes::SlopeTwoPt5E);

            {
                BuildingTile *tileN = entry->tile(BTC_RoofSlopes::ShallowSlopeN1);
                BuildingTile *tileE = entry->tile(BTC_RoofSlopes::ShallowSlopeE1);
                BuildingTile *tileS = entry->tile(BTC_RoofSlopes::ShallowSlopeS1);
                BuildingTile *tileW = entry->tile(BTC_RoofSlopes::ShallowSlopeW1);
                if (!tileN->isNone() && !tileE->isNone() && !tileS->isNone() && !tileW->isNone()) {
                    QString tileNames[MAP_ROTATION_COUNT];
                    tileNames[TileRotateInfo::NORTH] = tileN->name();
                    tileNames[TileRotateInfo::EAST] = tileE->name();
                    tileNames[TileRotateInfo::SOUTH] = tileS->name();
                    tileNames[TileRotateInfo::WEST] = tileW->name();
                    addFurnitureTilesForRotateInfo(tileNames, TileRotateType::None);
                }
            }

            {
                BuildingTile *tileN = entry->tile(BTC_RoofSlopes::ShallowSlopeN2);
                BuildingTile *tileE = entry->tile(BTC_RoofSlopes::ShallowSlopeE2);
                BuildingTile *tileS = entry->tile(BTC_RoofSlopes::ShallowSlopeS2);
                BuildingTile *tileW = entry->tile(BTC_RoofSlopes::ShallowSlopeW2);
                if (!tileN->isNone() && !tileE->isNone() && !tileS->isNone() && !tileW->isNone()) {
                    QString tileNames[MAP_ROTATION_COUNT];
                    tileNames[TileRotateInfo::NORTH] = tileN->name();
                    tileNames[TileRotateInfo::EAST] = tileE->name();
                    tileNames[TileRotateInfo::SOUTH] = tileS->name();
                    tileNames[TileRotateInfo::WEST] = tileW->name();
                    addFurnitureTilesForRotateInfo(tileNames, TileRotateType::None);
                }
            }

            initRoofSlope(entry, BTC_RoofSlopes::Outer1, BTC_RoofSlopes::CornerSW1, BTC_RoofSlopes::CornerNE1);
            initRoofSlope(entry, BTC_RoofSlopes::Outer2, BTC_RoofSlopes::CornerSW2, BTC_RoofSlopes::CornerNE2);
            initRoofSlope(entry, BTC_RoofSlopes::Outer3, BTC_RoofSlopes::CornerSW3, BTC_RoofSlopes::CornerNE3);
        }
    }

    void initFromBuildingTiles()
    {
        initFromBuildingTiles(BuildingTilesMgr::instance()->catDoors()->entries(), BTC_Doors::North, BTC_Doors::West, TileRotateType::Door);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catDoorFrames()->entries(), BTC_DoorFrames::North, BTC_DoorFrames::West, TileRotateType::DoorFrame);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::Wall);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::NorthDoor, BTC_Walls::WestDoor, TileRotateType::DoorFrame);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::NorthWindow, BTC_Walls::WestWindow, TileRotateType::WindowFrame);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::Wall);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::NorthDoor, BTC_Walls::WestDoor, TileRotateType::DoorFrame);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::NorthWindow, BTC_Walls::WestWindow, TileRotateType::WindowFrame);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWallTrim()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::WallExtra);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWallTrim()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::WallExtra);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catWindows()->entries(), BTC_Windows::North, BTC_Windows::West, TileRotateType::Window);

        initGrime();
        initRoofCaps();
        initRoofSlopes();


        QList<FurnitureTile*> addThese;
        const QList<FurnitureGroup*> furnitureGroups = FurnitureGroups::instance()->groups();
#if 1
        for (FurnitureGroup* furnitureGroup : furnitureGroups) {
            for (FurnitureTiles* furnitureTiles : furnitureGroup->mTiles) {
                FurnitureTile* ft[4];
                ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureN);
                ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureE);
                ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureS);
                ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureW);
                if (initFromBuildingTiles(ft)) {
                    addThese << ft[0] << ft[1] << ft[2] << ft[3];
                }

                if (furnitureTiles->hasCorners()) {
                    ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureNE);
                    ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureSE);
                    ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureSW);
                    ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureNW);
                    if (initFromBuildingTiles(ft)) {
                        addThese << ft[0] << ft[1] << ft[2] << ft[3];
                    }
                }
            }
        }
#else
        // size=1x1
        for (FurnitureGroup* furnitureGroup : furnitureGroups) {
            for (FurnitureTiles* furnitureTiles : furnitureGroup->mTiles) {
                FurnitureTile* ft[4];
                ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureN);
                ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureE);
                ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureS);
                ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureW);
                if (ft[0]->isEmpty() && ft[1]->isEmpty() && ft[2]->isEmpty() && ft[3]->isEmpty())
                    continue;
                if (ft[0]->size() != QSize(1, 1))
                    continue;
                TileRotateInfo entry;
                bool empty = true;
                for (int i = 0; i < 4; i++) {
                    BuildingTile* buildingTile = ft[i]->tile(0, 0);
                    if (buildingTile != nullptr) {
                        entry.mTileNames[i] = buildingTile->name();
                        empty = false;
                    }
                }
                if (empty)
                    continue;
                addThese << ft[0] << ft[1] << ft[2] << ft[3];
            }
        }
        // size > 1x1
        for (FurnitureGroup* furnitureGroup : furnitureGroups) {
            for (FurnitureTiles* furnitureTiles : furnitureGroup->mTiles) {
                FurnitureTile* ft[4];
                ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureN);
                ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureE);
                ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureS);
                ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureW);
                if (ft[0]->isEmpty() && ft[1]->isEmpty() && ft[2]->isEmpty() && ft[3]->isEmpty())
                    continue;
//                if (ft[0]->size() == QSize(1, 1))
//                    continue;
                int width = ft[0]->width();
                int height = ft[0]->height();
                int add = false;
                // One new TileRotateEntry per BuildingTile in the grid.
                for (int dy = 0; dy < height; dy++) {
                    for (int dx = 0; dx < width; dx++) {
                        TileRotateInfo entry;
                        bool empty = true;
                        for (int i = 0; i < 4; i++) {
                            QPoint p = rotatePoint(width, height, rotation[i], QPoint(dx, dy));
                            BuildingTile* buildingTile = ft[i]->tile(p.x(), p.y());
                            if (buildingTile != nullptr) {
                                entry.mTileNames[i] = buildingTile->name();
                                empty = false;
                            }
                        }
                        if (empty)
                            continue;
                        add = true;
                    }
                }
                if (add) {
                    addThese << ft[0] << ft[1] << ft[2] << ft[3];
                }
            }
        }
#endif

        FurnitureTile::FurnitureOrientation orient[4] = {
            FurnitureTile::FurnitureN,
            FurnitureTile::FurnitureE,
            FurnitureTile::FurnitureS,
            FurnitureTile::FurnitureW
        };

        for (int i = 0; i < addThese.size(); i += 4) {
            FurnitureTile* ft[4];
            ft[0] = addThese[i+0];
            ft[1] = addThese[i+1];
            ft[2] = addThese[i+2];
            ft[3] = addThese[i+3];
            TRWFurnitureTiles* furnitureTiles1 = new TRWFurnitureTiles();
            FurnitureTiles::FurnitureLayer layer = FurnitureTiles::FurnitureLayer::InvalidLayer;
            if (ft[0])
                layer = ft[0]->owner()->layer();
            else if (ft[1])
                layer = ft[1]->owner()->layer();
            switch (layer) {
            case FurnitureTiles::FurnitureLayer::LayerWallOverlay:
            case FurnitureTiles::FurnitureLayer::LayerFrames:
            case FurnitureTiles::FurnitureLayer::LayerWalls:
                furnitureTiles1->mType = TileRotateType::WallExtra;
                break;
            default:
                furnitureTiles1->mType = TileRotateType::None;
                break;
            }
            for (int j = 0; j < 4; j++) {
                FurnitureTile* furnitureTile1 = new FurnitureTile(furnitureTiles1, orient[j]);
                for (int dy = 0; dy < ft[j]->height(); dy++) {
                    for (int dx = 0; dx < ft[j]->width(); dx++) {
                        BuildingTile* buildingTile = ft[j]->tile(dx, dy);
                        if (buildingTile != nullptr) {
                            furnitureTile1->setTile(dx, dy, buildingTile);
                        }
                    }
                }
                furnitureTiles1->setTile(furnitureTile1);
            }
            mFurnitureTiles += furnitureTiles1;
        }
    }

    QList<TRWFurnitureTiles*> mFurnitureTiles;
};
#endif

// // // // //

TileRotationWindow::TileRotationWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TileRotationWindow),
    mZoomable(new Zoomable),
    mCurrentTilesetRotated(nullptr),
    mCurrentTileset(nullptr),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this))
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);

    mUndoGroup->addStack(mUndoStack);
    mUndoGroup->setActiveStack(mUndoStack);

    QAction *undoAction = mUndoGroup->createUndoAction(this, tr("Undo"));
    QAction *redoAction = mUndoGroup->createRedoAction(this, tr("Redo"));
    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
    undoAction->setIcon(undoIcon);
    redoAction->setIcon(redoIcon);
    ui->toolBar->addActions(QList<QAction*>() << undoAction << redoAction);

    for (int i = 0; TILE_ROTATE_NAMES[i] != nullptr; i++) {
        ui->typeComboBox->addItem(QLatin1String(TILE_ROTATE_NAMES[i]));
    }
    connect(ui->typeComboBox, QOverload<int>::of(&QComboBox::activated), this, &TileRotationWindow::typeComboActivated);

    connect(mUndoGroup, SIGNAL(cleanChanged(bool)), SLOT(syncUI()));

    connect(ui->actionNew, &QAction::triggered, this, &TileRotationWindow::fileNew);
    connect(ui->actionOpen, &QAction::triggered, this, QOverload<>::of(&TileRotationWindow::fileOpen));
    connect(ui->actionSave, &QAction::triggered, this, QOverload<>::of(&TileRotationWindow::fileSave));
    connect(ui->actionSaveAs, &QAction::triggered, this, &TileRotationWindow::fileSaveAs);
    connect(ui->actionClose, &QAction::triggered, this, &TileRotationWindow::close);
    connect(ui->actionAddTiles, &QAction::triggered, this, &TileRotationWindow::addTiles);
    connect(ui->actionClearTiles, &QAction::triggered, this, &TileRotationWindow::clearTiles);
    connect(ui->actionRemove, &QAction::triggered, this, &TileRotationWindow::removeTiles);

    ui->tilesetRotatedFilter->setClearButtonEnabled(true);
    ui->tilesetRotatedFilter->setEnabled(false);
    connect(ui->tilesetRotatedFilter, &QLineEdit::textEdited, this, &TileRotationWindow::rotatedFilterEdited);


    ui->tilesetRotatedList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    connect(ui->tilesetRotatedList, &QListWidget::itemSelectionChanged, this, &TileRotationWindow::tilesetRotatedSelectionChanged);

    ui->tilesetList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    connect(ui->tilesetList, &QListWidget::itemSelectionChanged, this, &TileRotationWindow::tilesetSelectionChanged);
    connect(ui->tilesetMgr, &QToolButton::clicked, this,&TileRotationWindow::manageTilesets);

    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAdded, this, &TileRotationWindow::tilesetAdded);
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAboutToBeRemoved, this, &TileRotationWindow::tilesetAboutToBeRemoved);
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetRemoved, this, &TileRotationWindow::tilesetRemoved);

    connect(TilesetManager::instance(), &TilesetManager::tilesetChanged, this, &TileRotationWindow::tilesetChanged);

    ui->tilesetFilter->setClearButtonEnabled(true);
    ui->tilesetFilter->setEnabled(false);
    connect(ui->tilesetFilter, &QLineEdit::textEdited, this, &TileRotationWindow::filterEdited);

//    ui->tilesetTilesView->setZoomable(mZoomable);
    ui->tilesetTilesView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tilesetTilesView->setDragEnabled(true);
//    connect(ui->tilesetTilesView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TileRotationWindow::syncUI);
    connect(ui->tilesetTilesView, &MixedTilesetView::activated, this, &TileRotationWindow::tileActivated);

    ui->tileRotateView->setZoomable(mZoomable);
    ui->tileRotateView->setAcceptDrops(true);
#if 1
    ui->tileRotateView->setItemDelegate(new TileRotateDelegate(ui->tileRotateView, ui->tileRotateView, this));
#else
    connect(ui->tileRotateView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TileRotationWindow::syncUI);
    connect(ui->tileRotateView->model(), &TileRotateModel::tileDropped, this, &TileRotationWindow::tileDropped);
    connect(ui->tileRotateView, &TileRotateView::activated, this, &TileRotationWindow::tileRotatedActivated);
#endif
    setTilesetList();
    syncUI();

#if 1
    InitFromBuildingTiles initFromBuildingTiles;
    initFromBuildingTiles.init();
    mTilesets = initFromBuildingTiles.mTilesets;
    mVisuals = initFromBuildingTiles.mVisuals;
    mCurrentTilesetRotated = nullptr;
    std::sort(mTilesets.begin(), mTilesets.end(),
              [](TilesetRotated* a, TilesetRotated *b) {
        if (a->nameUnrotated() == b->nameUnrotated()) {
            return int(a->mRotation) < int(b->mRotation);
        }
                  return a->nameRotated() < b->nameRotated();
              });
    for (TilesetRotated *tileset : mTilesets) {
        mTilesetByNameRotated[tileset->nameRotated()] = tileset;
    }
    for (TilesetRotated *tileset : mTilesets) {
        if (tileset == nullptr)
            continue;
        mCurrentTilesetRotated = tileset;
#if 1
        ui->tileRotateView->setTileset(getRotatedTileset(tileset->nameRotated()));
#else
        ui->tileRotateView->setTiles(mCurrentTilesetRotated->mTiles);
#endif
        break;
    }
    setTilesetRotatedList();
    mFileName = QLatin1Literal("D:/pz/TileRotation.txt");
#endif
}

TileRotationWindow::~TileRotationWindow()
{
    delete ui;
    qDeleteAll(mTilesets);
}

void TileRotationWindow::fileNew()
{
    if (!confirmSave())
        return;

    QString fileName = getSaveLocation();
    if (fileName.isEmpty())
        return;

    mUndoStack->clear();
    mFileName = fileName;
    qDeleteAll(mTilesets);
    mTilesets.clear();
    qDeleteAll(mTilesetRotated);
    mTilesetRotated.clear();
    mCurrentTilesetRotated = nullptr;
#if 1
    ui->tileRotateView->clear();
#else
    ui->tileRotateView->setTiles(QList<TileRotated*>());
#endif
    syncUI();
}

void TileRotationWindow::fileOpen()
{
    if (!confirmSave())
        return;

    QSettings settings;
    QString key = QLatin1String("ContainerOverlay/LastOpenPath");
    QString lastPath = settings.value(key).toString();

    QString fileName = QFileDialog::getOpenFileName(this, tr("Choose .txt file"),
                                                    lastPath,
                                                    QLatin1String("Text files (*.txt)"));
    if (fileName.isEmpty())
        return;

    settings.setValue(key, QFileInfo(fileName).absolutePath());

    fileOpen(fileName);

    syncUI();
}

void TileRotationWindow::fileOpen(const QString &fileName)
{
    QList<TilesetRotated*> tilesets;
    QList<QSharedPointer<TileRotatedVisual>> visuals;
    if (!fileOpen(fileName, tilesets, visuals)) {
        QMessageBox::warning(this, tr("Error reading file"), mError);
        return;
    }

    mUndoStack->clear();
    mFileName = fileName;
    qDeleteAll(mTilesets);
    mTilesets = tilesets;
    qDeleteAll(mTilesetRotated);
    mTilesetRotated.clear();
    for (TilesetRotated *tileset : mTilesets) {
        mTilesetByNameRotated[tileset->nameRotated()] = tileset;
    }
    mCurrentTilesetRotated = nullptr;
    for (TilesetRotated *tileset : mTilesets) {
        if (tileset == nullptr)
            continue;
        mCurrentTilesetRotated = tileset;
#if 1
        ui->tileRotateView->setTileset(getRotatedTileset(tileset->nameRotated()));
#else
        ui->tileRotateView->setTiles(mCurrentTilesetRotated->mTiles);
#endif
        break;
    }
    setTilesetRotatedList();
    syncUI();
}

void TileRotationWindow::closeEvent(QCloseEvent *event)
{
    if (confirmSave()) {
        mFileName.clear();
        ui->tileRotateView->clear();
        qDeleteAll(mTilesets);
        mTilesets.clear();
        mUndoStack->clear();
        syncUI();
        event->accept();
    } else {
        event->ignore();
    }
}

bool TileRotationWindow::confirmSave()
{
    if (mFileName.isEmpty() || mUndoStack->isClean())
        return true;

    int ret = QMessageBox::warning(
            this, tr("Unsaved Changes"),
            tr("There are unsaved changes. Do you want to save now?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    switch (ret) {
    case QMessageBox::Save:    return fileSave();
    case QMessageBox::Discard: return true;
    case QMessageBox::Cancel:
    default:
        return false;
    }
}

QString TileRotationWindow::getSaveLocation()
{
    QSettings settings;
    QString key = QLatin1String("ContainerOverlay/LastOpenPath");
    QString suggestedFileName = QLatin1String("TileRotation.txt");
    if (mFileName.isEmpty()) {
        QString lastPath = settings.value(key).toString();
        if (!lastPath.isEmpty()) {
            suggestedFileName = lastPath + QLatin1String("/TileRotation.txt");
        }
    } else {
        suggestedFileName = mFileName;
    }
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                                    suggestedFileName,
                                                    QLatin1String("Text files (*.txt)"));
    if (fileName.isEmpty())
        return QString();

    settings.setValue(key, QFileInfo(fileName).absoluteFilePath());

    return fileName;
}

bool TileRotationWindow::fileSave()
{
    if (mFileName.length())
        return fileSave(mFileName);
    return fileSaveAs();
}

bool TileRotationWindow::fileSaveAs()
{
    QString fileName = getSaveLocation();
    if (fileName.isEmpty())
        return false;
    return fileSave(fileName);
}

void TileRotationWindow::addTiles()
{

}

void TileRotationWindow::clearTiles()
{
#if 0
    TileRotateView *v = ui->tileRotateView;
    QModelIndexList selection = v->selectionModel()->selectedIndexes();
    for (QModelIndex index : selection) {
        TileRotated *tile = v->model()->tileAt(index);
    }
    mUndoStack->beginMacro(tr("Clear Tiles"));
    // TODO: r90 or r180 or r270
    mUndoStack->endMacro();
#endif
}

void TileRotationWindow::removeTiles()
{

}

bool TileRotationWindow::fileSave(const QString &fileName)
{
    if (!fileSave(fileName, mTilesets, mVisuals)) {
        QMessageBox::warning(this, tr("Error writing file"), mError);
        return false;
    }
    mFileName = fileName;
    mUndoStack->setClean();
    syncUI();
    TileRotation::instance()->reload(); // hack
    return true;
}

bool TileRotationWindow::fileOpen(const QString &fileName, QList<TilesetRotated *> &tilesets, QList<QSharedPointer<Tiled::TileRotatedVisual>>& visuals)
{
    TileRotationFile file;
    if (!file.read(fileName)) {
        mError = file.errorString();
        return false;
    }
    tilesets = file.takeTilesets();
    visuals = file.takeVisuals();
    return true;
}

bool TileRotationWindow::fileSave(const QString &fileName, const QList<TilesetRotated *> &tilesets, const QList<QSharedPointer<Tiled::TileRotatedVisual>>& visuals)
{
    TileRotationFile file;
    if (!file.write(fileName, tilesets, visuals)) {
        mError = file.errorString();
        return false;
    }
    return true;
}

void TileRotationWindow::updateWindowTitle()
{
    if (mFileName.length()) {
        QString fileName = QDir::toNativeSeparators(mFileName);
        setWindowTitle(tr("[*]%1 - Tile Rotation").arg(fileName));
    } else {
        setWindowTitle(QLatin1Literal("Tile Rotation"));
    }
    setWindowModified(!mUndoStack->isClean());
}

void TileRotationWindow::syncUI()
{
    ui->actionSave->setEnabled(!mFileName.isEmpty() && !mUndoStack->isClean());
    ui->actionSaveAs->setEnabled(!mFileName.isEmpty());

    QModelIndexList selected = ui->tileRotateView->selectionModel()->selectedIndexes();
    ui->actionAddTiles->setEnabled(!mFileName.isEmpty());
    ui->actionClearTiles->setEnabled(selected.size() > 0);
    ui->actionRemove->setEnabled(selected.size() > 0);

    ui->typeComboBox->setEnabled(selected.isEmpty() == false);
    if (selected.isEmpty()) {
        ui->typeComboBox->setCurrentIndex(0);
    } else {
//        TileRotated *tile = ui->tileRotateView->model()->tileAt(selected.first());
//        TRWFurnitureTiles* furnitureTiles = dynamic_cast<TRWFurnitureTiles*>(furnitureTile->owner());
//        ui->typeComboBox->setCurrentIndex(int(furnitureTiles->mType));
    }

    updateWindowTitle();
}

void TileRotationWindow::tileActivated(const QModelIndex &index)
{
    Tile *tile = ui->tilesetTilesView->model()->tileAt(index);
    if (tile == nullptr)
        return;
    QString tileName = BuildingTilesMgr::nameForTile(tile);
//    BuildingTile* buildingTile = BuildingTilesMgr::instance()->get(tileName);
    for (TileRotated *tile : mCurrentTilesetRotated->mTiles) {
        if (BuildingTilesMgr::instance()->nameForTile(tile->mTileset->nameUnrotated(), tile->mID) == tileName) {
            ui->tileRotateView->setCurrentIndex(ui->tileRotateView->model()->index(tile));
            return;
        }
    }
}

void TileRotationWindow::tilesetSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesetList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : nullptr;
    mCurrentTileset = nullptr;
    if (item) {
        int row = ui->tilesetList->row(item);
        mCurrentTileset = TileMetaInfoMgr::instance()->tileset(row);
        if (mCurrentTileset->isMissing()) {
            ui->tilesetTilesView->clear();
        } else {
            ui->tilesetTilesView->setTileset(mCurrentTileset);
            updateUsedTiles();
        }
    } else {
        ui->tilesetTilesView->clear();
    }
    syncUI();
}

void TileRotationWindow::rotatedFilterEdited(const QString &text)
{
    QListWidget* mTilesetNamesView = ui->tilesetRotatedList;

    for (int row = 0; row < mTilesetNamesView->count(); row++) {
        QListWidgetItem* item = mTilesetNamesView->item(row);
        item->setHidden(text.trimmed().isEmpty() ? false : !item->text().contains(text));
    }

    QListWidgetItem* current = mTilesetNamesView->currentItem();
    if (current != nullptr && current->isHidden()) {
        // Select previous visible row.
        int row = mTilesetNamesView->row(current) - 1;
        while (row >= 0 && mTilesetNamesView->item(row)->isHidden()) {
            row--;
        }
        if (row >= 0) {
            current = mTilesetNamesView->item(row);
            mTilesetNamesView->setCurrentItem(current);
            mTilesetNamesView->scrollToItem(current);
            return;
        }

        // Select next visible row.
        row = mTilesetNamesView->row(current) + 1;
        while (row < mTilesetNamesView->count() && mTilesetNamesView->item(row)->isHidden()) {
            row++;
        }
        if (row < mTilesetNamesView->count()) {
            current = mTilesetNamesView->item(row);
            mTilesetNamesView->setCurrentItem(current);
            mTilesetNamesView->scrollToItem(current);
            return;
        }

        // All items hidden
        mTilesetNamesView->setCurrentItem(nullptr);
    }

    current = mTilesetNamesView->currentItem();
    if (current != nullptr) {
        mTilesetNamesView->scrollToItem(current);
    }
}

void TileRotationWindow::tileRotatedActivated(const QModelIndex &index)
{
#if 0 // TODO
    TileRotateModel *m = ui->tileRotateView->model();
    if (TileRotated *tile = m->tileAt(index)) {
        for (BuildingTile *btile : ftile->tiles()) {
            if (btile != nullptr) {
                displayTileInTileset(btile);
                break;
            }
        }
    }
#endif
}

void TileRotationWindow::tilesetRotatedSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesetRotatedList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : nullptr;
    mCurrentTilesetRotated = nullptr;
    if (item) {
        int row = ui->tilesetRotatedList->row(item);
        mCurrentTilesetRotated = mTilesets[row];
#if 1
        ui->tileRotateView->setTileset(getRotatedTileset(mCurrentTilesetRotated->nameRotated()));
#else
        ui->tileRotateView->setTiles(mCurrentTilesetRotated->mTiles);
#endif
        updateUsedTiles();
    } else {
        ui->tileRotateView->clear();
    }
    syncUI();
}

void TileRotationWindow::typeComboActivated(int index)
{
}

void TileRotationWindow::displayTileInTileset(Tiled::Tile *tile)
{
    if (tile == nullptr)
        return;
    int row = TileMetaInfoMgr::instance()->indexOf(tile->tileset());
    if (row >= 0) {
        ui->tilesetList->setCurrentRow(row);
        ui->tilesetTilesView->setCurrentIndex(ui->tilesetTilesView->model()->index(tile));
    }
}

void TileRotationWindow::displayTileInTileset(BuildingTile *tile)
{
    displayTileInTileset(BuildingTilesMgr::instance()->tileFor(tile));
}

void TileRotationWindow::setTilesetRotatedList()
{
    ui->tilesetRotatedList->clear();
    ui->tilesetRotatedFilter->setEnabled(!mTilesets.isEmpty());
    // Add the list of tilesets, and resize it to fit
    int width = 64;
    QFontMetrics fm = ui->tilesetRotatedList->fontMetrics();
    for (TilesetRotated *tileset : mTilesets) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(tileset->nameRotated());
        ui->tilesetRotatedList->addItem(item);
        width = qMax(width, fm.width(tileset->nameRotated()));
    }
    int sbw = ui->tilesetRotatedList->verticalScrollBar()->sizeHint().width();
    ui->tilesetRotatedList->setFixedWidth(width + 16 + sbw);
    ui->tilesetRotatedFilter->setFixedWidth(ui->tilesetRotatedList->width());
}

void TileRotationWindow::setTilesetList()
{
    ui->tilesetList->clear();
    ui->tilesetFilter->setEnabled(!TileMetaInfoMgr::instance()->tilesets().isEmpty());
    // Add the list of tilesets, and resize it to fit
    int width = 64;
    QFontMetrics fm = ui->tilesetList->fontMetrics();
    for (Tileset *tileset : TileMetaInfoMgr::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(tileset->name());
        if (tileset->isMissing()) {
            item->setForeground(Qt::red);
        }
        ui->tilesetList->addItem(item);
        width = qMax(width, fm.width(tileset->name()));
    }
    int sbw = ui->tilesetList->verticalScrollBar()->sizeHint().width();
    ui->tilesetList->setFixedWidth(width + 16 + sbw);
    ui->tilesetFilter->setFixedWidth(ui->tilesetList->width());
}

void TileRotationWindow::updateUsedTiles()
{
    return; // FIXME: slow

    if (mCurrentTileset == nullptr)
        return;

    for (int i = 0; i < mCurrentTileset->tileCount(); i++) {
        QString tileName = BuildingEditor::BuildingTilesMgr::nameForTile(mCurrentTileset->name(), i);
        if (mHoverTileName.isEmpty() == false) {
            if (mHoverTileName == tileName) {
                ui->tilesetTilesView->model()->setCategoryBounds(i, QRect(0, 0, 1, 1));
            } else {
                ui->tilesetTilesView->model()->setCategoryBounds(i, QRect());
            }
            continue;
        }
        if (isTileUsed(tileName)) {
            ui->tilesetTilesView->model()->setCategoryBounds(i, QRect(0, 0, 1, 1));
        } else {
            ui->tilesetTilesView->model()->setCategoryBounds(i, QRect());
        }
    }
    ui->tilesetTilesView->model()->redisplay();
}

void TileRotationWindow::filterEdited(const QString &text)
{
    QListWidget* mTilesetNamesView = ui->tilesetList;

    for (int row = 0; row < mTilesetNamesView->count(); row++) {
        QListWidgetItem* item = mTilesetNamesView->item(row);
        item->setHidden(text.trimmed().isEmpty() ? false : !item->text().contains(text));
    }

    QListWidgetItem* current = mTilesetNamesView->currentItem();
    if (current != nullptr && current->isHidden()) {
        // Select previous visible row.
        int row = mTilesetNamesView->row(current) - 1;
        while (row >= 0 && mTilesetNamesView->item(row)->isHidden()) {
            row--;
        }
        if (row >= 0) {
            current = mTilesetNamesView->item(row);
            mTilesetNamesView->setCurrentItem(current);
            mTilesetNamesView->scrollToItem(current);
            return;
        }

        // Select next visible row.
        row = mTilesetNamesView->row(current) + 1;
        while (row < mTilesetNamesView->count() && mTilesetNamesView->item(row)->isHidden()) {
            row++;
        }
        if (row < mTilesetNamesView->count()) {
            current = mTilesetNamesView->item(row);
            mTilesetNamesView->setCurrentItem(current);
            mTilesetNamesView->scrollToItem(current);
            return;
        }

        // All items hidden
        mTilesetNamesView->setCurrentItem(nullptr);
    }

    current = mTilesetNamesView->currentItem();
    if (current != nullptr) {
        mTilesetNamesView->scrollToItem(current);
    }
}

bool TileRotationWindow::isTileUsed(const QString &_tileName)
{
    for (TilesetRotated *tileset : mTilesets) {
        for (TileRotated *tile : tileset->mTiles) {
            if (BuildingTilesMgr::instance()->nameForTile(tileset->nameUnrotated(), tile->mID) == _tileName) {
                return true;
            }
        }
    }
    return false;
}

void TileRotationWindow::manageTilesets()
{
    TileMetaInfoDialog dialog(this);
    dialog.exec();

    TileMetaInfoMgr *mgr = TileMetaInfoMgr::instance();
    if (!mgr->writeTxt()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"), mgr->errorString());
    }
}

void TileRotationWindow::tilesetAdded(Tileset *tileset)
{
    setTilesetList();
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    ui->tilesetList->setCurrentRow(row);
}

void TileRotationWindow::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    delete ui->tilesetList->takeItem(row);
}

void TileRotationWindow::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
}

// Called when a tileset image changes or a missing tileset was found.
void TileRotationWindow::tilesetChanged(Tileset *tileset)
{
    if (tileset == mCurrentTileset) {
        if (tileset->isMissing()) {
            ui->tilesetTilesView->clear();
        } else {
            ui->tilesetTilesView->setTileset(tileset);
        }
    }

    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    if (QListWidgetItem *item = ui->tilesetList->item(row)) {
        item->setForeground(tileset->isMissing() ? Qt::red : Qt::black);
    }
}

void TileRotationWindow::tileDropped(TileRotated *tile, int x, const QString &tileName)
{
    mUndoStack->push(new AddTile(this, tile, x, tileName));
}

void TileRotationWindow::addTile(TileRotated *tile, int index, const QString& tileName)
{
//    tile->mVisual.addTile(tileName);
//    updateUsedTiles();
}

QString TileRotationWindow::removeTile(TileRotated *tile, int index)
{
//    QString old = tile->mVisual.mTileNames.last();
//    tile->mVisual.mTileNames.removeAt(tile->mVisual.mTileNames.size() - 1);
    // FIXME: and mOffsets and mEdges
//    updateUsedTiles();
//    return old;
    return QString();
}

Tileset* TileRotationWindow::getRotatedTileset(const QString tilesetName)
{
    if (!tilesetName.contains(QLatin1Literal("_R")))
        return nullptr;
//    if (mTilesetByNameRotated.contains(tilesetName) == false) {
//        return nullptr;
//    }
    Tileset *tileset = mTilesetRotated[tilesetName];
    if (tileset == nullptr) {
        tileset = new Tileset(tilesetName, 64, 128);
        tileset->loadFromNothing(QSize(64 * 8, 128 * 16), tilesetName + QLatin1Literal(".png"));
        mTilesetRotated[tilesetName] = tileset;
    }
    return tileset;
}

TileRotated *TileRotationWindow::rotatedTileFor(Tile *tileR)
{
    if (!mTilesetByNameRotated.contains(tileR->tileset()->name())) {
        return nullptr;
    }
    TilesetRotated* tilesetR = mTilesetByNameRotated[tileR->tileset()->name()];
    if (tileR->id() >= tilesetR->mTileByID.size()) {
        return nullptr;
    }
    return tilesetR->mTileByID[tileR->id()];
}

