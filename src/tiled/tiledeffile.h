/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#ifndef TILEDEFFILE_H
#define TILEDEFFILE_H

#include <QDebug>
#include <QMap>
#include <QObject>
#include <QPoint>
#include <QStringList>
#include <QVector>

class SimpleFileBlock;
class SimpleFileKeyValue;

namespace Tiled {

class Tileset;

namespace Internal {

class BooleanTileDefProperty;
class EnumTileDefProperty;
class IntegerTileDefProperty;
class StringTileDefProperty;

class TileDefTileset;

class TileDefProperty
{
public:
    TileDefProperty(const QString &name, const QString &shortName) :
        mName(name),
        mShortName(shortName)
    {}
    virtual ~TileDefProperty() {}

    virtual BooleanTileDefProperty *asBoolean() { return 0; }
    virtual EnumTileDefProperty *asEnum() { return 0; }
    virtual IntegerTileDefProperty *asInteger() { return 0; }
    virtual StringTileDefProperty *asString() { return 0; }

    QString mName;
    QString mShortName;
};

class BooleanTileDefProperty : public TileDefProperty
{
public:
    BooleanTileDefProperty(const QString &name,
                           const QString &shortName,
                           bool defaultValue = false,
                           bool reverseLogic = false) :
        TileDefProperty(name, shortName),
        mDefault(defaultValue),
        mReverseLogic(reverseLogic)
    {

    }

    BooleanTileDefProperty *asBoolean() { return this; }

    bool mDefault;
    bool mReverseLogic;
};

class IntegerTileDefProperty : public TileDefProperty
{
public:
    IntegerTileDefProperty(const QString &name,
                           const QString &shortName,
                           int min, int max,
                           int defaultValue = 0) :
        TileDefProperty(name, shortName),
        mMin(min),
        mMax(max),
        mDefault(defaultValue)
    {

    }

    IntegerTileDefProperty *asInteger() { return this; }

    int mMin;
    int mMax;
    int mDefault;
};

class StringTileDefProperty : public TileDefProperty
{
public:
    StringTileDefProperty(const QString &name,
                          const QString &shortName,
                          const QString &defaultValue = QString()) :
        TileDefProperty(name, shortName),
        mDefault(defaultValue)
    {

    }

    StringTileDefProperty *asString() { return this; }

    QString mDefault;
};

class EnumTileDefProperty : public TileDefProperty
{
public:
    EnumTileDefProperty(const QString &name,
                        const QString &shortName,
                        const QStringList &enums,
                        const QStringList &shortEnums,
                        const QString &defaultValue,
                        bool valueAsPropertyName,
                        const QString &extraPropertyIfSet) :
        TileDefProperty(name, shortName),
        mEnums(enums),
        mShortEnums(shortEnums),
        mDefault(defaultValue),
        mValueAsPropertyName(valueAsPropertyName),
        mExtraPropertyIfSet(extraPropertyIfSet)
    {

    }

    EnumTileDefProperty *asEnum() { return this; }

    QStringList mEnums;
    QStringList mShortEnums;
    QString mDefault;
    bool mValueAsPropertyName;
    QString mExtraPropertyIfSet;
};

class TileDefProperties
{
public:
    TileDefProperties();
    ~TileDefProperties();

    void addBoolean(const QString &name, const QString &shortName,
                    bool defaultValue, bool reverseLogic);
    void addInteger(const QString &name, const QString &shortName,
                    int min, int max, int defaultValue);
    void addString(const QString &name, const QString &shortName,
                   const QString &defaultValue);
    void addEnum(const QString &name, const QString &shortName,
                 const QStringList enums, const QStringList &shortEnums,
                 const QString &defaultValue,
                 bool valueAsPropertyName,
                 const QString &extraPropertyIfSet);
    void addSeparator()
    { mSeparators += mSeparators.size() + mProperties.size(); }

    TileDefProperty *property(const QString &name) const
    {
        if (mPropertyByName.contains(name))
            return mPropertyByName[name];
        return 0;
    }

    QList<TileDefProperty*> mProperties;
    QMap<QString,TileDefProperty*> mPropertyByName;
    QList<int> mSeparators;
};

class UIProperties
{
public:

