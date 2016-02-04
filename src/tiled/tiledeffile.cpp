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

#include "tiledeffile.h"

#include "tilesetmanager.h"
#include "virtualtileset.h"

#include "tileset.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QImageReader>

using namespace Tiled;
using namespace Tiled::Internal;

TileDefFile::TileDefFile()
{
}

TileDefFile::~TileDefFile()
{
    qDeleteAll(mTilesets);
}

static QString ReadString(QDataStream &in)
{
    QString str;
    quint8 c = ' ';
    while (c != '\n') {
        in >> c;
        if (c != '\n')
            str += QLatin1Char(c);
    }
    return str;
}

#define VERSION0 0
#define VERSION1 1
#define VERSION_LATEST VERSION1

bool TileDefFile::read(const QString &fileName)
{
    qDeleteAll(mTilesets);
    mTilesets.clear();
    mTilesetByName.clear();

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        mError = tr("Error opening file for reading.\n%1").arg(fileName);
        return false;
    }

    QDir dir = QFileInfo(fileName).absoluteDir();

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    quint8 tdef[4] = {0};
    in >> tdef[0];
    in >> tdef[1];
    in >> tdef[2];
    in >> tdef[3];
    int version = VERSION0;
    if (memcmp(tdef, "tdef", 4) == 0) {
        in >> version;
        if (version < 0 || version > VERSION_LATEST) {
            mError = tr("Unknown version number %1 in .tiles file.\n%2")
                    .arg(version).arg(fileName);
            return false;
        }
    } else
        file.seek(0);

    int numTilesets;
    in >> numTilesets;
    for (int i = 0; i < numTilesets; i++) {
        TileDefTileset *ts = new TileDefTileset;
        ts->mName = ReadString(in);
        ts->mImageSource = ReadString(in); // no path, just file + extension
        qint32 columns, rows;
        in >> columns;
        in >> rows;

        qint32 id = i + 1;
        if (version > VERSION0)
            in >> id;

        qint32 tileCount;
        in >> tileCount;

        ts->mColumns = columns;
        ts->mRows = rows;
        ts->mID = id;

        QVector<TileDefTile*> tiles(columns * rows);
        for (int j = 0; j < tileCount; j++) {
            TileDefTile *tile = new TileDefTile(ts, j);
            qint32 numProperties;
            in >> numProperties;
            QMap<QString,QString> properties;
            for (int k = 0; k < numProperties; k++) {
                QString propertyName = ReadString(in);
                QString propertyValue = ReadString(in);
                properties[propertyName] = propertyValue;
            }
            TilePropertyMgr::instance()->modify(properties);
            tile->mPropertyUI.FromProperties(properties);
            tile->mProperties = properties;
            tiles[j] = tile;
        }
        ts->mTiles = tiles;

        // Deal with the image being a different size now than it was when the
        // .tiles file was saved.
        VirtualTileset *vts = TilesetManager::instance()->useVirtualTilesets()
                ? VirtualTilesetMgr::instance().tilesetFromPath(ts->mImageSource)
                : 0;
        if (vts) {
            ts->resize(vts->columnCount(), vts->rowCount());
        } else {
            QImageReader bmp(dir.filePath(ts->mImageSource));
            if (bmp.size().isValid()) {
                int columns = bmp.size().width() / 64;
                int rows = bmp.size().height() / 128;
                ts->resize(columns, rows);
            }
        }

        insertTileset(mTilesets.size(), ts);
    }

    mFileName = fileName;

    return true;
}

static void SaveString(QDataStream& out, const QString& str)
{
    for (int i = 0; i < str.length(); i++)
        out << quint8(str[i].toLatin1());
    out << quint8('\n');
}

bool TileDefFile::write(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        mError = tr("Error opening file for writing.\n%1").arg(fileName);
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    out << quint8('t') << quint8('d') << quint8('e') << quint8('f');
    out << qint32(VERSION_LATEST);

    out << qint32(mTilesets.size());
    foreach (TileDefTileset *ts, mTilesets) {
        SaveString(out, ts->mName);
        SaveString(out, ts->mImageSource); // no path, just file + extension
        out << qint32(ts->mColumns);
        out << qint32(ts->mRows);
        out << qint32(ts->mID);
        out << qint32(ts->mTiles.size());
        foreach (TileDefTile *tile, ts->mTiles) {
            QMap<QString,QString> &properties = tile->mProperties;
            tile->mPropertyUI.ToProperties(properties);
            out << qint32(properties.size());
            foreach (QString key, properties.keys()) {
                SaveString(out, key);
                SaveString(out, properties[key]);
            }
        }
    }

    return true;
}

