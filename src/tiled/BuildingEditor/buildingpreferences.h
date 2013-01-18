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

#ifndef BUILDINGPREFERENCES_H
#define BUILDINGPREFERENCES_H

#include <QObject>
#include <QSettings>

namespace BuildingEditor {

class BuildingPreferences : public QObject
{
    Q_OBJECT
public:
    static BuildingPreferences *instance();
    static void deleteInstance();

    explicit BuildingPreferences(QObject *parent = 0);

    QString configPath() const;
    QString configPath(const QString &fileName) const;

    bool showGrid() const
    { return mShowGrid; }

    bool highlightFloor() const
    { return mHighlightFloor; }

    bool highlightRoom() const
    { return mHighlightRoom; }

    bool showWalls() const
    { return mShowWalls; }

    bool showObjects() const
    { return mShowObjects; }

    qreal tileScale() const
    { return mTileScale; }

    bool useOpenGL() const
    { return mUseOpenGL; }

    bool levelIsometric() const
    { return mLevelIsometric; }

signals:
    void showGridChanged(bool show);
    void highlightFloorChanged(bool highlight);
    void highlightRoomChanged(bool highlight);
    void showWallsChanged(bool show);
    void showObjectsChanged(bool show);
    void tileScaleChanged(qreal scale);
    void useOpenGLChanged(bool useOpenGL);
    void levelIsometricChanged(bool levels);

public slots:
    void setShowGrid(bool show);
    void setHighlightFloor(bool highlight);
    void setHighlightRoom(bool highlight);
    void setShowWalls(bool show);
    void setShowObjects(bool show);
    void setTileScale(qreal scale);
    void setUseOpenGL(bool useOpenGL);
    void setLevelIsometric(bool levels);

private:
    static BuildingPreferences *mInstance;
    QSettings mSettings;
    bool mShowGrid;
    bool mHighlightFloor;
    bool mHighlightRoom;
    bool mShowWalls;
    bool mShowObjects;
    qreal mTileScale;
    bool mUseOpenGL;
    bool mLevelIsometric;
};

} // namespace BuildingEditor

#endif // BUILDINGPREFERENCES_H
