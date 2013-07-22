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

#ifndef LUACONSOLE_H
#define LUACONSOLE_H

#include <QMainWindow>

namespace Ui {
class LuaConsole;
}

class LuaConsole : public QMainWindow
{
    Q_OBJECT
    
public:
    static LuaConsole *instance();
    static void deleteInstance();

    const QString fileName() const
    { return mFileName; }
    void setFile(const QString &fileName);

    void write(const QString &s, QColor color = Qt::black);

    // These are the luai_write* implementations!
    void writestring(const char *s, int len);
    void writeline();

private slots:
    void runScript();
    void runAgain();
    void runInDirectory();
    void runOnWorld();
    void clear();
    void helpContents();

private:
    Q_DISABLE_COPY(LuaConsole)
    static LuaConsole *mInstance;
    explicit LuaConsole(QWidget *parent = 0);
    ~LuaConsole();

    Ui::LuaConsole *ui;
    QString mFileName;
};

#endif // LUACONSOLE_H