QString TileDefFile::directory() const
{
    return QFileInfo(mFileName).absolutePath();
}

void TileDefFile::insertTileset(int index, TileDefTileset *ts)
{
    Q_ASSERT(!mTilesets.contains(ts));
    Q_ASSERT(!mTilesetByName.contains(ts->mName));
    mTilesets.insert(index, ts);
    mTilesetByName[ts->mName] = ts;
}

TileDefTileset *TileDefFile::removeTileset(int index)
{
    mTilesetByName.remove(mTilesets[index]->mName);
    return mTilesets.takeAt(index);
}

TileDefTileset *TileDefFile::tileset(const QString &name)
{
    if (mTilesetByName.contains(name))
        return mTilesetByName[name];
    return 0;
}

/////

TileDefProperties::TileDefProperties()
{
}

TileDefProperties::~TileDefProperties()
{
    qDeleteAll(mProperties);
}

void TileDefProperties::addBoolean(const QString &name, const QString &shortName,
                                   bool defaultValue, bool reverseLogic)
{
    TileDefProperty *prop = new BooleanTileDefProperty(name, shortName,
                                                       defaultValue,
                                                       reverseLogic);
    mProperties += prop;
    mPropertyByName[name] = prop;
}

void TileDefProperties::addInteger(const QString &name, const QString &shortName,
                                   int min, int max, int defaultValue)
{
    TileDefProperty *prop = new IntegerTileDefProperty(name, shortName,
                                                       min, max, defaultValue);
    mProperties += prop;
    mPropertyByName[name] = prop;
}

void TileDefProperties::addString(const QString &name, const QString &shortName,
                                  const QString &defaultValue)
{
    TileDefProperty *prop = new StringTileDefProperty(name, shortName,
                                                      defaultValue);
    mProperties += prop;
    mPropertyByName[name] = prop;
}

void TileDefProperties::addEnum(const QString &name, const QString &shortName,
                                const QStringList enums,
                                const QStringList &shortEnums,
                                const QString &defaultValue,
                                bool valueAsPropertyName,
                                const QString &extraPropertyIfSet)
{
    TileDefProperty *prop = new EnumTileDefProperty(name, shortName, enums,
                                                    shortEnums, defaultValue,
                                                    valueAsPropertyName,
                                                    extraPropertyIfSet);
    mProperties += prop;
    mPropertyByName[name] = prop;
}

/////

UIProperties::UIProperties()
{
    const TileDefProperties &props = TilePropertyMgr::instance()->properties();
    foreach (TileDefProperty *prop, props.mProperties) {
        if (BooleanTileDefProperty *p = prop->asBoolean()) {
            mProperties[p->mName] = new PropGenericBoolean(prop->mName,
                                                           p->mShortName,
                                                           p->mDefault,
                                                           p->mReverseLogic);
            continue;
        }
        if (IntegerTileDefProperty *p = prop->asInteger()) {
            mProperties[p->mName] = new PropGenericInteger(prop->mName,
                                                           p->mShortName,
                                                           p->mMin,
                                                           p->mMax,
                                                           p->mDefault);
            continue;
        }
        if (StringTileDefProperty *p = prop->asString()) {
            mProperties[p->mName] = new PropGenericString(prop->mName,
                                                          p->mShortName,
                                                          p->mDefault);
            continue;
        }
        if (EnumTileDefProperty *p = prop->asEnum()) {
            mProperties[p->mName] = new PropGenericEnum(prop->mName,
                                                        p->mShortName,
                                                        p->mEnums,
                                                        p->mShortEnums,
                                                        p->mDefault,
                                                        p->mValueAsPropertyName,
                                                        p->mExtraPropertyIfSet);
            continue;
        }
    }
}

UIProperties::~UIProperties()
{
    qDeleteAll(mProperties);
}

/////

TileDefTileset::TileDefTileset(Tileset *ts) :
    mID(0)
{
    mName = ts->name();
    mImageSource = QFileInfo(ts->imageSource()).fileName();
    mColumns = ts->columnCount();
    mRows = ts->tileCount() / mColumns;
    mTiles.resize(ts->tileCount());
    for (int i = 0; i < mTiles.size(); i++)
        mTiles[i] = new TileDefTile(this, i);
}

