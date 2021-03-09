#ifndef TILEEDITMODE_P_H
#define TILEEDITMODE_P_H

#include "maprotation.h"

#include <QObject>

class QGraphicsView;
class QStackedWidget;

namespace Tiled {
namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {
class BaseTool;
class BuildingBaseScene;
class BuildingDocument;
class BuildingOrthoScene;
class BuildingIsoScene;
class BuildingOrthoView;
class BuildingIsoView;

class TileEditMode;
class TileEditModePerDocumentStuff : public QObject
{
    Q_OBJECT
public:
    TileEditModePerDocumentStuff(TileEditMode *mode, BuildingDocument *doc);
    ~TileEditModePerDocumentStuff();

    BuildingDocument *document() const { return mDocument; }
    BuildingIsoView *view() const { return mIsoView; }
    BuildingIsoScene *scene() const { return mIsoScene; }
    Tiled::Internal::Zoomable *zoomable() const;

    void activate();
    void deactivate();

public slots:
    void updateDocumentTab();

    void zoomIn();
    void zoomOut();
    void zoomNormal();

    void updateActions();

    void mapRotationChanged(Tiled::MapRotation rotation);

private:
    TileEditMode *mMode;
    BuildingDocument *mDocument;
    BuildingIsoView *mIsoView;
    BuildingIsoScene *mIsoScene;
};

}

#endif // TILEEDITMODE_P_H
