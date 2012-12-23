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

#include "pathgeneratormgr.h"

#include "mainwindow.h"
#include "pathgenerator.h"
#include "tilesetmanager.h"

#include "pathgenerator.h"
#include "tile.h"
#include "tileset.h"

#include "BuildingEditor/buildingpreferences.h"
#include "BuildingEditor/listofstringsdialog.h"
#include "BuildingEditor/simplefile.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>

using namespace Tiled;
using namespace Internal;

static const char *TXT_FILE = "PathGenerators.txt";

PathGeneratorMgr *PathGeneratorMgr::mInstance = 0;

PathGeneratorMgr *PathGeneratorMgr::instance()
{
    if (!mInstance)
        mInstance = new PathGeneratorMgr;
    return mInstance;
}

void PathGeneratorMgr::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}


PathGeneratorMgr::PathGeneratorMgr(QObject *parent) :
    QObject(parent)
{
    mMissingTileset = new Tileset(QLatin1String("missing"), 64, 128);
    mMissingTileset->setTransparentColor(Qt::white);
    QString fileName = QLatin1String(":/BuildingEditor/icons/missing-tile.png");
    if (!mMissingTileset->loadFromImage(QImage(fileName), fileName)) {
        QImage image(64, 128, QImage::Format_ARGB32);
        image.fill(Qt::red);
        mMissingTileset->loadFromImage(image, fileName);
    }
    mMissingTile = mMissingTileset->tileAt(0);

#if 0
    // readTxt() gives us user-defined generators.
    if (PathGenerator *pgen = new PG_Fence(QLatin1String("Fence - Tall Wooden"))) {
        pgen->refCountUp();
        mGenerators += pgen;
    }

    if (PathGenerator *pgen = new PG_Fence(QLatin1String("Fence - Short Wooden"))) {
        for (int i = 0; i < PG_Fence::TileCount; i++)
            pgen->properties().at(i)->asTile()->mTileID += 16 + 8; // Short wooden
        pgen->refCountUp();
        mGenerators += pgen;
    }

    if (PathGenerator *pgen = new PG_StreetLight(QLatin1String("Street Light - West & North"))) {
        pgen->refCountUp();
        mGenerators += pgen;
    }

    if (PathGenerator *pgen = new PG_StreetLight(QLatin1String("Street Light - East & South"))) {
        pgen->properties().at(PG_StreetLight::Reverse)->asBoolean()->mValue = true;
        pgen->refCountUp();
        mGenerators += pgen;
    }

    // readTxt() gives us a list of tilesets.
    if (Tileset *ts = loadTileset(QLatin1String("C:\\Users\\Tim\\Desktop\\ProjectZomboid\\FromLemmy\\Maps\\Tiles\\fencing_01.png")))
        mTilesets += ts;
    if (Tileset *ts = loadTileset(QLatin1String("C:\\Users\\Tim\\Desktop\\ProjectZomboid\\FromLemmy\\Maps\\Tiles\\lighting_outdoor_01.png")))
        mTilesets += ts;
#endif
}

PathGeneratorMgr::~PathGeneratorMgr()
{
    TilesetManager::instance()->removeReferences(tilesets());
    TilesetManager::instance()->removeReferences(mRemovedTilesets);
}

void PathGeneratorMgr::insertGenerator(int index, PathGenerator *pgen)
{
    Q_ASSERT(pgen && !mGenerators.contains(pgen));
    pgen->refCountUp();
    mGenerators.insert(index, pgen);
}

PathGenerator *PathGeneratorMgr::removeGenerator(int index)
{
    mGenerators.at(index)->refCountDown();
    return mGenerators.takeAt(index);
}

const QList<PathGenerator *> &PathGeneratorMgr::generatorTypes() const
{
    return PathGeneratorTypes::instance()->types();
}

QString PathGeneratorMgr::tilesDirectory() const
{
    return BuildingEditor::BuildingPreferences::instance()->tilesDirectory();
}

void PathGeneratorMgr::setTilesDirectory(const QString &path)
{
    BuildingEditor::BuildingPreferences::instance()->setTilesDirectory(path);

    // Try to load any tilesets that weren't found.
    loadTilesets();
}

QString PathGeneratorMgr::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString PathGeneratorMgr::txtPath()
{
    return BuildingEditor::BuildingPreferences::instance()->configPath(txtName());
}

#define VERSION0 0
#define VERSION_LATEST VERSION0

static bool readProperty(const SimpleFileBlock &block, PathGeneratorProperty *prop,
                         QString &error)
{
    int index;
    if (prop->properties().size()) {
        if ((index = block.findBlock(prop->name())) < 0)
            return true;
        const SimpleFileBlock &block2 = block.blocks[index];
        foreach (PathGeneratorProperty *prop2, prop->properties()) {
            if (!readProperty(block2, prop2, error))
                return false;
        }
    } else {
        if ((index = block.findValue(prop->name())) < 0)
            return true;
        QString value = block.values[index].value;
        if (!prop->valueFromString(value)) {
            error = PathGeneratorMgr::instance()->tr("Error with property '%2 = %3'")
                    .arg(prop->name()).arg(value);
            return false;
        }
    }
    return true;
}

