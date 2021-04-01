#include "roomdefnamedialog.h"
#include "ui_roomdefnamedialog.h"

#include "mapobject.h"
#include "objectgroup.h"

using namespace Tiled;
using namespace Tiled::Internal;

RoomDefNameDialog::RoomDefNameDialog(const QList<ObjectGroup *> &ogList,
                                     const QString &name, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RoomDefNameDialog)
{
    ui->setupUi(this);

    connect(ui->names, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
            SLOT(currentChanged(QListWidgetItem*)));
    connect(ui->names, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            SLOT(doubleClicked(QListWidgetItem*)));

    ui->name->setText(name.mid(0, name.indexOf(QLatin1Char('#'))));

    QSet<QString> names;
    names << QLatin1String("bedroom")
          << QLatin1String("bathroom")
          << QLatin1String("closet")
          << QLatin1String("diningroom")
          << QLatin1String("livingroom")
          << QLatin1String("foyer")
          << QLatin1String("hall")
          << QLatin1String("kitchen")
          << QLatin1String("shed")
          << QLatin1String("shop")
          << QLatin1String("storeroom")
          << QLatin1String("office");
    foreach (ObjectGroup *og, ogList) {
        if (!og->name().endsWith(QLatin1String("RoomDefs")))
            continue;
        foreach (MapObject *object, og->objects()) {
            QString name = object->name();
            if (name.startsWith(QLatin1String("room")))
                continue;
            name = name.mid(0, name.indexOf(QLatin1Char('#')));
            if (!name.isEmpty())
                names += name;
        }
    }

    QStringList sorted(names.constBegin(), names.constEnd());
    sorted.sort();
    ui->names->addItems(sorted);
}

RoomDefNameDialog::~RoomDefNameDialog()
{
    delete ui;
}

QString RoomDefNameDialog::name() const
{
    return ui->name->text();
}

void RoomDefNameDialog::currentChanged(QListWidgetItem *item)
{
    ui->name->setText(item->text());
}

void RoomDefNameDialog::doubleClicked(QListWidgetItem *item)
{
    ui->name->setText(item->text());
    accept();
}
