#ifndef ROOMDEFNAMEDIALOG_H
#define ROOMDEFNAMEDIALOG_H

#include <QDialog>

class QListWidgetItem;

namespace Ui {
class RoomDefNameDialog;
}

namespace Tiled {
class ObjectGroup;

namespace Internal {

class RoomDefNameDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit RoomDefNameDialog(QList<ObjectGroup*> &ogList, const QString &name, QWidget *parent = 0);
    ~RoomDefNameDialog();

    QString name() const;
    
private slots:
    void currentChanged(QListWidgetItem *item);
    void doubleClicked(QListWidgetItem *item);

private:
    Ui::RoomDefNameDialog *ui;
};

} // namespace Internal
} // namespace Tiled

#endif // ROOMDEFNAMEDIALOG_H
