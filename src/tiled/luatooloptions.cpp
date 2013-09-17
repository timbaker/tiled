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

#include "luatooloptions.h"

using namespace Tiled::Lua;

LuaToolOptions::LuaToolOptions()
{
}

LuaToolOptions::~LuaToolOptions()
{
    qDeleteAll(mOptions);
}

void LuaToolOptions::addBoolean(const QString &name, const QString &label, bool defaultValue)
{
    LuaToolOption *prop = new BooleanLuaToolOption(name, label, defaultValue);
    mOptions += prop;
    mOptionByName[name] = prop;
}

void LuaToolOptions::addInteger(const QString &name, const QString &label, int min, int max, int defaultValue)
{
    LuaToolOption *prop = new IntegerLuaToolOption(name, label, min, max, defaultValue);
    mOptions += prop;
    mOptionByName[name] = prop;
}

void LuaToolOptions::addString(const QString &name, const QString &label, const QString &defaultValue)
{
    LuaToolOption *prop = new StringLuaToolOption(name, label, defaultValue);
    mOptions += prop;
    mOptionByName[name] = prop;
}

void LuaToolOptions::addEnum(const QString &name, const QString &label, const QStringList enums, const QString &defaultValue)
{
    LuaToolOption *prop = new EnumLuaToolOption(name, label, enums, defaultValue);
    mOptions += prop;
    mOptionByName[name] = prop;
}

void LuaToolOptions::addList(const QString &name, const QString &label, const QStringList enums, const QString &defaultValue)
{
    LuaToolOption *prop = new ListLuaToolOption(name, label, enums, defaultValue);
    mOptions += prop;
    mOptionByName[name] = prop;
}

/////

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>

LuaToolOptionsWidget::LuaToolOptionsWidget(QWidget *parent) :
    QWidget(parent),
    mOptions(0),
    mSynching(false)
{
    mLayout = new QFormLayout(this);
    mLayout->setContentsMargins(0, 0, 0, 0);
    mLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);
    mLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    setLayout(mLayout);
}

void LuaToolOptionsWidget::setOptions(LuaToolOptions *options)
{
    foreach (QObject *o, children())
        if (o->isWidgetType())
            delete o;
    mCheckBoxes.clear();
    mSpinBoxes.clear();
    mComboBoxes.clear();
    mListWidgets.clear();

    mOptions = options;
    if (!mOptions)
        return;

    foreach (LuaToolOption *prop, mOptions->mOptions) {
        if (mOptions->mSeparators.contains(mLayout->rowCount())) {
            QFrame *w = new QFrame(this);
            w->setObjectName(QString::fromUtf8("line"));
            w->setFrameShape(QFrame::HLine);
            w->setFrameShadow(QFrame::Sunken);
            mLayout->setWidget(mLayout->rowCount(), QFormLayout::SpanningRole, w);
        }
        if (BooleanLuaToolOption *p = prop->asBoolean()) {
            QCheckBox *w = new QCheckBox(p->mLabel, this);
            w->setObjectName(p->mName);
            connect(w, SIGNAL(toggled(bool)), SLOT(checkboxToggled(bool)));
            mCheckBoxes[p->mName] = w;
            mLayout->setWidget(mLayout->rowCount(), QFormLayout::SpanningRole, w);
            continue;
        }
        if (IntegerLuaToolOption *p = prop->asInteger()) {
            QSpinBox *w = new QSpinBox(this);
            w->setObjectName(p->mName);
            w->setRange(p->mMin, p->mMax);
            w->setMinimumWidth(96);
            w->installEventFilter(this); // to disable mousewheel
            connect(w, SIGNAL(valueChanged(int)), SLOT(spinBoxValueChanged(int)));
            mSpinBoxes[p->mName] = w;
            mLayout->addRow(p->mLabel, w);
            continue;
        }
        if (StringLuaToolOption *p = prop->asString()) {
            QComboBox *w = new QComboBox(this);
            w->setObjectName(p->mName);
            w->setSizeAdjustPolicy(QComboBox::AdjustToContents);
            w->setEditable(true);
            w->setInsertPolicy(QComboBox::InsertAlphabetically);
            w->installEventFilter(this); // to disable mousewheel
            connect(w->lineEdit(), SIGNAL(editingFinished()), SLOT(stringEdited()));
            mComboBoxes[p->mName] = w;
            mLayout->addRow(p->mLabel, w);
            continue;
        }
        if (EnumLuaToolOption *p = prop->asEnum()) {
            QComboBox *w = new QComboBox(this);
            w->setObjectName(p->mName);
            w->setSizeAdjustPolicy(QComboBox::AdjustToContents);
            w->addItems(p->mEnums);
            w->installEventFilter(this); // to disable mousewheel
            connect(w, SIGNAL(activated(int)), SLOT(comboBoxActivated(int)));
            mComboBoxes[p->mName] = w;
            mLayout->addRow(p->mLabel, w);
            continue;
        }
        if (ListLuaToolOption *p = prop->asList()) {
            QListWidget *w = new QListWidget(this);
            w->setObjectName(p->mName);
            QSizePolicy policy(w->sizePolicy());
            policy.setVerticalStretch(1);
            w->setSizePolicy(policy);
            w->addItems(p->mEnums);
            w->installEventFilter(this); // to disable mousewheel
            connect(w, SIGNAL(currentRowChanged(int)), SLOT(listRowChanged(int)));
            mListWidgets[p->mName] = w;
            mLayout->addRow(w);
            continue;
        }
    }
}

