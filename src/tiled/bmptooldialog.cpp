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

#include "bmptooldialog.h"
#include "ui_bmptooldialog.h"

#include "bmpblender.h"
#include "bmptool.h"
#include "documentmanager.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "mainwindow.h"
#include "preferences.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "BuildingEditor/buildingtiles.h"

#include "layer.h"
#include "layermodel.h"
#include "map.h"
#include "maplevel.h"
#include "tile.h"
#include "tileset.h"

#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QUrl>

using namespace Tiled;
using namespace Tiled::Internal;

namespace Tiled {
namespace Internal {

class ChangeBmpRules : public QUndoCommand
{
public:
    ChangeBmpRules(MapDocument *mapDocument, const QString &fileName,
                   const QList<BmpAlias*> &aliases,
                   const QList<BmpRule*> &rules)
        : QUndoCommand(QCoreApplication::translate("Undo Commands",
                                                   "Change BMP Settings (Rules.txt)"))
        , mMapDocument(mapDocument)
        , mFileName(fileName)
        , mAliases(aliases)
        , mRules(rules)
    {
    }

    ~ChangeBmpRules()
    {
        qDeleteAll(mAliases);
        qDeleteAll(mRules);
    }

    void undo() { swap(); }
    void redo() { swap(); }

    void swap()
    {
        QString oldFile = mMapDocument->map()->bmpSettings()->rulesFile();
        QList<BmpAlias*> oldAliases = mMapDocument->map()->bmpSettings()->aliasesCopy();
        QList<BmpRule*> oldRules = mMapDocument->map()->bmpSettings()->rulesCopy();
        mMapDocument->setBmpAliases(mAliases);
        mMapDocument->setBmpRules(mFileName, mRules);
        mAliases = oldAliases;
        mFileName = oldFile;
        mRules = oldRules;
    }

    MapDocument *mMapDocument;
    QString mFileName;
    QList<BmpAlias*> mAliases;
    QList<BmpRule*> mRules;
};

class ChangeBmpBlends : public QUndoCommand
{
public:
    ChangeBmpBlends(MapDocument *mapDocument, const QString &fileName,
                    const QList<BmpBlend*> &blends)
        : QUndoCommand(QCoreApplication::translate("Undo Commands",
                                                   "Change BMP Settings (Blends.txt)"))
        , mMapDocument(mapDocument)
        , mFileName(fileName)
        , mBlends(blends)
    {
    }

    ~ChangeBmpBlends()
    {
        qDeleteAll(mBlends);
    }

    void undo() { swap(); }
    void redo() { swap(); }

    void swap()
    {
        QString oldFile = mMapDocument->map()->bmpSettings()->blendsFile();
        QList<BmpBlend*> oldBlends = mMapDocument->map()->bmpSettings()->blendsCopy();
        mMapDocument->setBmpBlends(mFileName, mBlends);
        mFileName = oldFile;
        mBlends = oldBlends;
    }

    MapDocument *mMapDocument;
    QString mFileName;
    QList<BmpBlend*> mBlends;
};

} // namespace Internal
} // namespace Tiled

BmpToolDialog *BmpToolDialog::mInstance = 0;

BmpToolDialog *BmpToolDialog::instance()
{
    if (!mInstance)
        mInstance = new BmpToolDialog(MainWindow::instance());
    return mInstance;
}

