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

#ifndef BMPTOOL_H
#define BMPTOOL_H

#include "abstracttiletool.h"

namespace Tiled {
namespace Internal {
class BmpToolDialog;

class BmpTool : public AbstractTileTool
{
    Q_OBJECT
public:
    static BmpTool *instance();

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);

    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void setColor(int index, QRgb color)
    { mBmpIndex = index; mColor = color; }

    void setBrushSize(int size);

protected:
    void mapDocumentChanged(MapDocument *oldDocument,
                            MapDocument *newDocument);

    void languageChanged();

protected:
    void tilePositionChanged(const QPoint &tilePos);

    void paint(bool mergeable);

private:
    Q_DISABLE_COPY(BmpTool)
    static BmpTool *mInstance;
    BmpTool(QObject *parent = 0);
    ~BmpTool();

    bool mPainting;
    bool mErasing;
    int mBmpIndex;
    QRgb mColor;
    int mBrushSize;
    BmpToolDialog *mDialog;
};

} // namespace Internal
} // namespace Tiled

#endif // BMPTOOL_H
