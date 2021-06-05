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

#include "createpackdialog.h"
#include "ui_createpackdialog.h"

#include "texturepacker.h"
#include "zprogress.h"

#include "BuildingEditor/simplefile.h"

#include "preferences.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QToolBar>

#if defined(Q_OS_WIN) && (_MSC_VER >= 1600)
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

CreatePackDialog::CreatePackDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CreatePackDialog)
{
    ui->setupUi(this);

    QToolBar *tb = new QToolBar(this);
//    tb->setIconSize(QSize(16, 16));
    tb->addAction(ui->actionAddDirectory);
    tb->addAction(ui->actionRemoveDirectory);
    ui->directoryToolbarLayout->addWidget(tb);

    QToolBar *tb2 = new QToolBar(this);
    tb2->addAction(ui->actionAddTileDef);
    tb2->addAction(ui->actionRemoveTileDef);
    ui->tileDefToolbarLayout->addWidget(tb2);

    connect(ui->packNameEdit, SIGNAL(textChanged(QString)), SLOT(syncUI()));
    connect(ui->packNameBrowse, SIGNAL(clicked()), SLOT(savePackAs()));
    connect(ui->actionAddDirectory, SIGNAL(triggered()), SLOT(addDirectory()));
    connect(ui->actionRemoveDirectory, SIGNAL(triggered()), SLOT(removeDirectory()));
    connect(ui->dirList, SIGNAL(itemSelectionChanged()), SLOT(syncUI()));
    connect(ui->tileDefList, &QListWidget::itemSelectionChanged, this, &CreatePackDialog::syncUI);
    connect(ui->actionAddTileDef, &QAction::triggered, this, &CreatePackDialog::addTileDef);
    connect(ui->actionRemoveTileDef, &QAction::triggered, this, &CreatePackDialog::removeTileDef);
    connect(ui->btnLoad, SIGNAL(clicked()), SLOT(loadSettings()));
    connect(ui->btnSave, SIGNAL(clicked()), SLOT(saveSettings()));
    connect(ui->btnSaveAs, SIGNAL(clicked()), SLOT(saveSettingsAs()));
    connect(ui->jumboX, SIGNAL(valueChanged(int)), SLOT(tileSizeChangedX(int)));
    connect(ui->jumboY, SIGNAL(valueChanged(int)), SLOT(tileSizeChangedY(int)));

    ui->texSizeCombo->setCurrentIndex(1);

    readSettings();

    syncUI();
}

CreatePackDialog::~CreatePackDialog()
{
    delete ui;
}

void CreatePackDialog::settingsToUI(const TexturePackSettings &settings)
{
    ui->packNameEdit->setText(QDir::toNativeSeparators(settings.mPackFileName));

    ui->dirList->clear();
    for (const TexturePackSettings::Directory &tpd : settings.mInputImageDirectories) {
        QListWidgetItem *item = new QListWidgetItem(QDir::toNativeSeparators(tpd.mPath));
        item->setCheckState(tpd.mImagesAreTilesheets ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, tpd.mCustomTileSize);
        ui->dirList->addItem(item);
    }

    if (settings.mOutputImageSize == QSize(512, 512))
        ui->texSizeCombo->setCurrentIndex(0);
    else if (settings.mOutputImageSize == QSize(2048, 2048))
        ui->texSizeCombo->setCurrentIndex(2);
    else
        ui->texSizeCombo->setCurrentIndex(1); // default 1024x1024

    ui->scale50->setChecked(settings.mScale50);

    ui->tileDefList->clear();
    for (const QString &fileName : settings.mTileDefFiles) {
        QListWidgetItem *item = new QListWidgetItem(QDir::toNativeSeparators(fileName));
        ui->tileDefList->addItem(item);
    }
}

