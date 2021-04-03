/*
 * Copyright 2014, Tim Baker <treectrl@users.sf.net>
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

#include "packviewer.h"
#include "ui_packviewer.h"

#include "packextractdialog.h"
#include "zoomable.h"
#include "zprogress.h"

#include <QColorDialog>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneHoverEvent>
#include <QSettings>
#include <QToolTip>

using namespace Tiled::Internal;

static const QLatin1String KEY_BG("PackViewer/BackgroundColor");

PackViewer::PackViewer(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::PackViewer)
{
    ui->setupUi(this);

    ui->graphicsView->setScene(new QGraphicsScene(ui->graphicsView));
    ui->graphicsView->setBackgroundBrush(Qt::lightGray);
    ui->graphicsView->setAlignment(Qt::AlignCenter);
    ui->graphicsView->setMouseTracking(true);

    mRectItem = ui->graphicsView->scene()->addRect(QRectF(0, 0, 100, 100), QPen(Qt::gray));
    mRectItem->hide();
//    rect->setGraphicsEffect(new QGraphicsDropShadowEffect);

    mTileRectItem = ui->graphicsView->scene()->addRect(QRectF(0, 0, 100, 100), QPen(Qt::gray));
    mTileRectItem->hide();

    mPixmapItem = new PackImageItem(mTileRectItem);
    ui->graphicsView->scene()->addItem(mPixmapItem);
    mPixmapItem->hide();

    mTileRectItem->setZValue(mPixmapItem->zValue() + 1);

    mZoomable = new Zoomable(this);
    mZoomable->setZoomFactors(QVector<qreal>() << 0.25 << 0.5 << 1.0 << 2.0 << 4.0);
    mZoomable->connectToComboBox(ui->scaleCombo);
    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));

    connect(ui->actionOpen, SIGNAL(triggered()), SLOT(openPack()));
    connect(ui->listWidget, SIGNAL(itemSelectionChanged()), SLOT(itemSelectionChanged()));
    connect(ui->actionBackgroundColor, SIGNAL(triggered()), SLOT(chooseBackgroundColor()));
    connect(ui->actionExtractImages, SIGNAL(triggered()), SLOT(extractImages()));
    connect(ui->actionClose, SIGNAL(triggered()), SLOT(close()));

    QSettings settings;
    QVariant v = settings.value(KEY_BG, QColor(Qt::lightGray));
    if (v.canConvert<QColor>())
        setBackgroundColor(v.value<QColor>());

    readSettings();
}

PackViewer::~PackViewer()
{
    delete ui;
}

void PackViewer::openPack()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Choose .pack file"),
                                                    mPackDirectory,
                                                    QLatin1String("PZ pack files (*.pack)"));
    if (fileName.isEmpty())
        return;

    mPackDirectory = QFileInfo(fileName).absolutePath();
    writeSettings();

    QScopedPointer<PROGRESS> progress(new PROGRESS(tr("Loading %1").arg(QFileInfo(fileName).completeBaseName()), this));

    if (!mPackFile.read(fileName))
        return;

    ui->listWidget->clear();
    int numImages = 0;
    foreach (PackPage page, mPackFile.pages()) {
        ui->listWidget->addItem(page.name);
        numImages += page.subTextures().size();
    }
    if (numImages > 0)
        ui->listWidget->setCurrentRow(0);

    ui->label->setText(QString::fromLatin1("%1 images").arg(numImages));

    delete progress.take();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    raise();
    activateWindow();
}

void PackViewer::itemSelectionChanged()
{
    QList<QListWidgetItem*> items = ui->listWidget->selectedItems();
    if (items.size() == 1) {
        int row = ui->listWidget->row(items.first());
        QPixmap pixmap = QPixmap::fromImage(mPackFile.pages().at(row).image);
        mRectItem->setRect(QRectF(QPoint(-1, -1), pixmap.size() + QSize(1, 1)));
        mRectItem->show();
        mPixmapItem->setPackPage(mPackFile.pages().at(row));
        mPixmapItem->setPixmap(pixmap);
        mPixmapItem->show();
        ui->graphicsView->scene()->setSceneRect(QRectF(QPoint(), pixmap.size()).adjusted(-32, -32, 32, 32));
    }
}

void PackViewer::scaleChanged(qreal scale)
{
    ui->graphicsView->setTransform(QTransform::fromScale(scale, scale));
}

void PackViewer::chooseBackgroundColor()
{
    QColor color = QColorDialog::getColor(ui->graphicsView->backgroundBrush().color(),
                                          this, tr("Choose background color"));
    if (color.isValid())
        setBackgroundColor(color);
}

void PackViewer::setBackgroundColor(const QColor &color)
{
    ui->graphicsView->setBackgroundBrush(color);
    QSettings settings;
    settings.setValue(KEY_BG, color);
}

void PackViewer::extractImages()
{
    PackExtractDialog d(mPackFile, this);
    d.exec();
}

void PackViewer::readSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("PackViewer"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    mPackDirectory = settings.value(QLatin1String("directory")).toString();
    settings.endGroup();
}

void PackViewer::writeSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("PackViewer"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.setValue(QLatin1String("directory"), mPackDirectory);
    settings.endGroup();
}

/////

PackImageItem::PackImageItem(QGraphicsRectItem *rectItem) :
    QGraphicsPixmapItem(),
    mTileRectItem(rectItem)
{
    setAcceptHoverEvents(true);
}

void PackImageItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    qreal x = event->scenePos().x();
    qreal y = event->scenePos().y();

    for (const PackSubTexInfo &info : qAsConst(mPackPage.mInfo)) {
        if (x >= info.x && x < info.x + info.w &&
                y >= info.y && y < info.y + info.h) {
//            setToolTip(info.name);
            if (QGraphicsView *v = qobject_cast<QGraphicsView*>(event->widget()->parent())) {
                QRect sceneRect(int(scenePos().x()) + info.x, int(scenePos().y()) + info.y, info.w, info.h);
                QRect viewportRect = v->mapFromScene(sceneRect).boundingRect();
                QToolTip::showText(event->screenPos(), info.name, v, viewportRect);

                mTileRectItem->setRect(sceneRect);
                mTileRectItem->show();
                break;
            }
        }
    }
}
