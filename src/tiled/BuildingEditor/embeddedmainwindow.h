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

#ifndef EMBEDDEDMAINWINDOW_H
#define EMBEDDEDMAINWINDOW_H

#include <QMainWindow>

class QSettings;

namespace BuildingEditor {

class EmbeddedMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit EmbeddedMainWindow(QWidget *parent = 0);

    void registerDockWidget(QDockWidget *dockWidget);
    
    void readSettings(QSettings &settings);
    void writeSettings(QSettings &settings);

    QList<QDockWidget*> dockWidgets() const;
    QList<QToolBar*> toolBars() const;

protected:
    void showEvent(QShowEvent *e);
    void hideEvent(QHideEvent *e);
    
    void handleVisibilityChange(bool visible);

private slots:
    void onDockActionTriggered();
    void onDockVisibilityChange(bool visible);
    void onDockTopLevelChanged();

private:
    bool mHandleDockVisibilityChanges;
};

} // namespace BuildingEditor

#endif // EMBEDDEDMAINWINDOW_H
