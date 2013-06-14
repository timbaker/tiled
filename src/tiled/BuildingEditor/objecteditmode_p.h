#ifndef OBJECTEDITMODE_P_H
#define OBJECTEDITMODE_P_H

#include <QObject>
#include <QToolBar>

class QComboBox;
class QGraphicsView;
class QLabel;
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
    void roofCornerTypeChanged(QAction *action);

    void updateActions();

private:
    BuildingDocument *mCurrentDocument;
    QComboBox *mRoomComboBox;
    QLabel *mFloorLabel;
};

class ObjectEditModePerDocumentStuff : public QObject
{
    Q_OBJECT
public:
    ObjectEditModePerDocumentStuff(ObjectEditMode *mode, BuildingDocument *doc);
    ~ObjectEditModePerDocumentStuff();

    BuildingDocument *document() const { return mDocument; }
    BuildingOrthoView *orthoView() const { return mOrthoView; }
    BuildingIsoView *isoView() const { return mIsoView; }

    QGraphicsView *currentView() const;
    BuildingBaseScene *currentScene() const;
    Tiled::Internal::Zoomable *currentZoomable() const;

    QStackedWidget *stackedWidget() const { return mStackedWidget; }

    void activate();
    void deactivate();

    bool isOrtho() const { return mOrthoMode; }
    bool isIso() const { return !mOrthoMode; }

    void toOrtho();
    void toIso();

public slots:
    void updateDocumentTab();
    void showObjectsChanged();

    void zoomIn();
    void zoomOut();
    void zoomNormal();

    void updateActions();

private:
    ObjectEditMode *mMode;
    BuildingDocument *mDocument;
    QStackedWidget *mStackedWidget;
    BuildingOrthoView *mOrthoView;
    BuildingOrthoScene *mOrthoScene;
    BuildingIsoView *mIsoView;
    BuildingIsoScene *mIsoScene;
    bool mOrthoMode;
};

} // namespace BuildingEditor

#endif // OBJECTEDITMODE_P_H
