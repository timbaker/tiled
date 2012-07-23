/*
 * zprogress.cpp
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

#include "zprogress.h"

#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>

ZProgressManager *ZProgressManager::mInstance = 0;

ZProgressManager *ZProgressManager::instance()
{
    if (!mInstance)
        mInstance = new ZProgressManager();
    return mInstance;
}

ZProgressManager::ZProgressManager()
    : mMainWindow(0)
    , mDialog(0)
    , mDepth(0)
{
    mInstance = this;
}


void ZProgressManager::setMainWindow(QWidget *mainWindow)
{
    if (mMainWindow)
        return;
    mMainWindow = mainWindow;
    mDialog = new QDialog(mainWindow);
    QVBoxLayout *layout = new QVBoxLayout();
    mLabel = new QLabel();
    mLabel->setMinimumSize(300, 20);
    layout->addWidget(mLabel);
    mDialog->setWindowModality(Qt::WindowModal);
    mDialog->setLayout(layout);
    mDialog->setWindowFlags(Qt::CustomizeWindowHint | Qt::Dialog);
}

void ZProgressManager::begin(const QString &text)
{
    mLabel->setText(text);
    if (mDepth++ == 0)
        mDialog->show();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ZProgressManager::update(const QString &text)
{
    Q_ASSERT(mDepth > 0);
    mLabel->setText(text);
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ZProgressManager::end()
{
    Q_ASSERT(mDepth > 0);
//    mDialog->setValue(mDialog->maximum()); // hides dialog!
    if (--mDepth == 0)
        mDialog->hide();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

