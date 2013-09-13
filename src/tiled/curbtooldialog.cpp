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
#include "documentmanager.h"
#include "mainwindow.h"
#include "preferences.h"

#include <QFileInfo>
#include <QMessageBox>
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
    ui->suppressBlend->setChecked(CurbTool::instance().suppressBlendTiles());

    connect(ui->curbList, SIGNAL(currentRowChanged(int)), SLOT(currentRowChanged(int)));
    connect(ui->suppressBlend, SIGNAL(toggled(bool)), SLOT(suppressChanged(bool)));

    mVisibleLaterTimer.setSingleShot(true);
    mVisibleLaterTimer.setInterval(200);
    connect(&mVisibleLaterTimer, SIGNAL(timeout()), SLOT(setVisibleNow()));

//    readTxt();
}

CurbToolDialog::~CurbToolDialog()
{
    delete ui;
}

void CurbToolDialog::setVisible(bool visible)
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

        currentRowChanged(ui->curbList->currentRow());
    }

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
    bool suppress = settings.value(QLatin1String("suppress"), CurbTool::instance().suppressBlendTiles()).toBool();
    suppressChanged(suppress);
    settings.endGroup();
}

void CurbToolDialog::writeSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("CurbToolDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.setValue(QLatin1String("suppress"), CurbTool::instance().suppressBlendTiles());
    settings.endGroup();
}

void CurbToolDialog::currentRowChanged(int row)
{
    Curb *curb = (row >= 0) ? mCurbs.at(row) : 0;
    CurbTool::instance().setCurb(curb);

    if (!curb || curb->mLayer.isEmpty()) return;
    MapDocument *doc = DocumentManager::instance()->currentDocument();
    if (!doc) return;
    int index = doc->map()->indexOfLayer(curb->mLayer);
    if (index >= 0) {
        Layer *layer = doc->map()->layerAt(index);
        if (layer->asTileLayer())
            doc->setCurrentLayerIndex(index);
    }
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

void CurbToolDialog::readTxt()
{
    mCurbs.clear();

    // Read the user's Curbs.txt
    CurbFile curbFile1;
    QString fileName = Preferences::instance()->configPath(txtName());
    if (QFileInfo(fileName).exists()) {
        if (curbFile1.read(fileName)) {
            mCurbs += curbFile1.takeCurbs();
            mTxtModifiedTime1 = QFileInfo(fileName).lastModified();
        } else {
            QMessageBox::warning(MainWindow::instance(), tr("Error Reading Curbs.txt"),
                                 curbFile1.errorString());
        }
    }

    // Read the application's Curbs.txt
    CurbFile curbFile2;
    fileName = Preferences::instance()->appConfigPath(txtName());
    if (QFileInfo(fileName).exists()) {
        if (curbFile2.read(fileName)) {
            mCurbs += curbFile2.takeCurbs();
            mTxtModifiedTime2 = QFileInfo(fileName).lastModified();
        } else {
            QMessageBox::warning(MainWindow::instance(), tr("Error Reading Curbs.txt"),
                                 curbFile2.errorString());
        }
    }

    ui->curbList->clear();
    foreach (Curb *curb, mCurbs)
        ui->curbList->addItem(curb->mLabel);

    ui->curbList->setCurrentRow(mCurbs.size() ? 0 : -1);
}
