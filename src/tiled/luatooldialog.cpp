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

#include <QDebug>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>

using namespace Tiled;
using namespace Tiled::Internal;

SINGLETON_IMPL(LuaToolDialog)

LuaToolDialog::LuaToolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LuaToolDialog),
    mVisibleLater(true)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::Tool);

    readSettings();

    connect(ui->options, SIGNAL(valueChanged(LuaToolOption*,QVariant)),
            SIGNAL(valueChanged(LuaToolOption*,QVariant)));

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
    settings.beginGroup(QLatin1String("LuaToolDialog") );
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

void LuaToolDialog::setToolOptions(Tiled::Lua::LuaToolOptions *options)
{
    ui->options->setOptions(options);
}

void LuaToolDialog::setToolOptionValue(Lua::LuaToolOption *option, const QVariant &value)
{
    ui->options->setValue(option, value);
}

void LuaToolDialog::setVisibleNow()
{
    if (mVisibleLater != isVisible())
        setVisible(mVisibleLater);
}