TileDefTileset::TileDefTileset() :
    mID(0)
{
}

TileDefTileset::~TileDefTileset()
{
    qDeleteAll(mTiles);
}

TileDefTile *TileDefTileset::tileAt(int index)
{
    if (index >= 0 && index < mTiles.size())
        return mTiles.at(index);
    return 0;
}

void TileDefTileset::resize(int columns, int rows)
{
    if (columns == mColumns && rows == mRows)
        return;

    int oldColumns = mColumns;
    int oldRows = mRows;
    QVector<TileDefTile*> oldTiles = mTiles;

    mColumns = columns;
    mRows = rows;
    mTiles.resize(mColumns * mRows);
    for (int y = 0; y < qMin(mRows, oldRows); y++) {
        for (int x = 0; x < qMin(mColumns, oldColumns); x++) {
            mTiles[x + y * mColumns] = oldTiles[x + y * oldColumns];
            oldTiles[x + y * oldColumns] = 0;
        }
    }
    for (int i = 0; i < mTiles.size(); i++) {
        if (!mTiles[i]) {
            mTiles[i] = new TileDefTile(this, i);
        }
    }
    qDeleteAll(oldTiles);
}

/////

#include "preferences.h"
#include "BuildingEditor/simplefile.h"

#include <QCoreApplication>

/////

class Tiled::Internal::TilePropertyModifier
{
public:
    class Command
    {
    public:
        enum Type
        {
            Match,
            Reject,
            Remove,
            Rename,
            Replace,
            Set
        };

        Type mType;
        QString mKey;
        QString mValue;
        QStringList mParams;
        bool mHasValue;
    };

    void modify(QMap<QString,QString> &properties);

    QList<Command> mCommands;
};


void TilePropertyModifier::modify(QMap<QString, QString> &properties)
{
    foreach (Command cmd, mCommands) {
        switch (cmd.mType) {
        case Command::Match: {
            if (!properties.contains(cmd.mKey))
                return;
            if (cmd.mHasValue && (properties[cmd.mKey] != cmd.mValue))
                return;
            break;
        }
        case Command::Reject: {
            if (properties.contains(cmd.mKey)) {
                if (cmd.mHasValue) {
                    if (properties[cmd.mKey] == cmd.mValue)
                        return; // reject key == specific value
                } else
                    return; // reject key == any value
            }
            break;
        }
        case Command::Remove: {
            if (properties.contains(cmd.mKey)) {
                if (cmd.mHasValue) {
                    if (properties[cmd.mKey] != cmd.mValue)
                        return; // require key == specific value
                }
                qDebug() << "Command::Remove" << cmd.mKey << "=" << properties[cmd.mKey];
                properties.remove(cmd.mKey);
            }
            break;
        }
        case Command::Rename: {
            if (properties.contains(cmd.mKey)) {
                QString value = properties[cmd.mKey];
                qDebug() << "Command::Rename" << cmd.mKey << "=" << value << "==>" << cmd.mValue << "=" << value;
                properties.remove(cmd.mKey);
                properties[cmd.mValue] = value; // cmd.mValue is the new key name
            }
            break;
        }
        case Command::Replace: {
            QString key1 = cmd.mParams[0], value1 = cmd.mParams[1];
            QString key2 = cmd.mParams[2], value2 = cmd.mParams[3];
            if (properties.contains(key1) && (properties[key1] == value1)) {
                qDebug() << "Command::Replace" << key1 << "=" << value1 << "==>" << key2 << "=" << value2;
                properties.remove(key1);
                properties[key2] = value2;
            }
            break;
        }
        case Command::Set: {
            qDebug() << "Command::Set" << cmd.mKey << "=" << cmd.mValue;
            properties[cmd.mKey] = cmd.mValue;
            break;
        }
        }
    }
}


/////

TilePropertyMgr *TilePropertyMgr::mInstance = 0;

TilePropertyMgr *TilePropertyMgr::instance()
{
    if (!mInstance)
        mInstance = new TilePropertyMgr;
    return mInstance;
}

void TilePropertyMgr::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

QString TilePropertyMgr::txtName()
{
    return QLatin1String("TileProperties.txt");
}

QString TilePropertyMgr::txtPath()
{
    return Preferences::instance()->configPath(txtName());
}

