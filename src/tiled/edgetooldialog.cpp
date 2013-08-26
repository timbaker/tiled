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

#include "edgetooldialog.h"
#include "ui_edgetooldialog.h"

#include "edgetool.h"
#include "mainwindow.h"

#include <QSettings>

using namespace Tiled::Internal;

EdgeToolDialog *EdgeToolDialog::mInstance = 0;

EdgeToolDialog *EdgeToolDialog::instance()
{
    if (!mInstance)
        mInstance = new EdgeToolDialog(MainWindow::instance());
    return mInstance;
}

EdgeToolDialog::EdgeToolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EdgeToolDialog),
    mVisibleLater(true)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::Tool);

    connect(ui->edgeList, SIGNAL(currentRowChanged(int)), SLOT(currentRowChanged(int)));
    connect(ui->dashLen, SIGNAL(valueChanged(int)), SLOT(dashChanged()));
    connect(ui->dashGap, SIGNAL(valueChanged(int)), SLOT(dashChanged()));

    mVisibleLaterTimer.setSingleShot(true);
    mVisibleLaterTimer.setInterval(200);
    connect(&mVisibleLaterTimer, SIGNAL(timeout()), SLOT(setVisibleNow()));

    EdgeFile edgeFile;
    if (edgeFile.read(qApp->applicationDirPath() + QLatin1String("/Edges.txt"))) {
        mEdges = edgeFile.takeEdges();
    }
    foreach (Edges *edges, mEdges)
        ui->edgeList->addItem(edges->mLabel);

    ui->edgeList->setCurrentRow(mEdges.size() ? 0 : -1);
}

EdgeToolDialog::~EdgeToolDialog()
{
    delete ui;
}

void EdgeToolDialog::setVisible(bool visible)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("EdgeToolDialog"));
    if (visible) {
        QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
        if (!geom.isEmpty())
            restoreGeometry(geom);
    }

    QDialog::setVisible(visible);

    if (!visible) {
        settings.setValue(QLatin1String("geometry"), saveGeometry());
    }
    settings.endGroup();
}

void EdgeToolDialog::setVisibleLater(bool visible)
{
    mVisibleLater = visible;
    mVisibleLaterTimer.start();
}

void EdgeToolDialog::currentRowChanged(int row)
{
    EdgeTool::instance().setEdges((row >= 0) ? mEdges.at(row) : 0);
}

void EdgeToolDialog::dashChanged()
{
    EdgeTool::instance().setDash(ui->dashLen->value(), ui->dashGap->value());
}

void EdgeToolDialog::setVisibleNow()
{
    if (mVisibleLater != isVisible())
        setVisible(mVisibleLater);
}
