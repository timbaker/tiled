/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef PATHTILEENTRYDIALOG_H
#define PATHTILEENTRYDIALOG_H

#include <QDialog>

namespace Ui {
class PathTileEntryDialog;
}

namespace BuildingEditor {
class BuildingTileEntry;
}

namespace Tiled {

class PGP_TileEntry;
class Tile;

namespace Internal {

class Zoomable;

class PathTileEntryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PathTileEntryDialog(const QString &prompt,
                                 const QString &category,
                                 PGP_TileEntry *initialTile,
                                 QWidget *parent = 0);
    ~PathTileEntryDialog();

    PGP_TileEntry *selectedEntry() const;

private:
    void setTilesList(const QString &category,
                      PGP_TileEntry *initialEntry);

    QList<PGP_TileEntry*> getEntriesInCategory(const QString &category);
    PGP_TileEntry *BuildingEntryToPathEntry(const QString &category,
                                            BuildingEditor::BuildingTileEntry *entry);
    bool matches(PGP_TileEntry *entry, PGP_TileEntry *other);
    bool matches(PGP_TileEntry *entry, QList<PGP_TileEntry *> &entries);

private slots:
    void tilesDialog();
    void accept();

private:
    Ui::PathTileEntryDialog *ui;
    QString mCategory;
    QList<Tiled::Tile*> mTiles;
    QList<PGP_TileEntry*> mEntries;
    Zoomable *mZoomable;
};

} // namespace Internal
} // namespace Tiled

#endif // PATHTILEENTRYDIALOG_H
