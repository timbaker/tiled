#include "addvirtualtilesetdialog.h"
#include "ui_addvirtualtilesetdialog.h"

AddVirtualTilesetDialog::AddVirtualTilesetDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddVirtualTilesetDialog)
{
    ui->setupUi(this);

    ui->columnCount->setValue(8);
    ui->rowCount->setValue(8);
}

AddVirtualTilesetDialog::~AddVirtualTilesetDialog()
{
    delete ui;
}

QString AddVirtualTilesetDialog::name() const
{
    return ui->nameEdit->text();
}

int AddVirtualTilesetDialog::columnCount() const
{
    return ui->columnCount->value();
}

int AddVirtualTilesetDialog::rowCount() const
{
    return ui->rowCount->value();
}
