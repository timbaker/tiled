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

namespace Tiled {
namespace Internal {

class BooleanTileDefProperty;
class EnumTileDefProperty;
class IntegerTileDefProperty;
class StringTileDefProperty;

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
                           int defaultValue = 0) :
        TileDefProperty(name, shortName),
        mDefault(defaultValue)
    {

    }

    IntegerTileDefProperty *asInteger() { return this; }

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
                        const QStringList &enums) :
        TileDefProperty(name, shortName),
        mEnums(enums)
    {

    }

    EnumTileDefProperty *asEnum() { return this; }

    QStringList mEnums;
};

class TileDefProperties
{
public:
    TileDefProperties();
    ~TileDefProperties();

    void addBoolean(const char *name, const char *shortName = 0,
                    bool defaultValue = false, bool reverseLogic = false);
    void addInteger(const char *name, const char *shortName = 0, int defaultValue = 0);
    void addString(const char *name, const char *shortName = 0, const QString &defaultValue = QString());
    void addEnum(const char *name, const char *shortName, const char *enums[]);

    QMap<QString,TileDefProperty*> mProperties;
};

// TileDefTile.mProperties["bed"] -> TileDefProperties["IsBed"]

class UIProperties
{
public:
    class UIProperty
    {
    public:
        UIProperty(const QString &name, QMap<QString,QString> &properties) :
            mName(name),
            mProperties(properties)
        {

        }

        virtual void FromProperties() {}

        void set(const char *key, const char *value = "")
        {
            mProperties[QLatin1String(key)] = QLatin1String(value);
        }
        void set(const QString &key, const QString &value = QString())
        {
            mProperties[key] = value;
        }

        void remove(const char *key)
        {
            mProperties.remove(QLatin1String(key));
        }
        void remove(const QString &key)
        {
            mProperties.remove(key);
        }

        bool contains(const char *key)
        {
            return mProperties.contains(QLatin1String(key));
        }
        bool contains(const QString &key)
        {
            return mProperties.contains(key);
        }

        virtual bool isDefaultValue() const = 0;
        virtual QString valueAsString() const = 0;

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

        QString mName;
        QMap<QString,QString> &mProperties;
    };

    class PropGenericBoolean : public UIProperty
    {
    public:
        PropGenericBoolean(const QString &name, const QString &shortName,
                           QMap<QString,QString> &properties,
                           bool defaultValue = false,
                           bool reverseLogic = false) :
            UIProperty(name, properties),
            mShortName(shortName),
            mValue(defaultValue),
            mDefaultValue(defaultValue),
            mReverseLogic(reverseLogic)
        {
        }

        bool isDefaultValue() const
        {
            return mValue == mDefaultValue;
        }

        QString valueAsString() const
        {
            return mValue ? QLatin1String("True") : QLatin1String("False");
        }

        void FromProperties()
        {
            mValue = mProperties.contains(mShortName);
            if (mReverseLogic)
                mValue = !mValue;
        }

        void ChangeProperties(bool value)
        {
            remove(mShortName); // "bed"
            if (value == !mReverseLogic)
                set(mShortName);
            mValue = value;
        }

        bool getBoolean()
        {
            return mValue;
        }

        QString mShortName;
        bool mValue;
        bool mDefaultValue;
        bool mReverseLogic;
    };

    class PropGenericInteger : public UIProperty
    {
    public:
        PropGenericInteger(const QString &name, const QString &shortName,
                           QMap<QString,QString> &properties,
                           int defaultValue = 0) :
            UIProperty(name, properties),
            mShortName(shortName),
            mValue(defaultValue),
            mDefaultValue(defaultValue)
        {
        }

        bool isDefaultValue() const
        {
            return mValue == mDefaultValue;
        }

        QString valueAsString() const
        {
            return QString::number(mValue);
        }

        void FromProperties()
        {
            mValue = 0;
            if (contains(mShortName))
                mValue = mProperties[mShortName].toInt();
        }

