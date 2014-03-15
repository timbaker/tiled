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

#include "texturepacker.h"

#include "texturepackfile.h"

#include <QDir>
#include <QImage>

#define TILE_WIDTH 64
#define TILE_HEIGHT 128

TexturePacker::TexturePacker()
{
}

bool TexturePacker::pack(const TexturePackSettings &settings)
{
    mSettings = settings;
    mImageNameSet.clear();
    mImageFileNames.clear();
    mImageIsTilesheet.clear();

    foreach (TexturePackSettings::Directory tpd, settings.mInputImageDirectories) {
        if (!FindImages(tpd.mPath, tpd.mImagesAreTilesheets))
            return false;
    }

    if (mImageFileNames.isEmpty()) {
        mError = tr("There are no image files to pack.");
        return false;
    }

    PackFile packFile;
    int pageNum = 0;

    NextFileList = mImageFileNames;
    while (!NextFileList.isEmpty()) {
        QImage outputImage;
        if (!PackImages(outputImage))
            return false;

        PackPage packPage;
        packPage.name = QFileInfo(mSettings.mPackFileName).baseName() + QString::number(pageNum);
        packPage.image = outputImage;
        foreach (QString index, toPack) {
            QRect rectangle1(imagePlacement[index].topLeft(), imageTranslation[index].size);
            QRect rectangle2(imageTranslation[index].topLeft - imageTranslation[index].sheetOffset, imageTranslation[index].originalSize);
            QString name;
            if (index.contains(QLatin1String("_INDEX_"))) {
                QStringList ss = index.split(QLatin1String("_INDEX_"));
                name = QFileInfo(ss[1]).baseName() + QLatin1String("_") + ss[0];
            } else {
                name = QFileInfo(index).baseName();
            }
            PackSubTexInfo texInfo(rectangle1.x(), rectangle1.y(), rectangle1.width(), rectangle1.height(),
                                   rectangle2.x(), rectangle2.y(), rectangle2.width(), rectangle2.height(),
                                   name);
            packPage.mInfo += texInfo;
        }
        packFile.addPage(packPage);

        pageNum++;
    }

    packFile.write(mSettings.mPackFileName);

    return true;
}

bool TexturePacker::FindImages(const QString &directory, bool imagesAreTilesheets)
{
    QDir dir(directory);
    QStringList filters;
    filters += QLatin1String("*.png");
    foreach (QFileInfo fileInfo, dir.entryInfoList(filters)) {
        QString baseName = fileInfo.baseName();
        if (mImageNameSet.contains(baseName)) {
            mError = tr("There are two input image files with the same name.\nThe conflicting name is \"%1\".")
                    .arg(baseName);
        }
        mImageFileNames += fileInfo.absoluteFilePath();
        mImageNameSet += baseName;
        if (imagesAreTilesheets)
            mImageIsTilesheet += fileInfo.absoluteFilePath();
    }
    return true;
}

bool TexturePacker::CompareSubImages(const QString &lhs1, const QString &rhs1)
{
    Translation lhs = imageTranslation.value(lhs1);
    Translation rhs = imageTranslation.value(rhs1);
    if (lhs.size.width() != rhs.size.width())
        return lhs.size.width() > rhs.size.width();
    if (lhs.size.height() != rhs.size.height())
        return lhs.size.height() > rhs.size.height();
    return lhs1 < rhs1;
}

bool TexturePacker::PackImages(QImage &outputImage)
{
    QStringList nextFiles/* = NextFileList*/;
    toPack.clear();
    imageTranslation.clear();
    imagePlacement.clear();

    foreach (QString str, NextFileList) {
        if (imageTranslation.size() > 200) {
            nextFiles += str;
            continue;
        }

        QImage image(str);
        if (image.isNull()) {
            mError = tr("Failed to load an input image file.\n%1").arg(str);
            return false;
        }

        if (mImageIsTilesheet.contains(str)) {
            qreal cols = image.width() / (qreal) TILE_WIDTH;
            qreal rows = image.height() / (qreal) TILE_HEIGHT;
            if (image.width() % TILE_WIDTH || image.height() % TILE_HEIGHT) {
                imageTranslation[str] = WorkOutTranslation(image);
                toPack += str;
            } else {
                for (int y = 0; y < rows; y++) {
                    for (int x = 0; x < cols; x++) {
                        Translation tln = WorkOutTranslation(image, x * TILE_WIDTH, y * TILE_HEIGHT, TILE_WIDTH, TILE_HEIGHT);
                        if (!tln.size.isEmpty()) {
                            QString key = QString::fromLatin1("%1_INDEX_%2").arg(x + y * cols).arg(str);
                            imageTranslation[key] = tln;
                            toPack += key;
                        }
                    }
                }
            }
        } else {
            imageTranslation[str] = WorkOutTranslation(image);
            toPack += str;
        }
    }

    Comparator cmp(*this);
    std::sort(toPack.begin(), toPack.end(), cmp);

    outputWidth = mSettings.mOutputImageSize.width();
    outputHeight = mSettings.mOutputImageSize.height();
    if (!PackImageRectangles())
        return false;

    outputImage = CreateOutputImage();
    if (outputImage.isNull())
        return false;

    NextFileList = nextFiles;

    return true;
}

