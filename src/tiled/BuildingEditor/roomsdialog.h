/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef ROOMSDIALOG_H
#define ROOMSDIALOG_H

#include <QDialog>
#include <QMap>

class QListWidgetItem;

namespace Ui {
class RoomsDialog;
}

namespace BuildingEditor {

class BuildingTileEntry;
class Room;

class RoomsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit RoomsDialog(const QList<Room *> &rooms, QWidget *parent = 0);
    ~RoomsDialog();

    const QList<Room*> rooms() const
    { return mRooms; }

    Room *originalRoom(Room *dialogRoom) const;

private:
    void setRoomsList();
    void synchUI();
    void setTilePixmap();
    BuildingEditor::BuildingTileEntry *selectedTile();
    QRgb pickColorForNewRoom();

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
    void tileSelectionChanged();
    void chooseTile();

private:
    Ui::RoomsDialog *ui;
    QList<Room*> mRooms;
    QMap<Room*,Room*> mRoomsMap;
    Room *mRoom;
    QListWidgetItem *mRoomItem;
    int mTileRow;
};

} // namespace BuildingEditor

#endif // ROOMSDIALOG_H
