/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "buildingtilesetdock.h"
#include "ui_buildingtilesetdock.h"

#include "buildingdocument.h"
#include "buildingdocumentmgr.h"
#include "buildingmap.h"
#include "buildingpreferences.h"
#include "buildingtiles.h"
#include "buildingtiletools.h"

#include "preferences.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "tile.h"
#include "tileset.h"

#include <QScrollBar>
#include <QToolBar>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

#define HORIZONTAL_SCROLLBAR_FIX 1

BuildingTilesetDock::BuildingTilesetDock(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::BuildingTilesetDock),
    mDocument(0),
    mCurrentTileset(0),
    mZoomable(new Zoomable(this)),
    mActionSwitchLayer(new QAction(this))
{
    ui->setupUi(this);

#ifndef TILESET_LIST_FIXED_WIDTH
    mSplitter = ui->splitter;
#endif

#if HORIZONTAL_SCROLLBAR_FIX
    // https://stackoverflow.com/questions/44633066/qlistwidget-horizontal-scrollbar-causes-selection-to-go-out-of-view
    ui->tilesets->setAutoScroll(false);
#endif

    connect(ui->filter, &QLineEdit::textEdited, this, &BuildingTilesetDock::filterEdited);

    mIconTileLayer = QIcon(QLatin1String(":/images/16x16/layer-tile.png"));
    mIconTileLayerStop = QIcon(QLatin1String(":/images/16x16/layer-tile-stop.png"));
    mActionSwitchLayer->setCheckable(true);
    bool enabled = Preferences::instance()->autoSwitchLayer();
    mActionSwitchLayer->setChecked(enabled == false);
    mActionSwitchLayer->setIcon(enabled ? mIconTileLayer : mIconTileLayerStop);
    connect(mActionSwitchLayer, &QAction::toggled,
            this, &BuildingTilesetDock::layerSwitchToggled);
    connect(Preferences::instance(), &Preferences::autoSwitchLayerChanged,
            this, &BuildingTilesetDock::autoSwitchLayerChanged);

    QToolBar *toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(mActionSwitchLayer);

#ifdef BUILDINGED_SA
    toolBar->addWidget(mBackgroundColorButton = new Tiled::Internal::ColorButton(toolBar));
    mBackgroundColorButton->setColor(Preferences::instance()->tilesetBackgroundColor());
    tilesetBackgroundColorChanged(Preferences::instance()->tilesetBackgroundColor());
    connect(mBackgroundColorButton, &ColorButton::colorChanged, Preferences::instance(), &Preferences::setTilesetBackgroundColor);
    connect(Preferences::instance(), &Preferences::tilesetBackgroundColorChanged, this, &BuildingTilesetDock::tilesetBackgroundColorChanged);
#endif

    ui->toolBarLayout->insertWidget(0, toolBar, 1);

    ui->tiles->setSelectionMode(QAbstractItemView::ExtendedSelection);

    mZoomable->setScale(BuildingPreferences::instance()->tileScale());
    mZoomable->connectToComboBox(ui->scaleComboBox);
    ui->tiles->setZoomable(mZoomable);
    connect(mZoomable, &Zoomable::scaleChanged,
            BuildingPreferences::instance(), &BuildingPreferences::setTileScale);
    connect(BuildingPreferences::instance(), &BuildingPreferences::tileScaleChanged,
            this, &BuildingTilesetDock::tileScaleChanged);

    connect(ui->tilesets, &QListWidget::currentRowChanged,
            this, &BuildingTilesetDock::currentTilesetChanged);

    ui->tiles->model()->setShowHeaders(false);
    connect(ui->tiles->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &BuildingTilesetDock::tileSelectionChanged);
    connect(Preferences::instance(), &Preferences::autoSwitchLayerChanged,
            this, &BuildingTilesetDock::autoSwitchLayerChanged);

    connect(BuildingDocumentMgr::instance(), &BuildingDocumentMgr::currentDocumentChanged,
            this, &BuildingTilesetDock::currentDocumentChanged);

#ifndef BUILDINGED_SA
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAdded,
            this, &BuildingTilesetDock::tilesetAdded);
