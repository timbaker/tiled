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

#include "shortcuteditorwidget.h"

#include "qsettings.h"
#include "shortcuteditordelegate.h"
#include "shortcuteditormodel.h"

#include <QAction>
#include <QHeaderView>
#include <QTreeView>
#include <QVBoxLayout>

//! [0]
ShortcutEditorWidget::ShortcutEditorWidget(ActionManager *actionManager, QWidget *parent)
    : QWidget(parent)
{
    m_model = new ShortcutEditorModel(actionManager, this);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &ShortcutEditorWidget::dataChanged);
    m_delegate = new ShortcutEditorDelegate(this);
    m_view = new QTreeView(this);
    m_view->setModel(m_model);
    m_view->setItemDelegateForColumn(static_cast<int>(Column::Shortcut), m_delegate);
    m_view->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    m_view->setAllColumnsShowFocus(true);
    m_view->header()->resizeSection(0, 250);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
    setLayout(layout);

    QAction *clearAction = new QAction(QStringLiteral("Clear Shortcut"), this);
    connect(clearAction, &QAction::triggered, this, &ShortcutEditorWidget::clearShortcut);
    clearAction->setShortcut(Qt::Key_Delete);
    addAction(clearAction);

    setModelData();
}

void ShortcutEditorWidget::setModelData()
{
    m_model->setActions();
    m_view->expandAll();
}

void ShortcutEditorWidget::saveSettings(QSettings &settings)
{
    QHeaderView *header = m_view->header();
    settings.setValue(QStringLiteral("column0"), header->sectionSize(0));
}

void ShortcutEditorWidget::readSettings(QSettings &settings)
{
    QHeaderView *header = m_view->header();
    bool ok;
    int size0 = settings.value(QStringLiteral("column0")).toInt(&ok);
    header->resizeSection(0, ok ? std::max(size0, header->minimumSectionSize()) : header->sectionSize(0));
}

void ShortcutEditorWidget::clearShortcut()
{
    QModelIndex index = m_view->currentIndex();
    if (!index.isValid()) {
        return;
    }
    if (QAction *action = m_model->action(index)) {
        index.siblingAtColumn(static_cast<int>(Column::Shortcut));
        m_model->setData(index, QString(), Qt::EditRole);
    }
}

void ShortcutEditorWidget::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(roles)
    if (topLeft == bottomRight && topLeft.column() == static_cast<int>(Column::Shortcut)) {
        if (QAction *action = m_model->action(topLeft)) {
            emit shortcutEdited(action);
        }
    }
}
//! [0]
