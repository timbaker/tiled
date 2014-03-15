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

#include "BuildingEditor/simplefile.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QToolBar>

CreatePackDialog::CreatePackDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CreatePackDialog)
{
    ui->setupUi(this);

    QToolBar *tb = new QToolBar;
//    tb->setIconSize(QSize(16, 16));
    tb->addAction(ui->actionAddDirectory);
    tb->addAction(ui->actionRemoveDirectory);
    ui->directoryToolbarLayout->addWidget(tb);

    connect(ui->packNameEdit, SIGNAL(textChanged(QString)), SLOT(syncUI()));
    connect(ui->packNameBrowse, SIGNAL(clicked()), SLOT(savePackAs()));
    connect(ui->actionAddDirectory, SIGNAL(triggered()), SLOT(addDirectory()));
    connect(ui->actionRemoveDirectory, SIGNAL(triggered()), SLOT(removeDirectory()));
    connect(ui->dirList, SIGNAL(itemSelectionChanged()), SLOT(syncUI()));
    connect(ui->btnLoad, SIGNAL(clicked()), SLOT(loadSettings()));
    connect(ui->btnSave, SIGNAL(clicked()), SLOT(saveSettings()));
    connect(ui->btnSaveAs, SIGNAL(clicked()), SLOT(saveSettingsAs()));

    ui->texSizeCombo->setCurrentIndex(1);

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
    foreach (TexturePackSettings::Directory tpd, settings.mInputImageDirectories) {
        QListWidgetItem *item = new QListWidgetItem(QDir::toNativeSeparators(tpd.mPath));
        item->setCheckState(tpd.mImagesAreTilesheets ? Qt::Checked : Qt::Unchecked);
        ui->dirList->addItem(item);
    }

    if (settings.mOutputImageSize == QSize(512, 512))
        ui->texSizeCombo->setCurrentIndex(0);
    else if (settings.mOutputImageSize == QSize(2048, 2048))
        ui->texSizeCombo->setCurrentIndex(2);
    else
        ui->texSizeCombo->setCurrentIndex(1); // default 1024x1024
}

void CreatePackDialog::settingsFromUI(TexturePackSettings &settings)
{
    settings = TexturePackSettings();
    for (int i = 0; i < ui->dirList->count(); i++) {
        TexturePackSettings::Directory tpd;
        tpd.mPath = ui->dirList->item(i)->text();
        tpd.mImagesAreTilesheets = ui->dirList->item(i)->checkState() == Qt::Checked;
        settings.mInputImageDirectories += tpd;
    }
    settings.padding = 1;
    if (ui->texSizeCombo->currentIndex() == 0)
        settings.mOutputImageSize = QSize(512, 512);
    else if (ui->texSizeCombo->currentIndex() == 2)
        settings.mOutputImageSize = QSize(2048, 2048);
    else
        settings.mOutputImageSize = QSize(1024, 1024);
    settings.mPackFileName = ui->packNameEdit->text();
    settings.padding = 1;
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

void CreatePackDialog::syncUI()
{
    QList<QListWidgetItem*> items = ui->dirList->selectedItems();
    ui->actionRemoveDirectory->setEnabled(items.size() == 1);
    ui->btnSave->setEnabled(!mSettingsFileName.isEmpty());
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!ui->packNameEdit->text().isEmpty());
}

void CreatePackDialog::loadSettings()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open settings file"),
                                                    ui->label->text(),
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

    TexturePacker packer;
    if (!packer.pack(settings)) {
        QMessageBox::warning(this, tr("Error creating .pack file"), packer.errorString());
    }

    QDialog::accept();
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

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("settings")) {
            mSettings.mPackFileName = QDir::cleanPath(dir.filePath(block.value("packFileName")));

            QString sizeStr = block.value("outputImageSize");
            if (!stringToSize(sizeStr, mSettings.mOutputImageSize))
                return false;
            if (mSettings.mOutputImageSize.isEmpty()) {
                mError = tr("invalid outputImageSize '%1'").arg(sizeStr);
                return false;
            }

            foreach (SimpleFileBlock block2, block.blocks) {
                if (block2.name == QLatin1String("inputImageDirectory")) {
                    TexturePackSettings::Directory tpd;
                    tpd.mPath = QDir::cleanPath(dir.filePath(block2.value("path")));
                    tpd.mImagesAreTilesheets = block2.value("imagesAreTilesheets") == QLatin1String("true");
                    mSettings.mInputImageDirectories += tpd;
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

    foreach (TexturePackSettings::Directory tpd, mSettings.mInputImageDirectories) {
        SimpleFileBlock dirBlock;
        dirBlock.name = QLatin1String("inputImageDirectory");
        dirBlock.addValue("path", dir.relativeFilePath(tpd.mPath));
        dirBlock.addValue("imagesAreTilesheets", QLatin1String(tpd.mImagesAreTilesheets ? "true" : "false"));
        settingsBlock.blocks += dirBlock;
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

