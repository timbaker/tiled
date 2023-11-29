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

#ifndef ATTRIBUTEEDITMODEPERDOCUMENTSTUFF_H
#define ATTRIBUTEEDITMODEPERDOCUMENTSTUFF_H

#include <QObject>

namespace Tiled {
namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {

class BuildingDocument;
class BuildingIsoScene;
class BuildingIsoView;

class AttributeEditMode;

class AttributeEditModePerDocumentStuff : public QObject
{
    Q_OBJECT
public:
    AttributeEditModePerDocumentStuff(AttributeEditMode *mode, BuildingDocument *doc);
    ~AttributeEditModePerDocumentStuff();

    BuildingDocument *document() const { return mDocument; }
    BuildingIsoView *view() const { return mIsoView; }
    BuildingIsoScene *scene() const { return mIsoScene; }
    Tiled::Internal::Zoomable *zoomable() const;

    void activate();
    void deactivate();

public slots:
    void updateDocumentTab();

    void zoomIn();
    void zoomOut();
    void zoomNormal();

    void updateActions();

private:
    AttributeEditMode *mMode;
    BuildingDocument *mDocument;
    BuildingIsoView *mIsoView;
    BuildingIsoScene *mIsoScene;
};

}

#endif // ATTRIBUTEEDITMODEPERDOCUMENTSTUFF_H
