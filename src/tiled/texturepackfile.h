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

#ifndef TEXTUREPACKFILE_H
#define TEXTUREPACKFILE_H

#include <QCoreApplication>
#include <QImage>

class PackSubTexInfo
{
public:
    PackSubTexInfo(int x, int y, int w, int h, int ox, int oy, int fx, int fy, QString name) :
        x(x), y(y), w(w), h(h), ox(ox), oy(oy), fx(fx), fy(fy), name(name)
    {
    }

    int x;
    int y;
    int w;
    int h;
    int ox;
    int oy;
    int fx;
    int fy;
    QString name;
};

class PackPage
{
public:
    const QList<PackSubTexInfo> &subTextures() { return mInfo; }

    QString name;
    QList<PackSubTexInfo> mInfo;
    QImage image;
};

class PackFile
{
    Q_DECLARE_TR_FUNCTIONS(PackFile)
public:
    PackFile();
    ~PackFile();

    bool read(const QString &fileName);
    bool write(const QString &fileName);

    QString errorString() { return mError; }

    void addPage(PackPage &page) { mPages += page; }
    const QList<PackPage> &pages() const { return mPages; }

private:
    QList<PackPage> mPages;
    QString mError;
};

#endif // TEXTUREPACKFILE_H
