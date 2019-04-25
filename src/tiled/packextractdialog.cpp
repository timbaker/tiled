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
//    if (prefix.isEmpty())
//        return;

    QDir outputDir(ui->outputEdit->text());
    if (!outputDir.exists())
        return;

    if (ui->radioMultiple->isChecked()) {
        foreach (PackPage page, mPackFile.pages()) {
            foreach (PackSubTexInfo tex, page.mInfo) {
                if (prefix.isEmpty() || tex.name.startsWith(prefix, Qt::CaseInsensitive)) {
                    QImage image(tex.fx, tex.fy, QImage::Format_ARGB32);
                    image.fill(Qt::transparent);
                    QPainter painter(&image);
                    painter.drawImage(tex.ox, tex.oy, page.image, tex.x, tex.y, tex.w, tex.h);
                    painter.end();
                    image.save(outputDir.filePath(tex.name + QLatin1String(".png")));
                }
            }
        }
    } else if (!prefix.isEmpty()) {
        struct TileInfo {
            QString tileName;
            int tileIndex;
            QImage tileImage;
            QRect tileRect;
        };
        QRect bounds(0, 0, 0, 0);
        int TILE_WIDTH = 64 * (ui->checkBoxDouble->isChecked() ? 2 : 1);
        int TILE_HEIGHT = 128 * (ui->checkBoxDouble->isChecked() ? 2 : 1);
        QList<TileInfo> tiles;
        foreach (PackPage page, mPackFile.pages()) {
            foreach (PackSubTexInfo tex, page.mInfo) {
                if (tex.name.startsWith(prefix, Qt::CaseInsensitive)) {
                    QString tileName;
                    int tileIndex;
                    if (BuildingEditor::BuildingTilesMgr::parseTileName(tex.name, tileName, tileIndex)) {
                        if (tex.fx != TILE_WIDTH || tex.fy != TILE_HEIGHT) {
                            qDebug() << "WARNING:" << tex.name << "size is not" << TILE_WIDTH << "x" << TILE_HEIGHT << tex.fx << "x" << tex.fy;
                        }
                        QImage image(tex.fx, tex.fy, QImage::Format_ARGB32);
                        image.fill(Qt::transparent);
#if 1
                        for (int y = 0; y < tex.h; y++) {
                            for (int x = 0; x < tex.w; x++) {
                                image.setPixel(tex.ox + x, tex.oy + y, page.image.pixel(tex.x + x, tex.y + y));
                            }
                        }
#else
                        QPainter painter(&image);
                        painter.setCompositionMode(QPainter::CompositionMode_Source);
                        painter.drawImage(tex.ox, tex.oy, page.image, tex.x, tex.y, tex.w, tex.h);
                        painter.end();
#endif
                        TileInfo info;
                        info.tileName = tileName;
                        info.tileIndex = tileIndex;
                        info.tileImage = image;
                        info.tileRect = QRect((tileIndex % 8) * TILE_WIDTH, (tileIndex / 8) * TILE_HEIGHT, TILE_WIDTH, TILE_HEIGHT);
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
#if 1
                QImage& tileImage = info.tileImage;
                for (int y = 0; y < tileImage.height(); y++) {
                    for (int x = 0; x < tileImage.width(); x++) {
                        image.setPixel(info.tileRect.x() + x, info.tileRect.y() + y, tileImage.pixel(x, y));
                    }
                }
#else
                QPainter painter(&image);
                painter.setCompositionMode(QPainter::CompositionMode_Source);
                painter.drawImage(info.tileRect.topLeft(), info.tileImage);
                painter.end();
#endif
            }
            image.save(outputDir.filePath(prefix + QLatin1String(".png")));
        }
    }

    QDialog::accept();
}
