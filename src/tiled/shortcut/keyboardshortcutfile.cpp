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

#include "keyboardshortcutfile.h"

#include "BuildingEditor/simplefile.h"

#include <QFileInfo>

#define VERSION1 1
#define VERSION_LATEST VERSION1

KeyboardShortcutFile::KeyboardShortcutFile()
{

}

bool KeyboardShortcutFile::read(const QString &fileName)
{
    mShortcuts.clear();

    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = QStringLiteral("The %1 file doesn't exist.").arg(fileName);
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = QStringLiteral("Error reading %1.\n%2").arg(path).arg(simple.errorString());
        return false;
    }

    int version = simple.version();
    if (version < VERSION1 || version > VERSION_LATEST) {
        mError = QStringLiteral("Error reading %1.\nUnknown version %2.").arg(path).arg(version);
        return false;
    }

    for (const SimpleFileBlock &block : simple.blocks) {
        if (block.name == QLatin1String("shortcuts")) {
            for (const SimpleFileKeyValue &kv : block.values) {
                QString id = kv.name.trimmed();
                QString sequence = kv.value.trimmed();
                if (id.isEmpty()) {
                    continue;
                }
                KeyboardShortcut shortcut;
                shortcut.id = id;
                shortcut.sequence = QKeySequence(sequence, QKeySequence::SequenceFormat::PortableText);
                mShortcuts += shortcut;
            }
        } else {
            mError = QStringLiteral("Unknown block name '%1'.\n%2").arg(block.name).arg(path);
            return false;
        }
    }

    return true;
}

bool KeyboardShortcutFile::write(const QString &fileName, const QList<KeyboardShortcut> &shortcuts)
{
    SimpleFile simpleFile;
    simpleFile.setVersion(VERSION_LATEST);
    SimpleFileBlock block;
    block.name = QStringLiteral("shortcuts");
    QList<KeyboardShortcut> sorted(shortcuts);
    std::sort(sorted.begin(), sorted.end(), [](const KeyboardShortcut &a, const KeyboardShortcut &b) {
        return a.id < b.id;
    });
    for (const KeyboardShortcut &shortcut : sorted) {
        block.addValue(shortcut.id, shortcut.sequence.toString(QKeySequence::SequenceFormat::PortableText));
    }
    simpleFile.blocks += block;
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}
