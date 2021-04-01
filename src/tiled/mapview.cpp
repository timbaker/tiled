/*
 * mapview.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
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

#include "mapview.h"

#include "mapscene.h"
#include "preferences.h"
#include "zoomable.h"
#ifdef ZOMBOID
#include "mainwindow.h"
#include "maprenderer.h"
#include "tilelayerspanel.h"
#endif

#include <QApplication>
#include <QCursor>
#include <QWheelEvent>
#include <QScrollBar>

#ifndef QT_NO_OPENGL
#include <QGLWidget>
#endif

using namespace Tiled::Internal;

MapView::MapView(QWidget *parent)
    : QGraphicsView(parent)
    , mHandScrolling(false)
    , mZoomable(new Zoomable(this))
#ifdef ZOMBOID
    , mMiniMap(0)
#endif
{
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
#ifdef Q_OS_MAC
    setFrameStyle(QFrame::NoFrame);
#endif

#ifndef QT_NO_OPENGL
    Preferences *prefs = Preferences::instance();
    setUseOpenGL(prefs->useOpenGL());
    connect(prefs, SIGNAL(useOpenGLChanged(bool)), SLOT(setUseOpenGL(bool)));
#endif

    QWidget *v = viewport();

    /* Since Qt 4.5, setting this attribute yields significant repaint
     * reduction when the view is being resized. */
    v->setAttribute(Qt::WA_StaticContents);

    /* Since Qt 4.6, mouse tracking is disabled when no graphics item uses
     * hover events. We need to set it since our scene wants the events. */
    v->setMouseTracking(true);

    // Adjustment for antialiasing is done by the items that need it
    setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing);

    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(adjustScale(qreal)));
}

MapView::~MapView()
{
    setHandScrolling(false); // Just in case we didn't get a hide event
}

#ifdef ZOMBOID
#include "ZomboidScene.h"
void MapView::setMapScene(MapScene *scene)
{
    QGraphicsView::setScene(scene);

    mMiniMap = new MiniMap(this);
    mMiniMap->setMapScene(scene);

    mMiniMapItem = new MiniMapItem(static_cast<ZomboidScene*>(scene));
    mMiniMap->setExtraItem(mMiniMapItem);
}
#endif

MapScene *MapView::mapScene() const
{
    return static_cast<MapScene*>(scene());
}

void MapView::adjustScale(qreal scale)
{
    setTransform(QTransform::fromScale(scale, scale));
    setRenderHint(QPainter::SmoothPixmapTransform,
                  mZoomable->smoothTransform());
}

#ifdef ZOMBOID
void MapView::setUseOpenGL(bool useOpenGL)
{
#ifndef QT_NO_OPENGL
    QWidget *oldViewport = viewport();
    QWidget *newViewport = viewport();
    if (useOpenGL && QGLFormat::hasOpenGL()) {
        if (!qobject_cast<QGLWidget*>(viewport())) {
            QGLFormat format = QGLFormat::defaultFormat();
            format.setDepth(false); // No need for a depth buffer
            format.setSampleBuffers(true); // Enable anti-aliasing
            newViewport = new QGLWidget(format);
        }
    } else {
        if (qobject_cast<QGLWidget*>(viewport()))
            newViewport = 0;
    }

    // Changing the viewport destroys its child widgets
    if (newViewport != oldViewport) {
        if (mMiniMap) {
            mMiniMap->setVisible(false);
            mMiniMap->setParent(static_cast<QWidget*>(parent()));
        }
        setViewport(newViewport);
        if (mMiniMap) {
            mMiniMap->setParent(this);
            mMiniMap->setVisible(Preferences::instance()->showMiniMap());
//            mMiniMap->sceneRectChanged(scene()->sceneRect());
        }
    }

    QWidget *v = viewport();
    v->setAttribute(Qt::WA_StaticContents);
    v->setMouseTracking(true);
#endif
}
#else
void MapView::setUseOpenGL(bool useOpenGL)
{
#ifndef QT_NO_OPENGL
    if (useOpenGL && QGLFormat::hasOpenGL()) {
        if (!qobject_cast<QGLWidget*>(viewport())) {
            QGLFormat format = QGLFormat::defaultFormat();
            format.setDepth(false); // No need for a depth buffer
            format.setSampleBuffers(true); // Enable anti-aliasing
            setViewport(new QGLWidget(format));
        }
    } else {
        if (qobject_cast<QGLWidget*>(viewport()))
            setViewport(0);
    }

    QWidget *v = viewport();
    v->setAttribute(Qt::WA_StaticContents);
    v->setMouseTracking(true);
#endif
}
#endif

