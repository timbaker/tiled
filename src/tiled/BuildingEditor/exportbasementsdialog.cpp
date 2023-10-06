/*
 * Copyright 2023, Tim Baker <treectrl@users.sf.net>
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

#include "exportbasementsdialog.h"
#include "ui_exportbasementsdialog.h"

#include "buildingpreferences.h"

#include <QDir>
#include <QFileDialog>
#include <QMouseEvent>

using namespace BuildingEditor;

ExportBasementsDialog::ExportBasementsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ExportBasementsDialog)
{
    ui->setupUi(this);

    ui->files->setDialog(this);

    ExportBasementsDelegate *delegate = new ExportBasementsDelegate(this, ui->files);
    delete ui->files->itemDelegate();
    ui->files->setItemDelegate(delegate);

    connect(ui->browseTBXDir, &QAbstractButton::clicked, this, &ExportBasementsDialog::browseTBXDirectory);
    connect(ui->browseExportDir, &QAbstractButton::clicked, this, &ExportBasementsDialog::browseExportDirectory);
    connect(ui->files, &QListWidget::itemChanged, this, &ExportBasementsDialog::itemChanged);
    connect(ui->files, &QListWidget::itemSelectionChanged, this, &ExportBasementsDialog::itemSelectionChanged);
    connect(ui->checkAll, &QAbstractButton::clicked, this, &ExportBasementsDialog::checkAll);
    connect(ui->uncheckAll, &QAbstractButton::clicked, this, &ExportBasementsDialog::uncheckAll);

    setPrompt(QString());

    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QStringLiteral("ExportBasementsDialog"));
    mTBXDirectory = settings.value(QStringLiteral("TBXDirectory")).toString();
    ui->tbxPath->setText(mTBXDirectory);
    mExportDirectory = settings.value(QStringLiteral("ExportDirectory")).toString();
    ui->exportPath->setText(mExportDirectory);
    settings.endGroup();

    setFilesList();
}

ExportBasementsDialog::~ExportBasementsDialog()
{
    delete ui;
}

void ExportBasementsDialog::setPrompt(const QString &prompt)
{
    if (prompt.isEmpty()) {
        ui->prompt->hide();
    } else {
        ui->prompt->setText(prompt);
    }
}

QStringList ExportBasementsDialog::fileNames()
{
    QStringList ret;
    for (int i = 0; i < ui->files->count(); i++) {
        QListWidgetItem *item = ui->files->item(i);
        if (item->checkState() == Qt::Checked) {
            QString fileName = QDir(mTBXDirectory).filePath(item->text());
            ret += fileName;
        }
    }
    return ret;
}

void ExportBasementsDialog::setFilesList()
{
    ui->files->clear();

    QDir dir(mTBXDirectory);
    if (dir.exists() == false) {
        return;
    }
    dir.setFilter(QDir::Files);
    dir.setSorting(QDir::Name);
    QStringList nameFilters;
    nameFilters += QLatin1String("*.tbx");

    QFileInfoList fileInfoList = dir.entryInfoList(nameFilters);
    for (const QFileInfo& fileInfo : fileInfoList) {
        QString fileName = fileInfo.fileName();
        QListWidgetItem *item = new QListWidgetItem;
        item->setText(fileName);
        item->setCheckState(Qt::Unchecked);
        ui->files->addItem(item);
    }
}

void ExportBasementsDialog::beforeClickCheckbox(const QModelIndex &index)
{
    QListWidgetItem *item = ui->files->item(index.row());
    mSelection = ui->files->selectedItems();
    if (mSelection.isEmpty() == false && ui->files->selectionModel()->isSelected(index) == false) {
        mSelection.clear();
        mSelection << item;
    }
    mClickedItemIsChecked = item->checkState() == Qt::CheckState::Checked;
    mClickingInCheckbox = true;
}

void ExportBasementsDialog::afterClickCheckbox(const QModelIndex &index)
{
    Q_UNUSED(index)
    for (QListWidgetItem *item : mSelection) {
        item->setCheckState(mClickedItemIsChecked ? Qt::CheckState::Unchecked : Qt::CheckState::Checked);
        item->setSelected(true);
    }
    mSelection.clear();
    mClickingInCheckbox = false;
}

int ExportBasementsDialog::exec()
{
    return QDialog::exec();
}

void ExportBasementsDialog::browseTBXDirectory()
{
    QString f = QFileDialog::getExistingDirectory(this, QString(), ui->tbxPath->text());
    if (f.isEmpty()) {
        return;
    }
    mTBXDirectory = f;
    ui->tbxPath->setText(QDir::toNativeSeparators(f));

    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QStringLiteral("ExportBasementsDialog"));
    settings.setValue(QStringLiteral("TBXDirectory"), mTBXDirectory);
    settings.endGroup();

    setFilesList();
}

void ExportBasementsDialog::browseExportDirectory()
{
    QString f = QFileDialog::getExistingDirectory(this, QString(), ui->exportPath->text());
    if (f.isEmpty()) {
        return;
    }
    mExportDirectory = f;
    ui->exportPath->setText(QDir::toNativeSeparators(f));

    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QStringLiteral("ExportBasementsDialog"));
    settings.setValue(QStringLiteral("ExportDirectory"), mExportDirectory);
    settings.endGroup();
}

void ExportBasementsDialog::itemChanged(QListWidgetItem *item)
{
    if (item->checkState() == Qt::CheckState::Checked) {
        if (item->isSelected()) {
        }
    }
}

void ExportBasementsDialog::itemSelectionChanged()
{
    if (mClickingInCheckbox) {
        // Clicking and dragging in a checkbox changes the selection.
        ui->files->blockSignals(true);
        ui->files->clearSelection();
        for (QListWidgetItem *item : mSelection) {
            item->setSelected(true);
        }
        ui->files->blockSignals(false);
    }
}

void ExportBasementsDialog::checkAll()
{
    for (int i = 0; i < ui->files->count(); i++) {
        ui->files->item(i)->setCheckState(Qt::Checked);
    }
}

void ExportBasementsDialog::uncheckAll()
{
    for (int i = 0; i < ui->files->count(); i++) {
        ui->files->item(i)->setCheckState(Qt::Unchecked);
    }
}

void ExportBasementsDialog::accept()
{
    QDialog::accept();
}

/////

bool ExportBasementsDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() == QEvent::Type::MouseButtonPress) {
        QMouseEvent *event1 = static_cast<QMouseEvent*>(event);
        if (event1->button() == Qt::MouseButton::LeftButton) {
            auto option1 = QStyleOptionViewItem(option);
            initStyleOption(&option1, index);
            QRect elementRect = option1.widget->style()->subElementRect(QStyle::SubElement::SE_ItemViewItemCheckIndicator, &option1, option1.widget);
            if (elementRect.contains(event1->pos())) {
                mDialog->beforeClickCheckbox(index);
                return false;
            }
        }
    }
    if (event->type() == QEvent::Type::MouseButtonRelease) {
        QMouseEvent *event1 = static_cast<QMouseEvent*>(event);
        if (event1->button() == Qt::MouseButton::LeftButton) {
            auto option1 = QStyleOptionViewItem(option);
            initStyleOption(&option1, index);
            QRect elementRect = option1.widget->style()->subElementRect(QStyle::SubElement::SE_ItemViewItemCheckIndicator, &option1, option1.widget);
            if (elementRect.contains(event1->pos())) {
                return false;
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

/////

ExportBasementsListWidget::ExportBasementsListWidget(QWidget *parent)
    : QListWidget(parent)
{

}

void ExportBasementsListWidget::mousePressEvent(QMouseEvent *event)
{
    mMousePressed = true;
    QListWidget::mousePressEvent(event);
}

void ExportBasementsListWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QListWidget::mouseReleaseEvent(event);
    if (mMousePressed) {
        mMousePressed = false;
        if (mDialog->mClickingInCheckbox) {
            mDialog->afterClickCheckbox(QModelIndex());
        }
    }
}
