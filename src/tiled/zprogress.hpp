/*
 * zprogress.hpp
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

#ifndef ZPROGRESSMANAGER_H
#define ZPROGRESSMANAGER_H

#include "tiled_global.h"

#include <QDialog>

class QLabel;

class ZProgressManager : public QObject
{
    Q_OBJECT

public:
    static ZProgressManager *instance();

	void begin(const QString &text);
	void update(const QString &text);
	void end();

    void setMainWindow(QWidget *parent);

private:
    Q_DISABLE_COPY(ZProgressManager)

    ZProgressManager();

    QWidget *mMainWindow;
	QDialog *mDialog;
	QLabel *mLabel;
	int mDepth;
	static ZProgressManager *mInstance;
};

#endif /* ZPROGRESSMANAGER_H */
