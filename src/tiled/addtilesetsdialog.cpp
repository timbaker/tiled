/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "addtilesetsdialog.h"
#include "ui_addtilesetsdialog.h"

#include <QDir>
#include <QImageReader>

AddTilesetsDialog::AddTilesetsDialog(const QString &dir,
                                     const QStringList &ignore,
                                     QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddTilesetsDialog),
    mDirectory(dir),
    mIgnore(ignore)
{
    ui->setupUi(this);

    connect(ui->checkAll, SIGNAL(clicked()), SLOT(checkAll()));
    connect(ui->uncheckAll, SIGNAL(clicked()), SLOT(uncheckAll()));

    setFilesList();
}

AddTilesetsDialog::~AddTilesetsDialog()
{
    delete ui;
}

QStringList AddTilesetsDialog::fileNames()
{
    QStringList ret;
    for (int i = 0; i < ui->files->count(); i++) {
        QListWidgetItem *item = ui->files->item(i);
        if (item->checkState() == Qt::Checked)
            ret += QDir(mDirectory).filePath(item->text());
    }
    return ret;
}

void AddTilesetsDialog::setFilesList()
{
    QDir dir(mDirectory);
    dir.setFilter(QDir::Files);
    dir.setSorting(QDir::Name);
    QStringList nameFilters;
    foreach (QByteArray format, QImageReader::supportedImageFormats())
        nameFilters += QLatin1String("*.") + QString::fromAscii(format);

    QFileInfoList fileInfoList = dir.entryInfoList(nameFilters);
    foreach (QFileInfo fileInfo, fileInfoList) {
        QString fileName = fileInfo.fileName();
        if (mIgnore.contains(fileInfo.completeBaseName()))
            continue;
        QListWidgetItem *item = new QListWidgetItem;
        item->setText(fileName);
        item->setCheckState(Qt::Unchecked);
        ui->files->addItem(item);
    }
}

void AddTilesetsDialog::checkAll()
{
    for (int i = 0; i < ui->files->count(); i++)
        ui->files->item(i)->setCheckState(Qt::Checked);
}

void AddTilesetsDialog::uncheckAll()
{
    for (int i = 0; i < ui->files->count(); i++)
        ui->files->item(i)->setCheckState(Qt::Unchecked);
}

void AddTilesetsDialog::accept()
{
    QDialog::accept();
}