bool TexturePacker::PackImageRectangles()
{
    int minWidth = INT_MAX;
    int minHeight = INT_MAX;
    QMapIterator<QString,Translation> iter(imageTranslation);
    while (iter.hasNext()) {
        iter.next();
        minWidth = qMin(minWidth, iter.value().size.width());
        minHeight = qMin(minHeight, iter.value().size.height());
    }

    int newWidth = outputWidth;
    int testHeight = outputHeight;
    QMap<QString,QRect> testImagePlacement;
    bool flag = false;
    while (true)
    {
        testImagePlacement.clear();
        if (!TestPackingImages(newWidth, testHeight, testImagePlacement))
        {
            if (imagePlacement.size() != 0)
            {
                if (!flag)
                {
                    flag = true;
                    newWidth += minWidth + mSettings.padding + mSettings.padding;
                    testHeight += minHeight + mSettings.padding + mSettings.padding;
                }
                else
                    return true;
            }
            else {
                break;
            }
        }
        else
        {
            imagePlacement = testImagePlacement;
            int val1_3;
            newWidth = val1_3 = 0;
            QMapIterator<QString,QRect> it(imagePlacement);
            while (it.hasNext())
            {
                it.next();
                newWidth = qMax(newWidth, it.value().right() + 1);
                val1_3 = qMax(val1_3, it.value().bottom() + 1);
            }
            if (!flag)
                newWidth -= mSettings.padding;
            int newHeight = val1_3 - mSettings.padding;
/*
            if (this.requirePow2)
            {
                num1 = MiscHelper.FindNextPowerOfTwo(num1);
                num2 = MiscHelper.FindNextPowerOfTwo(num2);
            }
            if (this.requireSquare)
                num1 = num2 = Math.Max(num1, num2);
*/
            if (newWidth == outputWidth && newHeight == outputHeight)
            {
                if (!flag)
                    flag = true;
                else
                    return true;
            }
            outputWidth = newWidth;
            outputHeight = newHeight;
            if (!flag)
                newWidth -= minWidth;
            testHeight = newHeight - minHeight;
        }
    }
    return false;
}

bool TexturePacker::TestPackingImages(int testWidth, int testHeight, QMap<QString,QRect> &testImagePlacement)
{
    LemmyRectanglePacker lemmyRectanglePacker(testWidth, testHeight);
    foreach (QString key, toPack)
    {
        QSize size = imageTranslation[key].size;
        QPoint placement;
        if (!lemmyRectanglePacker.TryPack(size.width() + mSettings.padding, size.height() + mSettings.padding, placement)) {
            mError = tr("Couldn't pack %1").arg(key);
            return false;
        }
        testImagePlacement[key] = QRect(placement.x(), placement.y(), size.width() + mSettings.padding, size.height() + mSettings.padding);
    }
    return true;
}

QImage TexturePacker::CreateOutputImage()
{
    QImage bitmap1(outputWidth, outputHeight, QImage::Format_ARGB32);
    bitmap1.fill(Qt::transparent);
    foreach (QString index, toPack)
    {
        QRect rectangle = imagePlacement[index];
        Translation translation = imageTranslation[index];
        QString file = index;
        file.replace(QLatin1String("INDEX_"), QLatin1String(""));
        if (index.contains(QLatin1String("INDEX_")))
            file = file.mid(file.indexOf(QLatin1Char('_')) + 1);
        QString palette;
        if (file.contains(QLatin1Char('#')))
        {
            QStringList strArray = file.split(QLatin1Char('#'));
            file = strArray[0];
            palette = strArray[1];
        }
        if (!mInputImages.contains(file))
            mInputImages[file] = QImage(file);
//        QImage bitmap2(file/*, palette*/);
        QImage bitmap2 = mInputImages[file];
        if (bitmap2.isNull()) {
            mError = tr("Failed to load input image.\n%1").arg(file);
            return QImage();
        }
        for (int x = translation.topLeft.x(); x < translation.topLeft.x() + translation.size.width(); ++x)
        {
            for (int y = translation.topLeft.y(); y < translation.topLeft.y() + translation.size.height(); ++y)
                bitmap1.setPixel(rectangle.x() + (x - translation.topLeft.x()),
                                 rectangle.y() + (y - translation.topLeft.y()),
                                 bitmap2.pixel(x, y));
        }
    }
    return bitmap1;
}

