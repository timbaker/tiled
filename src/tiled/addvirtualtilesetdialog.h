#ifndef ADDVIRTUALTILESETDIALOG_H
#define ADDVIRTUALTILESETDIALOG_H

#include <QDialog>

namespace Ui {
class AddVirtualTilesetDialog;
}

class AddVirtualTilesetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddVirtualTilesetDialog(QWidget *parent = 0);
    ~AddVirtualTilesetDialog();

    QString name() const;
    int columnCount() const;
    int rowCount() const;

private:
    Ui::AddVirtualTilesetDialog *ui;
};

#endif // ADDVIRTUALTILESETDIALOG_H
