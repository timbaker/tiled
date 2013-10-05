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

#include "enflatulatordialog.h"
#include "ui_enflatulatordialog.h"

#include "preferences.h"

#include <QClipboard>
#include <QDebug>
#include <QFileDialog>
#include <QGraphicsSceneMouseEvent>

using namespace Tiled;
using namespace Internal;

EnflatulatorShapeItem::EnflatulatorShapeItem(QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mShape(0)
{

}

QRectF EnflatulatorShapeItem::boundingRect() const
{
    if (mShape) {
        QRectF r = mShape->mFaces.first()->mRect;
        foreach (EnflatulatorFace *face, mShape->mFaces)
            r |= face->mRect;
        return r;
    }
    return QRectF();
}

void EnflatulatorShapeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (mShape) {
        painter->setPen(QColor(0,0,0,64));
        painter->drawImage(0, 0, mShape->mImage);
        foreach (EnflatulatorFace *face, mShape->mFaces) {
//            painter->drawImage(face->mRect.topLeft(), face->mImage);
            painter->drawRect(face->mRect);
        }
    }

}

void EnflatulatorShapeItem::setShape(EnflatulatorShape *shape)
{
    prepareGeometryChange();
    mShape = shape;
    update();
}

/////

EnflatulatorFaceScene::EnflatulatorFaceScene(QObject *parent) :
    QGraphicsScene(parent),
    mShapeItem(new EnflatulatorShapeItem)
{
    addItem(mShapeItem);
}

void EnflatulatorFaceScene::setShape(EnflatulatorShape *shape)
{
    mShapeItem->setShape(shape);
}

/////

EnflatulatorFaceView::EnflatulatorFaceView(QWidget *parent) :
    QGraphicsView(parent),
    mScene(new EnflatulatorFaceScene(this))
{
    setScene(mScene);
    setTransform(QTransform::fromScale(2, 2));
}

/////

EnflatulatorFaceItem::EnflatulatorFaceItem(EnflatulatorIsoScene *scene, EnflatulatorFace *face, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene),
    mFace(face),
    mSelected(false)
{
    initPoly();
}

QRectF EnflatulatorFaceItem::boundingRect() const
{
    return mPoly.boundingRect().adjusted(-4, -4, 4, 4);
}

void EnflatulatorFaceItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (mSelected)
        painter->setPen(Qt::blue);
    painter->drawPolyline(mPoly);

    if (mSelected) {
        painter->setBrush(Qt::gray);
        qreal handleSize = 3;
        QPointF p = QLineF(mPoly[0], mPoly[1]).pointAt(0.5);
        painter->drawRect(QRectF(p.x() - handleSize/2, p.y() - handleSize/2, handleSize, handleSize));
        p = QLineF(mPoly[1], mPoly[2]).pointAt(0.5);
        painter->drawRect(QRectF(p.x() - handleSize/2, p.y() - handleSize/2, handleSize, handleSize));
        p = QLineF(mPoly[2], mPoly[3]).pointAt(0.5);
        painter->drawRect(QRectF(p.x() - handleSize/2, p.y() - handleSize/2, handleSize, handleSize));
        p = QLineF(mPoly[3], mPoly[4]).pointAt(0.5);
        painter->drawRect(QRectF(p.x() - handleSize/2, p.y() - handleSize/2, handleSize, handleSize));
    }
}

QPointF EnflatulatorFaceItem::toScene(qreal x, qreal y, qreal z)
{
    const int tileWidth = 64, tileHeight = 32;
    const int originX = 0; //tileWidth / 2;
    const int originY = 0;
    x /= 32, y /= 32/*, z /= tileHeight*/;
    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2 + originY + z);
}

QPolygonF EnflatulatorFaceItem::toScene(QRectF &r)
{
    QPolygonF polygon;
#if 0
    polygon << QPointF(toScene(r.x(), r.y()));
    polygon << QPointF(toScene(r.right(), r.y()));
    polygon << QPointF(toScene(r.right(), r.bottom()));
    polygon << QPointF(toScene(r.x(), r.bottom()));
#endif
    return polygon;
}

