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

#ifndef WELCOMEMODE_H
#define WELCOMEMODE_H

#include "imode.h"

#include <QGraphicsItem>
#include <QModelIndex>

class MapImage;

class QFileSystemModel;

namespace Ui {
class WelcomeMode;
}

namespace WelcomeModeNS {
class LinkItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    LinkItem(const QString &text1, const QString &text2 = QString(), QGraphicsItem *parent = 0);
    QRectF boundingRect() const;
    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *);

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *);

signals:
    void clicked();
    void hovered(bool hover);

private:
    QRectF mBoundingRect;
};
}

namespace BuildingEditor {

class WelcomeMode : public IMode
{
    Q_OBJECT
public:
    WelcomeMode(QObject *parent = 0);

    void readSettings(QSettings &settings);
    void writeSettings(QSettings &settings);

public slots:
    void onMapsDirectoryChanged();
    void onActivated(const QModelIndex &index);
    void browse();
    void editedMapsDirectory();
    void selectionChanged();

    void onMapImageChanged(MapImage *mapImage);
    void mapImageFailedToLoad(MapImage *mapImage);

    void setRecentFiles();
    void linkClicked();
    void linkHovered(bool hover);

private:
    Ui::WelcomeMode *ui;
    QFileSystemModel *mFSModel;
    MapImage *mPreviewMapImage;

    int mRecentItemsY;
    WelcomeModeNS::LinkItem *mNewItem;
    WelcomeModeNS::LinkItem *mOpenItem;
    QList<WelcomeModeNS::LinkItem*> mRecentItems;
};

} // namespace BuildingEditor

#endif // WELCOMEMODE_H
