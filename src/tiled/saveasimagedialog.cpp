/*
 * saveasimagedialog.cpp
 * Copyright 2009-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#include "saveasimagedialog.h"
#include "ui_saveasimagedialog.h"

#include "map.h"
#ifdef ZOMBOID
#include "mapcomposite.h"
#endif
#include "mapdocument.h"
#include "mapobjectitem.h"
#include "maprenderer.h"
#include "imagelayer.h"
#include "objectgroup.h"
#include "preferences.h"
#include "tilelayer.h"
#include "utils.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QImageWriter>
#ifdef ZOMBOID
#include <QPushButton>
#endif
#include <QSettings>

static const char * const VISIBLE_ONLY_KEY = "SaveAsImage/VisibleLayersOnly";
static const char * const CURRENT_SCALE_KEY = "SaveAsImage/CurrentScale";
static const char * const DRAW_GRID_KEY = "SaveAsImage/DrawGrid";
#ifdef ZOMBOID
static const char * const IMAGES_FOLDER_KEY = "SaveAsImage/Directory";
static const char * const OBJECT_LAYERS_KEY = "SaveAsImage/ObjectLayers";
static const char * const NORENDER_KEY = "SaveAsImage/NoRender";
static const char * const LOTS_KEY = "SaveAsImage/Lots";
static const char * const IMAGE_WIDTH_KEY = "SaveAsImage/ImageWidth";
#endif

using namespace Tiled;
using namespace Tiled::Internal;

QString SaveAsImageDialog::mPath;

SaveAsImageDialog::SaveAsImageDialog(MapDocument *mapDocument,
                                     const QString &fileName,
                                     qreal currentScale,
                                     QWidget *parent)
    : QDialog(parent)
    , mUi(new Ui::SaveAsImageDialog)
    , mMapDocument(mapDocument)
    , mCurrentScale(currentScale)
{
    mUi->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

#ifdef ZOMBOID
    QSettings *s = Preferences::instance()->settings();
    QString suggestion = s->value(QLatin1String(IMAGES_FOLDER_KEY)).toString();
#else
    // Default to the last chosen location
    QString suggestion = mPath;
#endif

    // Suggest a nice name for the image
    if (!fileName.isEmpty()) {
        QFileInfo fileInfo(fileName);
        const QString path = fileInfo.path();
        const QString baseName = fileInfo.completeBaseName();

        if (suggestion.isEmpty())
            suggestion = path;

        suggestion += QLatin1Char('/');
        suggestion += baseName;
        suggestion += QLatin1String(".png");
    } else {
        suggestion += QLatin1Char('/');
        suggestion += QLatin1String("map.png");
    }

    mUi->fileNameEdit->setText(suggestion);

    // Restore previously used settings
#ifndef ZOMBOID
    QSettings *s = Preferences::instance()->settings();
#endif
    const bool visibleLayersOnly =
            s->value(QLatin1String(VISIBLE_ONLY_KEY), true).toBool();
    const bool useCurrentScale =
            s->value(QLatin1String(CURRENT_SCALE_KEY), true).toBool();
    const bool drawTileGrid =
            s->value(QLatin1String(DRAW_GRID_KEY), false).toBool();

    mUi->visibleLayersOnly->setChecked(visibleLayersOnly);
    mUi->currentZoomLevel->setChecked(useCurrentScale);
    mUi->drawTileGrid->setChecked(drawTileGrid);
#ifdef ZOMBOID
    MapRenderer *renderer = mMapDocument->renderer();
    QSize mapSize = mMapDocument->mapComposite()->boundingRect(renderer).size().toSize() * mCurrentScale;
    QString imageSizeString = tr("(%1 x %2)").arg(mapSize.width()).arg(mapSize.height());
    mUi->imageSizeLabel->setText(imageSizeString);

    const bool drawObjectLayers =
            s->value(QLatin1String(OBJECT_LAYERS_KEY), false).toBool();
    mUi->drawObjectLayers->setChecked(drawObjectLayers);

    const bool drawNoRender =
            s->value(QLatin1String(NORENDER_KEY), false).toBool();
    mUi->drawNoRender->setChecked(drawNoRender);

    const bool drawLots =
            s->value(QLatin1String(LOTS_KEY), true).toBool();
    mUi->drawLots->setChecked(drawLots);

    const int customImageWidth =
            s->value(QLatin1String(IMAGE_WIDTH_KEY), 512).toInt();
    mUi->imageWidthSpinBox->setValue(customImageWidth);

    mUi->imageWidthRadio->setChecked(!useCurrentScale);
    mUi->imageWidthSpinBox->setEnabled(!useCurrentScale);
    connect(mUi->currentZoomLevel, SIGNAL(toggled(bool)), mUi->imageWidthSpinBox, SLOT(setDisabled(bool)));
#endif

    connect(mUi->browseButton, SIGNAL(clicked()), SLOT(browse()));
    connect(mUi->fileNameEdit, SIGNAL(textChanged(QString)),
            this, SLOT(updateAcceptEnabled()));
}

SaveAsImageDialog::~SaveAsImageDialog()
{
    delete mUi;
}

void SaveAsImageDialog::accept()
{
    const QString fileName = mUi->fileNameEdit->text();
    if (fileName.isEmpty())
        return;

    if (QFile::exists(fileName)) {
        const QMessageBox::StandardButton button =
                QMessageBox::warning(this,
                                     tr("Save as Image"),
                                     tr("%1 already exists.\n"
                                        "Do you want to replace it?")
                                     .arg(QFileInfo(fileName).fileName()),
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);

        if (button != QMessageBox::Yes)
            return;
    }

    const bool visibleLayersOnly = mUi->visibleLayersOnly->isChecked();
    const bool useCurrentScale = mUi->currentZoomLevel->isChecked();
    const bool drawTileGrid = mUi->drawTileGrid->isChecked();

#ifdef ZOMBOID
    const bool drawObjectLayers = mUi->drawObjectLayers->isChecked();
    const bool drawNoRender = mUi->drawNoRender->isChecked();
    const bool drawLots = mUi->drawLots->isChecked();
    const int customImageWidth = mUi->imageWidthSpinBox->value();

    MapComposite *mapComposite = mMapDocument->mapComposite();
    mapComposite->saveVisibility();
    if (!drawLots) {
        foreach (MapComposite *lot, mapComposite->subMaps())
            lot->setVisible(false);
    }
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups()) {
        if (!visibleLayersOnly || !drawNoRender) {
            foreach (TileLayer *tl, layerGroup->layers()) {
                bool isVisible = !visibleLayersOnly ||
                        (layerGroup->isVisible() && layerGroup->isLayerVisible(tl));
                if (!drawNoRender && tl->name().contains(QLatin1String("NoRender")))
                    isVisible = false;
                layerGroup->setLayerVisibility(tl, isVisible);
            }
        }
        layerGroup->synch();
    }

    MapRenderer *renderer = mMapDocument->renderer();

    // Don't draw empty levels
    int savedMaxLevel = renderer->maxLevel();
    int maxLevel = 0;
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups()) {
        if (!layerGroup->bounds().isEmpty())
            maxLevel = layerGroup->level();
    }
    renderer->setMaxLevel(maxLevel);

    QRectF sceneRect = mapComposite->boundingRect(renderer);
    QSize mapSize = sceneRect.size().toSize();

    qreal scale = mCurrentScale;
    if (!useCurrentScale)
        scale = customImageWidth / qreal(mapSize.width());
    mapSize *= scale;

    QImage image(mapSize, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);

    if (scale != qreal(1)) {
        painter.setRenderHints(QPainter::SmoothPixmapTransform |
                               QPainter::HighQualityAntialiasing);
        painter.setTransform(QTransform::fromScale(scale,
                                                   scale));
    }

    painter.translate(-sceneRect.left(), -sceneRect.top());

    MapComposite::ZOrderList zorder = mapComposite->zOrder();
    foreach (MapComposite::ZOrderItem zo, zorder) {
        if (zo.group) {
            if (visibleLayersOnly && !zo.group->isVisible())
                continue;
            renderer->drawTileLayerGroup(&painter, zo.group);
        } else if (TileLayer *tl = zo.layer->asTileLayer()) {
            if (visibleLayersOnly && !tl->isVisible())
                continue;
            if (tl->name().contains(QLatin1String("NoRender")))
                continue;
            renderer->drawTileLayer(&painter, tl);
        } else if (ObjectGroup *objGroup = zo.layer->asObjectGroup()) {
            if (!drawObjectLayers)
                continue;
            if (visibleLayersOnly && !objGroup->isVisible())
                continue;
            foreach (const MapObject *object, objGroup->objects()) {
                const QColor color = MapObjectItem::objectColor(object);
                renderer->drawMapObject(&painter, object, color);
            }
        } else if (ImageLayer *imageLayer = zo.layer->asImageLayer()) {
            if (visibleLayersOnly && !imageLayer->isVisible())
                continue;
            renderer->drawImageLayer(&painter, imageLayer);
        }
    }

    mapComposite->restoreVisibility();
    renderer->setMaxLevel(savedMaxLevel);
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups())
        layerGroup->synch();

#else // !ZOMBOID
    MapRenderer *renderer = mMapDocument->renderer();
    QSize mapSize = renderer->mapSize();

    if (useCurrentScale)
        mapSize *= mCurrentScale;

    QImage image(mapSize, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);

    if (useCurrentScale && mCurrentScale != qreal(1)) {
        painter.setRenderHints(QPainter::SmoothPixmapTransform |
                               QPainter::HighQualityAntialiasing);
        painter.setTransform(QTransform::fromScale(mCurrentScale,
                                                   mCurrentScale));
    }

    foreach (const Layer *layer, mMapDocument->map()->layers()) {
        if (visibleLayersOnly && !layer->isVisible())
            continue;

        painter.setOpacity(layer->opacity());

        const TileLayer *tileLayer = dynamic_cast<const TileLayer*>(layer);
        const ObjectGroup *objGroup = dynamic_cast<const ObjectGroup*>(layer);
        const ImageLayer *imageLayer = dynamic_cast<const ImageLayer*>(layer);

        if (tileLayer) {
            renderer->drawTileLayer(&painter, tileLayer);
        } else if (objGroup) {
            foreach (const MapObject *object, objGroup->objects()) {
                const QColor color = MapObjectItem::objectColor(object);
                renderer->drawMapObject(&painter, object, color);
            }
        } else if (imageLayer) {
            renderer->drawImageLayer(&painter, imageLayer);
        }
    }
#endif // !ZOMBOID
    if (drawTileGrid) {
        Preferences *prefs = Preferences::instance();
        renderer->drawGrid(&painter, QRectF(QPointF(), renderer->mapSize()),
                           prefs->gridColor());
    }

#ifdef ZOMBOID
    QFileInfo fileInfo(fileName);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        QString path = dir.cleanPath(dir.absolutePath());
        QMessageBox::StandardButton b = QMessageBox::question(this,
            tr("Directory not found"),
            tr("The images directory does not exist.\n%1\n\nCreate directory?")
            .arg(dir.path()), QMessageBox::Ok | QMessageBox::Cancel);
        if (b != QMessageBox::Ok)
            return;
        if (!dir.mkpath(path)) {
            QMessageBox::warning(this, tr("Error creating directory"),
                                 tr("Failed to create the images directory!\n%1")
                                 .arg(path));
            return;
        }
    }
    if (!image.save(fileName)) {
        QMessageBox::warning(this, tr("Error Saving Image"),
                             tr("Error saving map image!"));
    }
#else
    image.save(fileName);
#endif
    mPath = QFileInfo(fileName).path();

    // Store settings for next time
    QSettings *s = Preferences::instance()->settings();
    s->setValue(QLatin1String(VISIBLE_ONLY_KEY), visibleLayersOnly);
    s->setValue(QLatin1String(CURRENT_SCALE_KEY), useCurrentScale);
    s->setValue(QLatin1String(DRAW_GRID_KEY), drawTileGrid);
#ifdef ZOMBOID
    s->setValue(QLatin1String(IMAGES_FOLDER_KEY), mPath);
    s->setValue(QLatin1String(OBJECT_LAYERS_KEY), drawObjectLayers);
    s->setValue(QLatin1String(NORENDER_KEY), drawNoRender);
    s->setValue(QLatin1String(LOTS_KEY), drawLots);
    s->setValue(QLatin1String(IMAGE_WIDTH_KEY), customImageWidth);
#endif

    QDialog::accept();
}

void SaveAsImageDialog::browse()
{
    // Don't confirm overwrite here, since we'll confirm when the user presses
    // the Save button
    const QString filter = Utils::writableImageFormatsFilter();
    QString f = QFileDialog::getSaveFileName(this, tr("Image"),
                                             mUi->fileNameEdit->text(),
                                             filter, 0,
                                             QFileDialog::DontConfirmOverwrite);
    if (!f.isEmpty()) {
        mUi->fileNameEdit->setText(f);
        mPath = f;
    }
}

void SaveAsImageDialog::updateAcceptEnabled()
{
    QPushButton *saveButton = mUi->buttonBox->button(QDialogButtonBox::Save);
    saveButton->setEnabled(!mUi->fileNameEdit->text().isEmpty());
}
