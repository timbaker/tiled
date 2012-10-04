#include "converttolotdialog.h"
#include "ui_converttolotdialog.h"

#include "map.h"
#include "mapdocument.h"
#include "objectgroup.h"
#include "preferences.h"

#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>

using namespace Tiled;
using namespace Internal;

static const char * const KEY_OBJECT_GROUP = "ConvertToLot/ObjectGroup";
static const char * const KEY_EMPTY_LEVELS = "ConvertToLot/EmptyLevels";
static const char * const KEY_ERASE_SOURCE = "ConvertToLot/EraseSource";

ConvertToLotDialog::ConvertToLotDialog(const MapDocument *mapDocument,
                                       QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ConvertToLotDialog)
{
    ui->setupUi(this);

    // mapPath is the file path of the source map before conversion.
    QFileInfo info(mapDocument->fileName());
    QFileInfo info2;
    int n = 1;
    do {
        info2.setFile(info.absoluteDir(), QString(QLatin1String("%1_lot%2.tmx"))
                      .arg(info.completeBaseName())
                      .arg(n));
        ++n;
    } while (info2.exists());

    ui->mapNameEdit->setText(info2.absoluteFilePath());

    QSettings *s = Preferences::instance()->settings();
    QString layerName = s->value(QLatin1String(KEY_OBJECT_GROUP)).toString();

    ui->layerList->clear();
    mObjectGroup = 0;
    foreach (ObjectGroup *og, mapDocument->map()->objectGroups()) {
        ui->layerList->insertItem(0, og->name());
        mObjectGroups.prepend(og);
        if ((og->name() == layerName) ||
                ((ui->layerList->currentRow() < 0) && (og->level() == 0))) {
            ui->layerList->setCurrentRow(0);
            mObjectGroup = og;
        }
    }

    mEmptyLevels = s->value(QLatin1String(KEY_EMPTY_LEVELS), true).toBool();
    mEraseSource = s->value(QLatin1String(KEY_ERASE_SOURCE), true).toBool();

    ui->emptyLevels->setChecked(mEmptyLevels);
    ui->eraseSource->setChecked(mEraseSource);
}

void ConvertToLotDialog::accept()
{
    mMapPath = ui->mapNameEdit->text();
    mEmptyLevels = ui->emptyLevels->isChecked();
    mEraseSource = ui->eraseSource->isChecked();

    int row = ui->layerList->currentRow();
    mObjectGroup = (row > 0) ? mObjectGroups.at(row) : 0;

    if (mMapPath.isEmpty()) {
        QMessageBox::warning(this, tr("No Filename"),
                             tr("Please enter a filename for the lot file."));
        return;
    }

    // TODO: If replacing a map that was already loaded, must reload the map!
    // Disallow for now.
    if (QFileInfo(mMapPath).exists()) {
        QMessageBox::warning(this, tr("File Exists"),
                             tr("That file already exists, please choose another."));
        return;
    }

    if (QFileInfo(mMapPath).isRelative())
        mMapPath = QFileInfo(mMapPath).absoluteFilePath();
    if (QFileInfo(mMapPath).suffix() != QLatin1String("tmx"))
        mMapPath += QLatin1String(".tmx");

    QSettings *s = Preferences::instance()->settings();
    s->setValue(QLatin1String(KEY_OBJECT_GROUP),
                mObjectGroup ? mObjectGroup->name() : QString());
    s->setValue(QLatin1String(KEY_EMPTY_LEVELS), mEmptyLevels);
    s->setValue(QLatin1String(KEY_ERASE_SOURCE), mEraseSource);

    QDialog::accept();
}