QPointF EnflatulatorFaceItem::toFace(qreal x, qreal y)
{
    const int tileWidth = 64;
    const int tileHeight = 32;
    const qreal ratio = (qreal) tileWidth / tileHeight;

    //x -= tileWidth / 2;
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QPointF(mx / tileHeight,
                   my / tileHeight);
}

void EnflatulatorFaceItem::setSelected(bool selected)
{
    mSelected = selected;
    update();
}

int EnflatulatorFaceItem::handleAt(const QPointF &scenePos)
{
    QPointF itemPos = mapFromScene(scenePos);
    for (int i = 0; i < 4; i++) {
        QPointF p = QLineF(mPoly[i], mPoly[i+1]).pointAt(0.5);
        qreal handleSize = 3;
        QRectF r(p.x() - handleSize/2, p.y() - handleSize/2, handleSize, handleSize);
        if (r.contains(itemPos)) {
            return i + 1;
        }
    }
    return 0;
}

void EnflatulatorFaceItem::moveHandle(int handle, const QPointF &scenePos)
{
    handle -= 1;
    QPointF p = QLineF(mPoly[handle], mPoly[handle+1]).pointAt(0.5);
    QPointF p1 = toFace(p);
    QPointF p2 = toFace(mapFromScene(scenePos));
    QPoint delta = ((p2 - p1) * 32).toPoint();
    QRect oldRect = mFace->mRect;
    switch (mFace->mOrient) {
    case mFace->Flat:
        switch (handle) {
        case 0:
            mFace->mRect.setTop(qMin(mFace->mRect.top() + delta.y(), mFace->mRect.bottom()));
            delta = QPoint(0, mFace->mRect.top() - oldRect.top());
            break;
        case 1:
            mFace->mRect.setRight(qMax(mFace->mRect.right() + delta.x(), mFace->mRect.left()));
            delta = QPoint();
            break;
        case 2:
            mFace->mRect.setBottom(qMax(mFace->mRect.bottom() + delta.y(), mFace->mRect.top()));
            delta = QPoint();
            break;
        case 3:
            mFace->mRect.setLeft(qMin(mFace->mRect.left() + delta.x(), mFace->mRect.right()));
            delta = QPoint(mFace->mRect.left() - oldRect.left(), 0);
            break;
        }
        break;
    case mFace->North:
        switch (handle) {
        case 0:
            mFace->mRect.setTop(qMin(mFace->mRect.top() + delta.y(), mFace->mRect.bottom()));
            delta = QPoint(mFace->mRect.top() - oldRect.top(), mFace->mRect.top() - oldRect.top());
            break;
        case 1:
            mFace->mRect.setRight(qMax(mFace->mRect.right() + delta.x(), mFace->mRect.left()));
            delta = QPoint();
            break;
        case 2:
            mFace->mRect.setBottom(qMax(mFace->mRect.bottom() + delta.y(), mFace->mRect.top()));
            delta = QPoint();
            break;
        case 3:
            mFace->mRect.setLeft(qMin(mFace->mRect.left() + delta.x(), mFace->mRect.right()));
            delta = QPoint(mFace->mRect.left() - oldRect.left(), 0);
            break;
        }
        break;
    case mFace->West:
        switch (handle) {
        case 0:
            mFace->mRect.setTop(qMin(mFace->mRect.top() + delta.y(), mFace->mRect.bottom()));
            delta = QPoint(mFace->mRect.top() - oldRect.top(), mFace->mRect.top() - oldRect.top());
            break;
        case 1:
            mFace->mRect.setRight(qMax(mFace->mRect.right() - delta.y(), mFace->mRect.left()));
            delta = QPoint(0, oldRect.right() - mFace->mRect.right());
            break;
        case 2:
            mFace->mRect.setBottom(qMax(mFace->mRect.bottom() + delta.y(), mFace->mRect.top()));
            delta = QPoint();
            break;
        case 3:
            mFace->mRect.setLeft(qMin(mFace->mRect.left() - delta.y(), mFace->mRect.right()));
            delta = QPoint();
            break;
        }
        break;
    }

    prepareGeometryChange();
    initPoly();
    setPos(pos() + (toScene(delta.x(), delta.y(), 0) - toScene(0, 0, 0)));
    update();
}

