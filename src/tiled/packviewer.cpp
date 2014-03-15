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

#include "zoomable.h"
#include "zprogress.h"

#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsPixmapItem>
#include <QSettings>

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

    mRectItem = ui->graphicsView->scene()->addRect(QRectF(0, 0, 100, 100), QPen(Qt::gray));
    mRectItem->hide();
//    rect->setGraphicsEffect(new QGraphicsDropShadowEffect);

    mPixmapItem = ui->graphicsView->scene()->addPixmap(QPixmap());
    mPixmapItem->hide();

    mZoomable = new Zoomable(this);
    mZoomable->setZoomFactors(QVector<qreal>() << 0.25 << 0.5 << 1.0 << 2.0 << 4.0);
    mZoomable->connectToComboBox(ui->scaleCombo);
    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));

    connect(ui->actionOpen, SIGNAL(triggered()), SLOT(openPack()));
    connect(ui->listWidget, SIGNAL(itemSelectionChanged()), SLOT(itemSelectionChanged()));
    connect(ui->actionBackgroundColor, SIGNAL(triggered()), SLOT(chooseBackgroundColor()));
    connect(ui->actionClose, SIGNAL(triggered()), SLOT(close()));

    QSettings settings;
    QVariant v = settings.value(KEY_BG, QColor(Qt::lightGray));
    if (v.canConvert<QColor>())
        setBackgroundColor(v.value<QColor>());
}

PackViewer::~PackViewer()
{
    delete ui;
}

void PackViewer::openPack()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Choose .pack file"),
                                                    QString(),
                                                    QLatin1String("PZ pack files (*.pack)"));
    if (fileName.isEmpty())
        return;

    PROGRESS progress(tr("Loading %1").arg(QFileInfo(fileName).completeBaseName()), this);

    if (!mPackFile.read(fileName))
        return;

    ui->listWidget->clear();
    foreach (PackPage page, mPackFile.pages()) {
        ui->listWidget->addItem(page.name);
    }
}

void PackViewer::itemSelectionChanged()
{
    QList<QListWidgetItem*> items = ui->listWidget->selectedItems();
    if (items.size() == 1) {
        int row = ui->listWidget->row(items.first());
        QPixmap pixmap = QPixmap::fromImage(mPackFile.pages().at(row).image);
        mRectItem->setRect(QRectF(QPoint(-1, -1), pixmap.size() + QSize(1, 1)));
        mRectItem->show();
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
