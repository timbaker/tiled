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

#ifndef TILEEDITMODE_H
#define TILEEDITMODE_H

#include "imode.h"

#include <QMap>
#include <QToolBar>

class QAction;
class QLabel;
class QMainWindow;
class QTabWidget;

namespace BuildingEditor {

class BuildingDocument;
class BuildingFurnitureDock;
class BuildingLayersDock;
class BuildingTilesetDock;
class CategoryDock;
class EditModeStatusBar;
class EmbeddedMainWindow;

class TileEditModeToolBar;
class TileEditModePerDocumentStuff;

class TileEditModeToolBar : public QToolBar
{
    Q_OBJECT
public:
    TileEditModeToolBar(QWidget *parent = 0);

private slots:
    void currentDocumentChanged(BuildingDocument *doc);

    void updateActions();

public:
    BuildingDocument *mCurrentDocument;
    QLabel *mFloorLabel;
};

class TileEditMode : public IMode
{
    Q_OBJECT
public:
    explicit TileEditMode(QObject *parent = 0);

    void toTile();

    void readSettings(QSettings &settings);
    void writeSettings(QSettings &settings);

signals:
    
public slots:
    void onActiveStateChanged(bool active);

    void documentAdded(BuildingDocument *doc);
    void currentDocumentChanged(BuildingDocument *doc);
    void documentAboutToClose(int index, BuildingDocument *doc);

    void currentDocumentTabChanged(int index);
    void documentTabCloseRequested(int index);

    void updateActions();

private:
    EmbeddedMainWindow *mMainWindow;
    QTabWidget *mTabWidget;
    EditModeStatusBar *mStatusBar;

    TileEditModeToolBar *mToolBar;
    BuildingFurnitureDock *mFurnitureDock;
    BuildingLayersDock *mLayersDock;
    BuildingTilesetDock *mTilesetDock;
    bool mFirstTimeSeen;

    BuildingDocument *mCurrentDocument;
    TileEditModePerDocumentStuff *mCurrentDocumentStuff;

    friend class TileEditModePerDocumentStuff;
    QMap<BuildingDocument*,TileEditModePerDocumentStuff*> mDocumentStuff;
};

} // namespace BuildingEditor;

#endif // TILEEDITMODE_H
