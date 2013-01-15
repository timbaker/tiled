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
#if 0
    void addEnum(const char *name, const char *shortName,
                 const char *enums[], const char *defaultValue,
                 bool valueAsPropertyName = false);
#endif
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
        virtual void ChangeProperties(int value) // enum or int
        { Q_UNUSED(value) }
        virtual void ChangeProperties(const QString &value)
        { Q_UNUSED(value) }

        virtual bool getBoolean() { return false; }
        virtual int getEnum() { return 0; }
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
            mValue = mProperties.contains(mShortName);
            if (mReverseLogic)
                mValue = !mValue;
        }

        void ToProperties()
        {
            remove(mShortName); // "bed"
            if (mValue == !mReverseLogic)
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
                        int defaultValue = 0,
                        bool valueAsPropertyName = false,
                        const QString &extraPropertyIfSet = QString()) :
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
            return mEnums[mValue];
        }

        void setValue(const QVariant &value)
        {
            mValue = qBound(0, value.toInt(), mEnums.size() - 1);
        }

        void FromProperties()
        {
            mValue = mDefaultValue;

            // EnumName=""
            // ex WestRoofB=""
            if (mValueAsPropertyName) {
                for (int i = 0; i < mShortEnums.size(); i++) {
                    if (i == mDefaultValue)
                        continue; // skip "None"
                    if (contains(mShortEnums[i])) {
                        mValue = i;
                        return;
                    }
                }
                return;
            }

            // PropertyName=EnumName
            // ex LightPolyStyle=WallW
            if (contains(mShortName)) {
                QString enumName = mProperties[mShortName];
                if (mShortEnums.contains(enumName))
                    mValue = mShortEnums.indexOf(enumName);
                else
                    Q_ASSERT(false);
            }
        }

        void ToProperties()
        {
            if (mExtraPropertyIfSet.length())
                remove(mExtraPropertyIfSet);

            if (mValueAsPropertyName) {
                foreach (QString enumName, mShortEnums)
                    remove(enumName);
                if (mValue != mDefaultValue) {
                    set(mShortEnums[mValue]);
                    if (mExtraPropertyIfSet.length())
                        set(mExtraPropertyIfSet);
                }
                return;
            }

            remove(mShortName);
            if (mValue >= 0 && mValue < mShortEnums.size()) {
                if (mValue != mDefaultValue)
                    set(mShortName, mShortEnums[mValue]);
            } else
                Q_ASSERT(false);
        }

        void ChangePropertiesV(const QVariant &value)
        {
            ChangeProperties(value.toInt());
        }

        void ChangeProperties(int value)
        {
            if (value >= 0 && value < mEnums.size()) {
                mValue = value;
            } else
                Q_ASSERT(false);
        }

        int getEnum()
        {
            return mValue;
        }

        QStringList knownPropertyNames() const
        {
            if (mValueAsPropertyName) {
                QStringList ret = mShortEnums;
                ret.removeAt(mDefaultValue);
                if (mExtraPropertyIfSet.length())
                    ret += mExtraPropertyIfSet;
                return ret;
            }
            return QStringList() << mShortName;
        }

        QStringList mEnums;
        QStringList mShortEnums;
        int mValue;
        int mDefaultValue;
        bool mValueAsPropertyName;
        QString mExtraPropertyIfSet;
    };