BmpToolDialog::BmpToolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BmpToolDialog),
    mDocument(0),
    mExpanded(true)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::Tool);

    ui->tabWidget->setCurrentIndex(0);

    connect(ui->tableView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &BmpToolDialog::currentRuleChanged);

    connect(ui->expandCollapse, &QAbstractButton::clicked,
            this, &BmpToolDialog::expandCollapse);
    ui->tableView->zoomable()->connectToComboBox(ui->scaleCombo);

    connect(ui->blendView, &BmpBlendView::blendHighlighted,
            this, &BmpToolDialog::blendHighlighted);
    ui->blendView->zoomable()->connectToComboBox(ui->blendScaleCombo);

    ui->tilesInBlend->model()->setShowHeaders(false);
    ui->tilesInBlend->setZoomable(ui->blendView->zoomable());
    connect(ui->tilesInBlend->zoomable(), &Zoomable::scaleChanged,
            this, &BmpToolDialog::synchBlendTilesView);

    connect(ui->brushSize, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BmpToolDialog::brushSizeChanged);
    connect(ui->brushSquare, &QAbstractButton::clicked,
            this, &BmpToolDialog::brushSquare);
    connect(ui->brushCircle, &QAbstractButton::clicked,
            this, &BmpToolDialog::brushCircle);
    connect(ui->restrictToSelection, &QAbstractButton::toggled,
            this, &BmpToolDialog::restrictToSelection);
    connect(ui->toggleOverlayLayers, &QAbstractButton::clicked,
            this, &BmpToolDialog::toggleOverlayLayers);
    connect(ui->showBMPTiles, &QAbstractButton::toggled,
            this, &BmpToolDialog::showBMPTiles);
    connect(ui->showMapTiles, &QAbstractButton::toggled,
            this, &BmpToolDialog::showMapTiles);
    connect(ui->blendEdgesEverywhere, &QCheckBox::toggled, this, &BmpToolDialog::blendEdgesEverywhere);

    connect(ui->reloadRules, &QAbstractButton::clicked, this, &BmpToolDialog::reloadRules);
    connect(ui->importRules, &QAbstractButton::clicked, this, &BmpToolDialog::importRules);
    connect(ui->exportRules, &QAbstractButton::clicked, this, &BmpToolDialog::exportRules);
    connect(ui->trashRules, &QAbstractButton::clicked, this, &BmpToolDialog::trashRules);

    connect(ui->reloadBlends, &QAbstractButton::clicked, this, &BmpToolDialog::reloadBlends);
    connect(ui->importBlends, &QAbstractButton::clicked, this, &BmpToolDialog::importBlends);
    connect(ui->exportBlends, &QAbstractButton::clicked, this, &BmpToolDialog::exportBlends);
    connect(ui->trashBlends, &QAbstractButton::clicked, this, &BmpToolDialog::trashBlends);

    connect(ui->help, &QAbstractButton::clicked, this, &BmpToolDialog::help);

    QSettings settings;
    settings.beginGroup(QLatin1String("BmpToolDialog"));
    qreal scale = settings.value(QLatin1String("scale"), 0.5).toReal();
    ui->tableView->zoomable()->setScale(scale);

    scale = settings.value(QLatin1String("Blends/Scale"), 0.5).toReal();
    ui->blendView->zoomable()->setScale(scale);

    int brushSize = settings.value(QLatin1String("brushSize"), 1).toInt();
    BmpBrushTool::instance()->setBrushSize(brushSize);
    ui->brushSize->setValue(brushSize);

    QString shape = settings.value(QLatin1String("brushShape"),
                                   QLatin1String("square")).toString();
    if (shape == QLatin1String("square")) brushSquare();
    else if (shape == QLatin1String("circle")) brushCircle();

    bool isRestricted = settings.value(QLatin1String("restrictToSelection"),
                                       true).toBool();
    BmpBrushTool::instance()->setRestrictToSelection(isRestricted);

    bool expanded = settings.value(QLatin1String("expanded"), true).toBool();
    if (!expanded) expandCollapse();
    settings.endGroup();

    mVisibleLaterTimer.setSingleShot(true);
    mVisibleLaterTimer.setInterval(200);
    connect(&mVisibleLaterTimer, &QTimer::timeout, this, &BmpToolDialog::setVisibleNow);

    connect(BmpBrushTool::instance(), &BmpBrushTool::brushChanged,
            this, &BmpToolDialog::brushChanged);

    connect(DocumentManager::instance(), &DocumentManager::documentAboutToClose,
            this, &BmpToolDialog::documentAboutToClose);
}

BmpToolDialog::~BmpToolDialog()
{
    delete ui;
}

