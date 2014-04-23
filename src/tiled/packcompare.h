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

#ifndef PACKCOMPARE_H
#define PACKCOMPARE_H

#include "texturepackfile.h"
#include <QMainWindow>

namespace Ui {
class PackCompare;
}

class PackCompare : public QMainWindow
{
    Q_OBJECT

public:
    explicit PackCompare(QWidget *parent = 0);
    ~PackCompare();

private slots:
    void browse1();
    void browse2();
    void compare();

private:
    Ui::PackCompare *ui;
    PackFile mPackFile1;
    PackFile mPackFile2;
};

#endif // PACKCOMPARE_H
