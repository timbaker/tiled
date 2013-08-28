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

#include "curbtooldialog.h"
#include "ui_curbtooldialog.h"

#include "curbtool.h"
#include "mainwindow.h"

#include <QSettings>

using namespace Tiled::Internal;

CurbToolDialog *CurbToolDialog::mInstance = 0;

CurbToolDialog *CurbToolDialog::instance()
{
    if (!mInstance)
        mInstance = new CurbToolDialog(MainWindow::instance());
    return mInstance;
}

CurbToolDialog::CurbToolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CurbToolDialog),
    mVisibleLater(true)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::Tool);

    readSettings();
    ui->layer->setText(CurbTool::instance().defaultLayer());
    ui->suppressBlend->setChecked(CurbTool::instance().suppressBlendTiles());

    connect(ui->curbList, SIGNAL(currentRowChanged(int)), SLOT(currentRowChanged(int)));
    connect(ui->layer, SIGNAL(textChanged(QString)), SLOT(layerChanged(QString)));
    connect(ui->suppressBlend, SIGNAL(toggled(bool)), SLOT(suppressChanged(bool)));

    mVisibleLaterTimer.setSingleShot(true);
    mVisibleLaterTimer.setInterval(200);
    connect(&mVisibleLaterTimer, SIGNAL(timeout()), SLOT(setVisibleNow()));

    CurbFile curbFile;
    if (curbFile.read(qApp->applicationDirPath() + QLatin1String("/Curbs.txt"))) {
        mCurbs = curbFile.takeCurbs();
    }
    foreach (Curb *curb, mCurbs)
        ui->curbList->addItem(curb->mLabel);

    ui->curbList->setCurrentRow(mCurbs.size() ? 0 : -1);
}

CurbToolDialog::~CurbToolDialog()
{
    delete ui;
}

void CurbToolDialog::setVisible(bool visible)
{
//    if (visible)
//        readSettings();

    QDialog::setVisible(visible);

    if (!visible)
        writeSettings();
}

void CurbToolDialog::setVisibleLater(bool visible)
{
    mVisibleLater = visible;
    mVisibleLaterTimer.start();
}

void CurbToolDialog::readSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("CurbToolDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    QString layer = settings.value(QLatin1String("layer"), CurbTool::instance().defaultLayer()).toString();
    bool suppress = settings.value(QLatin1String("suppress"), CurbTool::instance().suppressBlendTiles()).toBool();
    layerChanged(layer);
    suppressChanged(suppress);
    settings.endGroup();
}

void CurbToolDialog::writeSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("CurbToolDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.setValue(QLatin1String("layer"), CurbTool::instance().defaultLayer());
    settings.value(QLatin1String("suppress"), CurbTool::instance().suppressBlendTiles());
    settings.endGroup();
}

void CurbToolDialog::currentRowChanged(int row)
{
    CurbTool::instance().setCurb((row >= 0) ? mCurbs.at(row) : 0);
}

void CurbToolDialog::layerChanged(const QString &layer)
{
    CurbTool::instance().setDefaultLayer(layer);
}

void CurbToolDialog::suppressChanged(bool suppress)
{
    CurbTool::instance().setSuppressBlendTiles(suppress);
}

void CurbToolDialog::setVisibleNow()
{
    if (mVisibleLater != isVisible())
        setVisible(mVisibleLater);
}