#endif
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAboutToBeRemoved,
            this, &BuildingTilesetDock::tilesetAboutToBeRemoved);

    connect(TilesetManager::instance(), &TilesetManager::tilesetChanged,
            this, &BuildingTilesetDock::tilesetChanged);
    connect(TilesetManager::instance(), &TilesetManager::tileLayerNameChanged,
            this, &BuildingTilesetDock::tileLayerNameChanged);

    connect(PickTileTool::instance(), &PickTileTool::tilePicked,
            this, &BuildingTilesetDock::buildingTilePicked);

    retranslateUi();
}

BuildingTilesetDock::~BuildingTilesetDock()
{
    delete ui;
}

void BuildingTilesetDock::firstTimeSetup()
{
    if (!ui->tilesets->count())
        setTilesetList(); // TileMetaInfoMgr signals might have done this already.
}

void BuildingTilesetDock::writeSettings(QSettings &settings)
{
#ifndef TILESET_LIST_FIXED_WIDTH
    settings.beginGroup(QLatin1String("TilesetDock"));
    QVariantList v;
    for (int size : mSplitter->sizes()) {
        v += size;
    }
    settings.setValue(tr("%1/sizes").arg(mSplitter->objectName()), v);
    settings.endGroup();
#endif
}

void BuildingTilesetDock::readSettings(QSettings &settings)
{
#ifndef TILESET_LIST_FIXED_WIDTH
    settings.beginGroup(QLatin1String("TilesetDock"));
    QVariant v = settings.value(tr("%1/sizes").arg(mSplitter->objectName()));
    if (v.canConvert(QVariant::List)) {
        QList<int> sizes;
        for (QVariant v2 : v.toList()) {
            sizes += v2.toInt();
        }
        mSplitter->setSizes(sizes);
    }
    settings.endGroup();
#endif
}

#ifdef BUILDINGED_SA
void BuildingTilesetDock::afterInitConfigFiles()
{
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAdded,
            this, &BuildingTilesetDock::tilesetAdded);
}
#endif

void BuildingTilesetDock::currentDocumentChanged(BuildingDocument *document)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = document;

    if (mDocument) {

    }
}

