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

#ifndef BUILDINGOBJECTS_H
#define BUILDINGOBJECTS_H

#include <QRect>

namespace BuildingEditor {

class BuildingFloor;
class BuildingTile;

class BuildingObject
{
public:
    enum Direction
    {
        N,
        S,
        E,
        W,
        Invalid
    };

    BuildingObject(BuildingFloor *floor, int x, int y, Direction mDir);

    BuildingFloor *floor() const
    { return mFloor; }

    int index();

    virtual QRect bounds() const
    { return QRect(mX, mY, 1, 1); }

    void setPos(int x, int y)
    { mX = x, mY = y; }

    void setPos(const QPoint &pos)
    { setPos(pos.x(), pos.y()); }

    QPoint pos() const
    { return QPoint(mX, mY); }

    int x() const { return mX; }
    int y() const { return mY; }

    void setDir(Direction dir)
    { mDir = dir; }

    Direction dir() const
    { return mDir; }

    QString dirString() const;
    static Direction dirFromString(const QString &s);

    void setTile(BuildingTile *tile)
    { mTile = tile; }

    BuildingTile *tile() const
    { return mTile; }

    virtual bool isValidPos(const QPoint &offset = QPoint(),
                            BuildingEditor::BuildingFloor *floor = 0) const;

    virtual void rotate(bool right);
    virtual void flip(bool horizontal);

protected:
    BuildingFloor *mFloor;
    Direction mDir;
    int mX;
    int mY;
    BuildingTile *mTile;
};

class Door : public BuildingObject
{
public:
    Door(BuildingFloor *floor, int x, int y, Direction dir) :
        BuildingObject(floor, x, y, dir),
        mFrameTile(0)
    {

    }

    int getOffset()
    { return (mDir == N) ? 1 : 0; }

    void setFrameTile(BuildingTile *tile)
    { mFrameTile = tile; }

    BuildingTile *frameTile() const
    { return mFrameTile; }

private:
    BuildingTile *mFrameTile;
};

class Stairs : public BuildingObject
{
public:
    Stairs(BuildingFloor *floor, int x, int y, Direction dir) :
        BuildingObject(floor, x, y, dir)
    {
    }

    QRect bounds() const;

    void rotate(bool right);
    void flip(bool horizontal);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    int getOffset(int x, int y);
};

class Window : public BuildingObject
{
public:
    Window(BuildingFloor *floor, int x, int y, Direction dir) :
        BuildingObject(floor, x, y, dir)
    {

    }

    int getOffset()
    { return (mDir == N) ? 1 : 0; }
};

} // namespace BulidingEditor

#endif // BUILDINGOBJECTS_H