    /**
      * The UIProperty class holds the value of a single property for a single
      * TileDefTile.  Subclasses handle the different property types.
      */
    class UIProperty
    {
    public:
        UIProperty(const QString &name, const QString &shortName) :
            mName(name),
            mShortName(shortName)
        {
            if (mShortName.isEmpty())
                mShortName = mName;
        }
        virtual ~UIProperty() {}

        void FromProperties(const QMap<QString,QString> &props)
        {
            mProperties = props;
            FromProperties();
        }

        void ToProperties(QMap<QString,QString> &props)
        {
            mProperties = props;

            ToProperties();

            props = mProperties;
        }

    protected:
        virtual void FromProperties() = 0;
        virtual void ToProperties() = 0;
    public:

        void set(const char *key, const char *value = "")
        {
            set(QLatin1String(key),  QLatin1String(value));
        }
        void set(const QString &key, const QString &value = QString())
        {
            mProperties[key] = value;
        }

        void remove(const char *key)
        {
            remove(QLatin1String(key));
        }
        void remove(const QString &key)
        {
            mProperties.remove(key);
        }

        bool contains(const char *key)
        {
            return contains(QLatin1String(key));
        }
        bool contains(const QString &key)
        {
            return mProperties.contains(key);
        }

        virtual bool isDefaultValue() const = 0;
        virtual QVariant defaultValue() const = 0;

        virtual QString valueAsString() const = 0;
        virtual QVariant value() const = 0;
        virtual void setValue(const QVariant &value) = 0;

        virtual void resetValue()
        { setValue(defaultValue()); }

        virtual void ChangePropertiesV(const QVariant &value)
        { Q_UNUSED(value) }
        virtual void ChangeProperties(bool value)
        { Q_UNUSED(value) }
        virtual void ChangeProperties(int value)
        { Q_UNUSED(value) }
        virtual void ChangeProperties(const QString &value) // enum or string
        { Q_UNUSED(value) }

        virtual bool getBoolean() { return false; }
        virtual QString getEnum() { return QString(); }
        virtual int getInteger() { return 0; }
        virtual QString getString() { return QString(); }

        virtual QStringList knownPropertyNames() const
        { return QStringList() << mShortName; }

        QString mName;
        QString mShortName;
        QMap<QString,QString> mProperties;
    };

    class PropGenericBoolean : public UIProperty
    {
    public:
        PropGenericBoolean(const QString &name, const QString &shortName,
                           bool defaultValue = false,
                           bool reverseLogic = false) :
            UIProperty(name, shortName),
            mValue(defaultValue),
            mDefaultValue(defaultValue),
            mReverseLogic(reverseLogic)
        {
        }

        bool isDefaultValue() const
        {
            return mValue == mDefaultValue;
        }

        QVariant defaultValue() const
        {
            return mDefaultValue;
        }

        QVariant value() const
        {
            return mValue;
        }

        QString valueAsString() const
        {
            return mValue ? QLatin1String("True") : QLatin1String("False");
        }

        void setValue(const QVariant &value)
        {
            mValue = value.toBool();
        }

        void FromProperties()
        {
            mValue = mDefaultValue;
            if (mProperties.contains(mShortName)) {
                if (!mReverseLogic)
                    mValue = !mDefaultValue;
            }
        }

        void ToProperties()
        {
            remove(mShortName);
            if (mReverseLogic && (mValue == mDefaultValue))
                set(mShortName);
            else if (!mReverseLogic && (mValue != mDefaultValue))
                set(mShortName);
        }

        void ChangePropertiesV(const QVariant &value)
        {
            ChangeProperties(value.toBool());
        }

        void ChangeProperties(bool value)
        {
            mValue = value;
        }

        bool getBoolean()
        {
            return mValue;
        }

        bool mValue;
        bool mDefaultValue;
        bool mReverseLogic;
    };

    class PropGenericInteger : public UIProperty
    {
    public:
        PropGenericInteger(const QString &name, const QString &shortName,
                           int min, int max, int defaultValue = 0) :
            UIProperty(name, shortName),
            mMin(min),
            mMax(max),
            mDefaultValue(defaultValue),
            mValue(defaultValue)
        {
        }

