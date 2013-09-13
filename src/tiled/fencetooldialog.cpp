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

#include "fencetooldialog.h"
#include "ui_fencetooldialog.h"

#include "documentmanager.h"
#include "fencetool.h"
#include "mainwindow.h"
#include "preferences.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>

using namespace Tiled::Internal;

FenceToolDialog *FenceToolDialog::mInstance = 0;

FenceToolDialog *FenceToolDialog::instance()
{
    if (!mInstance)
        mInstance = new FenceToolDialog(MainWindow::instance());
    return mInstance;
}

FenceToolDialog::FenceToolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FenceToolDialog),
    mVisibleLater(true)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::Tool);

    readSettings();

    connect(ui->fenceList, SIGNAL(currentRowChanged(int)), SLOT(currentRowChanged(int)));

    mVisibleLaterTimer.setSingleShot(true);
    mVisibleLaterTimer.setInterval(200);
    connect(&mVisibleLaterTimer, SIGNAL(timeout()), SLOT(setVisibleNow()));

//    readTxt();
}

FenceToolDialog::~FenceToolDialog()
{
    delete ui;
}

void FenceToolDialog::setVisible(bool visible)
{
    if (visible) {
//        readSettings();

        bool modified = false;
        QString fileName = Preferences::instance()->configPath(txtName());
        if (QFileInfo(fileName).exists()) {
            if (mTxtModifiedTime1 != QFileInfo(fileName).lastModified())
                modified = true;
        }
        fileName = Preferences::instance()->appConfigPath(txtName());
        if (QFileInfo(fileName).exists()) {
            if (mTxtModifiedTime2 != QFileInfo(fileName).lastModified())
                modified = true;
        }
        if (modified)
            readTxt();

        currentRowChanged(ui->fenceList->currentRow());
    }

    QDialog::setVisible(visible);

    if (!visible)
        writeSettings();
}

void FenceToolDialog::setVisibleLater(bool visible)
{
    mVisibleLater = visible;
    mVisibleLaterTimer.start();
}

void FenceToolDialog::readSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("FenceToolDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    settings.endGroup();
}

void FenceToolDialog::writeSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("FenceToolDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.endGroup();
}

void FenceToolDialog::currentRowChanged(int row)
{
    Fence *fence = (row >= 0) ? mFences.at(row) : 0;
    FenceTool::instance().setFence(fence);
#if 0
    if (!fence || fence->mLayer.isEmpty()) return;
    MapDocument *doc = DocumentManager::instance()->currentDocument();
    if (!doc) return;
    int index = doc->map()->indexOfLayer(fence->mLayer);
    if (index >= 0) {
        Layer *layer = doc->map()->layerAt(index);
        if (layer->asTileLayer())
            doc->setCurrentLayerIndex(index);
    }
#endif
}

void FenceToolDialog::setVisibleNow()
{
    if (mVisibleLater != isVisible())
        setVisible(mVisibleLater);
}

void FenceToolDialog::readTxt()
{
    mFences.clear();

    // Read the user's Fences.txt
    FenceFile fenceFile1;
    QString fileName = Preferences::instance()->configPath(txtName());
    if (QFileInfo(fileName).exists()) {
        if (fenceFile1.read(fileName)) {
            mFences += fenceFile1.takeFences();
            mTxtModifiedTime1 = QFileInfo(fileName).lastModified();
        } else {
            QMessageBox::warning(MainWindow::instance(), tr("Error Reading Fences.txt"),
                                 fenceFile1.errorString());
        }
    }

    // Read the application's Fences.txt
    FenceFile fenceFile2;
    fileName = Preferences::instance()->appConfigPath(txtName());
    if (QFileInfo(fileName).exists()) {
        if (fenceFile2.read(fileName)) {
            mFences += fenceFile2.takeFences();
            mTxtModifiedTime2 = QFileInfo(fileName).lastModified();
        } else {
            QMessageBox::warning(MainWindow::instance(), tr("Error Reading Fences.txt"),
                                 fenceFile2.errorString());
        }
    }

    ui->fenceList->clear();
    foreach (Fence *fence, mFences)
        ui->fenceList->addItem(fence->mLabel);

    ui->fenceList->setCurrentRow(mFences.size() ? 0 : -1);
}
