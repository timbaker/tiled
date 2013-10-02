/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
 *
 * This file is part of Tiled.
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

#include "texturemanager.h"

#include "preferences.h"

#include "BuildingEditor/simplefile.h"

#include "tileset.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>

using namespace Tiled;
using namespace Tiled::Internal;

SINGLETON_IMPL(TextureMgr)

TextureMgr::TextureMgr(QObject *parent) :
    QObject(parent)
{
}

TextureMgr::~TextureMgr()
{
    qDeleteAll(mTextureByName);
}

QString TextureMgr::txtPath() const
{
    return Preferences::instance()->configPath(txtName());
}

bool TextureMgr::readTxt()
{
    TexturesFile file;
    if (!file.read(txtPath())) {
        mError = file.errorString();
        return false;
    }

    foreach (TextureInfo *texInfo, file.takeTextures())
        mTextureByName[texInfo->name()] = texInfo;

    return true;
}

bool TextureMgr::writeTxt()
{
    TexturesFile file;
    if (!file.write(txtPath(), textures())) {
        mError = file.errorString();
        return false;
    }

    return true;
}

void TextureMgr::addTexture(TextureInfo *tex)
{
    Q_ASSERT(mTextureByName.contains(tex->name()) == false);
    mTextureByName[tex->name()] = tex;
    mRemovedTextures.removeAll(tex);
    emit textureAdded(tex);
}

void TextureMgr::removeTexture(TextureInfo *tex)
{
    Q_ASSERT(mTextureByName.contains(tex->name()));
    Q_ASSERT(mRemovedTextures.contains(tex) == false);
    emit textureAboutToBeRemoved(tex);
    mTextureByName.remove(tex->name());
    emit textureRemoved(tex);

    // To support undo/redo
    mRemovedTextures += tex;
}

Tileset *TextureMgr::tileset(TextureInfo *tex)
{
    Tileset *ts = tex->tileset();
    if (ts == 0) {
        ts = new Tileset(tex->name(), 32, 96);
        // ts->setTransparentColor(Qt::white); already transparent XXX
        ts->setMissing(true);
        tex->setTileset(ts);
    }
    if (ts->isMissing()) {
        QString imageSource = tex->fileName();
        if (QDir::isRelativePath(imageSource))
            imageSource = QDir(Preferences::instance()->texturesDirectory()).filePath(imageSource);
        if (ts->loadFromImage(QImage(imageSource), imageSource))
            ts->setMissing(false);
    }
    return ts;
}

/////

TexturesFile::TexturesFile()
{

}

TexturesFile::~TexturesFile()
{
    qDeleteAll(mTextures);
}

#define VERSION1 1
#define VERSION_LATEST VERSION1

bool TexturesFile::read(const QString &fileName)
{
    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(QFileInfo(fileName).fileName());
        return false;
    }
#if 0
    if (!upgradeTxt())
        return false;

    if (!mergeTxt())
        return false;
#endif
    QString path = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = simple.errorString();
        return false;
    }

    if (simple.version() < 1 || simple.version() > VERSION_LATEST) {
        mError = tr("Unknown version number %1.").arg(simple.version());
        return false;
    }

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("texture")) {
            QString texFileName = block.value("file");
            if (texFileName.isEmpty()) {
                mError = tr("Line %1: No-name textures aren't allowed.").arg(block.lineNumber);
                return false;
            }
            texFileName += QLatin1String(".png");
            QFileInfo finfo(texFileName); // relative to Textures directory
            QString textureName = finfo.completeBaseName();
            if (mTextureByName.contains(textureName) != 0) {
                mError = tr("Line %1: Duplicate texture '%2'.")
                        .arg(block.lineNumber)
                        .arg(textureName);
                return false;
            }

            QString sizeStr = block.value("size");
            int columns, rows;
            if (!parse2Ints(sizeStr, &columns, &rows) || (columns < 1) || (rows < 1)) {
                mError = tr("Line %1: Invalid texture size '%2'.")
                        .arg(block.lineNumber).arg(sizeStr);
                return false;
            }

            TextureInfo *info = new TextureInfo(textureName, texFileName, columns, rows);
            mTextures += info;
            mTextureByName[info->name()] = info;
        } else {
            mError = tr("Line %1: Unknown block name '%2'.")
                    .arg(block.lineNumber)
                    .arg(block.name);
            return false;
        }
    }

    return true;
}

bool TexturesFile::write(const QString &fileName, const QList<TextureInfo *> &textures)
{
    SimpleFile simpleFile;

    QDir texDir(Preferences::instance()->texturesDirectory());
    foreach (TextureInfo *tex, textures) {
        SimpleFileBlock tilesetBlock;
        tilesetBlock.name = QLatin1String("texture");

        QString relativePath = texDir.relativeFilePath(tex->fileName());
        relativePath.truncate(relativePath.length() - 4); // remove .png
        tilesetBlock.addValue("file", relativePath);

        int columns = tex->columnCount();
        int rows = tex->rowCount();
        tilesetBlock.addValue("size", QString(QLatin1String("%1,%2")).arg(columns).arg(rows));

        simpleFile.blocks += tilesetBlock;
    }

    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

bool TexturesFile::parse2Ints(const QString &s, int *pa, int *pb)
{
    QStringList coords = s.split(QLatin1Char(','), QString::SkipEmptyParts);
    if (coords.size() != 2)
        return false;
    bool ok;
    int a = coords[0].toInt(&ok);
    if (!ok) return false;
    int b = coords[1].toInt(&ok);
    if (!ok) return false;
    *pa = a, *pb = b;
    return true;
}
