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

#include "buildingfurnituredock.h"

#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingpreferences.h"
#include "buildingtiles.h"
#include "buildingtilesdialog.h"
#include "buildingtiletools.h"
#include "furnituregroups.h"
#include "furnitureview.h"

#include "zoomable.h"

#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QSettings>
#include <QSplitter>
#include <QVBoxLayout>

using namespace BuildingEditor;

BuildingFurnitureDock::BuildingFurnitureDock(QWidget *parent) :
    QDockWidget(parent),
    mGroupList(new QListWidget(this)),
    mFurnitureView(new FurnitureView(this))
{
    setObjectName(QLatin1String("FurnitureDock"));

    QHBoxLayout *comboLayout = new QHBoxLayout;
    comboLayout->setObjectName(QLatin1String("FurnitureDock.comboLayout"));
    QComboBox *scaleCombo = new QComboBox;
    scaleCombo->setObjectName(QLatin1String("FurnitureDock.scaleComboBox"));
    scaleCombo->setEditable(true);
    comboLayout->addStretch(1);
    comboLayout->addWidget(scaleCombo);

    QWidget *rightWidget = new QWidget(this);
    rightWidget->setObjectName(QLatin1String("FurnitureDock.rightWidget"));
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setObjectName(QLatin1String("FurnitureDock.rightLayout"));
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(mFurnitureView, 1);
    rightLayout->addLayout(comboLayout);

    QSplitter *splitter = mSplitter = new QSplitter;
    splitter->setObjectName(QLatin1String("FurnitureDock.splitter"));
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(mGroupList);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(1, 1);

    QWidget *outer = new QWidget(this);
    outer->setObjectName(QLatin1String("FurnitureDock.contents"));
    QHBoxLayout *outerLayout = new QHBoxLayout(outer);
    outerLayout->setObjectName(QLatin1String("FurnitureDock.contentsLayout"));
    outerLayout->setSpacing(5);
    outerLayout->setMargin(5);
    outerLayout->addWidget(splitter);
    setWidget(outer);

    BuildingPreferences *prefs = BuildingPreferences::instance();
    mFurnitureView->zoomable()->setScale(prefs->tileScale());
    mFurnitureView->zoomable()->connectToComboBox(scaleCombo);
    connect(prefs, SIGNAL(tileScaleChanged(qreal)),
            SLOT(tileScaleChanged(qreal)));
    connect(mFurnitureView->zoomable(), SIGNAL(scaleChanged(qreal)),
            prefs, SLOT(setTileScale(qreal)));

    connect(mGroupList, SIGNAL(currentRowChanged(int)), SLOT(currentGroupChanged(int)));
    connect(mFurnitureView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(currentFurnitureChanged()));

    connect(BuildingTilesDialog::instance(), SIGNAL(edited()),
            SLOT(tilesDialogEdited()));

    retranslateUi();
}

void BuildingFurnitureDock::readSettings(QSettings &settings)
{
    settings.beginGroup(QLatin1String("FurnitureDock"));
    BuildingEditorWindow::instance()->restoreSplitterSizes(mSplitter);
    settings.endGroup();

}

void BuildingFurnitureDock::writeSettings(QSettings &settings)
{
#if 0
    settings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    settings.setValue(QLatin1String("SelectedFurnitureGroup"),
                      mFurnitureGroup ? mFurnitureGroup->mLabel : QString());
    settings.endGroup();
#endif
    settings.beginGroup(QLatin1String("FurnitureDock"));
    BuildingEditorWindow::instance()->saveSplitterSizes(mSplitter);
    settings.endGroup();
}

void BuildingFurnitureDock::switchTo()
{
    setGroupsList();
}

void BuildingFurnitureDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void BuildingFurnitureDock::retranslateUi()
{
    setWindowTitle(tr("Furniture"));
}

void BuildingFurnitureDock::setGroupsList()
{
    mGroupList->clear();
    foreach (FurnitureGroup *group, FurnitureGroups::instance()->groups())
        mGroupList->addItem(group->mLabel);
}

void BuildingFurnitureDock::setFurnitureList()
{
    QList<FurnitureTiles*> ftiles;
    if (mCurrentGroup) {
        ftiles = mCurrentGroup->mTiles;
    }
    mFurnitureView->setTiles(ftiles);
}

void BuildingFurnitureDock::currentGroupChanged(int row)
{
    mCurrentGroup = 0;
    mCurrentTile = 0;
    if (row >= 0)
        mCurrentGroup = FurnitureGroups::instance()->group(row);
    setFurnitureList();
}

void BuildingFurnitureDock::currentFurnitureChanged()
{
    QModelIndexList indexes = mFurnitureView->selectionModel()->selectedIndexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        if (FurnitureTile *ftile = mFurnitureView->model()->tileAt(index)) {
            ftile = ftile->resolved();
            mCurrentTile = ftile;

            if (!DrawTileTool::instance()->action()->isEnabled())
                return;

            QRegion rgn;
            FloorTileGrid *tiles = ftile->toFloorTileGrid(rgn);
            if (!tiles) // empty
                return;

            DrawTileTool::instance()->setCaptureTiles(tiles, rgn);
        }
    }
}

void BuildingFurnitureDock::tileScaleChanged(qreal scale)
{
    mFurnitureView->zoomable()->setScale(scale);
}

void BuildingFurnitureDock::tilesDialogEdited()
{
    FurnitureGroup *group = mCurrentGroup;
    setGroupsList();
    if (group) {
        int row = FurnitureGroups::instance()->indexOf(group);
        if (row >= 0)
            mGroupList->setCurrentRow(row);
    }
}