void BuildingTilesetDock::changeEvent(QEvent *event)
{
    QDockWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void BuildingTilesetDock::retranslateUi()
{
    bool enabled = Preferences::instance()->autoSwitchLayer();
    QString text = enabled ? tr("Layer Switch Enabled")
                           : tr("Layer Switch Disabled");
    mActionSwitchLayer->setText(text);
}

void BuildingTilesetDock::filterEdited(const QString &text)
{
    QListWidget* listWidget = ui->tilesets;

    for (int row = 0; row < listWidget->count(); row++) {
        QListWidgetItem* item = listWidget->item(row);
        item->setHidden(text.trimmed().isEmpty() ? false : !item->text().contains(text));
    }

    QListWidgetItem* current = listWidget->currentItem();
    if (current != nullptr && current->isHidden()) {
        // Select previous visible row.
        int row = listWidget->row(current) - 1;
        while (row >= 0 && listWidget->item(row)->isHidden())
            row--;
        if (row >= 0) {
            current = listWidget->item(row);
            listWidget->setCurrentItem(current);
            listWidget->scrollToItem(current);
            return;
        }

        // Select next visible row.
        row = listWidget->row(current) + 1;
        while (row < listWidget->count() && listWidget->item(row)->isHidden())
            row++;
        if (row < listWidget->count()) {
            current = listWidget->item(row);
            listWidget->setCurrentItem(current);
            listWidget->scrollToItem(current);
            return;
        }

        // All items hidden
        listWidget->setCurrentItem(nullptr);
    }

    current = listWidget->currentItem();
    if (current != nullptr)
        listWidget->scrollToItem(current);
}

#ifdef BUILDINGED_SA
void BuildingTilesetDock::tilesetBackgroundColorChanged(const QColor &color)
{
    ui->tilesets->setStyleSheet(QStringLiteral("QTableView { alternate-background-color: %1; background-color: %1; }").arg(color.name()));
}
#endif

void BuildingTilesetDock::setTilesetList()
{
    ui->tilesets->clear();

#ifdef TILESET_LIST_FIXED_WIDTH
    int width = 64;
    QFontMetrics fm = ui->tilesets->fontMetrics();
#endif
    foreach (Tileset *tileset, TileMetaInfoMgr::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(tileset->name());
        if (tileset->isMissing())
            item->setForeground(Qt::red);
        ui->tilesets->addItem(item);
#ifdef TILESET_LIST_FIXED_WIDTH
        width = qMax(width, fm.horizontalAdvance(tileset->name()));
#endif
    }
#ifdef TILESET_LIST_FIXED_WIDTH
    int sbw = ui->tilesets->verticalScrollBar()->sizeHint().width();
    ui->tilesets->setFixedWidth(width + 16 + sbw);
    ui->filter->setFixedWidth(ui->tilesets->width());
#endif

    filterEdited(ui->filter->text());
}

void BuildingTilesetDock::setTilesList()
{
    MixedTilesetModel *model = ui->tiles->model();
    model->setShowLabels(Preferences::instance()->autoSwitchLayer());

    if (!mCurrentTileset || mCurrentTileset->isMissing())
        ui->tiles->clear();
    else {
        QStringList labels;
        for (int i = 0; i < mCurrentTileset->tileCount(); i++) {
            Tile *tile = mCurrentTileset->tileAt(i);
            QString label = TilesetManager::instance()->layerName(tile);
            if (label.isEmpty())
                label = tr("???");
            labels += label;
        }
        ui->tiles->setTileset(mCurrentTileset, QList<void*>(), labels);
    }
}

void BuildingTilesetDock::switchLayerForTile(Tiled::Tile *tile)
{
    if (!mDocument || !Preferences::instance()->autoSwitchLayer())
        return;
    int level = mDocument->currentLevel();
    QString layerName = TilesetManager::instance()->layerName(tile);
    if (!layerName.isEmpty()) {
        if (BuildingMap::layerNames(level).contains(layerName))
            mDocument->setCurrentLayer(layerName);
    }
}

void BuildingTilesetDock::currentTilesetChanged(int row)
{
    mCurrentTileset = 0;
    if (row >= 0)
        mCurrentTileset = TileMetaInfoMgr::instance()->tileset(row);
    setTilesList();

#if HORIZONTAL_SCROLLBAR_FIX
    const QRect rect = ui->tilesets->visualItemRect(ui->tilesets->currentItem());
    if (!rect.isValid()) {
        return;
    }
    const QRect viewport = ui->tilesets->viewport()->rect();
    if (viewport.contains(rect)) {
        return;
    }
    const bool above = rect.top() < viewport.top();
    const bool below = rect.bottom() > viewport.bottom();
    // Like QCommonListViewBase::verticalScrollToValue() but value is divided by item height.
    // The original code seems to assume the scrollbar min/max are pixel values.
    int value = ui->tilesets->verticalScrollBar()->value();
    int spacing = ui->tilesets->spacing();
    QRect adjusted = rect.adjusted(-spacing, -spacing, spacing, spacing);
    if (above) {
        value += adjusted.top() / rect.height();
    } else if (below) {
        value += qMin(adjusted.top(), adjusted.bottom() + 1 - viewport.height() + (viewport.height() % rect.height())) / rect.height();
    }
    ui->tilesets->verticalScrollBar()->setValue(value);
#endif
}

void BuildingTilesetDock::tileSelectionChanged()
{
    QModelIndexList selection = ui->tiles->selectionModel()->selectedIndexes();
    if (selection.size()) {
        QModelIndex index = selection.first();
        if (Tile *tile = ui->tiles->model()->tileAt(index)) {
            QString tileName = BuildingTilesMgr::instance()->nameForTile(tile);
            DrawTileTool::instance()->setTile(tileName);

            switchLayerForTile(tile);
        }
    }
}

void BuildingTilesetDock::tilesetAdded(Tileset *tileset)
{
    setTilesetList();
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    ui->tilesets->setCurrentRow(row);
}

void BuildingTilesetDock::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    delete ui->tilesets->takeItem(row);
}