bool TilePropertyMgr::readTxt()
{
    QFileInfo info(txtPath());

    // Create ~/.TileZed if needed.
    QString configPath = Preferences::instance()->configPath();
    QDir dir(configPath);
    if (!dir.exists()) {
        if (!dir.mkpath(configPath)) {
            mError = tr("Failed to create config directory:\n%1")
                    .arg(QDir::toNativeSeparators(configPath));
            return false;
        }
    }

    // Copy TileProperties.txt from the application directory to the ~/.TileZed
    // directory if needed.
    if (!info.exists()) {
        QString source = Preferences::instance()->appConfigPath(txtName());
        if (QFileInfo(source).exists()) {
            if (!QFile::copy(source, txtPath())) {
                mError = tr("Failed to copy file:\nFrom: %1\nTo: %2")
                        .arg(source).arg(txtPath());
                return false;
            }
        }
    }

    info.refresh();
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.").arg(path);
        return false;
    }

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("modify")) {
            if (!addModifier(block))
                return false;
            continue;
        }
        if (block.name == QLatin1String("property")) {
            if (!addProperty(block))
                return false;
            continue;
        }
        if (block.name == QLatin1String("separator")) {
            mProperties.addSeparator();
            continue;
        }
        mError = tr("Unknown block name '%1'\n%2")
                .arg(block.name)
                .arg(path);
        return false;
    }

    return true;
}

void TilePropertyMgr::modify(QMap<QString, QString> &properties)
{
    foreach (TilePropertyModifier *mod, mModifiers)
        mod->modify(properties);
}

bool TilePropertyMgr::addProperty(SimpleFileBlock &block)
{
    QString Type = block.value("Type");
    QString Name = block.value("Name");
    QString ShortName = block.value("ShortName");

    if (Name.isEmpty()) {
        mError = tr("Empty or missing Name value.\n\n%2").arg(block.toString());
        return false;
    }
    if (mProperties.property(Name) != 0) {
        mError = tr("Duplicate property name '%1'").arg(Name);
        return false;
    }

    if (ShortName.isEmpty())
        ShortName = Name;

    bool ok;
    if (Type == QLatin1String("Boolean")) {
        bool Default = toBoolean("Default", block, ok);
        if (!ok) return false;
        bool ReverseLogic = toBoolean("ReverseLogic", block, ok);
        if (!ok) return false;
        mProperties.addBoolean(Name, ShortName, Default, ReverseLogic);
        return true;
    }

    if (Type == QLatin1String("Integer")) {
        int Min = toInt("Min", block, ok);
        if (!ok) return false;
        int Max = toInt("Max", block, ok);
        if (!ok) return false;
        int Default = toInt("Default", block, ok);
        if (!ok) return false;
        if (Min >= Max || Default < Min || Default > Max) {
            mError = tr("Weird integer values: Min=%1 Max=%2 Default=%3.\n\n%4")
                    .arg(Min).arg(Max).arg(Default).arg(block.toString());
            return false;
        }
        mProperties.addInteger(Name, ShortName, Min, Max, Default);
        return true;
    }

    if (Type == QLatin1String("String")) {
        QString Default = block.value("Default");
        mProperties.addString(Name, ShortName, Default);
        return true;
    }

    if (Type == QLatin1String("Enum")) {
        if (block.findBlock(QLatin1String("Enums")) < 0) {
            mError = tr("Enum property '%1' is missing an Enums block.\n\n%2")
                    .arg(Name).arg(block.toString());
            return false;
        }
        QStringList enums, shortEnums;
        SimpleFileBlock enumsBlock = block.block("Enums");
        foreach (SimpleFileKeyValue kv, enumsBlock.values) {
            enums += kv.name;
            shortEnums += kv.value.length() ? kv.value : kv.name;
        }
        QString Default = block.value("Default");
        if (!enums.contains(Default)) {
            mError = tr("Enum property '%1' Default=%2 missing from Enums block.\n\n%3")
                    .arg(Name).arg(Default).arg(block.toString());
            return false;
        }
        bool ValueAsPropertyName = toBoolean("ValueAsPropertyName", block, ok);
        if (!ok) return false;
        QString ExtraPropertyIfSet = block.value("ExtraPropertyIfSet");
        mProperties.addEnum(Name, ShortName, enums, shortEnums, Default,
                            ValueAsPropertyName, ExtraPropertyIfSet);
        return true;
    }

    mError = tr("Unknown property Type '%1'.\n\n%2").arg(Type).arg(block.toString());
    return false;
}

