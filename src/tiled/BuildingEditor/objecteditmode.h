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

#ifndef OBJECTEDITMODE_H
#define OBJECTEDITMODE_H

#include "imode.h"

#include <QMap>

class QAction;
class QComboBox;
class QLabel;
class QMainWindow;
class QTabWidget;

namespace BuildingEditor {

class Building;
class BuildingDocument;
class CategoryDock;
class EditModeStatusBar;
class EmbeddedMainWindow;
class Room;

class ObjectEditModePerDocumentStuff;
class ObjectEditModeToolBar;

class ObjectEditMode : public IMode
{
    Q_OBJECT
public:
    ObjectEditMode(QObject *parent = 0);

    Building *currentBuilding() const;
    Room *currentRoom() const;

    void toOrtho();
    void toIso();

    void readSettings(QSettings &settings);
    void writeSettings(QSettings &settings);

private slots:
    void documentAdded(BuildingDocument *doc);
    void currentDocumentChanged(BuildingDocument *doc);
    void documentAboutToClose(int index, BuildingDocument *doc);

    void currentDocumentTabChanged(int index);
    void documentTabCloseRequested(int index);

    void roomAdded(Room *room);
    void roomRemoved(Room *room);
    void roomsReordered();
    void roomChanged(Room *room);

    void updateActions();

private:
    EmbeddedMainWindow *mMainWindow;
    QTabWidget *mTabWidget;
    ObjectEditModeToolBar *mToolBar;
    EditModeStatusBar *mStatusBar;
    CategoryDock *mCategoryDock;

    BuildingDocument *mCurrentDocument;
    ObjectEditModePerDocumentStuff *mCurrentDocumentStuff;

    friend class ObjectEditModePerDocumentStuff;
    QMap<BuildingDocument*,ObjectEditModePerDocumentStuff*> mDocumentStuff;

};

}

#endif // OBJECTEDITMODE_H