void EnflatulatorFaceItem::initPoly()
{
    mPoly.clear();
    switch (mFace->mOrient) {
    case mFace->Flat:
        mPoly << toScene(0, 0, 0)
              << toScene(mFace->mRect.width(), 0, 0)
              << toScene(mFace->mRect.width(), mFace->mRect.height(), 0)
              << toScene(0, mFace->mRect.height(), 0);
        break;
    case mFace->West:
        mPoly << toScene(0, mFace->mRect.width(), 0)
              << toScene(0, 0, 0)
              << toScene(0, 0, mFace->mRect.height())
              << toScene(0, mFace->mRect.width(), mFace->mRect.height());
        break;
    case mFace->North:
        mPoly << toScene(0, 0, 0)
              << toScene(mFace->mRect.width(), 0, 0)
              << toScene(mFace->mRect.width(), 0, mFace->mRect.height())
              << toScene(0, 0, mFace->mRect.height());
        break;
    case mFace->Roof:
        mPoly << toScene(0, 0, 0)
              << toScene(mFace->mRect.width(), 0, 0)
              << toScene(mFace->mRect.width() + 32, mFace->mRect.height(), 0)
              << toScene(32, mFace->mRect.height(), 0);
        break;
    }
    mPoly += mPoly.first();
}

void EnflatulatorFaceItem::enflatulate()
{
    mFace->mImage = QImage(mFace->mRect.size(), QImage::Format_ARGB32);
    mFace->mImage.fill(Qt::transparent);
    QImage isoImg = mScene->mImage;
    switch (mFace->mOrient) {
    case mFace->Flat:
        for (int y = 0; y < mFace->mRect.height(); y++) {
            for (int x = 0; x < mFace->mRect.width(); x++) {
                QPointF p = pos() + toScene(x, y, 0);
                if (isoImg.rect().contains(p.toPoint()))
                    mFace->mImage.setPixel(x, y, isoImg.pixel(p.toPoint()));
            }
        }
        break;
    case mFace->West:
        for (int x = 0; x < mFace->mRect.width(); x += 1) {
            for (int y = 0; y < mFace->mRect.height(); y++) {
                QPointF p = pos() + toScene(0, mFace->mRect.width() - x, y);
                if (isoImg.rect().contains(p.toPoint()))
                    mFace->mImage.setPixel(x, y, isoImg.pixel(p.toPoint()));
            }
        }
        break;
    case mFace->North:
        for (int x = 0; x < mFace->mRect.width(); x += 1) {
            for (int y = 0; y < mFace->mRect.height(); y++) {
                QPointF p = pos() + toScene(x, 0, y);
                if (isoImg.rect().contains(p.toPoint()))
                    mFace->mImage.setPixel(x, y, isoImg.pixel(p.toPoint()));
            }
        }
        break;
    case mFace->Roof:
        for (int y = 0; y < mFace->mRect.height(); y++) {
            for (int x = 0; x < mFace->mRect.width(); x++) {
                QPointF p = pos() + toScene(x + (y/64.0)*32, y, 0);
                if (isoImg.rect().contains(p.toPoint()))
                    mFace->mImage.setPixel(x, y, isoImg.pixel(p.toPoint()));
            }
        }
        break;
    }
}

/////

