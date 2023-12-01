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

#include <array>

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
    const QStringList& SQUARE_PROPERTIES = getSquarePropertyNames();
    for (int i = 0; i < SQUARE_PROPERTIES.size(); i++) {
        ui->listWidget->addItem(SQUARE_PROPERTIES.at(i));
        ui->listWidget->item(i)->setCheckState(Qt::Unchecked);

    }
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
    Tiled::PropertiesGrid *spg = floor->squarePropertiesGrid()->clone();
    for (const QRect& rect : selection) {
        for (int y = rect.top(); y <= rect.bottom(); y++) {
            for (int x = rect.left(); x <= rect.right(); x++) {
                if (spg->hasPropertiesAt(x, y)) {
                    Tiled::Properties properties = spg->at(x, y);
                    properties.remove(attrName);
                    spg->replace(x, y, properties);
                }
            }
        }
    }
    mDocument->undoStack()->push(new ChangeSquareProperties(mDocument, floor->level(), selection, spg));
}

void BuildingAttributesDock::setAttributeOnSelectedSquares(const QString &attrName)
{
    const QRegion &selection = mDocument->tileSelection();
    if (selection.isEmpty())
        return;
    BuildingFloor *floor = mDocument->currentFloor();
    Tiled::PropertiesGrid *spg = floor->squarePropertiesGrid()->clone();
    for (const QRect& rect : selection) {
        for (int y = rect.top(); y <= rect.bottom(); y++) {
            for (int x = rect.left(); x <= rect.right(); x++) {
                if (spg->hasPropertiesAt(x, y)) {
                    Tiled::Properties properties = spg->at(x, y);
                    if (properties.contains(attrName) == false) {
                        properties.insert(attrName, QString());
                        spg->replace(x, y, properties);
                    }
                } else {
                    Tiled::Properties properties;
                    properties.insert(attrName, QString());
                    spg->replace(x, y, properties);
                }
            }
        }
    }
    mDocument->undoStack()->push(new ChangeSquareProperties(mDocument, floor->level(), selection, spg));
}

void BuildingAttributesDock::syncListWithSelectedSquares()
{
    const QRegion &selection = mDocument->tileSelection();
    BuildingFloor *floor = mDocument->currentFloor();
    int numSelectedSquares = 0;
    std::array<int, 32> numWithAttribute;
    numWithAttribute.fill(0);
    const QStringList& SQUARE_PROPERTIES = getSquarePropertyNames();
    Tiled::PropertiesGrid *spg = floor->squarePropertiesGrid();
    for (const QRect& rect : selection) {
        for (int y = rect.top(); y <= rect.bottom(); y++) {
            for (int x = rect.left(); x <= rect.right(); x++) {
                if (spg->hasPropertiesAt(x, y)) {
                    const Tiled::Properties& properties = spg->at(x, y);
                    for (auto key : properties.keys()) {
                        int index = SQUARE_PROPERTIES.indexOf(key);
                        if (index != -1) {
                            numWithAttribute[index]++;
                        }
                    }
                }
                numSelectedSquares++;
            }
        }
    }
    mSynching = true;
    for (int i = 0; i < SQUARE_PROPERTIES.size(); i++) {
        QListWidgetItem *item = ui->listWidget->item(i);
        if (numSelectedSquares == 0) {
            item->setCheckState(Qt::CheckState::Unchecked);
        } else if (numSelectedSquares == numWithAttribute[i]) {
            item->setCheckState(Qt::CheckState::Checked);
        } else if (numWithAttribute[i] > 0) {
            item->setCheckState(Qt::CheckState::PartiallyChecked);
        } else {
            item->setCheckState(Qt::CheckState::Unchecked);
        }
    }
    mSynching = false;
}