        bool isDefaultValue() const
        {
            return mValue == mDefaultValue;
        }

        QVariant defaultValue() const
        {
            return mDefaultValue;
        }

        QVariant value() const
        {
            return mValue;
        }

        QString valueAsString() const
        {
            return QString::number(mValue);
        }

        void setValue(const QVariant &value)
        {
            mValue = value.toInt();
        }

        void FromProperties()
        {
            mValue = mDefaultValue;
            if (contains(mShortName))
                mValue = mProperties[mShortName].toInt();
        }

        void ToProperties()
        {
            remove(mShortName); // "waterAmount"
            if (mValue != mDefaultValue)
                set(mShortName, QString::number(mValue));
        }

        void ChangePropertiesV(const QVariant &value)
        {
            ChangeProperties(value.toInt());
        }

        void ChangeProperties(int value)
        {
            mValue = value;
        }

        int getInteger()
        {
            return mValue;
        }

        int mMin;
        int mMax;
        int mDefaultValue;
        int mValue;
    };

    class PropGenericString : public UIProperty
    {
    public:
        PropGenericString(const QString &name, const QString &shortName,
                          const QString &defaultValue = QString()) :
            UIProperty(name, shortName),
            mValue(defaultValue),
            mDefaultValue(defaultValue)
        {
        }

        bool isDefaultValue() const
        {
            return mValue == mDefaultValue;
        }

        QVariant defaultValue() const
        {
            return mDefaultValue;
        }

        QVariant value() const
        {
            return mValue;
        }

        QString valueAsString() const
        {
            return mValue;
        }

        void setValue(const QVariant &value)
        {
            mValue = value.toString();
        }

        void FromProperties()
        {
            mValue.clear();
            if (contains(mShortName))
                mValue = mProperties[mShortName];
        }

        void ToProperties()
        {
            remove(mShortName); // "container"
            if (mValue.length())
                set(mShortName, mValue);
        }

        void ChangePropertiesV(const QVariant &value)
        {
            ChangeProperties(value.toString());
        }

        void ChangeProperties(const QString &value)
        {
            mValue = value.trimmed();
        }

        QString getString()
        {
            return mValue;
        }

        QString mValue;
        QString mDefaultValue;
    };

    class PropGenericEnum : public UIProperty
    {
    public:
        PropGenericEnum(const QString &name,
                        const QString &shortName,
                        const QStringList &enums,
                        const QStringList &shortEnums,
                        const QString defaultValue,
                        bool valueAsPropertyName,
                        const QString &extraPropertyIfSet) :
            UIProperty(name, shortName),
            mEnums(enums),
            mShortEnums(shortEnums),
            mValue(defaultValue),
            mDefaultValue(defaultValue),
            mValueAsPropertyName(valueAsPropertyName),
            mExtraPropertyIfSet(extraPropertyIfSet)
        {
        }

        bool isDefaultValue() const
        {
            return mValue == mDefaultValue;
        }

        QVariant defaultValue() const
        {
            return mDefaultValue;
        }

        QVariant value() const
        {
            return mValue;
        }

        QString valueAsString() const
        {
            return mValue;
        }

        void setValue(const QVariant &value)
        {
            QString enumName = value.toString();
            if (mEnums.contains(enumName))
                mValue = enumName;
            else
                Q_ASSERT(false);
        }

        void FromProperties()
        {
            mValue = mDefaultValue;

            // EnumName=""
            // ex WestRoofB=""
            if (mValueAsPropertyName) {
                for (int i = 0; i < mShortEnums.size(); i++) {
                    if (mEnums[i] == mDefaultValue)
                        continue; // skip "None"
                    if (contains(mShortEnums[i])) {
                        mValue = mEnums[i];
                        return;
                    }
                }
                return;
            }

            // PropertyName=EnumName
            // ex LightPolyStyle=WallW
            if (contains(mShortName)) {
                QString enumName = mProperties[mShortName];
                int index = mShortEnums.indexOf(enumName);
                if (index >= 0)
                    mValue = mEnums[index];
            }
        }

