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

#include "texturepackfile.h"

#include <QBuffer>
#include <QDebug>
#include <QDataStream>
#include <QFile>

PackFile::PackFile()
{

}

PackFile::~PackFile()
{

}

static int readInt(QDataStream &in)
{
    qint32 ret;
    in >> ret;
    return ret;
}

static int chl1 = 0;
static int chl2 = 0;
static int chl3 = 0;
static int chl4 = 0;
static int readIntByte(QDataStream &in)
{
    int ch1 = chl2;
    int ch2 = chl3;
    int ch3 = chl4;
    quint8 ch;
    in >> ch;
    int ch4 = ch;
    chl1 = ch1;
    chl2 = ch2;
    chl3 = ch3;
    chl4 = ch4;
//    if ((ch1 | ch2 | ch3 | ch4) < 0)
//        throw new EOFException();

    return ((ch1 << 0) + (ch2 << 8) + (ch3 << 16) + (ch4 << 24));
}

static QString ReadString(QDataStream &in)
{
    QString str;
    int len = readInt(in);
    for (int n = 0; n < len; n++) {
        quint8 c;
        in >> c;
        str += QLatin1Char(c);
    }
    return str;
}

static void SaveString(QDataStream &out, QString &str)
{
    out << (qint32) str.length();
    for (int i = 0; i < str.length(); i++)
        out << (quint8) str.at(i).toLatin1();
}

bool PackFile::read(const QString &fileName)
{
    mPages.clear();

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        mError = tr("Error opening file for reading.\n%1").arg(fileName);
        return false;
    }

//    QDir dir = QFileInfo(fileName).absoluteDir();

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    int numPages = readInt(in);
    qDebug() << "PackFile: reading" << numPages << "pages";
    for (int i = 0; i < numPages; i++) {
        PackPage page;
        page.name = ReadString(in);
        int numEntries = readInt(in);
        bool mask = readInt(in) != 0;
        qDebug() << "PackFile: page=" << page.name << "numEntries=" << numEntries;

        for (int n = 0; n < numEntries; n++) {
            QString entryName = ReadString(in);
            qDebug() << entryName;
            int x = readInt(in);
            int y = readInt(in);
            int w = readInt(in);
            int h = readInt(in);
            int ox = readInt(in);
            int oy = readInt(in);
            int fx = readInt(in);
            int fy = readInt(in);
            page.mInfo += PackSubTexInfo(x, y, w, h, ox, oy, fx, fy, entryName);
        }

        QBuffer buf;
        buf.buffer().reserve(250 * 1024);
        buf.open(QBuffer::ReadWrite);
        while (true) {
            quint32 ii = readIntByte(in);
            if (ii == 0xDEADBEEF)
                break;
            char ch = ((ii >> 24) & 0xFF);
            int n = buf.write(&ch, 1);
            if (n != 1)
                break;
        }

        qDebug() << "Creating PNG" << page.name << "size=" << buf.size();
        page.image.loadFromData(buf.buffer(), "PNG");

#if 0
        for (int y = 0; y < page.image.height(); y++) {
            for (int x = 0; x < page.image.width(); x++) {
                QRgb rgb = page.image.pixel(x, y);
                if (qAlpha(rgb) == 0) {
                    int red = qRed(rgb);
                    int green = qGreen(rgb);
                    int blue = qBlue(rgb);
                    if (red + green + blue > 0) {
                        int dbg = 1;
                    }
                }
            }
        }
#endif

//        quint32 magic = readInt(in);
//        if (magic != 0xDEADBEEF) {
//            mError = tr("Expected 0xDEADBEEF after PNG data");
//            return false;
//        }

        mPages += page;
    }

    return true;
}

#include <QDir>
#include <QFileInfo>

bool PackFile::write(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        mError = tr("Error opening file for writing.\n%1").arg(fileName);
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    out << (qint32) mPages.size();

#if 0
    int pageNum = 1;
#endif
    foreach (PackPage page, mPages) {
        SaveString(out, page.name);
        out << (qint32) page.mInfo.size();
        out << (qint32) 1; // FIXME: mask???
        foreach (PackSubTexInfo info, page.mInfo) {
            SaveString(out, info.name);
            out << (qint32) info.x;
            out << (qint32) info.y;
            out << (qint32) info.w;
            out << (qint32) info.h;
            out << (qint32) info.ox;
            out << (qint32) info.oy;
            out << (qint32) info.fx;
            out << (qint32) info.fy;
        }
        QBuffer b;
        b.buffer().reserve(250 * 1024);
        page.image.save(&b, "PNG");
        out.writeRawData(b.buffer().data(), b.buffer().length());
        out << (quint32) 0xDEADBEEF;

#if 0
        page.image.save(QFileInfo(fileName).dir().filePath(QStringLiteral("page%1.png").arg(pageNum)));
        pageNum++;
#endif
    }

    return true;
}
