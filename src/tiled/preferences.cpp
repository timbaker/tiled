/*
 * preferences.cpp
 * Copyright 2009-2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#include "preferences.h"

#include "documentmanager.h"
#include "languagemanager.h"
#include "tilesetmanager.h"
#ifdef ZOMBOID
#include "zprogress.h"
#endif

#ifdef ZOMBOID
#include <QCoreApplication>
#include <QDir>
#endif
#include <QDesktopServices>
#include <QFileInfo>
#include <QSettings>

using namespace Tiled;
using namespace Tiled::Internal;

Preferences *Preferences::mInstance = 0;

Preferences *Preferences::instance()
{
    if (!mInstance)
        mInstance = new Preferences;
    return mInstance;
}

void Preferences::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

Preferences::Preferences()
    : mSettings(new QSettings)
{
    // Retrieve storage settings
    mSettings->beginGroup(QLatin1String("Storage"));
    mLayerDataFormat = (MapWriter::LayerDataFormat)
                       mSettings->value(QLatin1String("LayerDataFormat"),
                                        MapWriter::Base64Zlib).toInt();
    mDtdEnabled = mSettings->value(QLatin1String("DtdEnabled")).toBool();
    mReloadTilesetsOnChange =
            mSettings->value(QLatin1String("ReloadTilesets"), true).toBool();
    mSettings->endGroup();

    // Retrieve interface settings
    mSettings->beginGroup(QLatin1String("Interface"));
    mShowGrid = mSettings->value(QLatin1String("ShowGrid"), false).toBool();
    mSnapToGrid = mSettings->value(QLatin1String("SnapToGrid"),
                                   false).toBool();
    mGridColor = QColor(mSettings->value(QLatin1String("GridColor"),
                                  QColor(Qt::black).name()).toString());
    mHighlightCurrentLayer = mSettings->value(QLatin1String("HighlightCurrentLayer"),
                                              false).toBool();
    mShowTilesetGrid = mSettings->value(QLatin1String("ShowTilesetGrid"),
                                        true).toBool();
    mLanguage = mSettings->value(QLatin1String("Language"),
                                 QString()).toString();
    mUseOpenGL = mSettings->value(QLatin1String("OpenGL"), false).toBool();
#ifdef ZOMBOID
    mAutoSwitchLayer = mSettings->value(QLatin1String("AutoSwitchLayer"), true).toBool();
    mTilesetScale = mSettings->value(QLatin1String("TilesetScale"), 1.0).toReal();
    mSortTilesets = mSettings->value(QLatin1String("SortTilesets"), false).toBool();
    mShowMiniMap = mSettings->value(QLatin1String("ShowMiniMap"), true).toBool();
    mMiniMapWidth = mSettings->value(QLatin1String("MiniMapWidth"), 256).toInt();
    mShowTileLayersPanel = mSettings->value(QLatin1String("ShowTileLayersPanel"), true).toBool();
    mBackgroundColor = QColor(mSettings->value(QLatin1String("BackgroundColor"),
                                               QColor(Qt::darkGray).name()).toString());
    mShowAdjacentMaps = mSettings->value(QLatin1String("ShowAdjacentMaps"), true).toBool();
    mHighlightRoomUnderPointer = mSettings->value(QLatin1String("HighlightRoomUnderPointer"), false).toBool();
#endif
    mSettings->endGroup();
#ifdef ZOMBOID
    mEraserBrushSize = mSettings->value(QLatin1String("Tools/Eraser/BrushSize"), 1).toInt();
#endif

    // Retrieve defined object types
    mSettings->beginGroup(QLatin1String("ObjectTypes"));
    const QStringList names =
            mSettings->value(QLatin1String("Names")).toStringList();
    const QStringList colors =
            mSettings->value(QLatin1String("Colors")).toStringList();
    mSettings->endGroup();

    const int count = qMin(names.size(), colors.size());
    for (int i = 0; i < count; ++i)
        mObjectTypes.append(ObjectType(names.at(i), QColor(colors.at(i))));

    mSettings->beginGroup(QLatin1String("Automapping"));
    mAutoMapDrawing = mSettings->value(QLatin1String("WhileDrawing"),
                                       false).toBool();
    mSettings->endGroup();

#ifdef ZOMBOID
    QString KEY_TILES_DIR = QLatin1String("BuildingEditor/TilesDirectory");
    QString tilesDirectory = mSettings->value(KEY_TILES_DIR).toString();
    if (tilesDirectory.isEmpty() || !QDir(tilesDirectory).exists()) {
        tilesDirectory = QCoreApplication::applicationDirPath() +
                QLatin1Char('/') + QLatin1String("../Tiles");
        if (!QDir(tilesDirectory).exists())
            tilesDirectory = QCoreApplication::applicationDirPath() +
                    QLatin1Char('/') + QLatin1String("../../Tiles");
    }
    if (tilesDirectory.length())
        tilesDirectory = QDir::cleanPath(tilesDirectory);
    if (!QDir(tilesDirectory).exists())
        tilesDirectory.clear();
    mSettings->beginGroup(QLatin1String("Tilesets"));
    mTilesDirectory = mSettings->value(QLatin1String("TilesDirectory"),
                                       tilesDirectory).toString();
    mSettings->endGroup();
    if (tilesDirectory.length()) {
        mSettings->setValue(QLatin1String("Tilesets/TilesDirectory"), mTilesDirectory);
        mSettings->remove(KEY_TILES_DIR);
    }

    mSettings->beginGroup(QLatin1String("MapsDirectory"));
    mMapsDirectory = mSettings->value(QLatin1String("Current"), QString()).toString();
    mSettings->endGroup();

    QString configPath = QDir::homePath() + QLatin1Char('/') + QLatin1String(".TileZed");
    mConfigDirectory = mSettings->value(QLatin1String("ConfigDirectory"),
                                        configPath).toString();

    mWorldEdFiles = mSettings->value(QLatin1String("WorldEd/ProjectFile")).toStringList();
#endif

    TilesetManager *tilesetManager = TilesetManager::instance();
    tilesetManager->setReloadTilesetsOnChange(mReloadTilesetsOnChange);
}

Preferences::~Preferences()
{
    delete mSettings;
}

void Preferences::setShowGrid(bool showGrid)
{
    if (mShowGrid == showGrid)
        return;

    mShowGrid = showGrid;
    mSettings->setValue(QLatin1String("Interface/ShowGrid"), mShowGrid);
    emit showGridChanged(mShowGrid);
}

void Preferences::setSnapToGrid(bool snapToGrid)
{
    if (mSnapToGrid == snapToGrid)
        return;

    mSnapToGrid = snapToGrid;
    mSettings->setValue(QLatin1String("Interface/SnapToGrid"), mSnapToGrid);
    emit snapToGridChanged(mSnapToGrid);
}

void Preferences::setGridColor(QColor gridColor)
{
    if (mGridColor == gridColor)
        return;

    mGridColor = gridColor;
    mSettings->setValue(QLatin1String("Interface/GridColor"), mGridColor.name());
    emit gridColorChanged(mGridColor);
}

void Preferences::setHighlightCurrentLayer(bool highlight)
{
    if (mHighlightCurrentLayer == highlight)
        return;

    mHighlightCurrentLayer = highlight;
    mSettings->setValue(QLatin1String("Interface/HighlightCurrentLayer"),
                        mHighlightCurrentLayer);
    emit highlightCurrentLayerChanged(mHighlightCurrentLayer);
}

void Preferences::setShowTilesetGrid(bool showTilesetGrid)
{
    if (mShowTilesetGrid == showTilesetGrid)
        return;

    mShowTilesetGrid = showTilesetGrid;
    mSettings->setValue(QLatin1String("Interface/ShowTilesetGrid"),
                        mShowTilesetGrid);
    emit showTilesetGridChanged(mShowTilesetGrid);
}

MapWriter::LayerDataFormat Preferences::layerDataFormat() const
{
    return mLayerDataFormat;
}

void Preferences::setLayerDataFormat(MapWriter::LayerDataFormat
                                     layerDataFormat)
{
    if (mLayerDataFormat == layerDataFormat)
        return;

    mLayerDataFormat = layerDataFormat;
    mSettings->setValue(QLatin1String("Storage/LayerDataFormat"),
                        mLayerDataFormat);
}

bool Preferences::dtdEnabled() const
{
    return mDtdEnabled;
}

void Preferences::setDtdEnabled(bool enabled)
{
    mDtdEnabled = enabled;
    mSettings->setValue(QLatin1String("Storage/DtdEnabled"), enabled);
}

QString Preferences::language() const
{
    return mLanguage;
}

void Preferences::setLanguage(const QString &language)
{
    if (mLanguage == language)
        return;

    mLanguage = language;
    mSettings->setValue(QLatin1String("Interface/Language"),
                        mLanguage);

    LanguageManager::instance()->installTranslators();
}

bool Preferences::reloadTilesetsOnChange() const
{
    return mReloadTilesetsOnChange;
}

void Preferences::setReloadTilesetsOnChanged(bool value)
{
    if (mReloadTilesetsOnChange == value)
        return;

    mReloadTilesetsOnChange = value;
    mSettings->setValue(QLatin1String("Storage/ReloadTilesets"),
                        mReloadTilesetsOnChange);

    TilesetManager *tilesetManager = TilesetManager::instance();
    tilesetManager->setReloadTilesetsOnChange(mReloadTilesetsOnChange);
}

void Preferences::setUseOpenGL(bool useOpenGL)
{
    if (mUseOpenGL == useOpenGL)
        return;

    mUseOpenGL = useOpenGL;
    mSettings->setValue(QLatin1String("Interface/OpenGL"), mUseOpenGL);

    emit useOpenGLChanged(mUseOpenGL);
}

void Preferences::setObjectTypes(const ObjectTypes &objectTypes)
{
    mObjectTypes = objectTypes;

    QStringList names;
    QStringList colors;
    foreach (const ObjectType &objectType, objectTypes) {
        names.append(objectType.name);
        colors.append(objectType.color.name());
    }

    mSettings->beginGroup(QLatin1String("ObjectTypes"));
    mSettings->setValue(QLatin1String("Names"), names);
    mSettings->setValue(QLatin1String("Colors"), colors);
    mSettings->endGroup();

    emit objectTypesChanged();
}

static QString lastPathKey(Preferences::FileType fileType)
{
    QString key = QLatin1String("LastPaths/");

    switch (fileType) {
    case Preferences::ObjectTypesFile:
        key.append(QLatin1String("ObjectTypes"));
        break;
    case Preferences::ImageFile:
        key.append(QLatin1String("Images"));
        break;
    case Preferences::ExportedFile:
        key.append(QLatin1String("ExportedFile"));
        break;
    default:
        Q_ASSERT(false); // Getting here means invalid file type
    }

    return key;
}

/**
 * Returns the last location of a file chooser for the given file type. As long
 * as it was set using setLastPath().
 *
 * When no last path for this file type exists yet, the path of the currently
 * selected map is returned.
 *
 * When no map is open, the user's 'Documents' folder is returned.
 */
