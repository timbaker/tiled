#ifndef ROOMDEFTOOL_H
#define ROOMDEFTOOL_H

#include "abstractobjecttool.h"

namespace Tiled {

class MapObject;

namespace Internal {

class MapObjectItem;

class RoomDefTool : public AbstractObjectTool
{
    Q_OBJECT
public:
    RoomDefTool(QObject *parent = 0);

    void mouseEntered();
    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void languageChanged();

private:
    void renameRoomDef(MapObject *object, const QString &newName);
    bool anyAdjacent(const QString &roomName,
                     const QList<MapObject*> object,
                     const QList<MapObject*> &others);
    QList<MapObject*> adjacentObjects(const QString &roomName, MapObject *object,
                                      const QList<MapObject*> &others);
    bool isAdjacent(MapObject *o1, MapObject *o2);
    QList<MapObject*> objectsNamed(const QString &name, const QList<MapObject*> &objects);

private:
    MapObjectItem *mObjectItem;
};

} // namespace Internal
} // namespace Tiled

#endif // ROOMDEFTOOL_H
