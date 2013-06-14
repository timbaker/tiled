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

#include "buildingdocumentmgr.h"

#include "buildingdocument.h"

#include <QDebug>
#include <QFileInfo>

using namespace BuildingEditor;

BuildingDocumentMgr *BuildingDocumentMgr::mInstance = 0;

BuildingDocumentMgr *BuildingDocumentMgr::instance()
{
    if (!mInstance)
        mInstance = new BuildingDocumentMgr;
    return mInstance;
}

BuildingDocumentMgr::BuildingDocumentMgr() :
    QObject(),
    mCurrent(0)
{
}


BuildingDocumentMgr::~BuildingDocumentMgr()
{
}


void BuildingDocumentMgr::addDocument(BuildingDocument *doc)
{
    mDocuments += doc;
    emit documentAdded(doc);
    setCurrentDocument(doc);
}

void BuildingDocumentMgr::closeDocument(int index)
{
    Q_ASSERT(index >= 0 && index < mDocuments.size());

    BuildingDocument *doc = mDocuments.takeAt(index);
    emit documentAboutToClose(index, doc);
    if (doc == mCurrent) {
        if (mDocuments.size())
            setCurrentDocument(mDocuments.first());
        else
            setCurrentDocument((BuildingDocument*)0);
    }
//    mUndoGroup->removeStack(doc->undoStack());
    delete doc;
}

void BuildingDocumentMgr::closeDocument(BuildingDocument *doc)
{
    if (doc)
        closeDocument(indexOf(doc));
}

void BuildingDocumentMgr::closeCurrentDocument()
{
    closeDocument(mCurrent);
}

void BuildingDocumentMgr::closeAllDocuments()
{
    while (documentCount())
        closeCurrentDocument();
}

int BuildingDocumentMgr::findDocument(const QString &fileName)
{
    const QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();
    if (canonicalFilePath.isEmpty()) // file doesn't exist
        return -1;

    for (int i = 0; i < mDocuments.size(); ++i) {
        QFileInfo fileInfo(mDocuments.at(i)->fileName());
        if (fileInfo.canonicalFilePath() == canonicalFilePath)
            return i;
    }

    return -1;
}

void BuildingDocumentMgr::setCurrentDocument(int index)
{
    Q_ASSERT(index >= -1 && index < mDocuments.size());
    setCurrentDocument((index >= 0) ? mDocuments.at(index) : 0);
}

void BuildingDocumentMgr::setCurrentDocument(BuildingDocument *doc)
{
    Q_ASSERT(!doc || mDocuments.contains(doc));

    if (doc == mCurrent)
        return;

    qDebug() << "current document was " << mCurrent << " is becoming " << doc;

    if (mCurrent) {
    }

    mCurrent = doc;

    if (mCurrent) {
    }

    emit currentDocumentChanged(doc);
}

BuildingDocument *BuildingDocumentMgr::documentAt(int index) const
{
    Q_ASSERT(index >= 0 && index < mDocuments.size());
    return mDocuments.at(index);
}