        void ChangeProperties(int value)
        {
            remove(mShortName); // "waterAmount"
            if (value != 0)
                set(mShortName, QString::number(value));
            mValue = value;
        }

        int getInteger()
        {
            return mValue;
        }

        QString mShortName;
        int mValue;
        int mDefaultValue;
    };

    class PropGenericString : public UIProperty
    {
    public:
        PropGenericString(const QString &name, const QString &shortName,
                          QMap<QString,QString> &properties,
                          const QString &defaultValue = QString()) :
            UIProperty(name, properties),
            mShortName(shortName),
            mValue(defaultValue),
            mDefaultValue(defaultValue)
        {
        }

        bool isDefaultValue() const
        {
            return mValue == mDefaultValue;
        }

        QString valueAsString() const
        {
            return mValue;
        }

        void FromProperties()
        {
            mValue.clear();
            if (contains(mShortName))
                mValue = mProperties[mShortName];
        }

        void ChangeProperties(const QString &value)
        {
            mValue = value.trimmed();
            remove(mShortName); // "container"
            if (mValue.length())
                set(mShortName, mValue);
        }

        QString getString()
        {
            return mValue;
        }

        QString mShortName;
        QString mValue;
        QString mDefaultValue;
    };

    class PropWallStyle : public UIProperty
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

        PropWallStyle(const QString &name, QMap<QString,QString> &properties) :
            UIProperty(name, properties),
            mEnum(None)
        {}

        bool isDefaultValue() const
        {
            return mEnum == None;
        }

        QString valueAsString() const
        {
            switch (mEnum) {
            case None: return QLatin1String("None");
            case WestWall: return QLatin1String("WestWall");
            case WestWallTrans: return QLatin1String("WestWallTrans");
            case WestWindow: return QLatin1String("WestWindow");
            case WestDoorFrame: return QLatin1String("WestDoorFrame");
            case NorthWall: return QLatin1String("NorthWall");
            case NorthWallTrans: return QLatin1String("NorthWallTrans");
            case NorthWindow: return QLatin1String("NorthWindow");
            case NorthDoorFrame: return QLatin1String("NorthDoorFrame");
            case NorthWestCorner: return QLatin1String("NorthWestCorner");
            case NorthWestCornerTrans: return QLatin1String("NorthWestCornerTrans");
            case SouthEastCorner: return QLatin1String("SouthEastCorner");
            default: Q_ASSERT(false); break;
            }
            return QString();
        }

        void FromProperties()
        {
            mEnum = None;

            if (contains("windowFW")) {
                mEnum = WestWindow;
                remove("collideW");
            } else if (contains("windowFN")) {
                mEnum = NorthWindow;
                remove("collideN");
            } else if (contains("collideW") && contains("cutW") && !contains("collideN") && !contains("windowFW"))
                mEnum = WestWall;
            else if (contains("collideW") && contains("cutW") && contains("collideN"))
                mEnum = NorthWestCorner;
            else if (contains("cutN") && contains("cutW") && !contains("collideN") && !contains("collideW"))
                mEnum = SouthEastCorner;
            else if (contains("cutN"))
                mEnum = NorthDoorFrame;
            else if (contains("cutW"))
                mEnum = WestDoorFrame;

            if (contains("WallW"))
                mEnum = WestWall;
            else if (contains("WallN"))
                mEnum = NorthWall;
            else if (contains("WallNW"))
                mEnum = NorthWestCorner;
            else if (contains("WallWTrans"))
                mEnum = WestWallTrans;
            else if (contains("WallNTrans"))
                mEnum = NorthWallTrans;
            else if (contains("WallNWTrans"))
                mEnum = NorthWestCornerTrans;
            else if (contains("WallSE"))
                mEnum = SouthEastCorner;
            else if (contains("WindowW"))
                mEnum = WestWindow;
            else if (contains("WindowN"))
                mEnum = NorthWindow;
            else if (contains("DoorWallW"))
                mEnum = WestDoorFrame;
            else if (contains("DoorWallN"))
                mEnum = NorthDoorFrame;
        }