void BmpToolDialog::setVisible(bool visible)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("BmpToolDialog"));
    if (visible) {
        QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
        if (!geom.isEmpty())
            restoreGeometry(geom);
    }

    synchBlendTilesView();

    QDialog::setVisible(visible);

    if (!visible) {
        settings.setValue(QLatin1String("geometry"), saveGeometry());
        settings.setValue(QLatin1String("scale"), ui->tableView->zoomable()->scale());
        settings.setValue(QLatin1String("Blends/Scale"), ui->blendView->zoomable()->scale());
        settings.setValue(QLatin1String("brushSize"), ui->brushSize->value());
        if (ui->brushSquare->isChecked())
            settings.setValue(QLatin1String("brushShape"), QLatin1String("square"));
        if (ui->brushCircle->isChecked())
            settings.setValue(QLatin1String("brushShape"), QLatin1String("circle"));
        settings.setValue(QLatin1String("restrictToSelection"),
                          BmpBrushTool::instance()->restrictToSelection());
        settings.setValue(QLatin1String("expanded"), mExpanded);
    }
    settings.endGroup();
}

void BmpToolDialog::setVisibleLater(bool visible)
{
    mVisibleLater = visible;
    mVisibleLaterTimer.start();
}

void BmpToolDialog::setVisibleNow()
{
    if (mVisibleLater != isVisible())
        setVisible(mVisibleLater);
}

void BmpToolDialog::blendHighlighted(BmpBlend *blend, int dir)
{
    QList<Tile*> tiles;
    QString header;
    if (blend && dir != -1) {
        QStringList tileNames;
        QStringList aliasTiles = ui->blendView->model()->aliasTiles(dir ? blend->blendTile : blend->mainTile);
        if (aliasTiles.isEmpty())
            tileNames << (dir ? blend->blendTile : blend->mainTile);
        else
            tileNames = aliasTiles;
        foreach (QString tileName, tileNames) {
            if (Tile *tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(tileName))
                tiles += tile;
            else
                tiles += TilesetManager::instance()->missingTile();
        }
        if (dir == 0)
            header = tr("<b>mainTile</b>=%1").arg(blend->mainTile);
        else
            header = tr("<b>blendTile</b>=%1, <b>dir</b>=%2, <b>layer</b>=%3")
                    .arg(blend->blendTile)
                    .arg(blend->dirAsString())
                    .arg(blend->targetLayer);
    }
    ui->blendLabel->setText(header);
    ui->tilesInBlend->model()->setColumnCount(qMax(8, tiles.size()));
    ui->tilesInBlend->setTiles(tiles);
}

void BmpToolDialog::synchBlendTilesView()
{
    int height = 2 + ui->tilesInBlend->fontMetrics().lineSpacing() + 2 + 128 * ui->tilesInBlend->zoomable()->scale() + 2;
    height += ui->tilesInBlend->frameWidth() * 2;
    ui->tilesInBlend->setFixedHeight(height);
}

void BmpToolDialog::expandCollapse()
{
    if (mExpanded) {
        ui->expandCollapse->setText(tr("Expand"));
    } else {
        ui->expandCollapse->setText(tr("Collapse"));
    }
    mExpanded = !mExpanded;

    ui->tableView->setExpanded(mExpanded);
}

void BmpToolDialog::reloadRules()
{
    QString f = mDocument->map()->bmpSettings()->rulesFile();
    if (!f.isEmpty()/* && QFileInfo(f).exists()*/) {
        BmpRulesFile file;
        if (!file.read(f)) {
            QMessageBox::warning(this, tr("Reload Rules Failed"), file.errorString());
            return;
        }
        mDocument->undoStack()->push(
                    new ChangeBmpRules(mDocument, f, file.aliasesCopy(),
                                       file.rulesCopy()));
    }
}