bool PathGeneratorMgr::readTxt()
{
    // Make sure the user has chosen the Tiles directory.
    QString tilesDirectory = this->tilesDirectory();
    QDir dir(tilesDirectory);
    if (!dir.exists()) {
        mError = tr("The Tiles directory specified in the preferences doesn't exist!\n%1")
                .arg(tilesDirectory);
        return false;
    }

    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

    if (!upgradeTxt())
        return false;

    if (!mergeTxt())
        return false;

    QString path = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = simple.errorString();
        return false;
    }

    if (simple.version() != VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName()).arg(VERSION_LATEST).arg(simple.version());
        return false;
    }

    mRevision = simple.value("revision").toInt();
    mSourceRevision = simple.value("source_revision").toInt();

    QStringList missingTilesets;

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("tilesets")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name == QLatin1String("tileset")) {
#if 1
                    // Just get the list of names.  Don't load the tilesets yet
                    // because the user might not have chosen the Tiles directory.
                    // The tilesets will be loaded when other code asks for them or
                    // when the Tiles directory is changed.
                    QFileInfo info(kv.value); // relative to Tiles directory
                    Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);
                    // We don't know how big the image actually is, so it only
                    // has one tile.
                    ts->loadFromImage(mMissingTile->image().toImage(),
                                      kv.value + QLatin1String(".png"));
                    ts->setMissing(true);
                    addTileset(ts);
#else
                    QString source = tilesDirectory + QLatin1Char('/') + kv.value
                            + QLatin1String(".png");
                    QFileInfo info(source);
                    if (!info.exists()) {
                        Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);
                        addTileset(ts);
                        missingTilesets += QDir::toNativeSeparators(info.absoluteFilePath());
                        continue;
                    }
                    source = info.canonicalFilePath();
                    Tileset *ts = loadTileset(source);
                    if (!ts)
                        return false;
                    addTileset(ts);
#endif
                } else {
                    mError = tr("Unknown value name '%1'.\n%2")
                            .arg(kv.name)
                            .arg(path);
                    return false;
                }
            }
        } else if (block.name == QLatin1String("Generator")) {
            QString type = block.value("type");
            if (PathGenerator *pgen = findGeneratorType(type)) {
                pgen = pgen->clone();
                pgen->setLabel(block.value("label"));
                foreach (PathGeneratorProperty *prop, pgen->properties()) {
#if 1
                    if (!readProperty(block, prop, mError))
                        return false;
#else
                    if (block.findValue(prop->name()) < 0)
                        continue;
                    QString value = block.value(prop->name());
                    if (!prop->valueFromString(value)) {
                        mError = tr("Error with '%1' property '%2 = %3'")
                                .arg(pgen->label()).arg(prop->name()).arg(value);
                        return false;
                    }
#endif
                }
                insertGenerator(mGenerators.size(), pgen);
            } else {
                mError = tr("Unknown generator type '%1'.").arg(type);
                return false;
            }
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    if (missingTilesets.size()) {
        BuildingEditor::ListOfStringsDialog dialog(tr("The following tileset files were not found."),
                                                   missingTilesets,
                                                   MainWindow::instance());
        dialog.setWindowTitle(tr("Missing Tilesets"));
        dialog.exec();
    }

    return true;
}

static void writeProperty(SimpleFileBlock &block, PathGeneratorProperty *prop)
{
    if (prop->properties().size()) {
        SimpleFileBlock block2;
        block2.name = prop->name();
        foreach (PathGeneratorProperty *prop2, prop->properties())
            writeProperty(block2, prop2);
        block.blocks += block2;
    } else {
        block.addValue(prop->name(), prop->valueToString());
    }
}

