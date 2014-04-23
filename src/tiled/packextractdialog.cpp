#include "packextractdialog.h"
#include "ui_packextractdialog.h"

#include "texturepackfile.h"

#include "BuildingEditor/buildingtiles.h"

#include <QDebug>
#include <QFileDialog>
#include <QPainter>

PackExtractDialog::PackExtractDialog(PackFile &packFile, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PackExtractDialog),
    mPackFile(packFile)
{
    ui->setupUi(this);

    connect(ui->outputBrowse, SIGNAL(clicked()), SLOT(browse()));

    ui->radioMultiple->setChecked(true);
    ui->radioSingle->setChecked(false);
}

PackExtractDialog::~PackExtractDialog()
{
    delete ui;
}

void PackExtractDialog::browse()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Choose output directory"), ui->outputEdit->text());
    if (!f.isEmpty()) {
        ui->outputEdit->setText(QDir::toNativeSeparators(f));
    }
}

void PackExtractDialog::accept()
{
    QString prefix = ui->prefixEdit->text();
    if (prefix.isEmpty())
        return;

    QDir outputDir(ui->outputEdit->text());
    if (!outputDir.exists())
        return;

    if (ui->radioMultiple->isChecked()) {
        foreach (PackPage page, mPackFile.pages()) {
            foreach (PackSubTexInfo tex, page.mInfo) {
                if (tex.name.startsWith(prefix, Qt::CaseInsensitive)) {
                    QImage image(tex.fx, tex.fy, QImage::Format_ARGB32);
                    image.fill(Qt::transparent);
                    QPainter painter(&image);
                    painter.drawImage(tex.ox, tex.oy, page.image, tex.x, tex.y, tex.w, tex.h);
                    painter.end();
                    image.save(outputDir.filePath(tex.name + QLatin1String(".png")));
                }
            }
        }
    } else {
        struct TileInfo {
            QString tileName;
            int tileIndex;
            QImage tileImage;
            QRect tileRect;
        };
        QRect bounds(0, 0, 0, 0);
        QList<TileInfo> tiles;
        foreach (PackPage page, mPackFile.pages()) {
            foreach (PackSubTexInfo tex, page.mInfo) {
                if (tex.name.startsWith(prefix, Qt::CaseInsensitive)) {
                    QString tileName;
                    int tileIndex;
                    if (BuildingEditor::BuildingTilesMgr::parseTileName(tex.name, tileName, tileIndex)) {
                        if (tex.fx != 64 || tex.fy != 128) {
                            qDebug() << "WARNING: " << tex.name << "size is not 64x128" << tex.fx << "x" << tex.fy;
                        }
                        QImage image(tex.fx, tex.fy, QImage::Format_ARGB32);
                        image.fill(Qt::transparent);
                        QPainter painter(&image);
                        painter.drawImage(tex.ox, tex.oy, page.image, tex.x, tex.y, tex.w, tex.h);
                        painter.end();

                        TileInfo info;
                        info.tileName = tileName;
                        info.tileIndex = tileIndex;
                        info.tileImage = image;
                        info.tileRect = QRect((tileIndex % 8) * 64, (tileIndex / 8) * 128, 64, 128);
                        tiles += info;

                        bounds |= info.tileRect;
                    }
                }
            }
        }
        if (!bounds.isEmpty()) {
            QImage image(bounds.size(), QImage::Format_ARGB32);
            image.fill(Qt::transparent);
            foreach (TileInfo info, tiles) {
                QPainter painter(&image);
                painter.drawImage(info.tileRect.topLeft(), info.tileImage);
                painter.end();
            }
            image.save(outputDir.filePath(prefix + QLatin1String(".png")));
        }
    }

    QDialog::accept();
}