void CreatePackDialog::settingsFromUI(TexturePackSettings &settings)
{
    settings = TexturePackSettings();
    for (int i = 0; i < ui->dirList->count(); i++) {
        TexturePackSettings::Directory tpd;
        QListWidgetItem *item = ui->dirList->item(i);
        tpd.mPath = item->text();
        tpd.mImagesAreTilesheets = item->checkState() == Qt::Checked;
        tpd.mCustomTileSize = item->data(Qt::UserRole).value<QSize>();
        settings.mInputImageDirectories += tpd;
    }
    if (ui->texSizeCombo->currentIndex() == 0)
        settings.mOutputImageSize = QSize(512, 512);
    else if (ui->texSizeCombo->currentIndex() == 2)
        settings.mOutputImageSize = QSize(2048, 2048);
    else
        settings.mOutputImageSize = QSize(1024, 1024);
    settings.mScale50 = ui->scale50->isChecked();
    settings.mPackFileName = ui->packNameEdit->text();
    settings.padding = 2;

    for (int i = 0; i < ui->tileDefList->count(); i++) {
        QListWidgetItem *item = ui->tileDefList->item(i);
        settings.mTileDefFiles += item->text();
    }
}

void CreatePackDialog::savePackAs()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save .pack file As"),
                                                ui->packNameEdit->text(),
                                                QLatin1String("PZ .pack files (*.pack)"));
    if (fileName.isEmpty())
        return;

    ui->packNameEdit->setText(QDir::toNativeSeparators(fileName));
    syncUI();
}

void CreatePackDialog::addDirectory()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Choose image directory"));
    if (path.isEmpty())
        return;

    QListWidgetItem *item = new QListWidgetItem(QDir::toNativeSeparators(path));
    item->setCheckState(Qt::Checked);
    item->setData(Qt::UserRole, QSize(64*2, 128*2));
    ui->dirList->addItem(item);
}

void CreatePackDialog::removeDirectory()
{
    QList<QListWidgetItem*> items = ui->dirList->selectedItems();
    if (items.size() == 1) {
        int row = ui->dirList->row(items.first());
        delete ui->dirList->takeItem(row);
    }
    syncUI();
}

void CreatePackDialog::addTileDef()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Choose .tiles file"), QString(), QStringLiteral("*.tiles"));
    if (path.isEmpty())
        return;

    QListWidgetItem *item = new QListWidgetItem(QDir::toNativeSeparators(path));
    ui->tileDefList->addItem(item);
}

void CreatePackDialog::removeTileDef()
{
    QList<QListWidgetItem*> items = ui->tileDefList->selectedItems();
    if (items.size() == 1) {
        int row = ui->tileDefList->row(items.first());
        delete ui->tileDefList->takeItem(row);
    }
    syncUI();
}

void CreatePackDialog::syncUI()
{
    QList<QListWidgetItem*> items = ui->dirList->selectedItems();
    ui->actionRemoveDirectory->setEnabled(items.size() == 1);
    ui->btnSave->setEnabled(!mSettingsFileName.isEmpty());
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!ui->packNameEdit->text().isEmpty());

    if (items.size() == 1) {
        ui->jumboX->setEnabled(true);
        ui->jumboY->setEnabled(true);
        QListWidgetItem *item = items.first();
        ui->jumboX->setValue(item->data(Qt::UserRole).value<QSize>().width());
        ui->jumboY->setValue(item->data(Qt::UserRole).value<QSize>().height());
    } else {
        ui->jumboX->setEnabled(false);
        ui->jumboY->setEnabled(false);
    }

    items = ui->tileDefList->selectedItems();
    ui->actionRemoveTileDef->setEnabled(items.size() == 1);
}

void CreatePackDialog::tileSizeChangedX(int value)
{
    QList<QListWidgetItem*> items = ui->dirList->selectedItems();
    if (items.size() == 1) {
        QListWidgetItem *item = items.first();
        QSize size = item->data(Qt::UserRole).value<QSize>();
        size.setWidth(value);
        item->setData(Qt::UserRole, size);
    }
}

void CreatePackDialog::tileSizeChangedY(int value)
{
    QList<QListWidgetItem*> items = ui->dirList->selectedItems();
    if (items.size() == 1) {
        QListWidgetItem *item = items.first();
        QSize size = item->data(Qt::UserRole).value<QSize>();
        size.setHeight(value);
        item->setData(Qt::UserRole, size);
    }
}