EnflatulatorIsoScene::EnflatulatorIsoScene(QObject *parent) :
    QGraphicsScene(parent),
    mShape(0),
    mImageItem(new QGraphicsPixmapItem),
    mSelectedFaceItem(0),
    mMode(NoMode)
{
    addItem(mImageItem);

    setIsoImage(QImage(Preferences::instance()->tilesDirectory() + QLatin1String("/appliances_refrigeration_01.png")));
}

void EnflatulatorIsoScene::setIsoImage(const QImage &image)
{
    mImage = image;
    mImageItem->setPixmap(QPixmap::fromImage(image));
}

void EnflatulatorIsoScene::setShape(EnflatulatorShape *shape)
{
    qDeleteAll(mFaceItems);
    mFaceItems.clear();
    mSelectedFaceItem = 0;

    qreal x = 16, y = 16;
    foreach (EnflatulatorFace *face, shape->mFaces) {
#if 0
        mFaceItems += new EnflatulatorFaceItem(this, face);
#else
        EnflatulatorFace *editFace = new EnflatulatorFace;
        *editFace = *face;
        mFaceItems += new EnflatulatorFaceItem(this, editFace);
#endif
        mFaceItems.last()->setPos(x, y);
        x += 48;
        addItem(mFaceItems.last());
    }
}

EnflatulatorFaceItem *EnflatulatorIsoScene::faceAt(const QPointF &scenePos)
{
    foreach (EnflatulatorFaceItem *item, mFaceItems) {
        if (item->mPoly.translated(item->pos()).containsPoint(scenePos, Qt::WindingFill))
            return item;
    }
    return 0;
}

void EnflatulatorIsoScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        foreach (EnflatulatorFaceItem *item, mFaceItems) {
            if (item->mSelected) {
                if (int handle = item->handleAt(event->scenePos())) {
                    mHandle = handle;
                    mLastScenePos = event->scenePos();
                    mMode = DragHandle;
                    return;
                }
            }
        }
        if (mSelectedFaceItem)
            mSelectedFaceItem->setSelected(false);
        if (mSelectedFaceItem = faceAt(event->scenePos())) {
            mSelectedFaceItem->setSelected(true);
            mLastScenePos = mSelectedFaceItem->pos();
            mMode = DragFace;
        }
    }
}

void EnflatulatorIsoScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMode == DragHandle) {
        mSelectedFaceItem->moveHandle(mHandle, event->scenePos());
        mLastScenePos = event->scenePos();
        mSelectedFaceItem->enflatulate();
        emit faceChanged();
    }
    if (mMode == DragFace) {
        mSelectedFaceItem->setPos(mLastScenePos + (event->scenePos() - event->buttonDownScenePos(Qt::LeftButton)).toPoint());
        mSelectedFaceItem->enflatulate();
        emit faceChanged();
    }
}

void EnflatulatorIsoScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    mMode = NoMode;
}

/////

EnflatulatorIsoView::EnflatulatorIsoView(QWidget *parent) :
    QGraphicsView(parent),
    mScene(new EnflatulatorIsoScene(this))
{
    setScene(mScene);
    setBackgroundBrush(Qt::lightGray);
    setTransform(QTransform::fromScale(4, 4));
}

/////