void MapView::setHandScrolling(bool handScrolling)
{
    if (mHandScrolling == handScrolling)
        return;

    mHandScrolling = handScrolling;
    setInteractive(!mHandScrolling);

    if (mHandScrolling) {
        mLastMousePos = QCursor::pos();
        QApplication::setOverrideCursor(QCursor(Qt::ClosedHandCursor));
        viewport()->grabMouse();
    } else {
        viewport()->releaseMouse();
        QApplication::restoreOverrideCursor();
    }
}

bool MapView::event(QEvent *e)
{
    // Ignore space bar events since they're handled by the MainWindow
    if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) {
        if (static_cast<QKeyEvent*>(e)->key() == Qt::Key_Space) {
            e->ignore();
            return false;
        }
    }

    return QGraphicsView::event(e);
}

void MapView::hideEvent(QHideEvent *event)
{
    // Disable hand scrolling when the view gets hidden in any way
    setHandScrolling(false);
    QGraphicsView::hideEvent(event);
}

/**
 * Override to support zooming in and out using the mouse wheel.
 */
void MapView::wheelEvent(QWheelEvent *event)
{
    QPoint numDegrees = event->angleDelta() / 8;
    if ((event->modifiers() & Qt::ControlModifier) && (numDegrees.y() != 0))
    {
        QPoint numSteps = numDegrees / 15;

        // No automatic anchoring since we'll do it manually
        setTransformationAnchor(QGraphicsView::NoAnchor);

        mZoomable->handleWheelDelta(numSteps.y() * 120);

        // Place the last known mouse scene pos below the mouse again
        QWidget *view = viewport();
        QPointF viewCenterScenePos = mapToScene(view->rect().center());
        QPointF mouseScenePos = mapToScene(view->mapFromGlobal(mLastMousePos));
        QPointF diff = viewCenterScenePos - mouseScenePos;
        centerOn(mLastMouseScenePos + diff);

        // Restore the centering anchor
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        return;
    }

    QGraphicsView::wheelEvent(event);
}

/**
 * Activates hand scrolling when the middle mouse button is pressed.
 */
void MapView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        setHandScrolling(true);
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

/**
 * Deactivates hand scrolling when the middle mouse button is released.
 */
void MapView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        setHandScrolling(false);
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

/**
 * Moves the view with the mouse while hand scrolling.
 */
void MapView::mouseMoveEvent(QMouseEvent *event)
{
    if (mHandScrolling) {
        QScrollBar *hBar = horizontalScrollBar();
        QScrollBar *vBar = verticalScrollBar();
        const QPoint d = event->globalPos() - mLastMousePos;
        hBar->setValue(hBar->value() + (isRightToLeft() ? d.x() : -d.x()));
        vBar->setValue(vBar->value() - d.y());

        mLastMousePos = event->globalPos();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
    mLastMousePos = event->globalPos();
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMousePos));

#ifdef ZOMBOID
    if (!(event->modifiers() & Qt::AltModifier)) {
        MapDocument *doc = mapScene()->mapDocument();
        QPoint tilePos = doc->renderer()->pixelToTileCoordsInt(mLastMouseScenePos,
                                                               doc->currentLevelIndex());
        MainWindow::instance()->tileLayersPanel()->setTilePosition(tilePos);
    }
#endif
}

#ifdef ZOMBOID

void MapView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (mMiniMap)
        mMiniMap->viewRectChanged();
}

void MapView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    if (mMiniMap)
        mMiniMap->viewRectChanged();
}

#endif // ZOMBOID
