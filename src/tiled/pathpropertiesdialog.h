/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef PATHPROPERTIESDIALOG_H
#define PATHPROPERTIESDIALOG_H

#include <QDialog>
#include <QModelIndex>

class QToolButton;

namespace Ui {
class PathPropertiesDialog;
}

namespace Tiled {

class Path;
class PathGenerator;
class PathGeneratorProperty;
class PGP_Tile;
class PGP_TileEntry;

namespace Internal {
class MapDocument;

class PathPropertiesDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit PathPropertiesDialog(QWidget *parent = 0);
    ~PathPropertiesDialog();

    void setPath(MapDocument *doc, Path *path);
    
private slots:
    void currentGeneratorChanged(int row);
    void currentPropertyChanged(int row);
    void currentGeneratorTemplateChanged(int row);

    void propertyActivated(const QModelIndex &index);
    void templateActivated(const QModelIndex &index);

    void nameEdited(const QString &text);
    void booleanToggled(bool newValue);
    void integerValueChanged(int newValue);
    void chooseTile();
    void clearTile();
    void stringEdited(const QString &text);

    void duplicate();
    void moveUp();
    void moveDown();
    void removeGenerator();

    void addGenerator();
    void addGeneratorTilesets(PGP_Tile *prop);
    void addGeneratorTilesets(PGP_TileEntry *prop);
    void addTilesetIfNeeded(const QString &tilesetName);

    void undoTextChanged(const QString &text);
    void redoTextChanged(const QString &text);

    void setClosed(bool closed);
    void setCenters(bool centers);

    void synchUI();

    void generatorsDialog();

private:
    void setGeneratorsList();
    void setGeneratorTypesList();
    void setPropertyList();
    void setPropertyPage();

private slots:
    void pathGeneratorAdded(Path *path, int index);
    void pathGeneratorRemoved(Path *path, int index);
    void pathGeneratorReordered(Path *path, int oldIndex, int newIndex);
    void pathGeneratorPropertyChanged(Path *path, PathGeneratorProperty *prop);

private:
    Ui::PathPropertiesDialog *ui;
    MapDocument *mMapDocument;
    Path *mPath;
    PathGenerator *mCurrentGenerator;
    PathGeneratorProperty *mCurrentProperty;
    PathGenerator *mCurrentGeneratorTemplate;
    QList<PathGenerator*> mGeneratorTemplates;
    QString mPropertyName;
    bool mSynching;

    QToolButton *mUndoButton;
    QToolButton *mRedoButton;
    bool mWasEditingString;
    int mUndoIndex;
};

} // namespace Internal;
} // namespace Tiled

#endif // PATHPROPERTIESDIALOG_H
