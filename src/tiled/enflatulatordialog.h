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

#ifndef ENFLATULATORDIALOG_H
#define ENFLATULATORDIALOG_H

#include <QDialog>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPolygonF>

namespace Ui {
class EnflatulatorDialog;
}

namespace Tiled {
namespace Internal {

class EnflatulatorIsoScene;

class EnflatulatorFace
{
public:
    enum Orient {
        Flat,
        West,
        North,
        Roof
    };

    QString mLabel;
    Orient mOrient;
    QRect mRect;
    QImage mImage;
};

class EnflatulatorShape
{
public:
    QRect bounds() const
    {
        QRect r = mFaces.first()->mRect;
        foreach (EnflatulatorFace *face, mFaces)
            r |= face->mRect;
        return r;
    }

    QString mLabel;
    QList<EnflatulatorFace*> mFaces;
    QImage mImage;
};

class EnflatulatorShapeItem : public QGraphicsItem
{
public:
    EnflatulatorShapeItem(QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setShape(EnflatulatorShape *shape);

    EnflatulatorShape *mShape;
};

/* Shows all the faces in the current shape in 2D and
 * indicates the selected face. */
class EnflatulatorFaceScene : public QGraphicsScene
{
public:
    EnflatulatorFaceScene(QObject *parent = 0);

    void setShape(EnflatulatorShape *shape);

    EnflatulatorShapeItem *mShapeItem;
};

class EnflatulatorFaceView : public QGraphicsView
{
public:
    EnflatulatorFaceView(QWidget *parent = 0);

    EnflatulatorFaceScene *mScene;
};

/* One of these goes in EnflatulatorIsoScene for each face of the current shape. */
class EnflatulatorFaceItem : public QGraphicsItem
{
public:
    EnflatulatorFaceItem(EnflatulatorIsoScene *scene, EnflatulatorFace *face, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    QPointF toScene(qreal x, qreal y, qreal z);
    QPolygonF toScene(QRectF &r);
    QPointF toFace(const QPointF &p) { return toFace(p.x(), p.y()); }
    QPointF toFace(qreal x, qreal y);

    void setSelected(bool selected);

    int handleAt(const QPointF &scenePos);
    void moveHandle(int handle, const QPointF &scenePos);

    void initPoly();
    void enflatulate();

    EnflatulatorIsoScene *mScene;
    EnflatulatorFace *mFace;
    QPolygonF mPoly;
    bool mSelected;
};

class BaseEnflatulatorTool : public QObject
{
public:
    BaseEnflatulatorTool(EnflatulatorIsoScene *scene);

    virtual void activate() {}
    virtual void deactivate() {}

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) = 0;

    EnflatulatorIsoScene *mScene;
    QAction *mAction;
};

class EnflatulatorTool : public BaseEnflatulatorTool
{
    Q_OBJECT
public:
    EnflatulatorTool(EnflatulatorIsoScene *scene);

    void activate();
    void deactivate();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void setShape(EnflatulatorShape *shape);

    EnflatulatorFaceItem *faceAt(const QPointF &scenePos);

signals:
    void faceChanged();

public:
    EnflatulatorShape *mShape;
    QList<EnflatulatorFaceItem*> mFaceItems;
    EnflatulatorFaceItem *mSelectedFaceItem;

    enum Mode {
        NoMode,
        DragFace,
        DragHandle
    };
    Mode mMode;
    int mHandle;
    QPointF mLastScenePos;
};

class WestNorthTool : public BaseEnflatulatorTool
{
public:
    WestNorthTool(EnflatulatorIsoScene *scene);

    void activate();
    void deactivate();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    QGraphicsPixmapItem *mFlatTilesetPixmapItem;
    QImage mFlatTilesetImg;
};

/* Shows the iso image file, allows positioning and resizing the faces. */
class EnflatulatorIsoScene : public QGraphicsScene
{
    Q_OBJECT
public:
    EnflatulatorIsoScene(QObject *parent = 0);

    void setIsoImage(const QImage &image);

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void activateTool(BaseEnflatulatorTool *tool);

public:
    QImage mImage;
    QGraphicsPixmapItem *mImageItem;
    EnflatulatorShape *mShape;
    BaseEnflatulatorTool *mActiveTool;
};

class EnflatulatorIsoView : public QGraphicsView
{
public:
    EnflatulatorIsoView(QWidget *parent = 0);

    EnflatulatorIsoScene *mScene;
};

class EnflatulatorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EnflatulatorDialog(QWidget *parent = 0);
    ~EnflatulatorDialog();

private slots:
    void toolToggled(bool active);
    void changeTool();
    void chooseIsoImage();
    void currentShapeChanged(int row);
    void faceChanged();

private:
    Ui::EnflatulatorDialog *ui;
    QList<EnflatulatorShape*> mShapes;
    BaseEnflatulatorTool *mPendingTool;
    EnflatulatorTool *mShapeTool;
    WestNorthTool *mWNTool;
};

} // namespace Internal
} // namespace Tiled

#endif // ENFLATULATORDIALOG_H
