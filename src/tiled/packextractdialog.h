#ifndef PACKEXTRACTDIALOG_H
#define PACKEXTRACTDIALOG_H

class PackFile;

#include <QDialog>

namespace Ui {
class PackExtractDialog;
}

class PackExtractDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PackExtractDialog(PackFile &packFile, QWidget *parent = 0);
    ~PackExtractDialog();

public slots:
    void browse();
    void accept();

private:
    Ui::PackExtractDialog *ui;
    PackFile &mPackFile;
};

#endif // PACKEXTRACTDIALOG_H
