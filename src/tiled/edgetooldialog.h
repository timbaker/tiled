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

#ifndef EDGETOOLDIALOG_H
#define EDGETOOLDIALOG_H

#include <QDateTime>
#include <QDialog>
#include <QTimer>

namespace Ui {
class EdgeToolDialog;
}

namespace Tiled {
namespace Internal {

class Edges;

class EdgeToolDialog : public QDialog
{
    Q_OBJECT
    
public:
    static EdgeToolDialog *instance();

    void setVisible(bool visible);

    void setVisibleLater(bool visible);

    void readSettings();
    void writeSettings();

    QString txtName() const { return QLatin1String("Edges.txt"); }

private slots:
    void currentRowChanged(int row);
    void dashChanged();
    void suppressChanged(bool suppress);
    void setVisibleNow();

private:
    void readTxt();

private:
    Q_DISABLE_COPY(EdgeToolDialog)
    static EdgeToolDialog *mInstance;
    explicit EdgeToolDialog(QWidget *parent = 0);
    ~EdgeToolDialog();

    Ui::EdgeToolDialog *ui;
    bool mVisibleLater;
    QTimer mVisibleLaterTimer;
    QList<Edges*> mEdges;
    QDateTime mTxtModifiedTime1;
    QDateTime mTxtModifiedTime2;
};

} // namespace Internal
} // namespace Tiled

#endif // EDGETOOLDIALOG_H
