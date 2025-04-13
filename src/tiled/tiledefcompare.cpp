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

#include "tiledefcompare.h"
#include "ui_tiledefcompare.h"

#include "preferences.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "zprogress.h"

#include "tile.h"
#include "tileset.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

using namespace Tiled::Internal;

#if defined(Q_OS_WIN) && (_MSC_VER >= 1600)
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

TileDefCompare::TileDefCompare(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TileDefCompare)
{
    ui->setupUi(this);

    connect(ui->packBrowse1, &QAbstractButton::clicked, this, &TileDefCompare::browse1);
    connect(ui->packBrowse2, &QAbstractButton::clicked, this, &TileDefCompare::browse2);
    connect(ui->switchButton, &QAbstractButton::clicked, this, &TileDefCompare::swapPaths);
    connect(ui->compare, &QAbstractButton::clicked, this, &TileDefCompare::compare);
    connect(ui->use1, &QAbstractButton::clicked, this, &TileDefCompare::use1);
    connect(ui->use2, &QAbstractButton::clicked, this, &TileDefCompare::use2);
    connect(ui->saveMerged, &QAbstractButton::clicked, this, &TileDefCompare::saveMerged);
    connect(ui->listWidget, &QListWidget::currentRowChanged, this, &TileDefCompare::currentRowChanged);

    readSettings();
}

TileDefCompare::~TileDefCompare()
{
    delete ui;
}

void TileDefCompare::browse1()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose .tiles File"),
                                             ui->packEdit1->currentText(),
                                             tr("Binary property files (*.tiles);;Text property files (*.tiles.txt)"));
    if (!f.isEmpty()) {
        addRecentFile1(QDir::toNativeSeparators(f));
        writeSettings();
    }
}

void TileDefCompare::browse2()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose .tiles File"),
                                             ui->packEdit2->currentText(),
                                             tr("Binary property files (*.tiles);;Text property files (*.tiles.txt)"));
    if (!f.isEmpty()) {
        addRecentFile2(QDir::toNativeSeparators(f));
        writeSettings();
    }
}

void TileDefCompare::swapPaths()
{
    QString path1 = ui->packEdit1->currentText();
    QString path2 = ui->packEdit2->currentText();
    addRecentFile1(path2);
    addRecentFile2(path1);
}

void TileDefCompare::compare()
{
    PROGRESS progress(tr("Reading file 1"), this);
    TileDefFileReader reader;
    if (!reader.read(ui->packEdit1->currentText(), mTileDefFile1)) {
        QMessageBox::warning(this, tr("Error reading .tiles file"), mTileDefFile1.errorString());
        return;
    }

    progress.update(tr("Reading file 2"));
    if (!reader.read(ui->packEdit2->currentText(), mTileDefFile2)) {
        QMessageBox::warning(this, tr("Error reading .tiles file"), mTileDefFile2.errorString());
        return;
    }
    if (!reader.read(ui->packEdit2->currentText(), mMergedFile)) {
        QMessageBox::warning(this, tr("Error reading .tiles file"), mMergedFile.errorString());
        return;
    }

    QSet<QString> in1, in2, unique1, unique2;
    for (TileDefTileset *ts : mTileDefFile1.tilesets()) {
        in1.insert(ts->mName);
    }
    for (TileDefTileset *ts : mTileDefFile2.tilesets()) {
        in2.insert(ts->mName);
    }
    unique1 = in1 - in2;
    unique2 = in2 - in1;

    ui->textBrowser->clear();

    ui->textBrowser->insertHtml(QLatin1String("<b>Unique tilesets in pack 1:</b><br>"));
    QStringList sorted1 = QStringList(unique1.values());
    sorted1.sort();
    for (const QString &s : sorted1) {
        ui->textBrowser->insertPlainText(s + QLatin1String("\n"));
    }

    ui->textBrowser->insertHtml(QLatin1String("<br><b>Unique tilesets in pack 2:</b><br>"));
    QStringList sorted2 = QStringList(unique2.values());
    sorted2.sort();
    for (const QString &s : sorted2) {
        ui->textBrowser->insertPlainText(s + QLatin1String("\n"));
    }

    ui->listWidget->clear();
    mTileMap1.clear();
    mTileMap2.clear();

    ui->textBrowser->insertHtml(QLatin1String("<br><b>Properties differences:</b><br>"));
    QSet<QString> shared = in1 & in2;
    QStringList sorted3(shared.begin(), shared.end());
    sorted3.sort();
    for (const QString &tsName : sorted3) {
        TileDefTileset *ts1 = mTileDefFile1.tileset(tsName);
        TileDefTileset *ts2 = mTileDefFile2.tileset(tsName);
        for (int i = 0; i < qMin(ts1->mTiles.size(), ts2->mTiles.size()); i++) {
            if (ts1->mTiles[i]->mProperties != ts2->mTiles[i]->mProperties) {
                ui->textBrowser->insertPlainText(QString(QLatin1String("%1 %2\n")).arg(tsName).arg(i));
                QString s1 = propertiesString(1, ts1->mTiles[i]);
                QString s2 = propertiesString(2, ts2->mTiles[i]);
                ui->textBrowser->insertPlainText(s1 + QLatin1String("\n"));
                ui->textBrowser->insertPlainText(s2 + QLatin1String("\n"));
                ui->listWidget->addItem(listString(2, ts1->mTiles[i], ts2->mTiles[i]));
                QListWidgetItem *item = ui->listWidget->item(ui->listWidget->count()-1);
                mTileMap1[item] = ts1->mTiles[i];
                mTileMap2[item] = ts2->mTiles[i];
//                item->setIcon(QIcon(QPixmap::fromImage(getTileImage(ts1->mTiles[1]))));
            }
        }
    }
}