QString Preferences::lastPath(FileType fileType) const
{
    QString path = mSettings->value(lastPathKey(fileType)).toString();

    if (path.isEmpty()) {
        DocumentManager *documentManager = DocumentManager::instance();
        MapDocument *mapDocument = documentManager->currentDocument();
        if (mapDocument)
            path = QFileInfo(mapDocument->fileName()).path();
    }

    if (path.isEmpty())
#if QT_VERSION >= 0x050000
        path = QStandardPaths::writableLocation(
                    QStandardPaths::DocumentsLocation);
#else
        path = QDesktopServices::storageLocation(
                    QDesktopServices::DocumentsLocation);
#endif

    return path;
}

/**
 * \see lastPath()
 */
void Preferences::setLastPath(FileType fileType, const QString &path)
{
    mSettings->setValue(lastPathKey(fileType), path);
}

void Preferences::setAutomappingDrawing(bool enabled)
{
    mAutoMapDrawing = enabled;
    mSettings->setValue(QLatin1String("Automapping/WhileDrawing"), enabled);
}

#ifdef ZOMBOID
QString Preferences::configPath() const
{
    return mConfigDirectory;
}

QString Preferences::configPath(const QString &fileName) const
{
    return configPath() + QLatin1Char('/') + fileName;
}

QString Preferences::appConfigPath() const
{
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath();
#elif defined(Q_OS_UNIX)
    return QCoreApplication::applicationDirPath() + QLatin1String("/../share/tilezed/config");
#elif defined(Q_OS_MAC)
    return QCoreApplication::applicationDirPath() + QLatin1String("/../Config");
#else
#error "wtf system is this???"
#endif
}

