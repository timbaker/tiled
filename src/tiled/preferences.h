/*
 * preferences.h
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

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QObject>
#include <QColor>

#include "mapwriter.h"
#include "objecttypes.h"

class QSettings;

namespace Tiled {
namespace Internal {

/**
 * This class holds user preferences and provides a convenient interface to
 * access them.
 */
class Preferences : public QObject
{
    Q_OBJECT

public:
    static Preferences *instance();
    static void deleteInstance();

    bool showGrid() const { return mShowGrid; }
    bool snapToGrid() const { return mSnapToGrid; }
    QColor gridColor() const { return mGridColor; }

    bool highlightCurrentLayer() const { return mHighlightCurrentLayer; }
    bool showTilesetGrid() const { return mShowTilesetGrid; }

    MapWriter::LayerDataFormat layerDataFormat() const;
    void setLayerDataFormat(MapWriter::LayerDataFormat layerDataFormat);

    bool dtdEnabled() const;
    void setDtdEnabled(bool enabled);

    QString language() const;
    void setLanguage(const QString &language);

    bool reloadTilesetsOnChange() const;
    void setReloadTilesetsOnChanged(bool value);

    bool useOpenGL() const { return mUseOpenGL; }
    void setUseOpenGL(bool useOpenGL);

    const ObjectTypes &objectTypes() const { return mObjectTypes; }
    void setObjectTypes(const ObjectTypes &objectTypes);

    enum FileType {
        ObjectTypesFile,
        ImageFile,
        ExportedFile
    };

    QString lastPath(FileType fileType) const;
    void setLastPath(FileType fileType, const QString &path);

    bool automappingDrawing() const { return mAutoMapDrawing; }
    void setAutomappingDrawing(bool enabled);

#ifdef ZOMBOID
    QString userPath() const;
    QString userPath(const QString &fileName) const;

    QString configPath() const;
    QString configPath(const QString &fileName) const;

    QString appConfigPath() const;
    QString appConfigPath(const QString &fileName) const;

    QString docsPath() const;
    QString docsPath(const QString &fileName) const;

    QString luaPath() const;
    QString luaPath(const QString &fileName) const;

    QString mapsDirectory() const;
    void setMapsDirectory(const QString &path);

    bool autoSwitchLayer() const;
    void setAutoSwitchLayer(bool enable);

    QString tilesDirectory() const;
    QString tiles2xDirectory() const;

    qreal tilesetScale() const;

    bool sortTilesets() const;

    bool showMiniMap() const;
#define MINIMAP_MIN_WIDTH 128
#define MINIMAP_MAX_WIDTH 512
    void setMiniMapWidth(int width);
    int miniMapWidth() const;

    bool showTileLayersPanel() const
    { return mShowTileLayersPanel; }

    bool showTileSelection() const
    { return mShowTileSelection; }

    bool showInvisibleTiles() const
    { return mShowInvisibleTiles; }

    QColor backgroundColor() const
    { return mBackgroundColor; }

    bool showAdjacentMaps() const
    { return mShowAdjacentMaps; }

    QStringList worldedFiles() const
    { return mWorldEdFiles; }

    QStringList tilePropertiesFiles() const
    { return mTilePropertiesFiles; }

    bool highlightRoomUnderPointer() const
    { return mHighlightRoomUnderPointer; }

    bool showLotFloorsOnly() const
    { return mShowLotFloorsOnly; }

    int eraserBrushSize() const
    { return mEraserBrushSize; }

    QColor tilesetBackgroundColor() const
    { return mTilesetBackgroundColor; }

    QString thumbnailsDirectory() const
    { return mThumbnailsDirectory; }

    bool showCellBorder() const
    { return mShowCellBorder; }

    QString theme() const
    { return mTheme; }

    void applyTheme() const;

#endif // ZOMBOID

