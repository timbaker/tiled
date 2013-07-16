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

#include "luaconsole.h"
#include "ui_luaconsole.h"

#include "luamapsdialog.h"
#include "mainwindow.h"

#include <QDesktopServices>
#include <QUrl>

using namespace Tiled;
using namespace Internal;

LuaConsole *LuaConsole::mInstance = 0;

LuaConsole *LuaConsole::instance()
{
    if (!mInstance)
        mInstance = new LuaConsole;
    return mInstance;
}

LuaConsole::LuaConsole(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LuaConsole)
{
    setWindowFlags(windowFlags() | Qt::Tool);

    ui->setupUi(this);

    connect(ui->actionRun_Script, SIGNAL(triggered()), SLOT(runScript()));
    connect(ui->actionRunAgain, SIGNAL(triggered()), SLOT(runAgain()));
    connect(ui->btnRunAgain, SIGNAL(clicked()), SLOT(runAgain()));
    connect(ui->actionRunInDirectory, SIGNAL(triggered()), SLOT(runInDirectory()));

    connect(ui->clear, SIGNAL(clicked()), SLOT(clear()));

    connect(ui->actionHelpContents, SIGNAL(triggered()), SLOT(helpContents()));
}

LuaConsole::~LuaConsole()
{
    delete ui;
}


void LuaConsole::runScript()
{
    MainWindow::instance()->LuaScript(QString());
}

void LuaConsole::runAgain()
{
    MainWindow::instance()->LuaScript(mFileName);
}

void LuaConsole::runInDirectory()
{
    LuaMapsDialog d(this);
    d.exec();

    // Not sure why MainWindow comes to the front, maybe due to this
    // window having the Qt::Tool flag?
    raise();
    activateWindow();
}

void LuaConsole::clear()
{
    ui->textEdit->clear();
}

void LuaConsole::writestring(const char *s, int len)
{
    ui->textEdit->moveCursor(QTextCursor::End);
    ui->textEdit->insertPlainText(QString::fromLatin1(s, len));
}

void LuaConsole::writeline()
{
    ui->textEdit->moveCursor(QTextCursor::End);
    ui->textEdit->insertPlainText(QLatin1String("\n"));
}

void LuaConsole::write(const QString &s, QColor color)
{
    if (s.isEmpty()) return;

    ui->textEdit->moveCursor(QTextCursor::End);
    ui->textEdit->setTextColor(color);
    ui->textEdit->insertPlainText(s);

//    ui->textEdit->moveCursor(QTextCursor::End);
    ui->textEdit->setTextColor(Qt::black);
    ui->textEdit->insertPlainText(QLatin1String("\n"));
}

void LuaConsole::helpContents()
{
    QUrl url = QUrl::fromLocalFile(
            QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + QLatin1String("docs/TileZed/LuaScripting.html"));
    QDesktopServices::openUrl(url);
}
