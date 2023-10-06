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

#ifndef EXPORTBASEMENTSDIALOG_H
#define EXPORTBASEMENTSDIALOG_H

#include <QDialog>
#include <QListWidgetItem>
#include <QStyledItemDelegate>

namespace Ui {
class ExportBasementsDialog;
}

namespace BuildingEditor {

class ExportBasementsDialog;

class ExportBasementsDelegate : public QStyledItemDelegate
{
public:
    ExportBasementsDelegate(ExportBasementsDialog *dialog, QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , mDialog(dialog)
    {

    }

protected:
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;

private:
    ExportBasementsDialog *mDialog;
};

/////

class ExportBasementsListWidget : public QListWidget
{
public:
    ExportBasementsListWidget(QWidget *parent = nullptr);

    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void setDialog(ExportBasementsDialog *dialog) {
        mDialog = dialog;
    }
private:
    ExportBasementsDialog * mDialog = nullptr;
    bool mMousePressed = false;
};

/////

class ExportBasementsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportBasementsDialog(QWidget *parent = nullptr);
    ~ExportBasementsDialog();

    void setPrompt(const QString &prompt);
    QStringList fileNames();

    QString exportDirectory() const
    { return mExportDirectory; }

public slots:
    int exec() override;

private:
    void setFilesList();
    void beforeClickCheckbox(const QModelIndex &index);
    void afterClickCheckbox(const QModelIndex &index);

private slots:
    void browseTBXDirectory();
    void browseExportDirectory();
    void itemChanged(QListWidgetItem *item);
    void itemSelectionChanged();
    void checkAll();
    void uncheckAll();
    void accept() override;

private:
    Ui::ExportBasementsDialog *ui;
    QString mTBXDirectory;
    QString mExportDirectory;
    QList<QListWidgetItem*> mSelection;
    bool mClickedItemIsChecked;
    bool mClickingInCheckbox = false;

    friend class ExportBasementsDelegate;
    friend class ExportBasementsListWidget;
};

} // namespace BuildingEditor

#endif // EXPORTBASEMENTSDIALOG_H
