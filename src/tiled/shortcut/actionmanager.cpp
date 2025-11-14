// Copyright (C) 2022 Laszlo Papp <lpapp@kde.org>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     1. Redistributions of source code must retain the above copyright notice,
//        this list of conditions and the following disclaimer.
//     2. Redistributions in binary form must reproduce the above copyright
//        notice, this list of conditions and the following disclaimer in the
//        documentation and/or other materials provided with the distribution.
//     3. Neither the name of the copyright holder nor the names of its
//        contributors may be used to endorse or promote products derived from this
//        software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "actionmanager.h"

#include "keyboardshortcutfile.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QVariant>

static const char *kDefaultShortcutPropertyName = "defaultShortcuts";
static const char *kIdPropertyName = "id";
static const char *kAuthorName = "qt";

ActionManager::ActionManager(const QString &fileName, QObject *parent)
    : QObject(parent)
    , mFileName(fileName)
{

}

QList<QAction *> ActionManager::registeredActions() const
{
    return m_actions;
}

void ActionManager::registerAction(QAction *action)
{
    action->setProperty(kDefaultShortcutPropertyName, QVariant::fromValue(action->shortcut()));
    m_actions.append(action);
}

void ActionManager::registerAction(QAction *action, const QString &context, const QString &category)
{
    registerAction(action, context, category, actionFileID(action));
}

void ActionManager::registerAction(QAction *action, const QString &context, const QString &category, const QString &fileID)
{
    registerAction(action, context, category, actionFileID(action), actionLabel(action));
}

void ActionManager::registerAction(QAction *action, const QString &context, const QString &category, const QString &fileID, const QString &label)
{
    ActionIdentifier ident{ QLatin1String(kAuthorName), context, category, fileID, label };
    action->setProperty(kIdPropertyName, QVariant::fromValue(ident));
    registerAction(action);
}

QAction *ActionManager::registerAction(const QString &name, const QString &shortcut, const QString &context, const QString &category)
{
    QAction *action = new QAction(name, qApp);
    action->setShortcut(QKeySequence(shortcut));
    registerAction(action, context, category);
    return action;
}

QAction *ActionManager::findAction(const QString &fileID)
{
    for (QAction *action : m_actions) {
        if (fileIDForAction(action) == fileID) {
            return action;
        }
    }
    return nullptr;
}

QString ActionManager::contextForAction(QAction *action)
{
    return action->property(kIdPropertyName).value<ActionIdentifier>().context;
}

QString ActionManager::categoryForAction(QAction *action)
{
    return action->property(kIdPropertyName).value<ActionIdentifier>().category;
}

QString ActionManager::fileIDForAction(QAction *action)
{
    return action->property(kIdPropertyName).value<ActionIdentifier>().fileID;
}

QString ActionManager::labelForAction(QAction *action)
{
    return action->property(kIdPropertyName).value<ActionIdentifier>().label;
}

bool ActionManager::save(QString &error)
{
    KeyboardShortcutFile file;
    QList<KeyboardShortcut> shortcuts;
    for (QAction *action : m_actions) {
        KeyboardShortcut shortcut;
        shortcut.id = fileIDForAction(action);
        shortcut.sequence = action->shortcut();
        shortcuts += shortcut;
    }
    error.clear();
    QFileInfo fileInfo(mFileName);
    QDir dir;
    dir.mkpath(fileInfo.absolutePath());
    if (!file.write(mFileName, shortcuts)) {
        error = file.errorString();
        return false;
    }
    return true;
}

bool ActionManager::load(QString &error)
{
    KeyboardShortcutFile file;
    if (!file.read(mFileName)) {
        error = file.errorString();
        return false;
    }
    for (const KeyboardShortcut &shortcut : file.shortcuts()) {
        if (QAction *action = findAction(shortcut.id)) {
            action->setShortcut(shortcut.sequence);
        }
    }
    return true;
}

QString ActionManager::actionFileID(const QAction *action)
{
    return action->text();
}

QString ActionManager::actionLabel(const QAction *action)
{
    return action->text().replace(QStringLiteral("&"), QString()).replace(QStringLiteral("..."), QString());
}

void ActionManager::emitShortcutEditedForAllActions()
{
    for (QAction *action : m_actions) {
        emit shortcutEdited(action);
    }
}
