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

#ifndef KEYBOARDSHORTCUTWINDOW_H
#define KEYBOARDSHORTCUTWINDOW_H

#include <QMainWindow>

class ActionManager;
class ShortcutEditorWidget;

class QSettings;

namespace Ui {
class KeyboardShortcutWindow;
}

class KeyboardShortcutWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit KeyboardShortcutWindow(ActionManager *actionManager, QSettings *settings, const QString &settingsKey, QWidget *parent = nullptr);
    ~KeyboardShortcutWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void saveSettings();
    void readSettings();

private slots:
    void reloadFromDisk();

private:
    Ui::KeyboardShortcutWindow *ui;
    ActionManager *mActionManager;
    ShortcutEditorWidget *mEditorWidget;
    QSettings *mSettings;
    QString mSettingsKey;
};

#endif // KEYBOARDSHORTCUTWINDOW_H
