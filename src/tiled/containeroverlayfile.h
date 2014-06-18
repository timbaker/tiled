/*
 * Copyright 2014, Tim Baker <treectrl@users.sf.net>
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

#ifndef CONTAINEROVERLAYFILE_H
#define CONTAINEROVERLAYFILE_H

#include <QStringList>

class ContainerOverlay;

class ContainerOverlayEntry
{
public:
    ContainerOverlay *mParent;
    QString mRoomName;
    QStringList mTiles;
};

class ContainerOverlay
{
public:
    QString mTileName;
    QList<ContainerOverlayEntry*> mEntries;
};

class ContainerOverlayFile
{
public:
    ContainerOverlayFile();
    ~ContainerOverlayFile() { qDeleteAll(mOverlays); }

    bool read(const QString &fileName);
    bool write(const QString &fileName);

    QString errorString() { return mError; }

    QList<ContainerOverlay*> mOverlays;
    QString mError;
};

#endif // CONTAINEROVERLAYFILE_H
