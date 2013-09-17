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

#ifndef LUATOOLDIALOG_H
#define LUATOOLDIALOG_H

#include "BuildingEditor/singleton.h"

#include <QDateTime>
#include <QDialog>
#include <QTimer>

namespace Ui {
class LuaToolDialog;
}

namespace Tiled {

namespace Lua {
class LuaToolOption;
class LuaToolOptions;
}

namespace Internal {

class LuaToolDialog : public QDialog, public Singleton<LuaToolDialog>
{
    Q_OBJECT

public:
    explicit LuaToolDialog(QWidget *parent = 0);
    ~LuaToolDialog();

    void setVisible(bool visible);

    void setVisibleLater(bool visible);

    void readSettings();
    void writeSettings();

    void setToolOptions(Lua::LuaToolOptions *options);
    void setToolOptionValue(Lua::LuaToolOption *option, const QVariant &value);

    typedef Lua::LuaToolOption LuaToolOption; // hack for signals/slots

signals:
    void valueChanged(LuaToolOption *option, const QVariant &value);

private slots:
    void setVisibleNow();

private:
    Ui::LuaToolDialog *ui;
    bool mVisibleLater;
    QTimer mVisibleLaterTimer;
};

} // namespace Internal
} // namespace Tiled

#endif // LUATOOLDIALOG_H