void TileDefCompare::use1()
{
    foreach (QListWidgetItem *item, ui->listWidget->selectedItems()) {
        TileDefTile *tile1 = mTileMap1[item];
        TileDefTile *tile2 = mTileMap2[item];
        mMergedFile.tileset(tile1->mTileset->mName)->mTiles[tile1->id()]->mProperties = tile1->mProperties;
        item->setText(listString(1, tile1, tile2));
    }
}

void TileDefCompare::use2()
{
    foreach (QListWidgetItem *item, ui->listWidget->selectedItems()) {
        TileDefTile *tile1 = mTileMap1[item];
        TileDefTile *tile2 = mTileMap2[item];
        mMergedFile.tileset(tile2->mTileset->mName)->mTiles[tile2->id()]->mProperties = tile2->mProperties;
        item->setText(listString(2, tile1, tile2));
    }
}

void TileDefCompare::saveMerged()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                                    mMergedFile.fileName(),
                                                    QLatin1String("Tile properties files (*.tiles)"));
    if (fileName.isEmpty())
        return;

    foreach (TileDefTileset *ts, mMergedFile.tilesets()) {
        foreach (TileDefTile *tile, ts->mTiles) {
            // we copied these in use1()/use2()
            // normally the UI updates mPropertyUI and then TileDefFile.write() does mPropertyUI.ToProperties()
            tile->mPropertyUI.FromProperties(tile->mProperties);
        }
    }

    if (!mMergedFile.write(fileName)) {
        QMessageBox::warning(this, tr("Error writing .tiles file"),
                             mMergedFile.errorString());
        return;
    };
    mMergedFile.setFileName(fileName);

    TileDefTextFile textFile;
    if (!textFile.write(fileName + QLatin1String(".txt"), mMergedFile.tilesets())) {
        QMessageBox::warning(this, tr("Error writing .tiles.txt file"), textFile.errorString());
        return;
    };
}

void TileDefCompare::currentRowChanged(int row)
{
    if (row < 0)
        return;
    QListWidgetItem *item = ui->listWidget->item(row);
    ui->tileimage->setPixmap(QPixmap::fromImage(getTileImage(mTileMap1[item])));
}

QString TileDefCompare::propertiesString(int fileIndex, TileDefTile *tdt)
{
    QString s1 = QString::fromLatin1("%1: ").arg(fileIndex);
    foreach (QString prop, tdt->mProperties.keys())
        s1 += QString::fromLatin1("%1=%2, ").arg(prop).arg(tdt->mProperties[prop]);
    return s1;
}