void BmpToolDialog::importRules()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("BmpToolDialog"));
    QString initialDir = settings.value(QLatin1String("RulesFile")).toString();
    if (initialDir.isEmpty() || !QFileInfo(initialDir).exists())
        initialDir = QApplication::applicationDirPath() + QLatin1String("/WorldEd/Rules.txt");
    settings.endGroup();

    QString f = QFileDialog::getOpenFileName(this, tr("Import Rules"),
                                             initialDir,
                                             tr("Rules.txt files (*.txt)"));
    if (!f.isEmpty()) {
        BmpRulesFile file;
        if (!file.read(f)) {
            QMessageBox::warning(this, tr("Import Rules Failed"), file.errorString());
            return;
        }
        settings.setValue(QLatin1String("BmpToolDialog/RulesFile"), f);
        mDocument->undoStack()->push(new ChangeBmpRules(mDocument, f,
                                                        file.aliasesCopy(),
                                                        file.rulesCopy()));
    }
}

void BmpToolDialog::exportRules()
{
    QString f = QFileDialog::getSaveFileName(this, tr("Export Rules"),
                                             mDocument->map()->bmpSettings()->rulesFile(),
                                             tr("Rules.txt files (*.txt)"));
    if (!f.isEmpty()) {
        BmpRulesFile file;
        file.fromMap(mDocument->map());
        if (!file.write(f)) {
            QMessageBox::warning(this, tr("Export Rules Failed"), file.errorString());
            return;
        }
    }
}

void BmpToolDialog::trashRules()
{
    mDocument->undoStack()->push(new ChangeBmpRules(mDocument, QString(),
                                                    QList<BmpAlias*>(),
                                                    QList<BmpRule*>()));
}

void BmpToolDialog::reloadBlends()
{
    QString f = mDocument->map()->bmpSettings()->blendsFile();
    if (!f.isEmpty()/* && QFileInfo(f).exists()*/) {
        BmpBlendsFile file;
        if (!file.read(f, mDocument->map()->bmpSettings()->aliases())) {
            QMessageBox::warning(this, tr("Reload Blends Failed"), file.errorString());
            return;
        }
        mDocument->undoStack()->push(new ChangeBmpBlends(mDocument, f, file.blendsCopy()));
    }
}

void BmpToolDialog::importBlends()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("BmpToolDialog"));
    QString initialDir = settings.value(QLatin1String("BlendsFile")).toString();
    if (initialDir.isEmpty() || !QFileInfo(initialDir).exists())
        initialDir = QApplication::applicationDirPath() + QLatin1String("/WorldEd/Blends.txt");
    settings.endGroup();

    QString f = QFileDialog::getOpenFileName(this, tr("Import Blends"),
                                             initialDir,
                                             tr("Blends.txt files (*.txt)"));
    if (!f.isEmpty()) {
        BmpBlendsFile file;
        if (!file.read(f, mDocument->map()->bmpSettings()->aliases())) {
            QMessageBox::warning(this, tr("Import Blends Failed"), file.errorString());
            return;
        }
        settings.setValue(QLatin1String("BmpToolDialog/BlendsFile"), f);
        mDocument->undoStack()->push(new ChangeBmpBlends(mDocument, f, file.blendsCopy()));
    }
}

void BmpToolDialog::exportBlends()
{
    QString f = QFileDialog::getSaveFileName(this, tr("Export Blends"),
                                             mDocument->map()->bmpSettings()->blendsFile(),
                                             tr("Blends.txt files (*.txt)"));
    if (!f.isEmpty()) {
        BmpBlendsFile file;
        file.fromMap(mDocument->map());
        if (!file.write(f)) {
            QMessageBox::warning(this, tr("Export Blends Failed"), file.errorString());
            return;
        }
    }
}

void BmpToolDialog::trashBlends()
{
    mDocument->undoStack()->push(new ChangeBmpBlends(mDocument, QString(),
                                                     QList<BmpBlend*>()));
}

void BmpToolDialog::help()
{
    QUrl url = QUrl::fromLocalFile(
                Preferences::instance()->docsPath(QLatin1String("TileZed/BMPTools.html")));
    QDesktopServices::openUrl(url);
}

void BmpToolDialog::bmpRulesChanged()
{
    setDocument(mDocument);
}

void BmpToolDialog::bmpBlendsChanged()
{
    setDocument(mDocument);
}