        void ChangeProperties(int value)
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
            remove("doorFrW");
            remove("doorFrN");
            remove("WallSE");
            remove("WindowW");
            remove("WindowN");
            remove("collideW");
            remove("collideN");
            remove("cutW");
            remove("cutN");
            remove("transparentW");
            remove("transparentN");
            remove("windowFW");
            remove("windowFN");
            set("wall");
            switch (value) {
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
            mEnum = static_cast<WallStyle>(value);
        }

        int getEnum() { return mEnum; }

        WallStyle mEnum;
    };

    class PropRoofStyle : public UIProperty
    {
    public:
        enum RoofStyle
        {
            None,
            WestRoofB,
            WestRoofM,
            WestRoofT
        };

        PropRoofStyle(const QString &name, QMap<QString,QString> &properties) :
            UIProperty(name, properties),
            mEnum(None)
        {

        }

        bool isDefaultValue() const
        {
            return mEnum == None;
        }

        QString valueAsString() const
        {
            switch (mEnum) {
            case None: return QLatin1String("None");
            case WestRoofB: return QLatin1String("WestRoofB");
            case WestRoofM: return QLatin1String("WestRoofM");
            case WestRoofT: return QLatin1String("WestRoofT");
            default: Q_ASSERT(false); break;
            }
            return QString();
        }

        void FromProperties()
        {
            if (contains("WestRoofB"))
                mEnum = WestRoofB;
            else if (contains("WestRoofM"))
                mEnum = WestRoofM;
            else if (contains("WestRoofT"))
                mEnum = WestRoofT;
        }

        void ChangeProperties(int value)
        {
            mEnum = None;
            remove("WestRoofB");
            remove("WestRoofM");
            remove("WestRoofT");
            switch (value) {
            case None: break;
            case WestRoofB: set("WestRoofB"); break;
            case WestRoofM: set("WestRoofM"); break;
            case WestRoofT: set("WestRoofT"); break;
            default: Q_ASSERT(false); return;
            }
            mEnum = static_cast<RoofStyle>(value);
        }

        int getEnum() { return mEnum; }

        RoofStyle mEnum;
    };

    class PropDoorStyle : public UIProperty
    {
    public:
        enum DoorStyle
        {
            None,
            North,
            West
        };

        PropDoorStyle(const QString &name, const QString &prefix,
                      QMap<QString,QString> &properties) :
            UIProperty(name, properties),
            mPrefix(prefix),
            mEnum(None)
        {

        }

        bool isDefaultValue() const
        {
            return mEnum == None;
        }

        QString valueAsString() const
        {
            switch (mEnum) {
            case None: return QLatin1String("None");
            case North: return QLatin1String("North");
            case West: return QLatin1String("West");
            default: Q_ASSERT(false); break;
            }
            return QString();
        }

        void FromProperties()
        {
            mEnum = None;
            if (contains("N"))
                mEnum = North;
            else if (contains("W"))
                mEnum = West;
        }

        void ChangeProperties(int value)
        {
            remove(mPrefix + QLatin1String("N"));
            remove(mPrefix + QLatin1String("W"));
            switch (value)
            {
            case None: break;
            case North: set(mPrefix + QLatin1String("N")); break;
            case West: set(mPrefix + QLatin1String("W")); break;
            default: Q_ASSERT(false); return;
            }
            mEnum = static_cast<DoorStyle>(value);
        }

        int getEnum() { return mEnum; }

        bool contains(const char *key)
        {
            return UIProperty::contains(mPrefix + QLatin1String(key)); // doorW, doorFrW, windowW
        }

        QString mPrefix;
        DoorStyle mEnum;
    };

    class PropDirection : public UIProperty
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
                      QMap<QString,QString> &properties) :
            UIProperty(name, properties),
            mPrefix(prefix),
            mEnum(None)
        {

        }

        bool isDefaultValue() const
        {
            return mEnum == None;
        }

        QString valueAsString() const
        {
            switch (mEnum) {
            case None: return QLatin1String("None");
            case N: return QLatin1String("N");
            case NE: return QLatin1String("NE");
            case E: return QLatin1String("E");
            case SE: return QLatin1String("SE");
            case S: return QLatin1String("S");
            case SW: return QLatin1String("SW");
            case W: return QLatin1String("W");
            case NW: return QLatin1String("NW");
            default: Q_ASSERT(false); break;
            }
            return QString();
        }

        void FromProperties()
        {
            mEnum = None;
            if (contains("S") && contains("W"))
                mEnum = SW;
            else if (contains("N") && contains("W"))
                mEnum = NW;
            else if (contains("S") && contains("E"))
                mEnum = SE;
            else if (contains("N") && contains("E"))
                mEnum = NE;
            else if (contains("E"))
                mEnum = E;
            else if (contains("N"))
                mEnum = N;
            else if (contains("S"))
                mEnum = S;
            else if (contains("W"))
                mEnum = W;
        }

        void ChangeProperties(int value)
        {
            Q_UNUSED(value)
            Q_ASSERT(false); // unimplemented in original???
        }

        int getEnum() { return mEnum; }

        bool contains(const char *key)
        {
            return UIProperty::contains(mPrefix + QLatin1String(key)); // shelfW, tableW
        }

        QString mPrefix;
        Direction mEnum;
    };

    class PropStairStyle : public UIProperty
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
                       QMap<QString,QString> &properties) :
            UIProperty(name, properties),
            mPrefix(prefix),
            mEnum(None)
        {

        }

        bool isDefaultValue() const
        {
            return mEnum == None;
        }

        QString valueAsString() const
        {
            switch (mEnum) {
            case None: return QLatin1String("None");
            case BottomW: return QLatin1String("BottomW");
            case MiddleW: return QLatin1String("MiddleW");
            case TopW: return QLatin1String("TopW");
            case BottomN: return QLatin1String("BottomN");
            case MiddleN: return QLatin1String("MiddleN");
            case TopN: return QLatin1String("TopN");
            default: Q_ASSERT(false); break;
            }
            return QString();
        }

        void FromProperties()
        {
            mEnum = None;
            if (contains("BW"))
                mEnum = BottomW;
            else if (contains("MW"))
                mEnum = MiddleW;
            else if (contains("TW"))
                mEnum = TopW;
            else if (contains("BN"))
                mEnum = BottomN;
            else if (contains("MN"))
                mEnum = MiddleN;
            else if (contains("TN"))
                mEnum = TopN;
        }

        void ChangeProperties(int value)
        {
            remove("BW");
            remove("MW");
            remove("TW");
            remove("BN");
            remove("MN");
            remove("TN");
            switch (value) {
            case None: break;
            case BottomW: set("BW"); break;
            case MiddleW: set("MW"); break;
            case TopW: set("TW"); break;
            case BottomN: set("BN"); break;
            case MiddleN: set("MN"); break;
            case TopN: set("TN"); break;
            default: Q_ASSERT(false); return;
            }
            mEnum = static_cast<StairStyle>(value);
        }

        int getEnum() { return mEnum; }

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

        QString mPrefix;
        StairStyle mEnum;
    };

    class PropTileBlockStyle : public UIProperty
    {
    public:
        enum TileBlockStyle
        {
            None,
            Solid,
            SolidTransparent
        };

        PropTileBlockStyle(const QString &name, QMap<QString,QString> &properties) :
            UIProperty(name, properties),
            mEnum(None)
        {

        }

        bool isDefaultValue() const
        {
            return mEnum == None;
        }

        QString valueAsString() const
        {
            switch (mEnum) {
            case None: return QLatin1String("None");
            case Solid: return QLatin1String("Solid");
            case SolidTransparent: return QLatin1String("SolidTransparent");
            default: Q_ASSERT(false); break;
            }
            return QString();
        }

        void FromProperties()
        {
            mEnum = None;
            if (contains("solid"))
                mEnum = Solid;
            else if (contains("solidtrans"))
                mEnum = SolidTransparent;
        }

        void ChangeProperties(int value)
        {
            remove("solid");
            remove("solidtrans");
            switch (value) {
            case None: break;
            case Solid: set("solid"); break;
            case SolidTransparent: set("solidtrans"); break;
            default: Q_ASSERT(false); return;
            }
            mEnum = static_cast<TileBlockStyle>(value);
        }

        int getEnum() { return mEnum; }

        TileBlockStyle mEnum;
    };

    class PropLightPolyStyle : public UIProperty
    {
    public:
        enum LightPolyStyle
        {
            None,
            WallW,
            WallN
        };

        PropLightPolyStyle(const QString &name, QMap<QString,QString> &properties) :
            UIProperty(name, properties),
            mEnum(None)
        {

        }

        bool isDefaultValue() const
        {
            return mEnum == None;
        }

        QString valueAsString() const
        {
            switch (mEnum) {
            case None: return QLatin1String("None");
            case WallW: return QLatin1String("WallW");
            case WallN: return QLatin1String("WallN");
            default: Q_ASSERT(false); break;
            }
            return QString();
        }

        void FromProperties()
        {
            mEnum = None;
            QString label = QLatin1String("LightPolyStyle");
            if (contains(label)) {
                QString enumName = mProperties[label];
                if (enumName == QLatin1String("WallW"))
                    mEnum = WallW;
                else if (enumName == QLatin1String("WallN"))
                    mEnum = WallN;
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
            mEnum = static_cast<LightPolyStyle>(value);
        }

        int getEnum() { return mEnum; }

        LightPolyStyle mEnum;
    };

    UIProperties(QMap<QString,QString> &properties);

    void add(const char *label, UIProperty &prop)
    {
        mProperties[QLatin1String(label)] = &prop;
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

    void FromProperties()
    {
        foreach (UIProperty *p, mProperties)
            p->FromProperties();
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

    QMap<QString,UIProperty*> mProperties;

#if 0
    PropGenericBoolean mCollideWest;
    PropGenericBoolean mCollideNorth;

    PropDoorStyle mDoor;
    PropDoorStyle mDoorFrame;

    PropGenericBoolean mIsBed;

    PropGenericBoolean mFloorOverlay;
    PropGenericBoolean mIsFloor;
    PropGenericBoolean mIsIndoor;

    PropRoofStyle mRoofStyle;

    PropGenericBoolean mClimbSheetN;
    PropGenericBoolean mClimbSheetW;

    PropDirection mFloorItemShelf;
    PropDirection mHighItemShelf;
    PropDirection mTableItemShelf;

    //PropStairStyle mStairStyle;

    PropGenericBoolean mPreSeen;

    PropGenericBoolean mHoppableN;
    PropGenericBoolean mHoppableW;
    PropGenericBoolean mWallOverlay;
    PropWallStyle mWallStyle;

    PropGenericInteger mWaterAmount;
    PropGenericInteger mWaterMaxAmount;
    PropGenericBoolean mWaterPiped;

    PropGenericInteger mOpenTileOffset;
    PropGenericInteger mSmashedTileOffset;
    //PropWindowStyle mWindowStyle;
    PropGenericBoolean mWindowLocked;
#endif
};

class TileDefTile
{
public:
    TileDefTile() :
        mPropertyUI(mProperties)
    {

    }

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

    QMap<QString,QString> mProperties;
    UIProperties mPropertyUI;
};

class TileDefTileset
{
public:
    QString mName;
    QString mImageSource;
    int mColumns;
    int mRows;
    QVector<TileDefTile*> mTiles;
};

class TileDefFile : public QObject
{
    Q_OBJECT
public:
    TileDefFile();

    bool read(const QString &fileName);
    bool write(const QString &fileName);

    QString directory() const;

    TileDefTileset *tileset(const QString &name);

    QStringList tilesetNames() const
    { return mTilesets.keys(); }

    QString errorString() const
    { return mError; }

private:
    QMap<QString,TileDefTileset*> mTilesets;
    QString mFileName;
    QString mError;
};

} // namespace Internal
} // namespace Tiled

#endif // TILEDEFFILE_H