TexturePacker::Translation TexturePacker::WorkOutTranslation(QImage image)
{
    int y1 = 0;
    int x1 = 0;
    int width = image.width();
    int height = image.height();

    bool flag1 = false;
    for (int y2 = 0; y2 < image.height() && !flag1; ++y2) {
        for (int x2 = 0; x2 < image.width(); ++x2) {
            if (qAlpha(image.pixel(x2, y2)) > 0) {
                y1 = y2;
                flag1 = true;
                break;
            }
        }
    }

    bool flag2 = false;
    for (int y2 = image.height() - 1; y2 >= 0 && !flag2; --y2) {
        for (int x2 = 0; x2 < image.width(); ++x2) {
            if (qAlpha(image.pixel(x2, y2)) > 0) {
                height = y2 - (y1 - 1);
                flag2 = true;
                break;
            }
        }
    }

    bool flag3 = false;
    for (int x2 = 0; x2 < image.width() && !flag3; ++x2) {
        for (int y2 = 0; y2 < image.height(); ++y2) {
            if (qAlpha(image.pixel(x2, y2)) > 0) {
                x1 = x2;
                flag3 = true;
                break;
            }
        }
    }

    bool flag4 = false;
    for (int x2 = image.width() - 1; x2 >= 0 && !flag4; --x2) {
        for (int y2 = 0; y2 < image.height(); ++y2) {
            if (qAlpha(image.pixel(x2, y2)) > 0) {
                width = x2 - (x1 - 1);
                flag4 = true;
                break;
            }
        }
    }

    Translation tln;
    tln.topLeft = QPoint(x1, y1);
    tln.size = QSize(width, height);
    tln.originalSize = image.size();
    return tln;
}

TexturePacker::Translation TexturePacker::WorkOutTranslation(QImage image, int sx, int sy, int cutWidth, int cutHeight)
{
    int endY = sy + cutHeight;
    int endX = sx + cutWidth;
    int y1 = sy;
    int x1 = sx;
    int width1 = cutWidth;
    int height = cutHeight;

    bool flag1 = false;
    for (int y2 = sy; y2 < endY && !flag1; ++y2)
    {
        for (int x2 = sx; x2 < endX; ++x2)
        {
            if (qAlpha(image.pixel(x2, y2)) > 0)
            {
                y1 = y2;
                flag1 = true;
                break;
            }
        }
    }
    if (!flag1)
        return Translation();

    bool flag2 = false;
    for (int y2 = endY - 1; y2 >= sy && !flag2; --y2)
    {
        for (int x2 = sx; x2 < endX; ++x2)
        {
            if (qAlpha(image.pixel(x2, y2)) > 0)
            {
                height = y2 - (y1 - 1);
                flag2 = true;
                break;
            }
        }
    }
    if (!flag2)
        return Translation();

    bool flag3 = false;
    for (int x2 = sx; x2 < endX && !flag3; ++x2)
    {
        for (int y2 = sy; y2 < endY; ++y2)
        {
            if (qAlpha(image.pixel(x2, y2)) > 0)
            {
                x1 = x2;
                flag3 = true;
                break;
            }
        }
    }
    if (!flag3)
        return Translation();

    bool flag4 = false;
    for (int x2 = endX - 1; x2 >= sx && !flag4; --x2)
    {
        for (int y2 = sy; y2 < endY; ++y2)
        {
            if (qAlpha(image.pixel(x2, y2)) > 0)
            {
                width1 = x2 - (x1 - 1);
                flag4 = true;
                break;
            }
        }
    }
    if (!flag4)
        return Translation();

    Translation tln;
    tln.topLeft = QPoint(x1, y1);
    tln.size = QSize(width1, height);
//    int width2 = image.width();
//    int num3 = tln.topLeft.x + tln.size.width();
    tln.originalSize = QSize(cutWidth, cutHeight);
    tln.sheetOffset = QPoint(sx, sy);
    return tln;
}

/////