// Called when a tileset image changes or a missing tileset was found.
void BuildingTilesetDock::tilesetChanged(Tileset *tileset)
{
    if (tileset == mCurrentTileset)
        setTilesList();

    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    if (QListWidgetItem *item = ui->tilesets->item(row))
        item->setForeground(tileset->isMissing() ? Qt::red : QBrush());
}

void BuildingTilesetDock::tileLayerNameChanged(BuildingTilesetDock::Tile *tile)
{
    if (!mCurrentTileset)
        return;
    if (tile->tileset()->imageSource() == mCurrentTileset->imageSource()) {
        QString layerName = TilesetManager::instance()->layerName(tile);
        if (layerName.isEmpty())
            layerName = tr("???");
        ui->tiles->model()->setLabel(mCurrentTileset->tileAt(tile->id()), layerName);
    }
}

void BuildingTilesetDock::layerSwitchToggled(bool checked)
{
    Preferences::instance()->setAutoSwitchLayer(checked == false);
}

void BuildingTilesetDock::autoSwitchLayerChanged(bool enabled)
{
    mActionSwitchLayer->setIcon(enabled ? mIconTileLayer : mIconTileLayerStop);
    QString text = enabled ? tr("Layer Switch Enabled") : tr("Layer Switch Disabled");
    mActionSwitchLayer->setText(text);
    mActionSwitchLayer->setChecked(enabled == false);

    ui->tiles->model()->setShowLabels(enabled);
}


void BuildingTilesetDock::tileScaleChanged(qreal scale)
{
    mZoomable->setScale(scale);
}

void BuildingTilesetDock::buildingTilePicked(const QString &tileName)
{
    QString tilesetName;
    int tileID;

    if (BuildingTilesMgr::parseTileName(tileName, tilesetName, tileID)) {
        if (Tileset *ts = TileMetaInfoMgr::instance()->tileset(tilesetName)) {
            if (Tile *tile = ts->tileAt(tileID)) {
                ui->tilesets->setCurrentRow(TileMetaInfoMgr::instance()->indexOf(ts));
                ui->tiles->setCurrentIndex(ui->tiles->model()->index(tile));
            }
        }
    }
}

/////

#include <QContextMenuEvent>
#include <QMenu>
#include <QUndoCommand>

BuildingTilesetView::BuildingTilesetView(QWidget *parent) :
    MixedTilesetView(parent)
{
}

void BuildingTilesetView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    const MixedTilesetModel *m = model();
    Tile *tile = m->tileAt(index);

    if (!tile)
        return;

    QMenu menu;
    QVector<QAction*> layerActions;
    QStringList layerNames;
    if (tile) {
        // Get a list of layer names from the current map
        QStringList layerNames0 = BuildingMap::layerNames(0);
        QSet<QString> set(layerNames0.constBegin(), layerNames0.constEnd());

        // Get a list of layer names for the current tileset
        for (int i = 0; i < tile->tileset()->tileCount(); i++) {
            Tile *tile2 = tile->tileset()->tileAt(i);
            QString layerName = TilesetManager::instance()->layerName(tile2);
            if (!layerName.isEmpty())
                set.insert(layerName);
        }
        layerNames = QStringList(set.constBegin(), set.constEnd());
        layerNames.sort();

        QMenu *layersMenu = menu.addMenu(QLatin1String("Default Layer"));
        layerActions += layersMenu->addAction(tr("<None>"));
        foreach (QString layerName, layerNames)
            layerActions += layersMenu->addAction(layerName);
    }

    QAction *action = menu.exec(event->globalPos());

    if (action && layerActions.contains(action)) {
        int index = layerActions.indexOf(action);
        QString layerName = index ? layerNames[index - 1] : QString();
        QModelIndexList indexes = selectionModel()->selectedIndexes();

        // TODO: Undo/Redo would be nice here.
        foreach (QModelIndex index, indexes) {
            tile = m->tileAt(index);
            TilesetManager::instance()->setLayerName(tile, layerName);
        }
    }
}
