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

#ifndef BUILDINGDOCUMENTMGR_H
#define BUILDINGDOCUMENTMGR_H

#include <QList>
#include <QObject>

class QUndoGroup;

namespace BuildingEditor {

class BuildingDocument;

class BuildingDocumentMgr : public QObject
{
    Q_OBJECT
public:
    static BuildingDocumentMgr *instance();
    static void deleteInstance();

    void addDocument(BuildingDocument *doc);
    void closeDocument(int index);
    void closeDocument(BuildingDocument *doc);

    void closeCurrentDocument();
    void closeAllDocuments();

    int findDocument(const QString &fileName);

    void setCurrentDocument(int index);
    void setCurrentDocument(BuildingDocument *doc);
    BuildingDocument *currentDocument() const
    { return mCurrent; }

    const QList<BuildingDocument*> &documents() const
    { return mDocuments; }
    int documentCount() const
    { return mDocuments.size(); }

    BuildingDocument *documentAt(int index) const;
    int indexOf(BuildingDocument *doc)
    { return mDocuments.indexOf(doc); }

//    void setFailedToAdd();
//    bool failedToAdd();

signals:
    void currentDocumentChanged(BuildingDocument *doc);
    void documentAdded(BuildingDocument *doc);
    void documentAboutToClose(int index, BuildingDocument *doc);

private:
    Q_DISABLE_COPY(BuildingDocumentMgr)
    static BuildingDocumentMgr *mInstance;
    BuildingDocumentMgr();
    ~BuildingDocumentMgr();

    QList<BuildingDocument*> mDocuments;
    BuildingDocument *mCurrent;
};

} // namespace BuildingEditor

#endif // BUILDINGDOCUMENTMGR_H
