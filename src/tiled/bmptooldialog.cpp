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
#include "tilemetainfomgr.h"
#include "zoomable.h"

#include "BuildingEditor/buildingtiles.h"

#include "layer.h"
#include "layermodel.h"
#include "map.h"
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
                   const QList<BmpRule*> &rules)
        : QUndoCommand(QCoreApplication::translate("Undo Commands",
                                                   "Change BMP Settings (Rules.txt)"))
        , mMapDocument(mapDocument)
        , mFileName(fileName)
        , mRules(rules)
    {
    }

    void undo() { swap(); }
    void redo() { swap(); }

    void swap()
    {
        QString oldFile = mMapDocument->map()->bmpSettings()->rulesFile();
        QList<BmpRule*> oldRules = mMapDocument->map()->bmpSettings()->rulesCopy();
        mMapDocument->setBmpRules(mFileName, mRules);
        mFileName = oldFile;
        mRules = oldRules;
    }

    MapDocument *mMapDocument;
    QString mFileName;
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
    mDocument(0)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::Tool);

    ui->tabWidget->setCurrentIndex(0);

    connect(ui->tableView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            SLOT(currentRuleChanged(QModelIndex)));

    connect(ui->brushSize, SIGNAL(valueChanged(int)),
            SLOT(brushSizeChanged(int)));
    connect(ui->brushSquare, SIGNAL(clicked()),
            SLOT(brushSquare()));
    connect(ui->brushCircle, SIGNAL(clicked()),
            SLOT(brushCircle()));
    connect(ui->restrictToSelection, SIGNAL(toggled(bool)),
            SLOT(restrictToSelection(bool)));
    connect(ui->toggleOverlayLayers, SIGNAL(clicked()),
            SLOT(toggleOverlayLayers()));
    connect(ui->showMapTiles, SIGNAL(toggled(bool)),
            SLOT(showMapTiles(bool)));

    connect(ui->reloadRules, SIGNAL(clicked()), SLOT(reloadRules()));
    connect(ui->importRules, SIGNAL(clicked()), SLOT(importRules()));
    connect(ui->reloadBlends, SIGNAL(clicked()), SLOT(reloadBlends()));
    connect(ui->importBlends, SIGNAL(clicked()), SLOT(importBlends()));

    connect(ui->help, SIGNAL(clicked()), SLOT(help()));

    QSettings settings;
    settings.beginGroup(QLatin1String("BmpToolDialog"));
    qreal scale = settings.value(QLatin1String("scale"), 0.5).toReal();
    ui->tableView->zoomable()->setScale(scale);

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
    settings.endGroup();

    mVisibleLaterTimer.setSingleShot(true);
    mVisibleLaterTimer.setInterval(200);
    connect(&mVisibleLaterTimer, SIGNAL(timeout()), SLOT(setVisibleNow()));

    connect(BmpBrushTool::instance(), SIGNAL(brushChanged()),
            SLOT(brushChanged()));

    connect(DocumentManager::instance(), SIGNAL(documentAboutToClose(int,MapDocument*)),
            SLOT(documentAboutToClose(int,MapDocument*)));
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

    QDialog::setVisible(visible);

    if (!visible) {
        settings.setValue(QLatin1String("geometry"), saveGeometry());
        settings.setValue(QLatin1String("scale"), ui->tableView->zoomable()->scale());
        settings.setValue(QLatin1String("brushSize"), ui->brushSize->value());
        if (ui->brushSquare->isChecked())
            settings.setValue(QLatin1String("brushShape"), QLatin1String("square"));
        if (ui->brushCircle->isChecked())
            settings.setValue(QLatin1String("brushShape"), QLatin1String("circle"));
        settings.setValue(QLatin1String("restrictToSelection"),
                          BmpBrushTool::instance()->restrictToSelection());
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

void BmpToolDialog::reloadRules()
{
    QString f = mDocument->map()->bmpSettings()->rulesFile();
    if (!f.isEmpty()/* && QFileInfo(f).exists()*/) {
        BmpRulesFile file;
        if (!file.read(f)) {
            QMessageBox::warning(this, tr("Reload Rules Failed"), file.errorString());
            return;
        }
        mDocument->undoStack()->push(new ChangeBmpRules(mDocument, f, file.rulesCopy()));
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
        mDocument->undoStack()->push(new ChangeBmpRules(mDocument, f, file.rulesCopy()));
    }
}

void BmpToolDialog::reloadBlends()
{
    QString f = mDocument->map()->bmpSettings()->blendsFile();
    if (!f.isEmpty()/* && QFileInfo(f).exists()*/) {
        BmpBlendsFile file;
        if (!file.read(f)) {
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
        if (!file.read(f)) {
            QMessageBox::warning(this, tr("Import Blends Failed"), file.errorString());
            return;
        }
        settings.setValue(QLatin1String("BmpToolDialog/BlendsFile"), f);
        mDocument->undoStack()->push(new ChangeBmpBlends(mDocument, f, file.blendsCopy()));
    }
}

void BmpToolDialog::help()
{
    QUrl url = QUrl::fromLocalFile(
            QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + QLatin1String("docs/TileZed/BMPTools.html"));
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

void BmpToolDialog::setDocument(MapDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = doc;

    ui->tableView->clear();
    ui->rulesFile->setText(tr("<none>"));
    ui->blendsFile->setText(tr("<none>"));

    ui->reloadRules->setEnabled(mDocument != 0 &&
            !mDocument->map()->bmpSettings()->rulesFile().isEmpty());
    ui->importRules->setEnabled(mDocument != 0);
    ui->exportRules->setEnabled(mDocument != 0);

    ui->reloadBlends->setEnabled(mDocument != 0 &&
            !mDocument->map()->bmpSettings()->blendsFile().isEmpty());
    ui->importBlends->setEnabled(mDocument != 0);
    ui->exportBlends->setEnabled(mDocument != 0);

    switch (BmpBrushTool::instance()->brushShape()) {
    case BmpBrushTool::Square: ui->brushSquare->setChecked(true); break;
    case BmpBrushTool::Circle: ui->brushCircle->setChecked(true); break;
    }
    ui->restrictToSelection->setChecked(BmpBrushTool::instance()->restrictToSelection());
    ui->showMapTiles->setEnabled(mDocument != 0);

    if (mDocument) {
        QSet<Tileset*> tilesets;
        QList<Tiled::Tile*> tiles;
        QList<void*> userData;
        QStringList headers;
        int ruleIndex = 1;
        QList<BmpRule*> rules = mDocument->map()->bmpSettings()->rules();
        foreach (BmpRule *rule, rules) {
            foreach (QString tileName, rule->tileChoices) {
                Tile *tile;
                if (tileName.isEmpty())
                    tile = BuildingEditor::BuildingTilesMgr::instance()->noneTiledTile();
                else {
                    tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(tileName);
                    if (tile && TileMetaInfoMgr::instance()->indexOf(tile->tileset()) != -1)
                        tilesets += tile->tileset();
                }
                tiles += tile;
                userData += rule;
                if (rule->bitmapIndex == 0) {
                    headers += tr("Rule #%1: index=%2 color=%3,%4,%5")
                            .arg(ruleIndex)
                            .arg(rule->bitmapIndex)
                            .arg(qRed(rule->color)).arg(qGreen(rule->color)).arg(qBlue(rule->color));
                } else {
                    headers += tr("Rule #%1: index=%2 color=%3,%4,%5 condition=%6,%7,%8")
                            .arg(ruleIndex)
                            .arg(rule->bitmapIndex)
                            .arg(qRed(rule->color)).arg(qGreen(rule->color)).arg(qBlue(rule->color))
                            .arg(qRed(rule->condition)).arg(qGreen(rule->condition)).arg(qBlue(rule->condition));
                }
            }
            ++ruleIndex;
        }
        ui->tableView->setTiles(tiles, userData, headers);
        TileMetaInfoMgr::instance()->loadTilesets(tilesets.toList());

        ruleIndex = 0;
        if (mCurrentRuleForDocument.contains(mDocument))
            ruleIndex = mCurrentRuleForDocument[mDocument];
        if (ruleIndex >= 0 && ruleIndex < rules.size())
            ui->tableView->setCurrentIndex(
                        ui->tableView->model()->index((void*)rules.at(ruleIndex)));

        const BmpSettings *settings = mDocument->map()->bmpSettings();
        QString fileName = settings->rulesFile();
        if (!fileName.isEmpty())
            ui->rulesFile->setText(QDir::toNativeSeparators(fileName));

        fileName = settings->blendsFile();
        if (!fileName.isEmpty())
            ui->blendsFile->setText(QDir::toNativeSeparators(fileName));

        ui->showMapTiles->setChecked(mDocument->mapComposite()->showMapTiles());

        connect(mDocument, SIGNAL(bmpRulesChanged()), SLOT(bmpRulesChanged()));
        connect(mDocument, SIGNAL(bmpBlendsChanged()), SLOT(bmpBlendsChanged()));
    }
}

void BmpToolDialog::currentRuleChanged(const QModelIndex &current)
{
    BmpRule *rule = static_cast<BmpRule*>(ui->tableView->model()->userDataAt(current));
    if (rule) {
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
    LayerModel *m = mDocument->layerModel();
    Map *map = mDocument->map();
    int visible = -1;
    foreach (QString layerName, mDocument->mapComposite()->bmpBlender()->blendLayers()) {
        int index = map->indexOfLayer(layerName);
        if (index != -1) {
            Layer *layer = map->layerAt(index);
            if (visible == -1)
                visible = !layer->isVisible();
            m->setData(m->index(map->layerCount() - index - 1),
                       visible ? Qt::Checked : Qt::Unchecked,
                       Qt::CheckStateRole);
        }
    }
}

void BmpToolDialog::showMapTiles(bool show)
{
    if (!mDocument)
        return;
    mDocument->mapComposite()->setShowMapTiles(show);
    if (mDocument->map()->layerCount())
        mDocument->emitRegionChanged(QRect(QPoint(0, 0), mDocument->map()->size()),
                                     mDocument->map()->layerAt(0));
}
