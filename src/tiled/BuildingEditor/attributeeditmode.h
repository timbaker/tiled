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

#ifndef ATTRIBUTEEDITMODE_H
#define ATTRIBUTEEDITMODE_H

#include "imode.h"

#include <QMap>
#include <QToolBar>
#include <QToolButton>

class QTabWidget;

namespace BuildingEditor
{
class BuildingAttributesDock;
class BuildingDocument;
class BuildingIsoView;
class BuildingLayersDock;
class EmbeddedMainWindow;
class EditModeStatusBar;

class AttributeEditModePerDocumentStuff;

class AttributeEditModeToolBar : public QToolBar
{
    Q_OBJECT
public:
    AttributeEditModeToolBar(QWidget *parent = 0);

private slots:
    void currentDocumentChanged(BuildingEditor::BuildingDocument *doc);

    void updateActions();

public:
    BuildingDocument *mCurrentDocument;
    QToolButton *mFloorLabel;
};

class AttributeEditMode : public IMode
{
    Q_OBJECT
public:
    explicit AttributeEditMode(QObject *parent = nullptr);

    void readSettings(QSettings &settings) override;
    void writeSettings(QSettings &settings) override;

signals:
    void viewAddedForDocument(BuildingEditor::BuildingDocument *doc, BuildingEditor::BuildingIsoView *view);

public slots:
    void onActiveStateChanged(bool active);

    void documentAdded(BuildingEditor::BuildingDocument *doc);
    void currentDocumentChanged(BuildingEditor::BuildingDocument *doc);
    void documentAboutToClose(int index, BuildingEditor::BuildingDocument *doc);

    void currentDocumentTabChanged(int index);
    void documentTabCloseRequested(int index);

    void updateActions();

private:
    EmbeddedMainWindow *mMainWindow;
    QTabWidget *mTabWidget;
    EditModeStatusBar *mStatusBar;

    AttributeEditModeToolBar *mToolBar;
    BuildingAttributesDock *mAttributesDock;
    BuildingLayersDock *mLayersDock;

    BuildingDocument *mCurrentDocument;
    AttributeEditModePerDocumentStuff *mCurrentDocumentStuff;

    friend class AttributeEditModePerDocumentStuff;
    QMap<BuildingDocument*, AttributeEditModePerDocumentStuff*> mDocumentStuff;

};

} // namespace BuildingEditor

#endif // ATTRIBUTEEDITMODE_H
