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

#ifndef KEYBOARDSHORTCUTFILE_H
#define KEYBOARDSHORTCUTFILE_H

#include <QKeySequence>
#include <QMap>
#include <QString>

class KeyboardShortcut
{
public:
    QString id;
    QKeySequence sequence;
};

class KeyboardShortcutFile
{
public:
    KeyboardShortcutFile();

    bool read(const QString &fileName);
    bool write(const QString &fileName, const QList<KeyboardShortcut> &shortcuts);

    const QList<KeyboardShortcut> &shortcuts() const
    { return mShortcuts; }

    const QString &errorString() const
    { return mError; }

private:
    QList<KeyboardShortcut> mShortcuts;
    QString mError;
};

#endif // KEYBOARDSHORTCUTFILE_H
