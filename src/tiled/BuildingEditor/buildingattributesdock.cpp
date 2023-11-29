/*
 * Copyright 2023, Tim Baker <treectrl@users.sf.net>
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

#include "buildingattributesdock.h"
#include "ui_buildingattributesdock.h"

#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingdocumentmgr.h"

using namespace BuildingEditor;

BuildingAttributesDock::BuildingAttributesDock(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::BuildingAttributesDock),
    mDocument(nullptr),
    mSynching(false)
{
    ui->setupUi(this);

    connect(ui->listWidget, &QListWidget::itemChanged,
            this, &BuildingAttributesDock::itemChanged);
    connect(BuildingDocumentMgr::instance(), &BuildingDocumentMgr::currentDocumentChanged,
            this, &BuildingAttributesDock::currentDocumentChanged);

    mSynching = true;
    ui->listWidget->addItem(QStringLiteral("KeepFloors"));
    ui->listWidget->addItem(QStringLiteral("KeepWalls"));
    ui->listWidget->item(0)->setCheckState(Qt::Unchecked);
    ui->listWidget->item(1)->setCheckState(Qt::Unchecked);
    mSynching = false;

    updateActions();
}

BuildingAttributesDock::~BuildingAttributesDock()
{
    delete ui;
}

void BuildingAttributesDock::currentDocumentChanged(BuildingDocument *doc)
{
    if (mDocument) {
        mDocument->disconnect(this);
    }

    mDocument = doc;

    if (mDocument) {
        connect(mDocument, &BuildingDocument::tileSelectionChanged,
                this, &BuildingAttributesDock::tileSelectionChanged);
    }
}

void BuildingAttributesDock::tileSelectionChanged(const QRegion &old)
{
    Q_UNUSED(old)
    syncListWithSelectedSquares();
}

void BuildingAttributesDock::itemChanged(QListWidgetItem *item)
{
    if (mSynching)
        return;

    switch (item->checkState()) {
    case Qt::CheckState::Unchecked:
        clearAttributeOnSelectedSquares(item->text());
        break;
    case Qt::CheckState::PartiallyChecked:
        clearAttributeOnSelectedSquares(item->text());
        break;
    case Qt::CheckState::Checked:
        setAttributeOnSelectedSquares(item->text());
        break;
    }
}

void BuildingAttributesDock::updateActions()
{
    mSynching = true;


    mSynching = false;
}

void BuildingAttributesDock::clearAttributeOnSelectedSquares(const QString &attrName)
{
    const QRegion &selection = mDocument->tileSelection();
    if (selection.isEmpty())
        return;
    BuildingFloor *floor = mDocument->currentFloor();
    SquareAttributesGrid *attributes = floor->squareAttributesGrid()->clone();
    for (const QRect& rect : selection) {
        for (int y = rect.top(); y <= rect.bottom(); y++) {
            for (int x = rect.left(); x <= rect.right(); x++) {
                if (attributes->hasAttributesFor(x, y)) {
                    SquareAttributes sa = attributes->at(x, y);
                    sa.removeAll(attrName);
                    attributes->replace(x, y, sa);
                }
            }
        }
    }
    mDocument->undoStack()->push(new ChangeSquareAttributes(mDocument, floor->level(), selection, attributes));
}

void BuildingAttributesDock::setAttributeOnSelectedSquares(const QString &attrName)
{
    const QRegion &selection = mDocument->tileSelection();
    if (selection.isEmpty())
        return;
    BuildingFloor *floor = mDocument->currentFloor();
    SquareAttributesGrid *attributes = floor->squareAttributesGrid()->clone();
    for (const QRect& rect : selection) {
        for (int y = rect.top(); y <= rect.bottom(); y++) {
            for (int x = rect.left(); x <= rect.right(); x++) {
                if (attributes->hasAttributesFor(x, y)) {
                    SquareAttributes sa = attributes->at(x, y);
                    if (sa.contains(attrName) == false) {
                        sa += attrName;
                        attributes->replace(x, y, sa);
                    }
                } else {
                    SquareAttributes sa;
                    sa += attrName;
                    attributes->replace(x, y, sa);
                }
            }
        }
    }
    mDocument->undoStack()->push(new ChangeSquareAttributes(mDocument, floor->level(), selection, attributes));
}

void BuildingAttributesDock::syncListWithSelectedSquares()
{
    const QRegion &selection = mDocument->tileSelection();
    BuildingFloor *floor = mDocument->currentFloor();
    int numSelectedSquares = 0;
    int numKeepFloors = 0;
    int numKeepWalls = 0;
    SquareAttributesGrid *sag = floor->squareAttributesGrid();
    for (const QRect& rect : selection) {
        for (int y = rect.top(); y <= rect.bottom(); y++) {
            for (int x = rect.left(); x <= rect.right(); x++) {
                if (sag->hasAttributesFor(x, y)) {
                    const SquareAttributes& sa = sag->at(x, y);
                    if (sa.contains(QStringLiteral("KeepFloors"))) {
                        numKeepFloors++;
                    }
                    if (sa.contains(QStringLiteral("KeepWalls"))) {
                        numKeepWalls++;
                    }
                }
                numSelectedSquares++;
            }
        }
    }
    mSynching = true;
    ui->listWidget->item(0)->setCheckState((numSelectedSquares > 0 && numSelectedSquares == numKeepFloors) ? Qt::CheckState::Checked : (numKeepFloors > 0 ? Qt::CheckState::PartiallyChecked : Qt::Unchecked));
    ui->listWidget->item(1)->setCheckState((numSelectedSquares > 0 && numSelectedSquares == numKeepWalls) ? Qt::CheckState::Checked : (numKeepWalls > 0 ? Qt::CheckState::PartiallyChecked : Qt::Unchecked));
    mSynching = false;
}