        void ToProperties()
        {
            if (mExtraPropertyIfSet.length())
                remove(mExtraPropertyIfSet);

            int index = mEnums.indexOf(mValue);

            if (mValueAsPropertyName) {
                foreach (QString enumName, mShortEnums)
                    remove(enumName);
                if (mValue != mDefaultValue) {
                    if (index >= 0) {
                        set(mShortEnums[index]);
                        if (mExtraPropertyIfSet.length())
                            set(mExtraPropertyIfSet);
                    } else
                        Q_ASSERT(false);
                }
                return;
            }

            remove(mShortName);
            if (index >= 0) {
                if (mEnums[index] != mDefaultValue)
                    set(mShortName, mShortEnums[index]);
            } else
                Q_ASSERT(false);
        }

        void ChangePropertiesV(const QVariant &value)
        {
            ChangeProperties(value.toString());
        }

        void ChangeProperties(const QString &value)
        {
            if (mEnums.contains(value)) {
                mValue = value;
            } else
                Q_ASSERT(false);
        }

        QString getEnum()
        {
            return mValue;
        }

        QStringList knownPropertyNames() const
        {
            if (mValueAsPropertyName) {
                QStringList ret = mShortEnums;
                int index = mEnums.indexOf(mDefaultValue);
                ret.removeAt(index);
                if (mExtraPropertyIfSet.length())
                    ret += mExtraPropertyIfSet;
                return ret;
            }
            return QStringList() << mShortName;
        }

        QStringList mEnums;
        QStringList mShortEnums;
        QString mValue;
        QString mDefaultValue;
        bool mValueAsPropertyName;
        QString mExtraPropertyIfSet;
    };

    UIProperties();
    ~UIProperties();

    UIProperty *property(const QString &name) const
    {
        if (mProperties.contains(name))
            return mProperties[name];
        return 0;
    }

    void ChangeProperties(const QString &label, bool newValue)
    {
        Q_ASSERT(mProperties.contains(label));
        if (mProperties.contains(label))
            mProperties[label]->ChangeProperties(newValue);
    }

    void ChangeProperties(const QString &label, int newValue)
    {
        Q_ASSERT(mProperties.contains(label));
        if (mProperties.contains(label))
            mProperties[label]->ChangeProperties(newValue);
    }

    void ChangePropertiesV(const QString &label, const QVariant &newValue)
    {
        Q_ASSERT(mProperties.contains(label));
        if (mProperties.contains(label))
            mProperties[label]->ChangePropertiesV(newValue);
    }

    void FromProperties(const QMap<QString,QString> &properties)
    {
        foreach (UIProperty *p, mProperties)
            p->FromProperties(properties);
    }

    void ToProperties(QMap<QString,QString> &properties)
    {
        foreach (UIProperty *p, mProperties)
            p->ToProperties(properties);
    }

    bool getBoolean(const QString &label)
    {
        Q_ASSERT(mProperties.contains(label));
        if (mProperties.contains(label))
            return mProperties[label]->getBoolean();
        else
            qDebug() << "UIProperties::getBoolean" << label;
        return false;
    }

    int getInteger(const QString &label)
    {
        Q_ASSERT(mProperties.contains(label));
        if (mProperties.contains(label))
            return mProperties[label]->getInteger();
        else
            qDebug() << "UIProperties::getInteger" << label;
        return 0;
    }

    QString getString(const QString &label)
    {
        Q_ASSERT(mProperties.contains(label));
        if (mProperties.contains(label))
            return mProperties[label]->getString();
        else
            qDebug() << "UIProperties::getString" << label;
        return QString();
    }

    QString getEnum(const QString &label)
    {
        Q_ASSERT(mProperties.contains(label));
        if (mProperties.contains(label))
            return mProperties[label]->getEnum();
        else
            qDebug() << "UIProperties::getEnum" << label;
        return QString();
    }

    QList<UIProperty*> nonDefaultProperties() const
    {
        QList<UIProperty*> ret;
        foreach (UIProperty *prop, mProperties) {
            if (!prop->isDefaultValue())
                ret += prop;
        }
        return ret;
    }