LemmyRectanglePacker::LemmyRectanglePacker(int packingAreaWidth, int packingAreaHeight) :
    PackingAreaWidth(packingAreaWidth),
    PackingAreaHeight(packingAreaHeight),
    actualPackingAreaWidth(1),
    actualPackingAreaHeight(1)
{
    anchors += QPoint();
}

bool LemmyRectanglePacker::TryPack(int rectangleWidth, int rectangleHeight, QPoint &placement)
{
    int index = SelectAnchorRecursive(rectangleWidth, rectangleHeight, actualPackingAreaWidth, actualPackingAreaHeight);
    if (index == -1)
    {
        placement = QPoint();
        return false;
    }
    else
    {
        placement = anchors[index];
        OptimizePlacement(placement, rectangleWidth, rectangleHeight);
         if ((placement.x() + rectangleWidth > anchors[index].x()) && (placement.y() + rectangleHeight > anchors[index].y()))
            anchors.removeAt(index);
        InsertAnchor(QPoint(placement.x() + rectangleWidth, placement.y()));
        InsertAnchor(QPoint(placement.x(), placement.y() + rectangleHeight));
        packedRectangles += QRect(placement.x(), placement.y(), rectangleWidth, rectangleHeight);
        return true;
    }
}

int LemmyRectanglePacker::FindFirstFreeAnchor(int rectangleWidth, int rectangleHeight, int testedPackingAreaWidth, int testedPackingAreaHeight)
{
    QRect rectangle(0, 0, rectangleWidth, rectangleHeight);
    for (int index = 0; index < anchors.size(); ++index)
    {
        rectangle.moveTo(anchors[index]);
        if (IsFree(rectangle, testedPackingAreaWidth, testedPackingAreaHeight))
            return index;
    }
    return -1;
}

void LemmyRectanglePacker::InsertAnchor(QPoint anchor)
{
//    int index = anchors.BinarySearch(anchor, (IComparer<Point>) LemmtRectanglePacker.AnchorRankComparer.Default);
    for (int index = 0; index < anchors.size(); ++index) {
        if (Compare(anchor, anchors[index])) {
            anchors.insert(index, anchor);
            return;
        }
    }
    anchors.append(anchor);
}

bool LemmyRectanglePacker::IsFree(QRect &rectangle, int testedPackingAreaWidth, int testedPackingAreaHeight)
{
    if (rectangle.x() < 0 || rectangle.y() < 0 || rectangle.right()+1 > testedPackingAreaWidth || rectangle.bottom()+1 > testedPackingAreaHeight)
        return false;
    for (int index = 0; index < packedRectangles.size(); ++index)
    {
        if (packedRectangles[index].intersects(rectangle))
            return false;
    }
    return true;
}

void LemmyRectanglePacker::OptimizePlacement(QPoint &placement, int rectangleWidth, int rectangleHeight)
{
    QRect rectangle(placement.x(), placement.y(), rectangleWidth, rectangleHeight);
    int x = placement.x();
    while (IsFree(rectangle, PackingAreaWidth, PackingAreaHeight))
    {
        x = rectangle.x();
        rectangle.moveLeft(x - 1);
    }
    rectangle.moveLeft(placement.x());
    int y = placement.y();
    while (IsFree(rectangle, PackingAreaWidth, PackingAreaHeight))
    {
        y = rectangle.y();
        rectangle.moveTop(y-1);
    }
    if (placement.x() - x > placement.y() - y)
        placement.setX(x);
    else
        placement.setY(y);
}

int LemmyRectanglePacker::SelectAnchorRecursive(int rectangleWidth, int rectangleHeight, int testedPackingAreaWidth, int testedPackingAreaHeight)
{
    int firstFreeAnchor = FindFirstFreeAnchor(rectangleWidth, rectangleHeight, testedPackingAreaWidth, testedPackingAreaHeight);
    if (firstFreeAnchor != -1)
    {
        actualPackingAreaWidth = testedPackingAreaWidth;
        actualPackingAreaHeight = testedPackingAreaHeight;
        return firstFreeAnchor;
    }
    else
    {
        bool flag = testedPackingAreaWidth < PackingAreaWidth;
        if (testedPackingAreaHeight < PackingAreaHeight && (!flag || testedPackingAreaHeight < testedPackingAreaWidth))
            return SelectAnchorRecursive(rectangleWidth, rectangleHeight, testedPackingAreaWidth, qMin(testedPackingAreaHeight * 2, PackingAreaHeight));
        if (flag)
            return SelectAnchorRecursive(rectangleWidth, rectangleHeight, qMin(testedPackingAreaWidth * 2, PackingAreaWidth), testedPackingAreaHeight);
        else
            return -1;
    }
}
