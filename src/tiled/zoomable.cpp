/*
 * zoomable.cpp
 * Copyright 2009-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
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

#include "zoomable.h"

#ifdef ZOMBOID
#include <QComboBox>
#endif

using namespace Tiled::Internal;

static const qreal zoomFactors[] = {
    0.0625,
    0.125,
    0.25,
    0.33,
    0.5,
    0.75,
    1.0,
    1.5,
    2.0,
    3.0,
    4.0
};
const int zoomFactorCount = sizeof(zoomFactors) / sizeof(zoomFactors[0]);

Zoomable::Zoomable(QObject *parent)
    : QObject(parent)
    , mScale(1)
#ifdef ZOMBOID
    , mComboBox(0)
#endif
{
#ifdef ZOMBOID
    for (int i = 0; i < zoomFactorCount; i++)
        mZoomFactors << zoomFactors[i];
#endif
}

void Zoomable::setScale(qreal scale)
{
    if (scale == mScale)
        return;

    mScale = scale;
#ifdef ZOMBOID
    if (mComboBox) {
        int index = mComboBox->findData(scale);
        if (index != -1)
            mComboBox->setCurrentIndex(index);
    }
#endif
    emit scaleChanged(mScale);
}

bool Zoomable::canZoomIn() const
{
#ifdef ZOMBOID
   return mScale < mZoomFactors.back();
#else
   return mScale < zoomFactors[zoomFactorCount - 1];
#endif
}

bool Zoomable::canZoomOut() const
{
#ifdef ZOMBOID
    return mScale > mZoomFactors.first();
#else
    return mScale > zoomFactors[0];
#endif
}

#ifdef ZOMBOID
void Zoomable::setZoomFactors(const QVector<qreal>& factors)
{
    mZoomFactors = factors;
}

void Zoomable::connectToComboBox(QComboBox *comboBox)
{
    if (mComboBox)
        disconnect(mComboBox, SIGNAL(activated(int)), this, SLOT(comboActivated(int)));

    mComboBox = comboBox;

    if (mComboBox) {
        mComboBox->clear();
        int index = 0;
        foreach (qreal scale, mZoomFactors) {
            mComboBox->addItem(QString(QLatin1String("%1 %")).arg(int(scale * 100)), scale);
            if (scale == mScale)
                mComboBox->setCurrentIndex(index);
            ++index;
        }
        connect(mComboBox, SIGNAL(activated(int)), this, SLOT(comboActivated(int)));
    }
}

void Zoomable::comboActivated(int index)
{
    QVariant data = mComboBox->itemData(index);
    qreal scale = data.toReal();
    setScale(scale);
}
#endif // ZOMBOID

void Zoomable::zoomIn()
{
#ifdef ZOMBOID
    foreach (qreal scale, mZoomFactors) {
        if (scale > mScale) {
            setScale(scale);
#else
    for (int i = 0; i < zoomFactorCount; ++i) {
        if (zoomFactors[i] > mScale) {
            setScale(zoomFactors[i]);
#endif
            break;
        }
    }
}

void Zoomable::zoomOut()
{
#ifdef ZOMBOID
    for (int i = mZoomFactors.count() - 1; i >= 0; --i) {
        if (mZoomFactors[i] < mScale) {
            setScale(mZoomFactors[i]);
#else
    for (int i = mZoomFactors.count() - 1; i >= 0; --i) {
        if (zoomFactors[i] < mScale) {
            setScale(zoomFactors[i]);
#endif
            break;
        }
    }
}

void Zoomable::resetZoom()
{
    setScale(1);
}