QString TileDefCompare::listString(int use, TileDefTile *tdt1, TileDefTile *tdt2)
{
    QString s1 = propertiesString(1, tdt1);
    QString s2 = propertiesString(2, tdt2);
    return QString::fromLatin1("%1_%2\n  %5 %3\n  %6 %4").arg(tdt1->mTileset->mName).arg(tdt1->mID).arg(s1).arg(s2)
            .arg(QString::fromLatin1(use == 1 ? ">" : "   ")).arg(QString::fromLatin1(use == 2 ? ">" : "   "));
}

void TileDefCompare::readSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("TileDefCompare"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    QString file1 = settings.value(QLatin1String("file1")).toString();
    addRecentFile1(file1);
    ui->packEdit1->setCurrentIndex(0);
    QString file2 = settings.value(QLatin1String("file2")).toString();
    addRecentFile2(file2);
    ui->packEdit2->setCurrentIndex(0);
    settings.endGroup();
}

void TileDefCompare::writeSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("TileDefCompare"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.setValue(QLatin1String("file1"), ui->packEdit1->currentText());
    settings.setValue(QLatin1String("file2"), ui->packEdit2->currentText());
    settings.endGroup();
}

static Tiled::Tileset *findTileset(QString name)
{
    foreach (Tiled::Tileset *tileset, TileMetaInfoMgr::instance()->tilesets()) {
        if (tileset->name() == name)
            return tileset;
    }
    return 0;
}

QImage TileDefCompare::getTileImage(TileDefTile *tdt)
{
    if (Tiled::Tileset *ts = TileMetaInfoMgr::instance()->tileset(tdt->mTileset->mName)) {
        if (ts->isMissing()) {
            TileMetaInfoMgr::instance()->loadTilesets(QList<Tiled::Tileset*>() << ts, true);
            TilesetManager::instance()->waitForTilesets();
        }
        return ts->tileAt(tdt->id())->image();
    }
    return TilesetManager::instance()->missingTile()->image();
}

void TileDefCompare::addRecentFile1(const QString &fileName)
{
    // Remember the file by its canonical file path
    const QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();
    if (canonicalFilePath.isEmpty()) {
        setRecentFilesCombo1();
        return;
    }
    QStringList files = recentFiles1();
    files.removeAll(canonicalFilePath);
    files.prepend(canonicalFilePath);
    while (files.size() > MaxRecentFiles) {
        files.removeLast();
    }
    QSettings settings;
    settings.beginGroup(QLatin1String("TileDefCompare"));
    settings.setValue(QLatin1String("RecentFiles1"), files);
    settings.endGroup();
    setRecentFilesCombo1();
}

void TileDefCompare::addRecentFile2(const QString &fileName)
{
    // Remember the file by its canonical file path
    const QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();
    if (canonicalFilePath.isEmpty()) {
        setRecentFilesCombo2();
        return;
    }
    QStringList files = recentFiles2();
    files.removeAll(canonicalFilePath);
    files.prepend(canonicalFilePath);
    while (files.size() > MaxRecentFiles) {
        files.removeLast();
    }
    QSettings settings;
    settings.beginGroup(QLatin1String("TileDefCompare"));
    settings.setValue(QLatin1String("RecentFiles2"), files);
    settings.endGroup();
    setRecentFilesCombo2();
}

QStringList TileDefCompare::recentFiles1() const
{
    QSettings settings;
    settings.beginGroup(QLatin1String("TileDefCompare"));
    QStringList paths = settings.value(QLatin1String("RecentFiles1")).toStringList();
    settings.endGroup();
    return paths;
}

QStringList TileDefCompare::recentFiles2() const
{
    QSettings settings;
    settings.beginGroup(QLatin1String("TileDefCompare"));
    QStringList paths = settings.value(QLatin1String("RecentFiles2")).toStringList();
    settings.endGroup();
    return paths;
}

void TileDefCompare::setRecentFilesCombo1()
{
    ui->packEdit1->clear();
    ui->packEdit1->insertItems(0, recentFiles1());
    ui->packEdit1->setEnabled(ui->packEdit1->count() > 0);
    ui->packEdit1->setCurrentIndex(0);
}

void TileDefCompare::setRecentFilesCombo2()
{
    ui->packEdit2->clear();
    ui->packEdit2->insertItems(0, recentFiles2());
    ui->packEdit2->setEnabled(ui->packEdit2->count() > 0);
    ui->packEdit2->setCurrentIndex(0);
}

