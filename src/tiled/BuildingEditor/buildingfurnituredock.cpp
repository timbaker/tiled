#include "buildingfurnituredock.h"

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
    QComboBox *scaleCombo = new QComboBox;
    scaleCombo->setEditable(true);
    comboLayout->addStretch(1);
    comboLayout->addWidget(scaleCombo);

    QWidget *rightWidget = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(mFurnitureView, 1);
    rightLayout->addLayout(comboLayout);

    QSplitter *splitter = new QSplitter;
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(mGroupList);
    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(1, 1);

    QWidget *outer = new QWidget(this);
    QHBoxLayout *outerLayout = new QHBoxLayout(outer);
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
