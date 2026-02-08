/*
 * Copyright 2026, Tim Baker <treectrl@users.sf.net>
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

#include "buildingtilesfile.h"

#include "buildingtiles.h"
#include "simplefile.h"

#include <QDir>
#include <QMessageBox>

using namespace BuildingEditor;

BuildingTilesFile::BuildingTilesFile()
{
    BuildingTilesMgr::createCategories(mCategories);

    for (BuildingTileCategory *category : std::as_const(mCategories)) {
        mCategoryByName[category->name()] = category;
    }
}

BuildingEditor::BuildingTilesFile::~BuildingTilesFile()
{
    qDeleteAll(mCategories);
    mCategories.clear();
}

int BuildingTilesFile::getVersionLatest()
{
    return VERSION_LATEST;
}

static SimpleFileBlock findCategoryBlock(const SimpleFileBlock &parent, const QString &categoryName)
{
    for (const SimpleFileBlock &block : parent.blocks) {
        if (block.name == QStringLiteral("category")) {
            if (block.value("name") == categoryName) {
                return block;
            }
        }
    }
    return SimpleFileBlock();
}

bool BuildingTilesFile::read(const QString &fileName)
{
    const QString nativePath = QDir::toNativeSeparators(fileName);

    SimpleFile simple;
    if (!simple.read(fileName)) {
        mError = QStringLiteral("%1\n(while reading %2)").arg(simple.errorString()).arg(nativePath);
        return false;
    }

    mVersion = simple.version();

    if (mVersion > VERSION_LATEST) {
        mError = QStringLiteral("Version %1 is from a newer version of TileZed\n(while reading %2)").arg(mVersion).arg(nativePath);
        return false;
    }

    mRevision = simple.value("revision").toInt();
    mSourceRevision = simple.value("source_revision").toInt();

    if (mVersion == VERSION0) {
        simple.blocks += findCategoryBlock(simple, QStringLiteral("curtains"));
    }

    if (mVersion < VERSION2) {
        SimpleFileBlock newFile;
        // Massive rewrite -> BuildingTileEntry stuff
        for (const SimpleFileBlock &block : std::as_const(simple.blocks)) {
            if (block.name == QLatin1String("category")) {
                QString categoryName = block.value(QLatin1String("name"));
                SimpleFileBlock newCatBlock;
                newCatBlock.name = block.name;
                newCatBlock.values += SimpleFileKeyValue(QLatin1String("name"), categoryName);
                BuildingTileCategory *category = mCategoryByName[categoryName];
                for (const SimpleFileKeyValue &kv : block.block("tiles").values) {
                    QString tileName = kv.value;
                    BuildingTileEntry *entry = category->createEntryFromSingleTile(tileName);
                    SimpleFileBlock newEntryBlock;
                    newEntryBlock.name = QLatin1String("entry");
                    for (int i = 0; i < category->enumCount(); i++) {
                        newEntryBlock.values += SimpleFileKeyValue(category->enumToString(i), entry->tile(i)->name());
                        if (!entry->offset(i).isNull()) {
                            newEntryBlock.values += SimpleFileKeyValue(QLatin1String("offset"), QLatin1String("FIXME"));
                        }
                    }
                    newCatBlock.blocks += newEntryBlock;
                }
                newFile.blocks += newCatBlock;
            }
        }
        simple.blocks = newFile.blocks;
        simple.values = newFile.values;
    }

    for (const SimpleFileBlock &block : std::as_const(simple.blocks)) {
        if (block.name == QStringLiteral("category")) {
            QString categoryName = block.value("name");
            BuildingTileCategory *category = mCategoryByName[categoryName];
            if (category == nullptr) {
                mError = QStringLiteral("Unknown category '%1'\n(while reading %2)").arg(categoryName).arg(nativePath);
                return false;
            }
            for (const SimpleFileBlock &block2 : block.blocks) {
                if (block2.name == QStringLiteral("entry")) {
                    if (BuildingTileEntry *entry = readTileEntry(category, block2, mError)) {
                        category->insertEntry(category->entryCount(), entry);
                    } else {
                        return false;
                    }
                } else {
                    mError = QStringLiteral("Unknown block name '%1'\n(while reading %2)").arg(block2.name).arg(nativePath);
                    return false;
                }
            }
        } else {
            mError = QStringLiteral("Unknown block name '%1'.\n(while reading %2)").arg(block.name).arg(nativePath);
            return false;
        }
    }

    for (BuildingTileCategory *category : std::as_const(mCategories)) {
        category->setDefaultEntry(category->entry(0));
    }
    BuildingTilesMgr *btm = BuildingTilesMgr::instance();
    mCategories[BuildingTilesMgr::CategoryEnum::Curtains]->setDefaultEntry(btm->noneTileEntry());

    return true;
}

bool BuildingTilesFile::write(const QString &fileName, const int revision, const int sourceRevision, const QVector<BuildingTileCategory *> &categories)
{
    SimpleFile simpleFile;
    for (BuildingTileCategory *category : categories) {
        SimpleFileBlock categoryBlock;
        categoryBlock.name = QStringLiteral("category");
        categoryBlock.values += SimpleFileKeyValue(QStringLiteral("label"), category->label());
        categoryBlock.values += SimpleFileKeyValue(QStringLiteral("name"), category->name());
        for (BuildingTileEntry *entry : category->entries()) {
            writeTileEntry(categoryBlock, entry);
        }
        simpleFile.blocks += categoryBlock;
    }
    simpleFile.setVersion(VERSION_LATEST);
    simpleFile.replaceValue("revision", QString::number(revision));
    simpleFile.replaceValue("source_revision", QString::number(sourceRevision));
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

bool BuildingTilesFile::write(const QString &fileName)
{
    return write(fileName, mRevision, mSourceRevision, mCategories);
}

int BuildingTilesFile::getVersion() const
{
    return mVersion;
}

int BuildingTilesFile::getRevision() const
{
    return mRevision;
}

int BuildingTilesFile::getSourceRevision() const
{
    return mSourceRevision;
}

const QVector<BuildingTileCategory*> &BuildingTilesFile::categories() const
{
    return mCategories;
}

const QString &BuildingTilesFile::errorString() const
{
    return mError;
}

void BuildingTilesFile::writeTileEntry(SimpleFileBlock &parentBlock, BuildingTileEntry *entry)
{
    BuildingTileCategory *category = entry->category();
    SimpleFileBlock block;
    block.name = QStringLiteral("entry");
//    block.addValue("category", category->name());
    for (int i = 0; i < category->enumCount(); i++) {
        block.addValue(category->enumToString(i), entry->tile(i)->name());
    }
    for (int i = 0; i < category->enumCount(); i++) {
        QPoint p = entry->offset(i);
        if (p.isNull())
            continue;
        block.addValue("offset", QStringLiteral("%1 %2 %3").arg(category->enumToString(i)).arg(p.x()).arg(p.y()));
    }
    parentBlock.blocks += block;
}

BuildingTileEntry *BuildingTilesFile::readTileEntry(BuildingTileCategory *category, const SimpleFileBlock &block, QString &error)
{
    BuildingTileEntry *entry = new BuildingTileEntry(category);

    for (const SimpleFileKeyValue &kv : block.values) {
        if (kv.name == QStringLiteral("offset")) {
            QStringList split = kv.value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (split.size() != 3) {
                error = QStringLiteral("Expected 'offset = name x y', got '%1'").arg(kv.value);
                delete entry;
                return 0;
            }
            int e = category->enumFromString(split[0]);
            if (e == BuildingTileCategory::Invalid) {
                error = QStringLiteral("Unknown %1 enum name '%2'").arg(category->name()).arg(split[0]);
                delete entry;
                return 0;
            }
            entry->mOffsets[e] = QPoint(split[1].toInt(), split[2].toInt());
            continue;
        }
        int e = category->enumFromString(kv.name);
        if (e == BuildingTileCategory::Invalid) {
            error = QStringLiteral("Unknown %1 enum name '%2'").arg(category->name()).arg(kv.name);
            delete entry;
            return 0;
        }
        entry->mTiles[e] = BuildingTilesMgr::instance()->get(kv.value);
    }

    return entry;
}
