/*
 * Copyright 2025, Tim Baker <treectrl@users.sf.net>
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

#include "keyboardshortcutwindow.h"
#include "ui_keyboardshortcutwindow.h"

#include "actionmanager.h"
#include "shortcuteditorwidget.h"

#include <QAction>
#include <QHBoxLayout>
#include <QSettings>

KeyboardShortcutWindow::KeyboardShortcutWindow(ActionManager *actionManager, QSettings *settings, const QString &settingsKey, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::KeyboardShortcutWindow),
    mActionManager(actionManager),
    mSettings(settings),
    mSettingsKey(settingsKey)
{
    ui->setupUi(this);

    QHBoxLayout *hBoxLayout = new QHBoxLayout;
    mEditorWidget = new ShortcutEditorWidget(mActionManager);
    connect(mEditorWidget, &ShortcutEditorWidget::shortcutEdited, actionManager, &ActionManager::shortcutEdited);
    hBoxLayout->addWidget(mEditorWidget);

    QWidget *centralWidget = new QWidget;
    centralWidget->setLayout(hBoxLayout);
    setCentralWidget(centralWidget);

    connect(ui->actionReloadFromDisk, &QAction::triggered, this, &KeyboardShortcutWindow::reloadFromDisk);
    ui->actionClose->setShortcuts(QList<QKeySequence>() << QKeySequence::StandardKey::Close << Qt::Key_Escape);
    connect(ui->actionClose, &QAction::triggered, this, &QMainWindow::close);

    readSettings();
}

KeyboardShortcutWindow::~KeyboardShortcutWindow()
{
    delete ui;
}

void KeyboardShortcutWindow::closeEvent(QCloseEvent *event)
{
    QString error;
    if (!mActionManager->save(error)) {

    }
    saveSettings();
    QMainWindow::closeEvent(event);
}

void KeyboardShortcutWindow::saveSettings()
{
    mSettings->beginGroup(mSettingsKey);
    mSettings->setValue(QStringLiteral("geometry"), saveGeometry());
    mEditorWidget->saveSettings(*mSettings);
    mSettings->endGroup();
}

void KeyboardShortcutWindow::readSettings()
{
    mSettings->beginGroup(mSettingsKey);
    QByteArray geom = mSettings->value(QStringLiteral("geometry")).toByteArray();
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
    }
    mEditorWidget->readSettings(*mSettings);
    mSettings->endGroup();
}

void KeyboardShortcutWindow::reloadFromDisk()
{
    QString error;
    mActionManager->load(error);
    mEditorWidget->setModelData();
    mActionManager->emitShortcutEditedForAllActions();
}
