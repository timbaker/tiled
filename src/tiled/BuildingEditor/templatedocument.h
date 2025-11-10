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

#ifndef TEMPLATEDOCUMENT_H
#define TEMPLATEDOCUMENT_H

#include <QObject>

namespace BuildingEditor {

class Room;
class BuildingTemplate;

class TemplateDocument : public QObject
{
    Q_OBJECT
public:
    explicit TemplateDocument(BuildingTemplate *templ8);
    ~TemplateDocument();

    BuildingTemplate *templ8()
    { return mTemplate; }

    void insertRoom(int index, Room *room);
    Room *removeRoom(int index);
    int reorderRoom(int index, Room *room);
    Room *changeRoom(Room *room, const Room *data);

signals:
    void roomAdded(BuildingEditor::Room *room);
    void roomAboutToBeRemoved(BuildingEditor::Room *room);
    void roomRemoved(BuildingEditor::Room *room);
    void roomsReordered();
    void roomChanged(BuildingEditor::Room *room);

private:
    BuildingTemplate *mTemplate;
};

} // namespace BuildingEditor

#endif // TEMPLATEDOCUMENT_H
