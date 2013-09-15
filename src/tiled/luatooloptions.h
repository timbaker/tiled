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

#ifndef LUATOOLOPTIONS_H
#define LUATOOLOPTIONS_H

#include <QList>
#include <QMap>
#include <QString>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QSpinBox;

namespace Tiled {
namespace Lua {

class BooleanLuaToolOption;
class EnumLuaToolOption;
class IntegerLuaToolOption;
class StringLuaToolOption;

class LuaToolOption
{
public:
    LuaToolOption(const QString &name, const QString &label) :
        mName(name),
        mLabel(label)
    {}
    virtual ~LuaToolOption() {}

    virtual BooleanLuaToolOption *asBoolean() { return 0; }
    virtual EnumLuaToolOption *asEnum() { return 0; }
    virtual IntegerLuaToolOption *asInteger() { return 0; }
    virtual StringLuaToolOption *asString() { return 0; }

    QString mName;
    QString mLabel;
};

class BooleanLuaToolOption : public LuaToolOption
{
public:
    BooleanLuaToolOption(const QString &name, const QString &label, bool defaultValue = false) :
        LuaToolOption(name, label),
        mDefault(defaultValue)
    {

    }

    BooleanLuaToolOption *asBoolean() { return this; }

    bool mDefault;
};

class IntegerLuaToolOption : public LuaToolOption
{
public:
    IntegerLuaToolOption(const QString &name, const QString &label, int min, int max, int defaultValue = 0) :
        LuaToolOption(name, label),
        mMin(min),
        mMax(max),
        mDefault(defaultValue)
    {

    }

    IntegerLuaToolOption *asInteger() { return this; }

    int mMin;
    int mMax;
    int mDefault;
};

class StringLuaToolOption : public LuaToolOption
{
public:
    StringLuaToolOption(const QString &name, const QString &label, const QString &defaultValue = QString()) :
        LuaToolOption(name, label),
        mDefault(defaultValue)
    {

    }

    StringLuaToolOption *asString() { return this; }

    QString mDefault;
};

class EnumLuaToolOption : public LuaToolOption
{
public:
    EnumLuaToolOption(const QString &name, const QString &label, const QStringList &enums, const QString &defaultValue) :
        LuaToolOption(name, label),
        mEnums(enums),
        mDefault(defaultValue)
    {

    }

    EnumLuaToolOption *asEnum() { return this; }

    QStringList mEnums;
    QString mDefault;
};

class LuaToolOptions
{
public:
    LuaToolOptions();
    ~LuaToolOptions();

    void addBoolean(const QString &name, const QString &label, bool defaultValue);
    void addInteger(const QString &name, const QString &label, int min, int max, int defaultValue);
    void addString(const QString &name, const QString &label, const QString &defaultValue);
    void addEnum(const QString &name, const QString &label, const QStringList enums, const QString &defaultValue);

    void addSeparator()
    { mSeparators += mSeparators.size() + mOptions.size(); }

    LuaToolOption *option(const QString &name) const
    {
        if (mOptionByName.contains(name))
            return mOptionByName[name];
        return 0;
    }

    void clear()
    {
        qDeleteAll(mOptions);
        mOptions.clear();
        mOptionByName.clear();
        mSeparators.clear();
    }

    QList<LuaToolOption*> mOptions;
    QMap<QString,LuaToolOption*> mOptionByName;
    QList<int> mSeparators;
};

class LuaToolOptionsWidget : public QWidget
{
    Q_OBJECT
public:
    LuaToolOptionsWidget(QWidget *parent = 0);

    void setOptions(LuaToolOptions *options);
    void setValue(LuaToolOption *option, const QVariant &value);

signals:
    void valueChanged(LuaToolOption *option, const QVariant &value);

private slots:
    void comboBoxActivated(int index);
    void checkboxToggled(bool value);
    void spinBoxValueChanged(int value);
    void stringEdited();

private:
    QFormLayout *mLayout;
    LuaToolOptions *mOptions;
    bool mSynching;

    QMap<QString,QCheckBox*> mCheckBoxes;
    QMap<QString,QComboBox*> mComboBoxes;
    QMap<QString,QSpinBox*> mSpinBoxes;
};

} // namespace Lua
} // namespace Tiled

#endif // LUATOOLOPTIONS_H
