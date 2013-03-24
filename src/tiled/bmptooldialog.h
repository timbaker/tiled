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

#ifndef BMPTOOLDIALOG_H
#define BMPTOOLDIALOG_H

#include <QDialog>
#include <QModelIndex>
#include <QTimer>

namespace Ui {
class BmpToolDialog;
}

namespace Tiled {
namespace Internal {
class MapDocument;

class BmpToolDialog : public QDialog
{
    Q_OBJECT
    
public:
    static BmpToolDialog *instance();

    void setVisible(bool visible);

    void setVisibleLater(bool visible);

    void setDocument(MapDocument *doc);

private slots:
    void currentRuleChanged(const QModelIndex &current);
    void brushSizeChanged(int size);
    void brushSquare();
    void brushCircle();
    void toggleOverlayLayers();
    void showMapTiles(bool show);
    void setVisibleNow();

    void reloadRules();
    void importRules();

    void reloadBlends();
    void importBlends();

    void bmpRulesChanged();
    void bmpBlendsChanged();

    void documentAboutToClose(int index, MapDocument *doc);

private:
    Q_DISABLE_COPY(BmpToolDialog)
    static BmpToolDialog *mInstance;
    explicit BmpToolDialog(QWidget *parent = 0);
    ~BmpToolDialog();

    Ui::BmpToolDialog *ui;
    MapDocument *mDocument;
    bool mVisibleLater;
    QTimer mVisibleLaterTimer;
    QMap<MapDocument*,int> mCurrentRuleForDocument;
};

} // namespace Internal
} // namespace Tiled

#endif // BMPTOOLDIALOG_H