void CreatePackDialog::loadSettings()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open settings file"),
                                                    ui->settingsName->text(),
                                                    QLatin1String("Pack settings file (*.txt)"));
    if (fileName.isEmpty())
        return;

    PackSettingsFile file;
    if (!file.read(fileName)) {
        QMessageBox::warning(this, tr("Error reading settings"), file.errorString());
        return;
    }

    mSettingsFileName = fileName;
    ui->settingsName->setText(QDir::toNativeSeparators(mSettingsFileName));
    settingsToUI(file.settings());
}

void CreatePackDialog::saveSettings()
{
    TexturePackSettings settings;
    settingsFromUI(settings);

    PackSettingsFile file;
    file.setSettings(settings);
    if (!file.write(mSettingsFileName)) {
        QMessageBox::warning(this, tr("Error saving settings"), file.errorString());
        return;
    }
}

void CreatePackDialog::saveSettingsAs()
{
    QString suggested = mSettingsFileName;
    if (suggested.isEmpty()) {
        suggested = ui->packNameEdit->text();
        if (!suggested.isEmpty()) {
            QFileInfo fileInfo(suggested);
            suggested = fileInfo.absolutePath() + QLatin1String("/") +  fileInfo.baseName() + QLatin1String(".txt");
        } else {
            suggested = QLatin1String("MyPackFile.txt");
        }
    }
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save settings As"),
                                                    suggested,
                                                    QLatin1String("Pack settings file (*.txt)"));
    if (fileName.isEmpty())
        return;

    TexturePackSettings settings;
    settingsFromUI(settings);
    PackSettingsFile file;
    file.setSettings(settings);
    if (!file.write(fileName)) {
        QMessageBox::warning(this, tr("Error saving settings"), file.errorString());
        return;
    }

    mSettingsFileName = fileName;
    ui->settingsName->setText(QDir::toNativeSeparators(mSettingsFileName));
    syncUI();
}

void CreatePackDialog::accept()
{
    TexturePackSettings settings;
    settingsFromUI(settings);

    PROGRESS progress(tr("Packing..."));

    TexturePacker packer;
    if (!packer.pack(settings)) {
        QMessageBox::warning(this, tr("Error creating .pack file"), packer.errorString());
    }

    writeSettings();

    QDialog::accept();
}

void CreatePackDialog::readSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("CreatePackDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    QString fileName = settings.value(QLatin1String("file")).toString();
    PackSettingsFile file;
    if (!fileName.isEmpty() && file.read(fileName)) {
        mSettingsFileName = fileName;
        ui->settingsName->setText(QDir::toNativeSeparators(mSettingsFileName));
        settingsToUI(file.settings());
    }
    settings.endGroup();
}

void CreatePackDialog::writeSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("CreatePackDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.setValue(QLatin1String("file"), mSettingsFileName);
    settings.endGroup();
}

/////

PackSettingsFile::PackSettingsFile()
{

}