void BmpToolDialog::bmpBlendEdgesEverywhereChanged()
{
    if (mDocument == nullptr)
        return;
    ui->blendEdgesEverywhere->setChecked(mDocument->map()->bmpSettings()->isBlendEdgesEverywhere());
}

void BmpToolDialog::brushChanged()
{
    int brushSize = BmpBrushTool::instance()->brushSize();
    ui->brushSize->setValue(brushSize);
}

void BmpToolDialog::documentAboutToClose(int index, MapDocument *doc)
{
    Q_UNUSED(index)
    mCurrentRuleForDocument.remove(doc);
}

void BmpToolDialog::warningsChanged()
{
    ui->warnings->clear();
    if (!mDocument)
        return;
    ui->warnings->addItems(mDocument->mapComposite()->bmpBlender()->warnings());
    ui->tabWidget->setTabIcon(3, ui->warnings->count()
                              ? QIcon(QLatin1String(":/images/24x24/warning.png"))
                              : QIcon());
}

void BmpToolDialog::setDocument(MapDocument *doc)
{
    if (mDocument) {
        mDocument->disconnect(this);
        mDocument->mapComposite()->bmpBlender()->disconnect(this);
        ui->tabWidget->disconnect(mDocument->mapComposite()->bmpBlender());
    }

    mDocument = doc;

    ui->tableView->clear();
    ui->blendView->clear();
    ui->rulesFile->setText(tr("<none>"));
    ui->blendsFile->setText(tr("<none>"));

    ui->reloadRules->setEnabled((mDocument != nullptr) &&
            !mDocument->map()->bmpSettings()->rulesFile().isEmpty());
    ui->importRules->setEnabled(mDocument != nullptr);
    ui->exportRules->setEnabled(mDocument != nullptr);
    ui->trashRules->setEnabled(ui->reloadRules->isEnabled());

    ui->reloadBlends->setEnabled((mDocument != nullptr) &&
            !mDocument->map()->bmpSettings()->blendsFile().isEmpty());
    ui->importBlends->setEnabled(mDocument != nullptr);
    ui->exportBlends->setEnabled(mDocument != nullptr);
    ui->trashBlends->setEnabled(ui->reloadBlends->isEnabled());

    switch (BmpBrushTool::instance()->brushShape()) {
    case BmpBrushTool::Square: ui->brushSquare->setChecked(true); break;
    case BmpBrushTool::Circle: ui->brushCircle->setChecked(true); break;
    }
    ui->restrictToSelection->setChecked(BmpBrushTool::instance()->restrictToSelection());
    ui->showBMPTiles->setEnabled(mDocument != nullptr);
    ui->showMapTiles->setEnabled(mDocument != nullptr);
    ui->blendEdgesEverywhere->setEnabled(mDocument != nullptr);

    if (mDocument) {
        ui->tableView->setRules(mDocument->map());
        QList<BmpRule*> rules = mDocument->map()->bmpSettings()->rules();

        ui->blendView->setBlends(mDocument->map());

        int ruleIndex = 0;
        if (mCurrentRuleForDocument.contains(mDocument))
            ruleIndex = mCurrentRuleForDocument[mDocument];
        if (ruleIndex >= 0 && ruleIndex < rules.size())
            ui->tableView->setCurrentIndex(
                        ui->tableView->model()->index(rules.at(ruleIndex)));

        const BmpSettings *settings = mDocument->map()->bmpSettings();
        QString fileName = settings->rulesFile();
        if (!fileName.isEmpty())
            ui->rulesFile->setText(QDir::toNativeSeparators(fileName));

        fileName = settings->blendsFile();
        if (!fileName.isEmpty())
            ui->blendsFile->setText(QDir::toNativeSeparators(fileName));

        ui->showBMPTiles->setChecked(mDocument->mapComposite()->showBMPTiles());
        ui->showMapTiles->setChecked(mDocument->mapComposite()->showMapTiles());
        ui->blendEdgesEverywhere->setChecked(mDocument->map()->bmpSettings()->isBlendEdgesEverywhere());

        connect(mDocument, SIGNAL(bmpAliasesChanged()), SLOT(bmpRulesChanged())); // XXXXX
        connect(mDocument, SIGNAL(bmpRulesChanged()), SLOT(bmpRulesChanged()));
        connect(mDocument, SIGNAL(bmpBlendsChanged()), SLOT(bmpBlendsChanged()));
        connect(mDocument, &MapDocument::bmpBlendEdgesEverywhereChanged, this, &BmpToolDialog::bmpBlendEdgesEverywhereChanged);
        connect(mDocument->mapComposite()->bmpBlender(), SIGNAL(warningsChanged()),
                SLOT(warningsChanged()));

        // This is to handle unknown pixels in the BMP images being erased.
        connect(ui->tabWidget, SIGNAL(currentChanged(int)),
                mDocument->mapComposite()->bmpBlender(), SLOT(updateWarnings()));
    }

    warningsChanged();
}

