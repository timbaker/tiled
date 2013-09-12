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

#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QTimer>

namespace Ui {
class LuaToolDialog;
}

class SimpleFileBlock;

namespace Tiled {
namespace Internal {

class LuaToolInfo
{
public:
    QString mLabel;
    QString mScript;
};

class LuaToolDialog : public QDialog
{
    Q_OBJECT

public:
    static LuaToolDialog *instance();

    void setVisible(bool visible);

    void setVisibleLater(bool visible);

    void readSettings();
    void writeSettings();

    QString txtName() const { return QLatin1String("LuaTools.txt"); }

private slots:
    void currentRowChanged(int row);
    void setVisibleNow();

private:
    void readTxt();

private:
    Q_DISABLE_COPY(LuaToolDialog)
    static LuaToolDialog *mInstance;
    explicit LuaToolDialog(QWidget *parent = 0);
    ~LuaToolDialog();

    Ui::LuaToolDialog *ui;
    bool mVisibleLater;
    QTimer mVisibleLaterTimer;
    QDateTime mTxtModifiedTime;
    QList<LuaToolInfo> mTools;
};

class LuaToolFile
{
    Q_DECLARE_TR_FUNCTIONS(LuaToolFile)

public:
    LuaToolFile();
    ~LuaToolFile();

    bool read(const QString &fileName);

    QString errorString() const
    { return mError; }

    QList<LuaToolInfo> takeTools();

private:
    bool validKeys(SimpleFileBlock &block, const QStringList &keys);

private:
    QList<LuaToolInfo> mTools;
    QString mFileName;
    QString mError;
};

} // namespace Internal
} // namespace Tiled

#endif // LUATOOLDIALOG_H
