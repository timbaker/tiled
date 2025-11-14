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

#ifndef ACTIONMANAGER_H
#define ACTIONMANAGER_H

#include <QList>
#include <QObject>
#include <QMetaType>
#include <QString>

QT_BEGIN_NAMESPACE
class QAction;
QT_END_NAMESPACE

class ActionManager : public QObject
{
    Q_OBJECT
public:
    ActionManager(const QString &fileName, QObject *parent = nullptr);
    ~ActionManager() = default;

    QList<QAction*> registeredActions() const;

    void registerAction(QAction *action);
    void registerAction(QAction *action, const QString &context, const QString &category);
    void registerAction(QAction *action, const QString &context, const QString &category, const QString &fileID);
    void registerAction(QAction *action, const QString &context, const QString &category, const QString &fileID, const QString &label);
    QAction *registerAction(const QString &name, const QString &shortcut, const QString &context, const QString &category);

    QAction *findAction(const QString &text);

    QString contextForAction(QAction *action);
    QString categoryForAction(QAction *action);
    QString fileIDForAction(QAction *action);
    QString labelForAction(QAction *action);

    bool save(QString &error);
    bool load(QString &error);
    void emitShortcutEditedForAllActions();

signals:
    void shortcutEdited(QAction *action);

protected:
    QString actionFileID(const QAction *action);
    QString actionLabel(const QAction *action);

private:
    QString mFileName;
    QList<QAction *> m_actions;
};

struct ActionIdentifier {
    QString author;
    QString context;
    QString category;
    QString fileID;
    QString label;
};

Q_DECLARE_METATYPE(ActionIdentifier)

#endif
