/*
 * Copyright 2014, Tim Baker <treectrl@users.sf.net>
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

#ifndef CREATEPACKDIALOG_H
#define CREATEPACKDIALOG_H

#include "texturepacker.h"

#include <QCoreApplication>
#include <QDialog>

namespace Ui {
class CreatePackDialog;
}

class PackSettingsFile
{
    Q_DECLARE_TR_FUNCTIONS(PackSettingsFile)
public:
    PackSettingsFile();

    void setSettings(const TexturePackSettings &settings)
    { mSettings = settings; }

    TexturePackSettings settings() const { return mSettings; }

    bool read(const QString &fileName);
    bool write(const QString &fileName);

    QString errorString() const { return mError; }

private:
    bool stringToSize(const QString &s, QSize &result);

private:
    QString mError;
    TexturePackSettings mSettings;
};

class CreatePackDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreatePackDialog(QWidget *parent = 0);
    ~CreatePackDialog();

private:
    void settingsToUI(const TexturePackSettings &settings);
    void settingsFromUI(TexturePackSettings &settings);
    void readSettings();
    void writeSettings();

private slots:
    void savePackAs();
    void addDirectory();
    void removeDirectory();
    void syncUI();
    void tileSizeChangedX(int value);
    void tileSizeChangedY(int value);

    void loadSettings();
    void saveSettings();
    void saveSettingsAs();

    void accept();

private:
    Ui::CreatePackDialog *ui;
    QString mSettingsFileName;
};

#endif // CREATEPACKDIALOG_H
