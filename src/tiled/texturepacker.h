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

#ifndef TEXTUREPACKER_H
#define TEXTUREPACKER_H

#include <QCoreApplication>
#include <QMap>
#include <QPoint>
#include <QSet>
#include <QSize>
#include <QStringList>

/*
 * Loosely based on Sprite Sheet Packer
 * http://spritesheetpacker.codeplex.com/
 * Originally under the MIT license
 * Copyright (c) 2009 Nick Gravelyn, Markus Ewald
 */

class TexturePackSettings
{
public:
    class Directory
    {
    public:
        QString mPath;
        bool mImagesAreTilesheets;
    };

    QString mPackFileName;
    QSize mOutputImageSize;
    QList<Directory> mInputImageDirectories;
    int padding;
};

class LemmyRectanglePacker
{
public:
    LemmyRectanglePacker(int packingAreaWidth, int packingAreaHeight);
    bool TryPack(int rectangleWidth, int rectangleHeight, QPoint &placement);

private:
    int FindFirstFreeAnchor(int rectangleWidth, int rectangleHeight, int testedPackingAreaWidth, int testedPackingAreaHeight);
    void InsertAnchor(QPoint anchor);
    bool IsFree(QRect &rectangle, int testedPackingAreaWidth, int testedPackingAreaHeight);
    void OptimizePlacement(QPoint &placement, int rectangleWidth, int rectangleHeight);
    int SelectAnchorRecursive(int rectangleWidth, int rectangleHeight, int testedPackingAreaWidth, int testedPackingAreaHeight);

    static bool Compare(const QPoint &lhs, const QPoint &rhs)
    {
        return lhs.x() + lhs.y() < rhs.x() + rhs.y();
    }

    int PackingAreaHeight;
    int PackingAreaWidth;
    int actualPackingAreaHeight;
    int actualPackingAreaWidth;
    QList<QPoint> anchors;
    QList<QRect> packedRectangles;
};

class TexturePacker
{
    Q_DECLARE_TR_FUNCTIONS(TexturePacker)
public:
    TexturePacker();

    bool pack(const TexturePackSettings &settings);
    QString errorString() { return mError; }

private:
    bool FindImages(const QString &directory, bool imagesAreTilesheets);
    bool PackImages(QImage &outputImage);

    bool PackImagesNew(QImage &outputImage);
    bool PackOneFile(const QString &str);
    bool PackListOfFiles(const QStringList &files);

    bool PackImageRectangles();
    bool TestPackingImages(int testWidth, int testHeight, QMap<QString,QRect> &testImagePlacement);
    QImage CreateOutputImage();
    bool LoadTileNamesFile(QString imageName, int columns);

private:
    QString mError;
    TexturePackSettings mSettings;
    QStringList mImageFileNames;
    QSet<QString> mImageIsTilesheet;
    QSet<QString> mImageNameSet;
    QStringList toPack;
    QMap<QString,QImage> mInputImages;
    QMap<QString,QString> mTileNames;

    class Translation
    {
    public:
        QSize size;
        QSize originalSize;
        QPoint sheetOffset;
        QPoint topLeft;
    };
    TexturePacker::Translation WorkOutTranslation(QImage image);
    TexturePacker::Translation WorkOutTranslation(QImage image, int sx, int sy, int cutWidth, int cutHeight);

    class Comparator
    {
    public:
        Comparator(TexturePacker &packer) :
            packer(packer)
        {}

        bool operator()(const QString &lhs, const QString &rhs)
        {
            return packer.CompareSubImages(lhs, rhs);
        }

        TexturePacker &packer;
    };
    bool CompareSubImages(const QString &lhs, const QString &rhs);

    QStringList NextFileList;
    QMap<QString,Translation> imageTranslation;
    QMap<QString,QRect> imagePlacement;
    QMap<QString,QMap<QString,Translation> > mImageTranslationMap;
    int outputHeight;
    int outputWidth;
};

#endif // TEXTUREPACKER_H
