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

#include "embeddedmainwindow.h"

#include <QAction>
#include <QDockWidget>
#include <QSettings>

using namespace BuildingEditor;

static const char KEY_DOCKWIDGET_ACTIVE_STATE[] = "DockWidgetActiveState";

// Much of this class is based on QtCreator's FancyMainWindow

EmbeddedMainWindow::EmbeddedMainWindow(QWidget *parent) :
    QMainWindow(parent, Qt::Widget),
    mHandleDockVisibilityChanges(false)
{
}

void EmbeddedMainWindow::registerDockWidget(QDockWidget *dockWidget)
{
    Q_ASSERT(!dockWidget->objectName().isEmpty());
    connect(dockWidget->toggleViewAction(), SIGNAL(triggered()),
        this, SLOT(onDockActionTriggered()), Qt::QueuedConnection);
    connect(dockWidget, SIGNAL(visibilityChanged(bool)),
            this, SLOT(onDockVisibilityChange(bool)));
    connect(dockWidget, SIGNAL(topLevelChanged(bool)),
            this, SLOT(onDockTopLevelChanged()));
    dockWidget->setProperty(KEY_DOCKWIDGET_ACTIVE_STATE, true);
}

#define STATE_VERSION 0
void EmbeddedMainWindow::readSettings(QSettings &settings)
{
    QByteArray state = settings.value(objectName() + QLatin1String(".state")).toByteArray();
    if (!state.isEmpty())
        restoreState(state, STATE_VERSION);

    foreach (QDockWidget *dockWidget, dockWidgets()) {
        if (dockWidget->isFloating())
            dockWidget->setVisible(false);
        dockWidget->setProperty(KEY_DOCKWIDGET_ACTIVE_STATE,
                                settings.value(dockWidget->objectName(), true));
    }
}

void EmbeddedMainWindow::writeSettings(QSettings &settings)
{
    settings.setValue(objectName() + QLatin1String(".state"), saveState(STATE_VERSION));

    foreach (QDockWidget *dockWidget, dockWidgets()) {
        settings.setValue(dockWidget->objectName(), dockWidget->property(KEY_DOCKWIDGET_ACTIVE_STATE));
    }
}

void EmbeddedMainWindow::showEvent(QShowEvent *e)
{
    QMainWindow::showEvent(e);
    handleVisibilityChange(true);
}

void EmbeddedMainWindow::hideEvent(QHideEvent *e)
{
    QMainWindow::hideEvent(e);
    handleVisibilityChange(false);
}

void EmbeddedMainWindow::handleVisibilityChange(bool visible)
{
    mHandleDockVisibilityChanges = false;
    foreach (QDockWidget *dockWidget, dockWidgets()) {
        if (dockWidget->isFloating()) {
            dockWidget->setVisible(visible
                && dockWidget->property(KEY_DOCKWIDGET_ACTIVE_STATE).toBool());
        }
    }
    if (visible)
        mHandleDockVisibilityChanges = true;
}

QList<QDockWidget *> EmbeddedMainWindow::dockWidgets() const
{
    return findChildren<QDockWidget*>();
}

void EmbeddedMainWindow::onDockActionTriggered()
{
}

void EmbeddedMainWindow::onDockVisibilityChange(bool visible)
{
    if (mHandleDockVisibilityChanges)
        sender()->setProperty(KEY_DOCKWIDGET_ACTIVE_STATE, visible);
}

void EmbeddedMainWindow::onDockTopLevelChanged()
{
}
