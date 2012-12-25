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

#include "horizontallinedelegate.h"

#include <QListWidget>
#include <QPainter>

using namespace BuildingEditor;

HorizontalLineDelegate *HorizontalLineDelegate::mInstance = 0;

void HorizontalLineDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    QRect r = option.rect;
    r.setTop(r.top() + (r.height() - 1) / 2);
    r.setBottom(r.top() + 1);
    painter->fillRect(r, Qt::gray);
}

QSize HorizontalLineDelegate::sizeHint(const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const
{
    return QSize(16, 5);
}

QListWidgetItem *HorizontalLineDelegate::addToList(QListWidget *w)
{
    QListWidgetItem *item = new QListWidgetItem();
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    w->addItem(item);
    w->setItemDelegateForRow(w->row(item), this);
    return item;
}