#if 0
    class PropWallStyle : public PropGenericEnum
    {
    public:
        enum WallStyle
        {
            None,
            WestWall,
            WestWallTrans,
            WestWindow,
            WestDoorFrame,
            NorthWall,
            NorthWallTrans,
            NorthWindow,
            NorthDoorFrame,
            NorthWestCorner,
            NorthWestCornerTrans,
            SouthEastCorner
        };

        PropWallStyle(const QString &name, const QStringList &enums) :
            PropGenericEnum(name, name, enums, None)
        {}

        void FromProperties()
        {
            mValue = None;
#if 0
            if (contains("windowFW")) {
                mValue = WestWindow;
                remove("collideW");
            } else if (contains("windowFN")) {
                mValue = NorthWindow;
                remove("collideN");
            } else if (contains("collideW") && contains("cutW") && !contains("collideN") && !contains("windowFW"))
                mValue = WestWall;
            else if (contains("collideW") && contains("cutW") && contains("collideN"))
                mValue = NorthWestCorner;
            else if (contains("cutN") && contains("cutW") && !contains("collideN") && !contains("collideW"))
                mValue = SouthEastCorner;
            else if (contains("cutN"))
                mValue = NorthDoorFrame;
            else if (contains("cutW"))
                mValue = WestDoorFrame;
#endif
            if (contains("WallW"))
                mValue = WestWall;
            else if (contains("WallN"))
                mValue = NorthWall;
            else if (contains("WallNW"))
                mValue = NorthWestCorner;
            else if (contains("WallWTrans"))
                mValue = WestWallTrans;
            else if (contains("WallNTrans"))
                mValue = NorthWallTrans;
            else if (contains("WallNWTrans"))
                mValue = NorthWestCornerTrans;
            else if (contains("WallSE"))
                mValue = SouthEastCorner;
            else if (contains("WindowW"))
                mValue = WestWindow;
            else if (contains("WindowN"))
                mValue = NorthWindow;
            else if (contains("DoorWallW"))
                mValue = WestDoorFrame;
            else if (contains("DoorWallN"))
                mValue = NorthDoorFrame;

            if (mValue != None)
                set("wall");
        }

        void ToProperties()
        {
            remove("wall");
            remove("WallN");
            remove("WallW");
            remove("WallNTrans");
            remove("WallWTrans");
            remove("DoorWallN");
            remove("DoorWallW");
            remove("WallNW");
            remove("WallNWTrans");
#if 0
            remove("doorFrW");
            remove("doorFrN");
#endif
            remove("WallSE");
            remove("WindowW");
            remove("WindowN");
#if 0
            remove("collideW");
            remove("collideN");
            remove("cutW");
            remove("cutN");
            remove("transparentW");
            remove("transparentN");
            remove("windowFW");
            remove("windowFN");
#endif
            set("wall");
            switch (mValue) {
            case None: remove("wall"); break;
            case NorthWall: set("WallN"); break;
            case WestWall: set("WallW"); break;
            case NorthWallTrans: set("WallNTrans"); break;
            case WestWallTrans: set("WallWTrans"); break;
            case NorthDoorFrame: set("DoorWallN"); break;
            case WestDoorFrame: set("DoorWallW"); break;
            case NorthWestCorner: set("WallNW"); break;
            case NorthWestCornerTrans: set("WallNWTrans"); break;
            case SouthEastCorner: set("WallSE"); break;
            case WestWindow: set("WindowW"); break;
            case NorthWindow: set("WindowN"); break;
            default: Q_ASSERT(false); return;
            }
        }

        QStringList knownPropertyNames() const
        {
            static const char *names[] = {
                "wall",
                "WallN",
                "WallW",
                "WallNTrans",
                "WallWTrans",
                "DoorWallN",
                "DoorWallW",
                "WallNW",
                "WallNWTrans",
                "WallSE",
                "WindowW",
                "WindowN",
                0
            };
            QStringList ret;
            for (int i = 0; names[i]; i++)
                ret += QLatin1String(names[i]);
            return ret;
        }
    };

    class PropRoofStyle : public PropGenericEnum
    {
    public:
        enum RoofStyle
        {
            None,
            WestRoofB,
            WestRoofM,
            WestRoofT
        };

        PropRoofStyle(const QString &name, const QStringList &enums) :
            PropGenericEnum(name, name, enums, None)
        {

        }

        void FromProperties()
        {
            mValue = None;
            if (contains("WestRoofB"))
                mValue = WestRoofB;
            else if (contains("WestRoofM"))
                mValue = WestRoofM;
            else if (contains("WestRoofT"))
                mValue = WestRoofT;
        }

        void ToProperties()
        {
            remove("WestRoofB");
            remove("WestRoofM");
            remove("WestRoofT");
            switch (mValue) {
            case None: break;
            case WestRoofB: set("WestRoofB"); break;
            case WestRoofM: set("WestRoofM"); break;
            case WestRoofT: set("WestRoofT"); break;
            default: Q_ASSERT(false); return;
            }
        }

        QStringList knownPropertyNames() const
        {
            static const char *names[] = {
                "WestRoofB",
                "WestRoofM",
                "WestRoofT",
                0
            };
            QStringList ret;
            for (int i = 0; names[i]; i++)
                ret += QLatin1String(names[i]);
            return ret;
        }
    };

    class PropDoorStyle : public PropGenericEnum
    {
    public:
        enum DoorStyle
        {
            None,
            North,
            West
        };

        PropDoorStyle(const QString &name, const QString &prefix,
                      const QStringList &enums) :
            PropGenericEnum(name, name, enums, None),
            mPrefix(prefix)
        {

        }

        void FromProperties()
        {
            mValue = None;
            if (contains("N"))
                mValue = North;
            else if (contains("W"))
                mValue = West;
        }

        void ToProperties()
        {
            remove(mPrefix + QLatin1String("N"));
            remove(mPrefix + QLatin1String("W"));
            switch (mValue)
            {
            case None: break;
            case North: set(mPrefix + QLatin1String("N")); break;
            case West: set(mPrefix + QLatin1String("W")); break;
            default: Q_ASSERT(false); return;
            }
        }

        bool contains(const char *key)
        {
            return UIProperty::contains(mPrefix + QLatin1String(key)); // doorW, doorFrW, windowW
        }

        QStringList knownPropertyNames() const
        {
            return QStringList() << mPrefix + QLatin1String("W")
                                 << mPrefix + QLatin1String("N");
        }

        QString mPrefix;
    };

    class PropDirection : public PropGenericEnum
    {
    public:
        enum Direction
        {
            None,
            N,
            NE,
            E,
            SE,
            S,
            SW,
            W,
            NW
        };

        PropDirection(const QString &name, const QString &prefix,
                      const QStringList &enums) :
            PropGenericEnum(name, name, enums, None),
            mPrefix(prefix)
        {

        }

        void FromProperties()
        {
            mValue = None;
            if (contains("S") && contains("W"))
                mValue = SW;
            else if (contains("N") && contains("W"))
                mValue = NW;
            else if (contains("S") && contains("E"))
                mValue = SE;
            else if (contains("N") && contains("E"))
                mValue = NE;
            else if (contains("E"))
                mValue = E;
            else if (contains("N"))
                mValue = N;
            else if (contains("S"))
                mValue = S;
            else if (contains("W"))
                mValue = W;
        }

        void ToProperties()
        {
            Q_ASSERT(false); // unimplemented in original???
        }

        bool contains(const char *key)
        {
            return UIProperty::contains(mPrefix + QLatin1String(key)); // shelfW, tableW
        }

        QStringList knownPropertyNames() const
        {
            return QStringList() << mPrefix + QLatin1String("N")
                                 << mPrefix + QLatin1String("NE")
                                 << mPrefix + QLatin1String("E")
                                 << mPrefix + QLatin1String("SE")
                                 << mPrefix + QLatin1String("S")
                                 << mPrefix + QLatin1String("SW")
                                 << mPrefix + QLatin1String("W")
                                 << mPrefix + QLatin1String("NW");
        }

        QString mPrefix;
    };

    class PropStairStyle : public PropGenericEnum
    {
    public:
        enum StairStyle
        {
            None,
            BottomW,
            MiddleW,
            TopW,
            BottomN,
            MiddleN,
            TopN
        };

        PropStairStyle(const QString &name, const QString &prefix,
                       const QStringList &enums) :
            PropGenericEnum(name, name, enums, None),
            mPrefix(prefix)
        {

        }

        void FromProperties()
        {
            mValue = None;
            if (contains("BW"))
                mValue = BottomW;
            else if (contains("MW"))
                mValue = MiddleW;
            else if (contains("TW"))
                mValue = TopW;
            else if (contains("BN"))
                mValue = BottomN;
            else if (contains("MN"))
                mValue = MiddleN;
            else if (contains("TN"))
                mValue = TopN;
        }

        void ToProperties()
        {
            remove("BW");
            remove("MW");
            remove("TW");
            remove("BN");
            remove("MN");
            remove("TN");
            switch (mValue) {
            case None: break;
            case BottomW: set("BW"); break;
            case MiddleW: set("MW"); break;
            case TopW: set("TW"); break;
            case BottomN: set("BN"); break;
            case MiddleN: set("MN"); break;
            case TopN: set("TN"); break;
            default: Q_ASSERT(false); return;
            }
        }

        bool contains(const char *key)
        {
            return UIProperty::contains(mPrefix + QLatin1String(key)); // stairsBW
        }

        void remove(const char *key)
        {
            UIProperty::remove(mPrefix + QLatin1String(key));
        }

        void set(const char *key)
        {
            UIProperty::set(mPrefix + QLatin1String(key));
        }

        QStringList knownPropertyNames() const
        {
            return QStringList() << mPrefix + QLatin1String("BW")
                                 << mPrefix + QLatin1String("MW")
                                 << mPrefix + QLatin1String("TW")
                                 << mPrefix + QLatin1String("BN")
                                 << mPrefix + QLatin1String("MN")
                                 << mPrefix + QLatin1String("TN");
        }

        QString mPrefix;
    };

    class PropTileBlockStyle : public PropGenericEnum
    {
    public:
        enum TileBlockStyle
        {
            None,
            Solid,
            SolidTransparent
        };

        PropTileBlockStyle(const QString &name, const QStringList &enums) :
            PropGenericEnum(name, name, enums, None)
        {

        }

        void FromProperties()
        {
            mValue = None;
            if (contains("solid"))
                mValue = Solid;
            else if (contains("solidtrans"))
                mValue = SolidTransparent;
        }

        void ToProperties()
        {
            remove("solid");
            remove("solidtrans");
            switch (mValue) {
            case None: break;
            case Solid: set("solid"); break;
            case SolidTransparent: set("solidtrans"); break;
            default: Q_ASSERT(false); return;
            }
        }

        QStringList knownPropertyNames() const
        {
            return QStringList() << QLatin1String("solid")
                                 << QLatin1String("solidtrans");
        }
    };

    class PropLightPolyStyle : public PropGenericEnum
    {
    public:
        enum LightPolyStyle
        {
            None,
            WallW,
            WallN
        };

        PropLightPolyStyle(const QString &name, QMap<QString,QString> &properties,
                           const QStringList &enums) :
            PropGenericEnum(name, properties, enums, None)
        {

        }

        void FromProperties()
        {
            mValue = None;
            QString label = QLatin1String("LightPolyStyle");
            if (contains(label)) {
                QString enumName = mProperties[label];
                if (enumName == QLatin1String("WallW"))
                    mValue = WallW;
                else if (enumName == QLatin1String("WallN"))
                    mValue = WallN;
                else
                    Q_ASSERT(false);
            }
        }

        void ChangeProperties(int value)
        {
            remove("LightPolyStyle");
            switch (value) {
            case None: break;
            case WallW: set("LightPolyStyle", "WallW"); break;
            case WallN: set("LightPolyStyle", "WallN"); break;
            default: Q_ASSERT(false); return;
            }
            mValue = qBound(0, value, mEnums.size() - 1);
        }
    };