void BmpToolDialog::changeBmpRules(MapDocument *doc, const QString &fileName,
                                   const QList<BmpAlias *> &aliases,
                                   const QList<BmpRule *> &rules)
{
    doc->undoStack()->push(new ChangeBmpRules(doc, fileName, aliases, rules));
}

void BmpToolDialog::changeBmpBlends(MapDocument *doc, const QString &fileName,
                                    const QList<BmpBlend *> &blends)
{
    doc->undoStack()->push(new ChangeBmpBlends(doc, fileName, blends));
}

void BmpToolDialog::currentRuleChanged(const QModelIndex &current)
{
    if (BmpRule *rule = ui->tableView->model()->ruleAt(current)) {
        BmpBrushTool::instance()->setColor(rule->bitmapIndex, rule->color);
        mCurrentRuleForDocument[mDocument]
                = mDocument->map()->bmpSettings()->rules().indexOf(rule);
    } else {
        BmpBrushTool::instance()->setColor(0, qRgb(0, 0, 0));
    }
}

void BmpToolDialog::brushSizeChanged(int size)
{
    BmpBrushTool::instance()->setBrushSize(size);
}

void BmpToolDialog::brushSquare()
{
    BmpBrushTool::instance()->setBrushShape(BmpBrushTool::Square);
}

void BmpToolDialog::brushCircle()
{
    BmpBrushTool::instance()->setBrushShape(BmpBrushTool::Circle);
}

void BmpToolDialog::restrictToSelection(bool isRestricted)
{
    BmpBrushTool::instance()->setRestrictToSelection(isRestricted);
}

void BmpToolDialog::toggleOverlayLayers()
{
    if (!mDocument)
        return;
    Map *map = mDocument->map();
    int visible = -1;
    MapLevel *mapLevel = map->levelAt(0);
    for (const QString& layerName : mDocument->mapComposite()->bmpBlender()->blendLayers()) {
        int index = mapLevel->indexOfLayer(layerName);
        if (index != -1) {
            Layer *layer = mapLevel->layerAt(index);
            if (visible == -1)
                visible = !layer->isVisible();
            mDocument->setLayerVisible(mapLevel->z(), index, visible);
        }
    }
}

void BmpToolDialog::showBMPTiles(bool show)
{
    if (!mDocument)
        return;
    mDocument->mapComposite()->setShowBMPTiles(show);
    if (mDocument->map()->levelAt(0)->layerCount()) {
        mDocument->emitRegionChanged(QRect(QPoint(0, 0), mDocument->map()->size()),
                                     mDocument->map()->levelAt(0)->layerAt(0));
    }
}

void BmpToolDialog::showMapTiles(bool show)
{
    if (!mDocument)
        return;
    mDocument->mapComposite()->setShowMapTiles(show);
    if (mDocument->map()->levelAt(0)->layerCount())
        mDocument->emitRegionChanged(QRect(QPoint(0, 0), mDocument->map()->size()),
                                     mDocument->map()->levelAt(0)->layerAt(0));
}

void BmpToolDialog::blendEdgesEverywhere(bool everywhere)
{
    if (mDocument == nullptr)
        return;
    mDocument->setBlendEdgesEverywhere(everywhere);
}
