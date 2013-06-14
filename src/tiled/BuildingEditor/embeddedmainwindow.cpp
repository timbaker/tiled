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

using namespace BuildingEditor;

static const char dockWidgetActiveState[] = "DockWidgetActiveState";

// Much of this class is based on QtCreator's FancyMainWindow

EmbeddedMainWindow::EmbeddedMainWindow(QWidget *parent) :
    QMainWindow(parent, Qt::Widget),
    mHandleDockVisibilityChanges(false)
{
}

void EmbeddedMainWindow::registerDockWidget(QDockWidget *dockWidget)
{
    connect(dockWidget->toggleViewAction(), SIGNAL(triggered()),
        this, SLOT(onDockActionTriggered()), Qt::QueuedConnection);
    connect(dockWidget, SIGNAL(visibilityChanged(bool)),
            this, SLOT(onDockVisibilityChange(bool)));
    connect(dockWidget, SIGNAL(topLevelChanged(bool)),
            this, SLOT(onDockTopLevelChanged()));
    dockWidget->setProperty(dockWidgetActiveState, true);
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
                && dockWidget->property(dockWidgetActiveState).toBool());
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

void EmbeddedMainWindow::onDockVisibilityChanged(bool visible)
{
    if (mHandleDockVisibilityChanges)
        sender()->setProperty(dockWidgetActiveState, visible);
}

void EmbeddedMainWindow::onDockTopLevelChanged()
{
}