#endif

    UIProperties();

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
        return 0;
    }

    int getEnum(const QString &label)
    {
        Q_ASSERT(mProperties.contains(label));
        if (mProperties.contains(label))
            return mProperties[label]->getEnum();
        else
            qDebug() << "UIProperties::getEnum" << label;
        return 0;
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

    int getEnum(const QString &name)
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
    TileDefTileset()
    {}

    QString mName;
    QString mImageSource;
    int mColumns;
    int mRows;
    QVector<TileDefTile*> mTiles;
};

/**
  * This class represents a single binary *.tiles file.
  */
class TileDefFile : public QObject
{
    Q_OBJECT
public:
    TileDefFile();

    QString fileName() const
    { return mFileName; }

    void setFileName(const QString &fileName)
    { mFileName = fileName; }

    bool read(const QString &fileName);
    bool write(const QString &fileName);

    QString directory() const;

    void insertTileset(int index, TileDefTileset *ts);
    TileDefTileset *removeTileset(int index);

    TileDefTileset *tileset(const QString &name);

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

    QString errorString() const
    { return mError; }

private:
    bool addProperty(SimpleFileBlock &block);
    bool toBoolean(const char *key, SimpleFileBlock &block, bool &ok);
    int toInt(const char *key, SimpleFileBlock &block, bool &ok);

private:
    Q_DISABLE_COPY(TilePropertyMgr)
    static TilePropertyMgr *mInstance;
    TilePropertyMgr();
    ~TilePropertyMgr();

    TileDefProperties mProperties;
    QString mError;
};

} // namespace Internal
} // namespace Tiled

#endif // TILEDEFFILE_H