bool PathGeneratorMgr::writeTxt()
{
    SimpleFile simpleFile;

    QDir tilesDir(tilesDirectory());
    SimpleFileBlock tilesetBlock;
    tilesetBlock.name = QLatin1String("tilesets");
    foreach (Tiled::Tileset *tileset, tilesets()) {
        QString relativePath = tilesDir.relativeFilePath(tileset->imageSource());
        relativePath.truncate(relativePath.length() - 4); // remove .png
        tilesetBlock.values += SimpleFileKeyValue(QLatin1String("tileset"), relativePath);
    }
    simpleFile.blocks += tilesetBlock;

    foreach (PathGenerator *pgen, mGenerators) {
        SimpleFileBlock generatorBlock;
        generatorBlock.name = QLatin1String("Generator");
        generatorBlock.addValue("label", pgen->label());
        generatorBlock.addValue("type", pgen->type());
        foreach (PathGeneratorProperty *prop, pgen->properties())
            writeProperty(generatorBlock, prop);
        simpleFile.blocks += generatorBlock;
    }

    simpleFile.setVersion(VERSION_LATEST);
    simpleFile.replaceValue("revision", QString::number(++mRevision));
    simpleFile.replaceValue("source_revision", QString::number(mSourceRevision));
    if (!simpleFile.write(txtPath())) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

bool PathGeneratorMgr::upgradeTxt()
{
    return true;
}

bool PathGeneratorMgr::mergeTxt()
{
    return true;
}

#include <QApplication>
#include <QMessageBox>

bool PathGeneratorMgr::Startup()
{
    // Refresh the ui before blocking while loading tilesets etc
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    // Create ~/.TileZed if needed.
    QString configPath = BuildingEditor::BuildingPreferences::instance()->configPath();
    QDir dir(configPath);
    if (!dir.exists()) {
        if (!dir.mkpath(configPath)) {
            QMessageBox::critical(MainWindow::instance(), tr("It's no good, Jim!"),
                                  tr("Failed to create config directory:\n%1")
                                  .arg(QDir::toNativeSeparators(configPath)));
            return false;
        }
    }

    // Copy config files from the application directory to ~/.TileZed if they
    // don't exist there.
    QStringList configFiles;
    configFiles += PathGeneratorMgr::instance()->txtName();

    foreach (QString configFile, configFiles) {
        QString fileName = configPath + QLatin1Char('/') + configFile;
        if (!QFileInfo(fileName).exists()) {
            QString source = QCoreApplication::applicationDirPath() + QLatin1Char('/')
                    + configFile;
            if (QFileInfo(source).exists()) {
                if (!QFile::copy(source, fileName)) {
                    QMessageBox::critical(MainWindow::instance(), tr("It's no good, Jim!"),
                                          tr("Failed to copy file:\nFrom: %1\nTo: %2")
                                          .arg(source).arg(fileName));
                    return false;
                }
            }
        }
    }

    if (!PathGeneratorMgr::instance()->readTxt()) {
        QMessageBox::critical(MainWindow::instance(), tr("It's no good, Jim!"),
                              tr("Error while reading %1\n%2")
                              .arg(PathGeneratorMgr::instance()->txtName())
                              .arg(PathGeneratorMgr::instance()->errorString()));
        return false;
    }

    return true;
}

void PathGeneratorMgr::addOrReplaceTileset(Tileset *ts)
{
    if (mTilesetByName.contains(ts->name())) {
        Tileset *old = mTilesetByName[ts->name()];
        if (!mRemovedTilesets.contains(old))
            mRemovedTilesets += old;
        if (!mRemovedTilesets.contains(ts)) // always true
            TilesetManager::instance()->addReference(ts);
        mRemovedTilesets.removeAll(ts);
        mTilesetByName[ts->name()] = ts;
    } else {
        addTileset(ts);
    }
}

Tileset *PathGeneratorMgr::loadTileset(const QString &source)
{
    QFileInfo info(source);
    Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);
    if (!loadTilesetImage(ts, source)) {
        delete ts;
        return 0;
    }
    return ts;
}

bool PathGeneratorMgr::loadTilesetImage(Tileset *ts, const QString &source)
{
    TilesetImageCache *cache = TilesetManager::instance()->imageCache();
    Tileset *cached = cache->findMatch(ts, source);
    if (!cached || !ts->loadFromCache(cached)) {
        const QImage tilesetImage = QImage(source);
        if (ts->loadFromImage(tilesetImage, source))
            cache->addTileset(ts);
        else {
            mError = tr("Error loading tileset image:\n'%1'").arg(source);
            return 0;
        }
    }

    return ts;
}

void PathGeneratorMgr::addTileset(Tileset *tileset)
{
    Q_ASSERT(mTilesetByName.contains(tileset->name()) == false);
    mTilesetByName[tileset->name()] = tileset;
    if (!mRemovedTilesets.contains(tileset))
        TilesetManager::instance()->addReference(tileset);
    mRemovedTilesets.removeAll(tileset);
//    emit tilesetAdded(tileset);
}

void PathGeneratorMgr::removeTileset(Tileset *tileset)
{
    Q_ASSERT(mTilesetByName.contains(tileset->name()));
    Q_ASSERT(mRemovedTilesets.contains(tileset) == false);
//    emit tilesetAboutToBeRemoved(tileset);
    mTilesetByName.remove(tileset->name());
//    emit tilesetRemoved(tileset);

    // Don't remove references now, that will delete the tileset, and the
    // user might undo the removal.
    mRemovedTilesets += tileset;
    //    TilesetManager::instance()->removeReference(tileset);
}

PathGenerator *PathGeneratorMgr::findGeneratorType(const QString &type)
{
    return PathGeneratorTypes::instance()->type(type);
}

void PathGeneratorMgr::loadTilesets()
{
    foreach (Tileset *ts, tilesets()) {
        if (ts->isMissing()) {
            QString source = tilesDirectory() + QLatin1Char('/')
                    // This is the name that was saved in PathGenerators.txt,
                    // relative to Tiles directory, plus .png.
                    + ts->imageSource();
            QFileInfo info(source);
            if (info.exists()) {
                source = info.canonicalFilePath();
                if (loadTilesetImage(ts, source)) {
                    ts->setMissing(false); // Yay!
                }
            }
        }
    }
}