bool TilePropertyMgr::toBoolean(const char *key, SimpleFileBlock &block, bool &ok)
{
    SimpleFileKeyValue kv;
    if (!block.keyValue(key, kv)) {
        mError = tr("Missing '%1' keyvalue.\n\n%2")
                .arg(QLatin1String(key))
                .arg(block.toString());
        ok = false;
        return false;
    }
    if (kv.value == QLatin1String("true")) {
        ok = true;
        return true;
    }
    if (kv.value == QLatin1String("false")) {
        ok = true;
        return false;
    }
    mError = tr("Expected boolean but got '%1 = %2'.\n\n%3")
            .arg(kv.name).arg(kv.value).arg(block.toString());
    ok = false;
    return false;
}

int TilePropertyMgr::toInt(const char *key, SimpleFileBlock &block, bool &ok)
{
    SimpleFileKeyValue kv;
    if (!block.keyValue(key, kv)) {
        mError = tr("Missing '%1' keyvalue.\n\n%2")
                .arg(QLatin1String(key))
                .arg(block.toString());
        ok = false;
        return false;
    }
    int i = kv.value.toInt(&ok);
    if (ok) return i;
    mError = tr("Expected integer but got '%1 = %2'.\n\n%3")
            .arg(kv.name).arg(kv.value).arg(block.toString());
    ok = false;
    return 0;
}

// Parse a sequence of possibly-quoted words into a list of words.
static QStringList parseModifierParams(const QString &s)
{
    QLatin1Char qt('\"'), sp(' ');
    QStringList ret;
    bool inQuote = false;
    int wordStart = 0;
    for (int i = 0; i < s.length(); i++) {
        if (s[i] == qt) {
            if (inQuote)
                ret += s.mid(wordStart, i - wordStart); // add word in quotes
            else if (wordStart < i)
                ret += s.mid(wordStart, i - wordStart); // add word before quote
            wordStart = i + 1; // first char after quote
            inQuote = !inQuote;
        } else if (inQuote) {
        } else {
            if (s[i] == sp) {
                if (wordStart < i)
                    ret += s.mid(wordStart, i - wordStart); // add word before space
                wordStart = i + 1; // first char after space
            }
        }
    }
    if (wordStart < s.length())
        ret += s.mid(wordStart);
    return ret;
}

bool TilePropertyMgr::addModifier(SimpleFileBlock &block)
{
    TilePropertyModifier *mod = new TilePropertyModifier;
    foreach (SimpleFileKeyValue kv, block.values) {
        QStringList values = parseModifierParams(kv.value);
        TilePropertyModifier::Command cmd;
        cmd.mKey = values[0];
        cmd.mHasValue = false;
        if (values.size() >= 2) {
            cmd.mHasValue = true;
            cmd.mValue = values[1];
        }
        cmd.mParams = values;
        if (values.size() == 0) {
bogus:
            mError = tr("bad modifier block\n\n%1").arg(block.toString());
            delete mod;
            return false;
        }
        if (kv.name == QLatin1String("match")) {
            if (values.size() > 2) goto bogus;
            cmd.mType = TilePropertyModifier::Command::Match;
            mod->mCommands += cmd;
            continue;
        }
        if (kv.name == QLatin1String("reject")) {
            if (values.size() > 2) goto bogus;
            cmd.mType = TilePropertyModifier::Command::Reject;
            mod->mCommands += cmd;
            continue;
        }
        if (kv.name == QLatin1String("remove")) {
            if (values.size() > 2) goto bogus;
            cmd.mType = TilePropertyModifier::Command::Remove;
            mod->mCommands += cmd;
            continue;
        }
        if (kv.name == QLatin1String("rename")) {
            if (values.size() != 2) goto bogus;
            cmd.mType = TilePropertyModifier::Command::Rename;
            mod->mCommands += cmd;
            continue;
        }
        if (kv.name == QLatin1String("replace")) {
            if (values.size() != 4) goto bogus;
            cmd.mType = TilePropertyModifier::Command::Replace;
            mod->mCommands += cmd;
            continue;
        }
        if (kv.name == QLatin1String("set")) {
            cmd.mType = TilePropertyModifier::Command::Set;
            mod->mCommands += cmd;
            continue;
        }
        goto bogus;
    }
    mModifiers += mod;

    return true;
}

TilePropertyMgr::TilePropertyMgr()
{
}

TilePropertyMgr::~TilePropertyMgr()
{
    qDeleteAll(mModifiers);
    mInstance = 0;
}
