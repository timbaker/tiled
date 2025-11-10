/*
 * Copyright 2025, Tim Baker <treectrl@users.sf.net>
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

#ifndef TEMPLATEROOMSDIALOG_H
#define TEMPLATEROOMSDIALOG_H

#include "BuildingEditor/buildingdocument.h"
#include "BuildingEditor/templatedocument.h"
#include <QDialog>
#include <QMap>

#include <set>

class QListWidgetItem;
class QToolButton;
class QUndoStack;

namespace Ui {
class RoomsDialog;
}

namespace BuildingEditor {

class BuildingTemplate;
class BuildingTemplatesDialog;
class BuildingTileEntry;
class Room;
class TemplateDocument;

class RoomName
{
public:
    QString label;
    QString internalName;
    QColor color;
};

extern bool compareQColors(const QColor& a, const QColor& b);

class TemplateRoomsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit TemplateRoomsDialog(TemplateDocument *doc, Room *initialRoom = nullptr, BuildingTemplatesDialog *parent = nullptr);
    ~TemplateRoomsDialog();

private:
    void readRoomNamesDotTxt(QList<RoomName> &roomNames);
    void readRoomNamesDotTxt(const QString &fileName, QList<RoomName> &roomNames);
    QListWidgetItem *itemFor(Room *room);
    int findRoomNameByLabel(const QString &label) const;
    int findRoomNameByInternalName(const QString &internalName) const;
    void setRoomsList();
    void synchUI();
    void setTilePixmap();
    BuildingEditor::BuildingTileEntry *selectedTile();
    QRgb pickColorForNewRoom();
    void saveSettings();
    void readSettings();

private slots:
    void roomSelectionChanged();
    void addRoom();
    void removeRoom();
    void duplicateRoom();
    void moveRoomUp();
    void moveRoomDown();

    void nameEdited(const QString &name);
    void internalNameEdited(const QString &name);
    void colorChanged(const QColor &color);
    void randomiseColor();
    void tileSelectionChanged();
    void clearTile();
    void randomTile();
    void chooseTile();

    void roomAdded(BuildingEditor::Room *room);
    void roomRemoved(BuildingEditor::Room *room);
    void roomChanged(BuildingEditor::Room *room);
    void roomsReordered();

    void undoTextChanged(const QString &text);
    void redoTextChanged(const QString &text);

    void accept() override;
    void reject() override;

private:
    Ui::RoomsDialog *ui;
    TemplateDocument *mDocument;
    BuildingTemplate *mTemplate;
    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
    Room *mRoom;
    QListWidgetItem *mRoomItem;
    int mTileRow;
    std::set<QColor, decltype(&compareQColors)> mRoomColorSet;
    QList<RoomName> mRoomNames;
    QToolButton *mUndoButton;
    QToolButton *mRedoButton;
};

} // namespace BuildingEditor

#endif // TEMPLATEROOMSDIALOG_H
