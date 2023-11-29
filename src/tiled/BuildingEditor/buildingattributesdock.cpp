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
#include "buildingdocumentmgr.h"

using namespace BuildingEditor;

BuildingAttributesDock::BuildingAttributesDock(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::BuildingAttributesDock),
    mDocument(nullptr),
    mSynching(false)
{
    ui->setupUi(this);

    connect(BuildingDocumentMgr::instance(), &BuildingDocumentMgr::currentDocumentChanged,
            this, &BuildingAttributesDock::currentDocumentChanged);

    ui->listWidget->addItem(QStringLiteral("Keep Floors"));
    ui->listWidget->addItem(QStringLiteral("Keep Walls"));
    ui->listWidget->item(0)->setCheckState(Qt::Unchecked);
    ui->listWidget->item(1)->setCheckState(Qt::Unchecked);

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
    }
}

void BuildingAttributesDock::updateActions()
{
    mSynching = true;


    mSynching = false;
}
