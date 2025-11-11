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

#ifndef SHORTCUTEDITORMODEL_H
#define SHORTCUTEDITORMODEL_H

#include <QAbstractItemModel>
#include <QList>
#include <QVariant>

QT_BEGIN_NAMESPACE
class QAction;
QT_END_NAMESPACE

class ActionManager;

enum class Column : uint8_t {
    Name,
    Shortcut
};

//! [0]
class ShortcutEditorModel : public QAbstractItemModel
{
    Q_OBJECT

    class ShortcutEditorModelItem
    {
    public:
        explicit ShortcutEditorModelItem(const QList<QVariant> &data,
                                         ShortcutEditorModelItem *parentItem = nullptr);
        ~ShortcutEditorModelItem();

        void appendChild(ShortcutEditorModelItem *child);

        ShortcutEditorModelItem *child(int row) const;
        int childCount() const;
        void deleteChildren();
        int columnCount() const;
        QVariant data(int column) const;
        int row() const;
        ShortcutEditorModelItem *parentItem() const;
        QAction *action() const;

    private:
        QList<ShortcutEditorModelItem *> m_childItems;
        QList<QVariant> m_itemData;
        ShortcutEditorModelItem *m_parentItem;
    };

public:
    explicit ShortcutEditorModel(ActionManager *actionManager, QObject *parent = nullptr);
    ~ShortcutEditorModel() override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &index = QModelIndex()) const override;
    int columnCount(const QModelIndex &index = QModelIndex()) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    void setActions();
    QAction *action(const QModelIndex &index) const;

private:
    void setupModelData(ShortcutEditorModelItem *parent);

    ActionManager *m_actionManager;
    ShortcutEditorModelItem *m_rootItem;
};
//! [0]

#endif