EnflatulatorDialog::EnflatulatorDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EnflatulatorDialog)
{
    ui->setupUi(this);

    connect(ui->chooseImage, SIGNAL(clicked()), SLOT(chooseIsoImage()));
    connect(ui->isoView->scene(), SIGNAL(faceChanged()), SLOT(faceChanged()));

    if (EnflatulatorShape *shape = new EnflatulatorShape) {
        shape->mLabel = QLatin1String("Counter");
        if (EnflatulatorFace *face = new EnflatulatorFace) {
            face->mLabel = tr("Top");
            face->mOrient = face->Flat;
            face->mRect = QRect(0, 0, 32, 32);
            shape->mFaces += face;
        }
        if (EnflatulatorFace *face = new EnflatulatorFace) {
            face->mLabel = tr("Back");
            face->mOrient = face->North;
            face->mRect = QRect(0, 32, 32, 32);
            shape->mFaces += face;
        }
        if (EnflatulatorFace *face = new EnflatulatorFace) {
            face->mLabel = tr("Front");
            face->mOrient = face->North;
            face->mRect = QRect(0, 32 + 32, 32, 32);
            shape->mFaces += face;
        }
        mShapes += shape;
    }

    if (EnflatulatorShape *shape = new EnflatulatorShape) {
        shape->mLabel = QLatin1String("Fridge");
        if (EnflatulatorFace *face = new EnflatulatorFace) {
            face->mLabel = tr("Back");
            face->mOrient = face->North;
            face->mRect = QRect(0, 32, 32, 96);
            shape->mFaces += face;
        }
        if (EnflatulatorFace *face = new EnflatulatorFace) {
            face->mLabel = tr("Left");
            face->mOrient = face->North;
            face->mRect = QRect(32, 32, 32, 96);
            shape->mFaces += face;
        }
        if (EnflatulatorFace *face = new EnflatulatorFace) {
            face->mLabel = tr("Front");
            face->mOrient = face->North;
            face->mRect = QRect(32 + 32, 32, 32, 96);
            shape->mFaces += face;
        }
        if (EnflatulatorFace *face = new EnflatulatorFace) {
            face->mLabel = tr("Right");
            face->mOrient = face->West;
            face->mRect = QRect(32 + 32 + 32, 32, 32, 96);
            shape->mFaces += face;
        }
        if (EnflatulatorFace *face = new EnflatulatorFace) {
            face->mLabel = tr("Top");
            face->mOrient = face->Flat;
            face->mRect = QRect(32 + 32, 0, 32, 32);
            shape->mFaces += face;
        }
        mShapes += shape;
    }

    if (EnflatulatorShape *shape = new EnflatulatorShape) {
        shape->mLabel = QLatin1String("Roof");
        if (EnflatulatorFace *face = new EnflatulatorFace) {
            face->mLabel = tr("Face");
            face->mOrient = face->Roof;
            face->mRect = QRect(0, 0, 32, 64);
            shape->mFaces += face;
        }
        mShapes += shape;
    }

    foreach (EnflatulatorShape *shape, mShapes) {
        shape->mImage = QImage(shape->bounds().size(), QImage::Format_ARGB32);
        shape->mImage.fill(Qt::transparent);
        ui->shapeList->addItem(shape->mLabel);
    }

    connect(ui->shapeList, SIGNAL(currentRowChanged(int)), SLOT(currentShapeChanged(int)));
}

EnflatulatorDialog::~EnflatulatorDialog()
{
    delete ui;
}

void EnflatulatorDialog::chooseIsoImage()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose Iso Image"), Preferences::instance()->tilesDirectory(), QLatin1String("PNG images (*.png)"));
    if (f.isEmpty())
        return;
    ui->isoView->mScene->setIsoImage(QImage(f));
}

void EnflatulatorDialog::currentShapeChanged(int row)
{
    EnflatulatorShape *shape = (row >= 0) ? mShapes.at(row) : 0;
    ui->faceView->mScene->mShapeItem->setShape(shape);
    ui->isoView->mScene->setShape(shape);
}

void EnflatulatorDialog::faceChanged()
{
    QImage &shapeImg = ui->faceView->mScene->mShapeItem->mShape->mImage;
    shapeImg.fill(Qt::transparent);
    QPainter p(&shapeImg);
    for (int i = 0; i < ui->isoView->mScene->mFaceItems.size(); i++) {
        EnflatulatorFace *face = ui->isoView->mScene->mFaceItems[i]->mFace;
        p.drawImage(ui->faceView->mScene->mShapeItem->mShape->mFaces[i]->mRect.topLeft(), face->mImage);
    }
    p.end();
    qApp->clipboard()->setImage(shapeImg);

    ui->faceView->mScene->mShapeItem->update();
}

