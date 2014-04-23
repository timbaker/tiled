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

#include "packcompare.h"
#include "ui_packcompare.h"

#include "zprogress.h"

#include <QFileDialog>
#include <QMessageBox>

PackCompare::PackCompare(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::PackCompare)
{
    ui->setupUi(this);

    connect(ui->packBrowse1, SIGNAL(clicked()), SLOT(browse1()));
    connect(ui->packBrowse2, SIGNAL(clicked()), SLOT(browse2()));
    connect(ui->compare, SIGNAL(clicked()), SLOT(compare()));
}

PackCompare::~PackCompare()
{
    delete ui;
}

void PackCompare::browse1()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose Pack File"),
                                             ui->packEdit1->text(),
                                             tr("Pack files (*.pack)"));
    if (!f.isEmpty()) {
        ui->packEdit1->setText(QDir::toNativeSeparators(f));
    }
}

void PackCompare::browse2()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose Pack File"),
                                             ui->packEdit2->text(),
                                             tr("Pack files (*.pack)"));
    if (!f.isEmpty()) {
        ui->packEdit2->setText(QDir::toNativeSeparators(f));
    }
}

void PackCompare::compare()
{
    PROGRESS progress(tr("Reading pack 1"), this);
    if (!mPackFile1.read(ui->packEdit1->text())) {
        QMessageBox::warning(this, tr("Error reading .pack file"), mPackFile1.errorString());
        return;
    }

    progress.update(tr("Reading pack 2"));
    if (!mPackFile2.read(ui->packEdit2->text())) {
        QMessageBox::warning(this, tr("Error reading .pack file"), mPackFile2.errorString());
        return;
    }

    QSet<QString> in1, in2, unique1, unique2;
    foreach (PackPage page, mPackFile1.pages()) {
        foreach (PackSubTexInfo tex, page.mInfo)
            in1.insert(tex.name);
    }
    foreach (PackPage page, mPackFile2.pages()) {
        foreach (PackSubTexInfo tex, page.mInfo)
            in2.insert(tex.name);
    }
    unique1 = in1 - in2;
    unique2 = in2 - in1;

    ui->textBrowser->clear();

    ui->textBrowser->insertHtml(QLatin1String("<b>Unique textures in pack 1:</b><br>"));
    QStringList sorted1 = QStringList(unique1.values());
    sorted1.sort();
    foreach (QString s, sorted1) {
        ui->textBrowser->insertPlainText(s + QLatin1String("\n"));
    }

    ui->textBrowser->insertHtml(QLatin1String("<br><b>Unique textures in pack 2:</b><br>"));
    QStringList sorted2 = QStringList(unique2.values());
    sorted2.sort();
    foreach (QString s, sorted2) {
        ui->textBrowser->insertPlainText(s + QLatin1String("\n"));
    }
}