void LuaToolOptionsWidget::setValue(LuaToolOption *option, const QVariant &value)
{
    mSynching = true;
    if (BooleanLuaToolOption *p = option->asBoolean()) {
        if (QCheckBox *w = mCheckBoxes[p->mName]) {
            w->setChecked(value.toBool());
        }
    } else if (IntegerLuaToolOption *p = option->asInteger()) {
        if (QSpinBox *w = mSpinBoxes[p->mName]) {
            w->setValue(value.toInt());
        }
    } else if (StringLuaToolOption *p = option->asString()) {
        if (QComboBox *w = mComboBoxes[p->mName]) {
            w->setEditText(value.toString());
        }
    } else if (EnumLuaToolOption *p = option->asEnum()) {
        if (QComboBox *w = mComboBoxes[p->mName]) {
            int index = p->mEnums.indexOf(value.toString());
            w->setCurrentIndex(index);
        }
    } else if (ListLuaToolOption *p = option->asList()) {
        if (QListWidget *w = mListWidgets[p->mName]) {
            int index = p->mEnums.indexOf(value.toString());
            w->setCurrentRow(index);
        }
    }
    mSynching = false;
}

void LuaToolOptionsWidget::comboBoxActivated(int index)
{
    if (mSynching)
        return;

    QObject *sender = this->sender();
    if (!sender) return;
    QComboBox *w = dynamic_cast<QComboBox*>(sender);
    if (!w) return;

    LuaToolOption *prop = mOptions->option(w->objectName());
    if (!prop || !prop->asEnum()) {
        Q_ASSERT(false);
        return;
    }
    emit valueChanged(prop, prop->asEnum()->mEnums[index]);
}

void LuaToolOptionsWidget::checkboxToggled(bool value)
{
    if (mSynching)
        return;

    QObject *sender = this->sender();
    if (!sender) return;
    QCheckBox *w = dynamic_cast<QCheckBox*>(sender);
    if (!w) return;

    LuaToolOption *prop = mOptions->option(w->objectName());
    if (!prop || !prop->asBoolean()) {
        Q_ASSERT(false);
        return;
    }
    emit valueChanged(prop, value);
}

void LuaToolOptionsWidget::listRowChanged(int row)
{
    if (mSynching)
        return;

    QObject *sender = this->sender();
    if (!sender) return;
    QListWidget *w = dynamic_cast<QListWidget*>(sender);
    if (!w) return;

    LuaToolOption *prop = mOptions->option(w->objectName());
    if (!prop || !prop->asList()) {
        Q_ASSERT(false);
        return;
    }
    emit valueChanged(prop, prop->asList()->mEnums[row]);
}

void LuaToolOptionsWidget::spinBoxValueChanged(int value)
{
    if (mSynching)
        return;

    QObject *sender = this->sender();
    if (!sender) return;
    QSpinBox *w = dynamic_cast<QSpinBox*>(sender);
    if (!w) return;

    LuaToolOption *prop = mOptions->option(w->objectName());
    if (!prop || !prop->asInteger()) {
        Q_ASSERT(false);
        return;
    }
    emit valueChanged(prop, value);
}

void LuaToolOptionsWidget::stringEdited()
{
    if (mSynching)
        return;

    QObject *sender = this->sender(); // QLineEdit
    if (!sender) return;
    QComboBox *w = dynamic_cast<QComboBox*>(sender->parent());
    if (!w) return;

    LuaToolOption *prop = mOptions->option(w->objectName());
    if (!prop || !prop->asString()) {
        Q_ASSERT(false);
        return;
    }
    emit valueChanged(prop, w->lineEdit()->text());
}
