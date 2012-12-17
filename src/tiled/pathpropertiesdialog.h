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

namespace Ui {
class PathPropertiesDialog;
}

namespace Tiled {

class Path;
class PathGenerator;
class PathGeneratorProperty;

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

    void nameEdited(const QString &text);
    void integerValueChanged(int newValue);

    void unlink();
    void removeGenerator();
    void addGenerator();

    void synchUI();

    void generatorsDialog();

private:
    void setGeneratorsList();
    void setGeneratorTypesList();
    void setPropertyList();
    void setPropertyPage();

    bool isCurrentGeneratorShared();

private:
    Ui::PathPropertiesDialog *ui;
    MapDocument *mMapDocument;
    Path *mPath;
    PathGenerator *mCurrentGenerator;
    PathGeneratorProperty *mCurrentProperty;
    PathGenerator *mCurrentGeneratorTemplate;
    QString mPropertyName;
    bool mSynching;
};

} // namespace Internal;
} // namespace Tiled

#endif // PATHPROPERTIESDIALOG_H
