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

#ifndef HORIZONTALLINEDELEGATE_H
#define HORIZONTALLINEDELEGATE_H

#include <QAbstractItemDelegate>

class QListWidget;
class QListWidgetItem;

namespace BuildingEditor {

class HorizontalLineDelegate : public QAbstractItemDelegate
{
public:
    HorizontalLineDelegate(QObject *parent = 0)
        : QAbstractItemDelegate(parent)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

    QListWidgetItem *addToList(QListWidget *w);

    static HorizontalLineDelegate *instance()
    {
        if (!mInstance)
            mInstance = new HorizontalLineDelegate;
        return mInstance;
    }

    static HorizontalLineDelegate *mInstance;
};

} // namespace BuildingEditor

#endif // HORIZONTALLINEDELEGATE_H