QString Preferences::appConfigPath(const QString &fileName) const
{
    return appConfigPath() + QLatin1Char('/') + fileName;
}

QString Preferences::docsPath() const
{
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath() + QLatin1String("/docs");
#elif defined(Q_OS_UNIX)
    return QCoreApplication::applicationDirPath() + QLatin1String("/../share/tilezed/docs");
#elif defined(Q_OS_MAC)
    return QCoreApplication::applicationDirPath() + QLatin1String("/../Docs");
#else
#error "wtf system is this???"
#endif
}

QString Preferences::docsPath(const QString &fileName) const
{
    return docsPath() + QLatin1Char('/') + fileName;
}

QString Preferences::luaPath() const
{
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath() + QLatin1String("/lua");
#elif defined(Q_OS_UNIX)
    return QCoreApplication::applicationDirPath() + QLatin1String("/../share/tilezed/lua");
#elif defined(Q_OS_MAC)
    return QCoreApplication::applicationDirPath() + QLatin1String("/../Lua");
#else
#error "wtf system is this???"
#endif
}

QString Preferences::luaPath(const QString &fileName) const
{
    return luaPath() + QLatin1Char('/') + fileName;
}

QString Preferences::mapsDirectory() const
{
    return mMapsDirectory;
}

void Preferences::setMapsDirectory(const QString &path)
{
    if (mMapsDirectory == path)
        return;
    mMapsDirectory = path;
    mSettings->setValue(QLatin1String("MapsDirectory/Current"), path);
#if 0
    // Put this up, otherwise the progress dialog shows and hides for each lot.
    // Since each open document has its own ZLotManager, this shows and hides for each document as well.
    PROGRESS progress(tr("Checking lots..."));
#endif
    emit mapsDirectoryChanged();
}