    QStringList knownPropertyNames() const
    {
        QSet<QString> ret;
        foreach (UIProperty *prop, mProperties) {
            foreach (QString s, prop->knownPropertyNames())
                ret.insert(s);
        }
        return ret.toList();
    }

    void copy(const UIProperties &other)
    {
        foreach (UIProperty *prop, mProperties)
            prop->setValue(other.property(prop->mName)->value());
    }

    QMap<QString,UIProperty*> mProperties;
};

class TileDefTile
{
public:
    TileDefTile(TileDefTileset *tileset, int id) :
        mTileset(tileset),
        mID(id),
        mPropertyUI()
    {
    }

    TileDefTileset *tileset() const { return mTileset; }
    int id() const { return mID; }

    UIProperties::UIProperty *property(const QString &name)
    { return mPropertyUI.property(name); }

    bool getBoolean(const QString &name)
    {
        return mPropertyUI.getBoolean(name);
    }

    int getInteger(const QString &name)
    {
        return mPropertyUI.getInteger(name);
    }

    QString getString(const QString &name)
    {
        return mPropertyUI.getString(name);
    }

    QString getEnum(const QString &name)
    {
        return mPropertyUI.getEnum(name);
    }

    TileDefTileset *mTileset;
    int mID;
    UIProperties mPropertyUI;

    // This is to preserve all the properties that were in the .tiles file
    // for this tile.  If TileProperties.txt changes so that these properties
    // can't be edited they will still persist in the .tiles file.
    // TODO: add a way to report/clean out obsolete properties.
    QMap<QString,QString> mProperties;
};

class TileDefTileset
{
public:
    TileDefTileset(Tileset *ts);
    TileDefTileset();
    ~TileDefTileset();

    TileDefTile *tileAt(int index);
    void resize(int columns, int rows);

    QString mName;
    QString mImageSource;
    int mColumns;
    int mRows;
    QVector<TileDefTile*> mTiles;
    int mID;
};

/**
  * This class represents a single binary *.tiles file.
  */
class TileDefFile : public QObject
{
    Q_OBJECT
public:
    TileDefFile();
    ~TileDefFile();

    QString fileName() const
    { return mFileName; }

    void setFileName(const QString &fileName)
    { mFileName = fileName; }

    bool read(const QString &fileName);
    bool write(const QString &fileName);

    QString directory() const;

    void insertTileset(int index, TileDefTileset *ts);
    TileDefTileset *removeTileset(int index);

    TileDefTileset *tileset(const QString &name) const;

    const QList<TileDefTileset*> &tilesets() const
    { return mTilesets; }

    QStringList tilesetNames() const
    { return mTilesetByName.keys(); }

    QString errorString() const
    { return mError; }

private:
    QList<TileDefTileset*> mTilesets;
    QMap<QString,TileDefTileset*> mTilesetByName;
    QString mFileName;
    QString mError;
};

class TilePropertyModifier;

/**
  * This class manages the TileProperties.txt file.
  */
class TilePropertyMgr : public QObject
{
    Q_OBJECT
public:
    static TilePropertyMgr *instance();
    static void deleteInstance();

    bool hasReadTxt()
    { return !mProperties.mProperties.isEmpty(); }

    QString txtName();
    QString txtPath();
    bool readTxt();

    const TileDefProperties &properties() const
    { return mProperties; }

    void modify(QMap<QString,QString> &properties);

    QString errorString() const
    { return mError; }

private:
    bool addProperty(SimpleFileBlock &block);
    bool toBoolean(const char *key, SimpleFileBlock &block, bool &ok);
    int toInt(const char *key, SimpleFileBlock &block, bool &ok);

    bool addModifier(SimpleFileBlock &block);

private:
    Q_DISABLE_COPY(TilePropertyMgr)
    static TilePropertyMgr *mInstance;
    TilePropertyMgr();
    ~TilePropertyMgr();

    TileDefProperties mProperties;
    QList<TilePropertyModifier*> mModifiers;
    QString mError;
};

} // namespace Internal
} // namespace Tiled

#endif // TILEDEFFILE_H
