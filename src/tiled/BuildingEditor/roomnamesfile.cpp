/*
 * Copyright 2026, Tim Baker <treectrl@users.sf.net>
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

#include "roomnamesfile.h"

#include "simplefile.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QRandomGenerator>

using namespace BuildingEditor;

RoomNamesFile::RoomNamesFile() :
    mRoomColorSet(compareQColors)
{

}

bool BuildingEditor::RoomNamesFile::read(const QString &fileName)
{
    mRoomNames.clear();
    mRoomColorSet.clear();

    SimpleFile simpleFile;
    if (!simpleFile.read(fileName)) {
        // if (QFileInfo::exists(fileName)) {
        //     QMessageBox::warning(this, QStringLiteral("Error reading RoomNames.txt"),
        //                          QStringLiteral("Failed to open %1").arg(fileName));
        // }
        mError = simpleFile.errorString();
        return false;
    }

    QRandomGenerator *generator = QRandomGenerator::global();
    for (const SimpleFileBlock &block : std::as_const(simpleFile.blocks)) {
        if (block.name == QStringLiteral("room")) {
            RoomName roomName;
            roomName.internalName = block.value("internal").trimmed();
            roomName.label = block.value("label").trimmed();
            if (block.hasValue("color") && !block.value("color").trimmed().isEmpty()) {
                QColor color = QColor(block.value("color").trimmed());
                if (color.isValid()) {
                    roomName.color = color;
                }
            }
            if (!roomName.color.isValid()) {
                QColor randomColor;
                do {
                    int red = generator->bounded(256); // 0 to 255
                    int green = generator->bounded(256); // 0 to 255
                    int blue = generator->bounded(256); // 0 to 255
                    randomColor = QColor(red, green, blue);
                } while (mRoomColorSet.find(randomColor) != mRoomColorSet.end());
                roomName.color = randomColor;
            }
            mRoomColorSet.insert(roomName.color);
            if (!roomName.label.isEmpty() && !roomName.internalName.isEmpty()) {
                mRoomNames += roomName;
            }
        }
    }

    return true;
}

const QStringList RoomNamesFile::internalNames() const
{
    QStringList result;
    for (const RoomName &roomName : mRoomNames)
    {
        if (!roomName.internalName.isEmpty() && !result.contains(roomName.internalName)) {
            result += roomName.internalName;
        }
    }
    return result;
}
