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

#include "shortcuteditordelegate.h"

#include <QAbstractItemModel>
#include <QKeySequenceEdit>

//! [0]
ShortcutEditorDelegate::ShortcutEditorDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}
//! [0]

//! [1]
QWidget *ShortcutEditorDelegate::createEditor(QWidget *parent,
                                              const QStyleOptionViewItem &/*option*/,
                                              const QModelIndex &/*index*/) const
{
    QKeySequenceEdit *editor = new QKeySequenceEdit(parent);
    connect(editor, &QKeySequenceEdit::editingFinished, this, &ShortcutEditorDelegate::commitAndCloseEditor);
    return editor;
}
//! [1]

//! [2]
void ShortcutEditorDelegate::commitAndCloseEditor()
{
    QKeySequenceEdit *editor = static_cast<QKeySequenceEdit *>(sender());
    Q_EMIT commitData(editor);
    Q_EMIT closeEditor(editor);
}
//! [2]

//! [3]
void ShortcutEditorDelegate::setEditorData(QWidget *editor,
                                           const QModelIndex &index) const
{
    if (!editor || !index.isValid())
        return;

    QString value = index.model()->data(index, Qt::EditRole).toString();

    QKeySequenceEdit *keySequenceEdit = static_cast<QKeySequenceEdit *>(editor);
    keySequenceEdit->setKeySequence(value);
}
//! [3]

//! [4]
void ShortcutEditorDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                          const QModelIndex &index) const
{
    if (!editor || !model || !index.isValid())
        return;

    const QKeySequenceEdit *keySequenceEdit = static_cast<QKeySequenceEdit *>(editor);
    const QKeySequence keySequence = keySequenceEdit->keySequence();
    QString keySequenceString = keySequence.toString(QKeySequence::NativeText);
    model->setData(index, keySequenceString);
}
//! [4]

//! [5]
void ShortcutEditorDelegate::updateEditorGeometry(QWidget *editor,
                                                  const QStyleOptionViewItem &option,
                                                  const QModelIndex &/*index*/) const
{
    editor->setGeometry(option.rect);
}
//! [5]
