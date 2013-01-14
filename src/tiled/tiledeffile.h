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

    TileDefProperty *property(const QString &name)
    {
        if (mProperties.contains(name))
            return mProperties[name];
        return 0;
    }

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

        void ChangePropertiesV(const QVariant &value)
        {
            ChangeProperties(value.toBool());
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
            mValue = 0;
            if (contains(mShortName))
                mValue = mProperties[mShortName].toInt();
        }

        void ChangePropertiesV(const QVariant &value)
        {
            ChangeProperties(value.toInt());
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

        void ChangePropertiesV(const QVariant &value)
        {
            ChangeProperties(value.toString());
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

    class PropGenericEnum : public UIProperty
    {
    public:
        PropGenericEnum(const QString &name,
                        QMap<QString,QString> &properties,
                        const QStringList &enums,
                        int defaultValue = 0) :
            UIProperty(name, properties),
            mEnums(enums),
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
            return mEnums[mValue];
        }

        void setValue(const QVariant &value)
        {
            mValue = qBound(0, value.toInt(), mEnums.size() - 1);
        }

        void ChangePropertiesV(const QVariant &value)
        {
            ChangeProperties(value.toInt());
        }

        int getEnum()
        {
            return mValue;
        }

        QStringList mEnums;
        int mValue;
        int mDefaultValue;
    };

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

        PropWallStyle(const QString &name, QMap<QString,QString> &properties,
                      const QStringList &enums) :
            PropGenericEnum(name, properties, enums, None)
        {}

        void FromProperties()
        {
            mValue = None;

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
            mValue = static_cast<WallStyle>(value);
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

        PropRoofStyle(const QString &name, QMap<QString,QString> &properties,
                      const QStringList &enums) :
            PropGenericEnum(name, properties, enums, None)
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

        void ChangeProperties(int value)
        {
            mValue = qBound(0, value, mEnums.size() - 1);
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
                      QMap<QString,QString> &properties,
                      const QStringList &enums) :
            PropGenericEnum(name, properties, enums, None),
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
            mValue = qBound(0, value, mEnums.size() - 1);
        }

        bool contains(const char *key)
        {
            return UIProperty::contains(mPrefix + QLatin1String(key)); // doorW, doorFrW, windowW
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
                      QMap<QString,QString> &properties,
                      const QStringList &enums) :
            PropGenericEnum(name, properties, enums, None),
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

        void ChangeProperties(int value)
        {
            Q_UNUSED(value)
            Q_ASSERT(false); // unimplemented in original???
        }

        bool contains(const char *key)
        {
            return UIProperty::contains(mPrefix + QLatin1String(key)); // shelfW, tableW
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
                       QMap<QString,QString> &properties,
                       const QStringList &enums) :
            PropGenericEnum(name, properties, enums, None),
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
            mValue = qBound(0, value, mEnums.size() - 1);
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

        PropTileBlockStyle(const QString &name, QMap<QString,QString> &properties,
                           const QStringList &enums) :
            PropGenericEnum(name, properties, enums, None)
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
            mValue = qBound(0, value, mEnums.size() - 1);
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

    UIProperty *property(const QString &name) const
    {
        if (mProperties.contains(name))
            return mProperties[name];
        return 0;
    }

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

    void ChangePropertiesV(const QString &label, const QVariant &newValue)
    {
        Q_ASSERT(mProperties.contains(label));
        if (mProperties.contains(label))
            mProperties[label]->ChangePropertiesV(newValue);
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
        mPropertyUI(mProperties)
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

    QString fileName() const
    { return mFileName; }

    void setFileName(const QString &fileName)
    { mFileName = fileName; }

    bool read(const QString &fileName);
    bool write(const QString &fileName);

    QString directory() const;

    TileDefTileset *tileset(const QString &name);

    QList<TileDefTileset*> tilesets() const
    { return mTilesets.values(); }

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
