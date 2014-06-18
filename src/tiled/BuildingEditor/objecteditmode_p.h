#ifndef OBJECTEDITMODE_P_H
#define OBJECTEDITMODE_P_H

#include <QObject>
#include <QToolBar>

class QComboBox;
class QGraphicsScene;
class QGraphicsView;
class QToolButton;
class QStackedWidget;

namespace Tiled {
namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {
class BaseTool;
class Building;
class BuildingBaseScene;
class BuildingDocument;
class BuildingOrthoScene;
class BuildingIsoScene;
class BuildingOrthoView;
class BuildingIsoView;
class Room;

class ObjectEditMode;
class OrthoObjectEditMode;
class IsoObjectEditMode;

class ObjectEditModeToolBar : public QToolBar
{
    Q_OBJECT
public:
    ObjectEditModeToolBar(ObjectEditMode *mode, QWidget *parent = 0);

    Building *currentBuilding() const;
    Room *currentRoom() const;

private slots:
    void currentDocumentChanged(BuildingDocument *doc);

    void currentRoomChanged();
    void roomIndexChanged(int index);
    void updateRoomComboBox();

    void roomAdded(Room *room);
    void roomRemoved(Room *room);
    void roomsReordered();
    void roomChanged(Room *room);

    void roofTypeChanged(QAction *action);
    void roofShallowTypeChanged(QAction *action);
    void roofCornerTypeChanged(QAction *action);

    void updateActions();

private:
    BuildingDocument *mCurrentDocument;
    QComboBox *mRoomComboBox;
    QToolButton *mFloorLabel;
};

class ObjectEditModePerDocumentStuff : public QObject
{
    Q_OBJECT
public:
    ObjectEditModePerDocumentStuff(ObjectEditMode *mode, BuildingDocument *doc);
    ~ObjectEditModePerDocumentStuff();

    BuildingDocument *document() const { return mDocument; }

    virtual QGraphicsView *view() const = 0;
    virtual BuildingBaseScene *scene() const = 0;
    virtual Tiled::Internal::Zoomable *zoomable() const = 0;

    void activate();
    void deactivate();

public slots:
    void updateDocumentTab();
    void showObjectsChanged();
    void showLowerFloorsChanged();

    void zoomIn();
    void zoomOut();
    void zoomNormal();

    void updateActions();

private:
    ObjectEditMode *mMode;
    BuildingDocument *mDocument;
};

class OrthoObjectEditModePerDocumentStuff : public ObjectEditModePerDocumentStuff
{
public:
    OrthoObjectEditModePerDocumentStuff(OrthoObjectEditMode *mode, BuildingDocument *doc);
    ~OrthoObjectEditModePerDocumentStuff();

    QGraphicsView *view() const { return (QGraphicsView*)mView; }
    BuildingBaseScene *scene() const { return (BuildingBaseScene*)mScene; }
    Tiled::Internal::Zoomable *zoomable() const;

private:
    BuildingOrthoView *mView;
    BuildingOrthoScene *mScene;
};

class IsoObjectEditModePerDocumentStuff : public ObjectEditModePerDocumentStuff
{
public:
    IsoObjectEditModePerDocumentStuff(IsoObjectEditMode *mode, BuildingDocument *doc);
    ~IsoObjectEditModePerDocumentStuff();

    QGraphicsView *view() const { return (QGraphicsView*)mView; }
    BuildingBaseScene *scene() const { return (BuildingBaseScene*)mScene; }
    Tiled::Internal::Zoomable *zoomable() const;

private:
    BuildingIsoView *mView;
    BuildingIsoScene *mScene;
};

} // namespace BuildingEditor

#endif // OBJECTEDITMODE_P_H