bool PackSettingsFile::read(const QString &fileName)
{
    SimpleFile simple;
    if (!simple.read(fileName)) {
        mError = simple.errorString();
        return false;
    }

    mSettings = TexturePackSettings();

    QDir dir = QFileInfo(fileName).absoluteDir();

    for (const SimpleFileBlock &block : qAsConst(simple.blocks)) {
        if (block.name == QLatin1String("settings")) {
            mSettings.mPackFileName = QDir::cleanPath(dir.filePath(block.value("packFileName")));

            QString sizeStr = block.value("outputImageSize");
            if (!stringToSize(sizeStr, mSettings.mOutputImageSize))
                return false;
            if (mSettings.mOutputImageSize.isEmpty()) {
                mError = tr("invalid outputImageSize '%1'").arg(sizeStr);
                return false;
            }

            QString scaleStr = block.value("scale50");
            mSettings.mScale50 = (scaleStr == QStringLiteral("true"));

            for (const SimpleFileBlock &block2 : qAsConst(block.blocks)) {
                if (block2.name == QLatin1String("inputImageDirectory")) {
                    TexturePackSettings::Directory tpd;
                    tpd.mPath = QDir::cleanPath(dir.filePath(block2.value("path")));
                    tpd.mImagesAreTilesheets = block2.value("imagesAreTilesheets") == QLatin1String("true");
                    if (block2.hasValue("tileSize")) {
                        if (!stringToSize(block2.value("tileSize"), tpd.mCustomTileSize))
                            return false;
                    } else {
                        tpd.mCustomTileSize = QSize(0, 0);
                    }
                    mSettings.mInputImageDirectories += tpd;
                } else if (block2.name == QLatin1String("tileDef")) {
                    QString path = QDir::cleanPath(dir.filePath(block2.value("path")));
                    mSettings.mTileDefFiles += path;
                } else {
                    mError = tr("Line %1: Unknown block name '%2'.")
                            .arg(block2.lineNumber)
                            .arg(block2.name);
                    return false;
                }
            }
        } else {
            mError = tr("Line %1: Unknown block name '%2'.")
                    .arg(block.lineNumber)
                    .arg(block.name);
            return false;
        }
    }

    bool contains = false;
    for (const QString &filePath : mSettings.mTileDefFiles) {
        if (filePath.endsWith(QStringLiteral("newtiledefinitions.tiles"))) {
            contains = true;
            break;
        }
    }
    if (contains == false) {
        QFileInfo fileInfo(Tiled::Internal::Preferences::instance()->tilesDirectory() + QString::fromLatin1("/newtiledefinitions.tiles"));
        if (fileInfo.exists()) {
            mSettings.mTileDefFiles += fileInfo.absoluteFilePath();
        }
    }

    return true;
}

bool PackSettingsFile::write(const QString &fileName)
{
    SimpleFile simpleFile;

    QDir dir = QFileInfo(fileName).absoluteDir();

    SimpleFileBlock settingsBlock;
    settingsBlock.name = QLatin1String("settings");

    QString relativePath = dir.relativeFilePath(mSettings.mPackFileName);
    settingsBlock.addValue("packFileName", relativePath);
    settingsBlock.addValue("outputImageSize", QString::fromLatin1("%1,%2")
                           .arg(mSettings.mOutputImageSize.width())
                           .arg(mSettings.mOutputImageSize.height()));
    settingsBlock.addValue("scale50", QLatin1Literal(mSettings.mScale50 ? "true" : "false"));

    for (const TexturePackSettings::Directory &tpd : mSettings.mInputImageDirectories) {
        SimpleFileBlock dirBlock;
        dirBlock.name = QLatin1String("inputImageDirectory");
        dirBlock.addValue("path", dir.relativeFilePath(tpd.mPath));
        dirBlock.addValue("imagesAreTilesheets", QLatin1String(tpd.mImagesAreTilesheets ? "true" : "false"));
        if (tpd.mCustomTileSize != QSize(64*2, 128*2) && tpd.mCustomTileSize != QSize(0, 0))
            dirBlock.addValue("tileSize", QString::fromLatin1("%1,%2")
                              .arg(tpd.mCustomTileSize.width())
                              .arg(tpd.mCustomTileSize.height()));
        settingsBlock.blocks += dirBlock;
    }

    for (const QString &fileName : mSettings.mTileDefFiles) {
        SimpleFileBlock tileDefBlock;
        tileDefBlock.name = QStringLiteral("tileDef");
        tileDefBlock.addValue("path", dir.relativeFilePath(fileName));
        settingsBlock.blocks += tileDefBlock;
    }

    simpleFile.blocks += settingsBlock;

//    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;

}

bool PackSettingsFile::stringToSize(const QString &s, QSize &result)
{
    QStringList split = s.split(QLatin1Char(','), QString::SkipEmptyParts);
    if (split.size() != 2) {
        mError = tr("expected w,h but got '%1'").arg(s);
        return false;
    }
    result.setWidth(split[0].toInt());
    result.setHeight(split[1].toInt());
    return true;
}

