#include "roomdeftool.h"

#include "mainwindow.h"
#include "mapobjectitem.h"
#include "mapscene.h"
#include "changemapobject.h"
#include "roomdefnamedialog.h"

#include "mapobject.h"
#include "objectgroup.h"

#include <QUndoStack>

using namespace Tiled;
using namespace Tiled::Internal;

RoomDefTool::RoomDefTool(QObject *parent) :
    AbstractObjectTool(QString(),
                       QIcon(QLatin1String(":images/22x22/roomdef-tool.png")),
                       QKeySequence(), parent),
    mObjectItem(0)
{
    languageChanged();
}

void RoomDefTool::mouseEntered()
{
}

void RoomDefTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    MapObjectItem *item = topMostObjectItemAt(pos);
    if (item != mObjectItem) {
        if (mObjectItem) {
        }
        mObjectItem = item;
        if (mObjectItem) {

        }
    }
}

void RoomDefTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (!topMostObjectItemAt(event->scenePos()))
            mapDocument()->setSelectedObjects(QList<MapObject*>());
    }
    if (event->button() == Qt::RightButton) {
        AbstractObjectTool::mousePressed(event);
    }
}

void RoomDefTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if ((mObjectItem = topMostObjectItemAt(event->scenePos()))) {
        if (event->button() == Qt::LeftButton) {
            if (!currentObjectGroup()->name().endsWith(QLatin1String("RoomDefs")))
                return;

            QList<MapObject*> selected = adjacentObjects(mObjectItem->mapObject()->name(),
                                                         mObjectItem->mapObject(),
                                                         currentObjectGroup()->objects());
            mapDocument()->setSelectedObjects(selected);

            RoomDefNameDialog dialog(mapDocument()->map()->objectGroups(),
                                     mObjectItem->mapObject()->name(),
                                     MainWindow::instance());
            if (dialog.exec() != QDialog::Accepted)
                return;

            renameRoomDef(mObjectItem->mapObject(), dialog.name());
        }
        if (event->button() == Qt::RightButton) {

        }
    }
}

void RoomDefTool::languageChanged()
{
    setName(tr("Rename RoomDefs"));
}

void RoomDefTool::renameRoomDef(MapObject *object, const QString &newName)
{
    if (newName.isEmpty())
        return;

    const QChar hash = QLatin1Char('#');
    QString roomName = newName;

    QList<MapObject*> objects;
    objects += object;
    if (object->name().contains(hash)) {
        roomName += hash;
        QList<MapObject*> others = currentObjectGroup()->objects();
        objects = adjacentObjects(object->name(), object, others);

        // Make the name unique if touching any other room with the new name.
        int n = 1;
        while (anyAdjacent(roomName, objects, others)) {
            ++n;
            roomName = newName + hash + QString::number(n);
        }
    }

    mapDocument()->undoStack()->beginMacro(tr("Rename RoomDefs"));
    foreach (MapObject *object, objects)
        mapDocument()->undoStack()->push(new ChangeMapObject(mapDocument(),
                                                             object,
                                                             roomName,
                                                             object->type()));
    mapDocument()->undoStack()->endMacro();
}

bool RoomDefTool::anyAdjacent(const QString &roomName,
                              const QList<MapObject *> objects,
                              const QList<MapObject *> &others)
{
    foreach (MapObject *o, objects) {
        if (adjacentObjects(roomName, o, others).size() > 1)
            return true;
    }
    return false;
}

QList<MapObject *> RoomDefTool::adjacentObjects(const QString &roomName,
                                                MapObject *object,
                                                const QList<MapObject *> &others)
{
    const QChar hash = QLatin1Char('#');
    if (!roomName.contains(hash))
        return QList<MapObject*>() << object;

    QList<MapObject *> remaining = objectsNamed(roomName, others);
    remaining.removeOne(object);

    QList<MapObject *> ret;
    ret += object;
    while (1) {
        int count = remaining.size();
        for (int i = 0; i < remaining.size(); i++) {
            MapObject *o = remaining[i];
            foreach (MapObject *o2, ret) {
                if (isAdjacent(o, o2)) {
                    ret += o;
                    remaining.removeAt(i);
                    i--;
                    break;
                }
            }
        }
        if (count == remaining.size())
            break;
    }

    return ret;
}

bool RoomDefTool::isAdjacent(MapObject *o1, MapObject *o2)
{
    if (o1 == o2) return false;
    return (o1->bounds().adjusted(-1, 0, 1, 0).intersects(o2->bounds())
            || (o1->bounds().adjusted(0, -1, 0, 1).intersects(o2->bounds())));
}

QList<MapObject*> RoomDefTool::objectsNamed(const QString &name,
                                             const QList<MapObject *> &objects)
{
    QList<MapObject*> ret;
    foreach (MapObject *o, objects) {
        if (o->name() == name)
            ret += o;
    }
    return ret;
}