    /**
     * Provides access to the QSettings instance to allow storing/retrieving
     * arbitrary values. The naming style for groups and keys is CamelCase.
     */
    QSettings *settings() const { return mSettings; }

public slots:
    void setShowGrid(bool showGrid);
    void setSnapToGrid(bool snapToGrid);
    void setGridColor(QColor gridColor);
    void setHighlightCurrentLayer(bool highlight);
    void setShowTilesetGrid(bool showTilesetGrid);
#ifdef ZOMBOID
    void setTilesDirectory(const QString &path);
    void setTilesetScale(qreal scale);
    void setSortTilesets(bool sort);
    void setShowLotFloorsOnly(bool show);
    void setShowMiniMap(bool show);
    void setShowTileLayersPanel(bool show);
    void setShowTileSelection(bool show);
    void setShowInvisibleTiles(bool show);
    void setBackgroundColor(const QColor &bgColor);
    void setShowAdjacentMaps(bool show);
    void setWorldEdFiles(const QStringList &fileNames);
    void setTilePropertiesFiles(const QStringList &fileNames);
    void setHighlightRoomUnderPointer(bool highlight);
    void setEraserBrushSize(int newSize);
    void setTilesetBackgroundColor(const QColor& color);
    void setThumbnailsDirectory(const QString &path);
    void setShowCellBorder(bool show);
    void setTheme(const QString &theme);
#endif

signals:
    void showGridChanged(bool showGrid);
    void snapToGridChanged(bool snapToGrid);
    void gridColorChanged(QColor gridColor);
    void highlightCurrentLayerChanged(bool highlight);
    void showTilesetGridChanged(bool showTilesetGrid);

    void useOpenGLChanged(bool useOpenGL);

    void objectTypesChanged();

#ifdef ZOMBOID
    void mapsDirectoryChanged();
    void autoSwitchLayerChanged(bool enabled);
    void tilesDirectoryChanged();
    void tilesetScaleChanged(qreal scale);
    void sortTilesetsChanged(bool sort);
    void showLotFloorsOnlyChanged(bool show);
    void showMiniMapChanged(bool show);
    void miniMapWidthChanged(int width);
    void showTileLayersPanelChanged(bool show);
    void showTileSelectionChanged(bool show);
    void showInvisibleTilesChanged(bool show);
    void backgroundColorChanged(const QColor &color);
    void showAdjacentMapsChanged(bool show);
    void worldEdFilesChanged(const QStringList &fileNames);
    void tilePropertiesFilesChanged(const QStringList &fileNames);
    void highlightRoomUnderPointerChanged(bool highlight);
    void eraserBrushSizeChanged(int newSize);
    void tilesetBackgroundColorChanged(const QColor &color);
    void thumbnailsDirectoryChanged(const QString &dir);
    void showCellBorderChanged(bool show);
#endif

private:
    Preferences();
    ~Preferences();

    QSettings *mSettings;

    bool mShowGrid;
    bool mSnapToGrid;
    QColor mGridColor;
    bool mHighlightCurrentLayer;
    bool mShowTilesetGrid;

    MapWriter::LayerDataFormat mLayerDataFormat;
    bool mDtdEnabled;
    QString mLanguage;
    bool mReloadTilesetsOnChange;
    bool mUseOpenGL;
    ObjectTypes mObjectTypes;

    bool mAutoMapDrawing;

#ifdef ZOMBOID
    QString mMapsDirectory;
    QString mConfigDirectory;
    bool mAutoSwitchLayer;
    QString mTilesDirectory;
    qreal mTilesetScale;
    bool mShowLotFloorsOnly = false;
    bool mSortTilesets;
    bool mShowMiniMap;
    int mMiniMapWidth;
    bool mShowTileLayersPanel;
    QColor mBackgroundColor;
    bool mShowAdjacentMaps;
    QStringList mWorldEdFiles;
    QStringList mTilePropertiesFiles;
    bool mHighlightRoomUnderPointer;
    int mEraserBrushSize;
    QColor mTilesetBackgroundColor;
    QString mThumbnailsDirectory;
    bool mShowTileSelection;
    bool mShowInvisibleTiles;
    bool mShowCellBorder;
    QString mTheme;
#endif

    static Preferences *mInstance;
};

} // namespace Internal
} // namespace Tiled

#endif // PREFERENCES_H
