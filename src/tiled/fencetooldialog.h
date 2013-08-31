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

#ifndef FENCETOOLDIALOG_H
#define FENCETOOLDIALOG_H

#include <QDateTime>
#include <QDialog>
#include <QTimer>

namespace Ui {
class FenceToolDialog;
}

namespace Tiled {
namespace Internal {

class Fence;

class FenceToolDialog : public QDialog
{
    Q_OBJECT

public:
    static FenceToolDialog *instance();

    void setVisible(bool visible);

    void setVisibleLater(bool visible);

    void readSettings();
    void writeSettings();

private slots:
    void currentRowChanged(int row);
    void setVisibleNow();

private:
    void readTxt();

private:
    Q_DISABLE_COPY(FenceToolDialog)
    static FenceToolDialog *mInstance;
    explicit FenceToolDialog(QWidget *parent = 0);
    ~FenceToolDialog();

    Ui::FenceToolDialog *ui;
    bool mVisibleLater;
    QTimer mVisibleLaterTimer;
    QList<Fence*> mFences;
    QDateTime mTxtModifiedTime;
};

} // namespace Internal
} // namespace Tiled

#endif // FENCETOOLDIALOG_H