bool Preferences::autoSwitchLayer() const
{
    return mAutoSwitchLayer;
}

void Preferences::setAutoSwitchLayer(bool enabled)
{
    if (mAutoSwitchLayer == enabled)
        return;
    mAutoSwitchLayer = enabled;
    mSettings->setValue(QLatin1String("Interface/AutoSwitchLayer"), enabled);
    emit autoSwitchLayerChanged(mAutoSwitchLayer);
}

QString Preferences::tilesDirectory() const
{
    return mTilesDirectory;
}

QString Preferences::texturesDirectory() const
{
    return mTilesDirectory + QLatin1String("/Textures");
}

qreal Preferences::tilesetScale() const
{
    return mTilesetScale;
}

bool Preferences::sortTilesets() const
{
    return mSortTilesets;
}

void Preferences::setTilesDirectory(const QString &path)
{
    mTilesDirectory = path;
    mSettings->setValue(QLatin1String("Tilesets/TilesDirectory"), mTilesDirectory);
    emit tilesDirectoryChanged();
}

void Preferences::setTilesetScale(qreal scale)
{
    if (mTilesetScale == scale)
        return;
    mTilesetScale = scale;
    mSettings->setValue(QLatin1String("Interface/TilesetScale"), scale);
    emit tilesetScaleChanged(mTilesetScale);
}

void Preferences::setSortTilesets(bool sort)
{
    if (mSortTilesets == sort)
        return;
    mSortTilesets = sort;
    mSettings->setValue(QLatin1String("Interface/SortTilesets"), sort);
    emit sortTilesetsChanged(mSortTilesets);
}

bool Preferences::showMiniMap() const
{
    return mShowMiniMap;
}

void Preferences::setMiniMapWidth(int width)
{
    width = qMin(width, MINIMAP_MAX_WIDTH);
    width = qMax(width, MINIMAP_MIN_WIDTH);

    if (mMiniMapWidth == width)
        return;
    mMiniMapWidth = width;
    mSettings->setValue(QLatin1String("Interface/MiniMapWidth"), width);
    emit miniMapWidthChanged(mMiniMapWidth);
}

int Preferences::miniMapWidth() const
{
    return mMiniMapWidth;
}

void Preferences::setShowMiniMap(bool show)
{
    if (mShowMiniMap == show)
        return;
    mShowMiniMap = show;
    mSettings->setValue(QLatin1String("Interface/ShowMiniMap"), show);
    emit showMiniMapChanged(mShowMiniMap);
}

void Preferences::setShowTileLayersPanel(bool show)
{
    if (mShowTileLayersPanel == show)
        return;
    mShowTileLayersPanel = show;
    mSettings->setValue(QLatin1String("Interface/ShowTileLayersPanel"), show);
    emit showTileLayersPanelChanged(mShowTileLayersPanel);
}

void Preferences::setBackgroundColor(const QColor &bgColor)
{
    if (mBackgroundColor == bgColor)
        return;

    mBackgroundColor = bgColor;
    mSettings->setValue(QLatin1String("Interface/BackgroundColor"), mBackgroundColor.name());
    emit backgroundColorChanged(mBackgroundColor);
}

void Preferences::setShowAdjacentMaps(bool show)
{
    if (mShowAdjacentMaps == show)
        return;
    mShowAdjacentMaps = show;
    mSettings->setValue(QLatin1String("Interface/ShowAdjacentMaps"), show);
    emit showAdjacentMapsChanged(mShowAdjacentMaps);
}

void Preferences::setWorldEdFiles(const QStringList &fileNames)
{
    if (mWorldEdFiles == fileNames)
        return;
    mWorldEdFiles = fileNames;
    mSettings->setValue(QLatin1String("WorldEd/ProjectFile"), mWorldEdFiles);
    emit worldEdFilesChanged(mWorldEdFiles);
}

void Preferences::setHighlightRoomUnderPointer(bool highlight)
{
    if (mHighlightRoomUnderPointer == highlight)
        return;
    mHighlightRoomUnderPointer = highlight;
    mSettings->setValue(QLatin1String("Interface/HighlightRoomUnderPointer"), highlight);
    emit highlightRoomUnderPointerChanged(mHighlightRoomUnderPointer);
}

void Preferences::setEraserBrushSize(int newSize)
{
    if (mEraserBrushSize == newSize)
        return;
    mEraserBrushSize = newSize;
    mSettings->setValue(QLatin1String("Tools/Eraser/BrushSize"), mEraserBrushSize);
    emit eraserBrushSizeChanged(mEraserBrushSize);
}
#endif // ZOMBOID
