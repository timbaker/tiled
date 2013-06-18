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

#ifndef IMODE_H
#define IMODE_H

#include "singleton.h"

#include <QObject>
#include <QIcon>

class QAction;
class QSettings;

namespace Core {
namespace Internal {
class FancyTabWidget;
}
}

namespace BuildingEditor {

class IMode : public QObject
{
    Q_OBJECT
public:
    explicit IMode(QObject *parent = 0);
    
    void setWidget(QWidget *widget) { mWidget = widget; }
    QWidget *widget() const { return mWidget; }

    void setIcon(const QIcon &icon) { mIcon = icon; }
    QIcon icon() const { return mIcon; }

    void setDisplayName(const QString &displayName) { mDisplayName = displayName; }
    QString displayName() const { return mDisplayName; }

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

    void setActive(bool active);
    bool isActive() const { return mActive; }

    virtual void readSettings(QSettings &settings) = 0;
    virtual void writeSettings(QSettings &settings) = 0;

signals:
    void enabledStateChanged(bool enabled);
    void activeStateChanged(bool active);

protected:
    QWidget *mWidget;
    QIcon mIcon;
    QString mDisplayName;
    bool mEnabled;
    bool mActive;
};

class ModeManager : public QObject, public Singleton<ModeManager>
{
    Q_OBJECT
public:
    ModeManager(Core::Internal::FancyTabWidget *tabWidget, QObject *parent = 0);

    void addMode(IMode *mode);

    void setCurrentMode(IMode *mode);
    IMode *currentMode() const { return mCurrentMode; }

signals:
    void currentModeAboutToChange(IMode *mode);
    void currentModeChanged();

private slots:
    void currentTabAboutToChange(int index);
    void currentTabChanged(int index);
    void enabledStateChanged(bool enabled);
    void modeActionTriggered();

private:
    Core::Internal::FancyTabWidget *mTabWidget;
    QList<IMode*> mModes;
    IMode *mCurrentMode;
    QList<QAction*> mActions;
};

} // namespace BuildingEditor

#endif // IMODE_H
