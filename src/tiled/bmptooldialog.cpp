/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "bmptooldialog.h"
#include "ui_bmptooldialog.h"

#include "bmpblender.h"
#include "bmptool.h"
#include "mapdocument.h"
#include "zoomable.h"

#include "BuildingEditor/buildingtiles.h"

using namespace Tiled;
using namespace Tiled::Internal;

BmpToolDialog::BmpToolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BmpToolDialog),
    mDocument(0)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::Tool);

    ui->tableView->zoomable()->setScale(0.5);

    connect(ui->tableView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            SLOT(currentRuleChanged(QModelIndex)));
}

BmpToolDialog::~BmpToolDialog()
{
    delete ui;
}

void BmpToolDialog::setDocument(MapDocument *doc)
{
    mDocument = doc;

    ui->tableView->clear();
    if (mDocument) {
        QList<Tiled::Tile*> tiles;
        QList<void*> userData;
        QStringList headers;
        int ruleIndex = 1;
        foreach (BmpBlender::Rule *rule, mDocument->bmpBlender()->mRules) {
            foreach (QString tileName, rule->tileChoices) {
                Tile *tile;
                if (tileName.isEmpty())
                    tile = BuildingEditor::BuildingTilesMgr::instance()->noneTiledTile();
                else
                    tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(tileName);
                tiles += tile;
                userData += rule;
                headers += tr("Rule #%8: index=%1 rgb=%2,%3,%4 condition=%5,%6,%7")
                        .arg(rule->bitmapIndex)
                        .arg(qRed(rule->color)).arg(qGreen(rule->color)).arg(qBlue(rule->color))
                        .arg(qRed(rule->condition)).arg(qGreen(rule->condition)).arg(qBlue(rule->condition))
                        .arg(ruleIndex);
            }
            ++ruleIndex;
        }
        ui->tableView->setTiles(tiles, userData, headers);
    }
}

void BmpToolDialog::currentRuleChanged(const QModelIndex &current)
{
    BmpBlender::Rule *rule = static_cast<BmpBlender::Rule*>(ui->tableView->model()->userDataAt(current));
    if (rule) {
        BmpTool::instance()->setColor(rule->bitmapIndex, rule->color);
    }
}
