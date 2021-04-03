#include "converttolotdialog.h"
#include "ui_converttolotdialog.h"

#include "map.h"
#include "mapdocument.h"
#include "objectgroup.h"
#include "preferences.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>

using namespace Tiled;
using namespace Internal;

static const char * const KEY_OBJECT_GROUP = "ConvertToLot/ObjectGroup";
static const char * const KEY_EMPTY_LEVELS = "ConvertToLot/EmptyLevels";
static const char * const KEY_ERASE_SOURCE = "ConvertToLot/EraseSource";
static const char * const KEY_ORIENT_LEVELS = "ConvertToLot/LevelIsometric";
static const char * const KEY_OPEN_LOT = "ConvertToLot/OpenLot";

ConvertToLotDialog::ConvertToLotDialog(const MapDocument *mapDocument,
                                       QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ConvertToLotDialog)
{
    ui->setupUi(this);

    connect(ui->mapBrowse, SIGNAL(clicked()), SLOT(mapBrowse()));

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

    ui->mapNameEdit->setText(QDir::toNativeSeparators(info2.absoluteFilePath()));

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
    mLevelIsometric = s->value(QLatin1String(KEY_ORIENT_LEVELS), true).toBool();
    mOpenLot = s->value(QLatin1String(KEY_OPEN_LOT), true).toBool();

    ui->emptyLevels->setChecked(mEmptyLevels);
    ui->eraseSource->setChecked(mEraseSource);
    ui->orientation->setChecked(mLevelIsometric);
    ui->openLot->setChecked(mOpenLot);
}

void ConvertToLotDialog::accept()
{
    mMapPath = ui->mapNameEdit->text();
    mEmptyLevels = ui->emptyLevels->isChecked();
    mEraseSource = ui->eraseSource->isChecked();
    mLevelIsometric = ui->orientation->isChecked();
    mOpenLot = ui->openLot->isChecked();

    int row = ui->layerList->currentRow();
    mObjectGroup = (row >= 0) ? mObjectGroups.at(row) : 0;

    if (mMapPath.isEmpty()) {
        QMessageBox::warning(this, tr("No Filename"),
                             tr("Please enter a filename for the lot file."));
        return;
    }

    // TODO: If replacing a map that was already loaded, must reload the map!
    if (QFileInfo::exists(mMapPath)) {
        QString fileName = QFileInfo(mMapPath).fileName();
        int r = QMessageBox::warning(this, tr("Confirm Save"),
                                     tr("%1 already exists.\nDo you want to replace it?")
                                     .arg(fileName),
                                     QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No);
        if (r == QMessageBox::No)
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
    s->setValue(QLatin1String(KEY_ORIENT_LEVELS), mLevelIsometric);
    s->setValue(QLatin1String(KEY_OPEN_LOT), mOpenLot);

    QDialog::accept();
}

void ConvertToLotDialog::mapBrowse()
{
    QString f = QFileDialog::getSaveFileName(this, tr("Save Lot As"),
                                             ui->mapNameEdit->text(),
                                             tr("Tiled map files (*.tmx)"));
    if (!f.isEmpty()) {
        ui->mapNameEdit->setText(QDir::toNativeSeparators(f));
    }
}
