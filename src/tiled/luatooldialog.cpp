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

#include "luatooldialog.h"
#include "ui_luatooldialog.h"

#include "luatiletool.h"
#include "documentmanager.h"
#include "mainwindow.h"
#include "preferences.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>

using namespace Tiled::Internal;

LuaToolDialog *LuaToolDialog::mInstance = 0;

LuaToolDialog *LuaToolDialog::instance()
{
    if (!mInstance)
        mInstance = new LuaToolDialog(MainWindow::instance());
    return mInstance;
}

LuaToolDialog::LuaToolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LuaToolDialog),
    mVisibleLater(true)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::Tool);

    readSettings();

    connect(ui->scriptList, SIGNAL(currentRowChanged(int)), SLOT(currentRowChanged(int)));

    mVisibleLaterTimer.setSingleShot(true);
    mVisibleLaterTimer.setInterval(200);
    connect(&mVisibleLaterTimer, SIGNAL(timeout()), SLOT(setVisibleNow()));
}

LuaToolDialog::~LuaToolDialog()
{
    delete ui;
}

void LuaToolDialog::setVisible(bool visible)
{
    if (visible) {
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

        currentRowChanged(ui->scriptList->currentRow());
    }

    QDialog::setVisible(visible);

    if (!visible)
        writeSettings();
}

void LuaToolDialog::setVisibleLater(bool visible)
{
    mVisibleLater = visible;
    mVisibleLaterTimer.start();
}

void LuaToolDialog::readSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("LuaToolDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    settings.endGroup();
}

void LuaToolDialog::writeSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("LuaToolDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.endGroup();
}

void LuaToolDialog::currentRowChanged(int row)
{
    QString script = (row >= 0) ? mTools.at(row).mScript : QString();
    if (!script.isEmpty())
        Lua::LuaTileTool::instance().setScript(script);
}

void LuaToolDialog::setVisibleNow()
{
    if (mVisibleLater != isVisible())
        setVisible(mVisibleLater);
}

void LuaToolDialog::readTxt()
{
    mTools.clear();

    // Load the user's LuaTools.txt.
    LuaToolFile file1;
    QString fileName = Preferences::instance()->configPath(txtName());
    if (QFileInfo(fileName).exists()) {
        if (file1.read(fileName)) {
            mTools += file1.takeTools();
            mTxtModifiedTime1 = QFileInfo(fileName).lastModified();
        } else {
            QMessageBox::warning(MainWindow::instance(), tr("Error Reading LuaTools.txt"),
                                 file1.errorString());
        }
    }

    // Load the application's LuaTools.txt.
    LuaToolFile file2;
    fileName = Preferences::instance()->appConfigPath(txtName());
    if (QFileInfo(fileName).exists()) {
        if (file2.read(fileName)) {
            mTools += file2.takeTools();
            mTxtModifiedTime2 = QFileInfo(fileName).lastModified();
        } else {
            QMessageBox::warning(MainWindow::instance(), tr("Error Reading LuaTools.txt"),
                                 file2.errorString());
        }
    }

    ui->scriptList->clear();
    foreach (LuaToolInfo info, mTools)
        ui->scriptList->addItem(info.mLabel);
    ui->scriptList->setCurrentRow(mTools.size() ? 0 : -1);
}

/////

#include "BuildingEditor/simplefile.h"

#include <QDir>
#include <QFileInfo>

LuaToolFile::LuaToolFile()
{
}

LuaToolFile::~LuaToolFile()
{
//    qDeleteAll(mScripts);
}

bool LuaToolFile::read(const QString &fileName)
{
    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(fileName);
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.\n%2").arg(path).arg(simple.errorString());
        return false;
    }

    mFileName = path;
    QDir dir(info.absoluteDir());

//    int version = simple.version();

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("tool")) {
            QStringList keys;
            keys << QLatin1String("label") << QLatin1String("script");
            if (!validKeys(block, keys))
                return false;

            LuaToolInfo info;
            info.mLabel = block.value("label");
            info.mScript = block.value("script");
            if (QFileInfo(info.mScript).isRelative()) {
                info.mScript = QLatin1String("lua/") + info.mScript;
                info.mScript = dir.filePath(info.mScript);
            }

            mTools += info;
        } else {
            mError = tr("Line %1: Unknown block name '%2'.\n%3")
                    .arg(block.lineNumber)
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

QList<LuaToolInfo> LuaToolFile::takeTools()
{
    QList<LuaToolInfo> ret = mTools;
    mTools.clear();
    return ret;
}

bool LuaToolFile::validKeys(SimpleFileBlock &block, const QStringList &keys)
{
    foreach (SimpleFileKeyValue kv, block.values) {
        if (!keys.contains(kv.name)) {
            mError = tr("Line %1: Unknown attribute '%2'.\n%3")
                    .arg(kv.lineNumber)
                    .arg(kv.name)
                    .arg(mFileName);
            return false;
        }
    }
    return true;
}
