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

#ifndef TEMPLATEUNDOREDO_H
#define TEMPLATEUNDOREDO_H

#include <QUndoCommand>

namespace BuildingEditor {

class Room;
class TemplateDocument;

enum struct TemplateUndoRedo {
    UndoCmd_ChangeRoom = 1001,
};

class TemplateAddRemoveRoom : public QUndoCommand
{
public:
    TemplateAddRemoveRoom(TemplateDocument *doc, int index, Room *room);
    ~TemplateAddRemoveRoom();

protected:
    void add();
    void remove();

    TemplateDocument *mDocument;
    int mIndex;
    Room *mRoom;
};

class TemplateAddRoom : public TemplateAddRemoveRoom
{
public:
    TemplateAddRoom(TemplateDocument *doc, int index, Room *room);

    void undo() { remove(); }
    void redo() { add(); }
};

class TemplateRemoveRoom : public TemplateAddRemoveRoom
{
public:
    TemplateRemoveRoom(TemplateDocument *doc, int index);

    void undo() { add(); }
    void redo() { remove(); }
};

class TemplateReorderRoom : public QUndoCommand
{
public:
    TemplateReorderRoom(TemplateDocument *doc, int index, Room *room);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    TemplateDocument *mDocument;
    int mIndex;
    Room *mRoom;
};

class TemplateChangeRoom : public QUndoCommand
{
public:
    enum struct Change
    {
        Name,
        InternalName,
        Color,
        Tile
    };

    TemplateChangeRoom(TemplateDocument *doc, Room *room, const Room *data, Change change, int tileINdex);
    ~TemplateChangeRoom();

    int id() const override;
    bool mergeWith(const QUndoCommand *other) override;

    void undo() override { swap(); }
    void redo() override { swap(); }

private:
    void swap();

    TemplateDocument *mDocument;
    Change mChange;
    int mTileIndex;
    Room *mRoom;
    Room *mData;
};

} // namespace BuildingEditor

#endif // TEMPLATEUNDOREDO_H
