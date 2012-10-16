#ifndef BUILDINGEDITORWINDOW_H
#define BUILDINGEDITORWINDOW_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMainWindow>
#include <QMap>
#include <QVector>

class QComboBox;

namespace Ui {
class BuildingEditorWindow;
}

namespace BuildingEditor {

class FloorEditor;

class WallTypes
{
public:
    class WallType
    {
    public:
        QString Tilesheet;
        int FirstIndex;

        WallType(QString tile, int ind) :
            Tilesheet(tile),
            FirstIndex(ind)
        {
        }

        QString ToString()
        {
            return Tilesheet + QLatin1String("_") + QString::number(FirstIndex);
        }
    };

    static WallTypes *instance;
    QList<WallType*> ETypes;
    QList<WallType*> ITypes;
    QMap<QString,WallType*> ITypesByName;

    void Add(QString name, int first);
    void AddExt(QString name, int first);

    QStringList AddExteriorWallsToList();

    WallType *getEWallFromName(QString exteriorWall);
    WallType *getOrAdd(QString exteriorWall);
};

class FloorTypes
{
public:
    static FloorTypes *instance;

    class FloorType
    {
    public:
        QString Tilesheet;
        int Index;

        FloorType(QString tile, int ind) :
            Tilesheet(tile),
            Index(ind)
        {

        }
    };

    QList<FloorType*> Types;
    QMap<QString,FloorType*> TypesByName;

    void Add(QString name, int first);
};

class BaseMapObject
{
public:
    enum Direction
    {
        N,
        S,
        E,
        W
    };

    virtual QRect bounds() const
    { return QRect(X, Y, 1, 1); }

    Direction dir;
    int X, Y;
 };

class Door : public BaseMapObject
{
public:
};

class Stairs : public BaseMapObject
{
public:
    QRect bounds() const;

    QString getStairsTexture(int x, int y);
};

class Window : public BaseMapObject
{
public:
};

class Layout
{
public:

    QVector<QVector<int> > grid;
    int w, h;
    WallTypes::WallType *exteriorWall;
    QVector<WallTypes::WallType*> interiorWalls;
    QVector<FloorTypes::FloorType*> floors;
    QList<QRgb> colList;

    Layout(int w, int h, bool bCreate);

    QList<BaseMapObject*> Objects;

    Layout(QImage *bmp, QList<BaseMapObject*> objects);

    Door *GetDoorAt(int x, int y);
    Window *GetWindowAt(int x, int y);
    Stairs *GetStairsAt(int x, int y);
};

class BuildingFloor
{
public:

    class WallTile
    {
    public:
        enum WallSection
        {
            N,
            NDoor,
            W,
            WDoor,
            NW,
            SE,
            WWindow,
            NWindow
        };

        WallSection Section;
        WallTypes::WallType *Type;

        WallTile(WallSection section, WallTypes::WallType *type) :
            Section(section),
            Type(type)
        {
        }
    };

    class FloorTile
    {
    public:
        FloorTypes::FloorType *Type;

        FloorTile(FloorTypes::FloorType *type) :
            Type(type)
        {

        }
    };

    int w, h;
    WallTypes::WallType *exteriorWall;
    QVector<WallTypes::WallType*> interiorWalls;
    QVector<FloorTypes::FloorType*> floors;

    class Square
    {
    public:
        FloorTile *floorTile;
        QList<WallTile*> walls;
        QString stairsTexture;

        bool Contains(WallTile::WallSection sec);

        void Replace(WallTile *tile, WallTile::WallSection secToReplace);
        void ReplaceWithDoor(WallTile::WallSection direction);
        void ReplaceWithWindow(WallTile::WallSection direction);
    };

    QVector<QVector<Square> > squares;

    Layout *layout;

    BuildingFloor(Layout *layout);

    bool IsTopStairAt(int x, int y);
};

class BuildingDefinition
{
public:
    QString Name;
    QString Wall;

    class Room
    {
    public:
        QString Name;
        QRgb Color;
        QString internalName;
        QString Floor;
        QString Wall;
    };
    QList<Room*> RoomList;

    static QList<BuildingDefinition*> Definitions;
    static QMap<QString,BuildingDefinition*> DefinitionMap;
};

class RoomDefinitionManager
{
public:
    static RoomDefinitionManager *instance;
    QString BuildingName;
    QMap<QRgb,QString> ColorToRoomName;
    QMap<QString,QRgb> RoomNameToColor;
    QMap<QString,QString> WallForRoom;
    QMap<QString,QString> FloorForRoom;
    QStringList Rooms;
    QString ExteriorWall;
    QString FrameStyleTilesheet;
    QString DoorStyleTilesheet;
    int DoorFrameStyleIDW;
    int DoorFrameStyleIDN;
    int DoorStyleIDW;
    int DoorStyleIDN;
    QString TopStairNorth;
    QString MidStairNorth;
    QString BotStairNorth;
    QString TopStairWest;
    QString MidStairWest;
    QString BotStairWest;

    void Add(QString roomName, QRgb col, QString wall, QString floor);

    void Init(BuildingDefinition *definition);

    void Init();

    QStringList FillCombo();

    QRgb Get(QString roomName);
    int GetIndex(QRgb col);
    int getRoomCount();

    WallTypes::WallType *getWallForRoom(int i);
    FloorTypes::FloorType *getFloorForRoom(int i);
    int getFromColor(QRgb pixel);
    WallTypes::WallType *getWallForRoom(QString room);
    FloorTypes::FloorType *getFloorForRoom(QString room);
    void setWallForRoom(QString room, QString tile);
    void setFloorForRoom(QString room, QString tile);
};

int WallType_getIndexFromSection(WallTypes::WallType *type, BuildingFloor::WallTile::WallSection section);

class EditorFloor
{
public:
    QImage *bmp;
    int width;
    int height;
    Layout *layout;
    QList<BaseMapObject*> Objects;

    EditorFloor(int w, int h);

    void UpdateLayout();

    QImage *getBMP()
    { return bmp; }
};

class MetaBuilding
{
public:
    QVector<Layout*> floors;
    QVector<BuildingFloor*> buildingFloors;
    QImage bmp;

    MetaBuilding(QVector<Layout*> floors, WallTypes::WallType *ext);
};

class GraphicsFloorItem : public QGraphicsItem
{
public:
    GraphicsFloorItem(EditorFloor *floor);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
    EditorFloor *mFloor;
};

class GraphicsGridItem : public QGraphicsItem
{
public:
    GraphicsGridItem(int width, int height);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setSize(int width, int height);

private:
    int mWidth, mHeight;
};

/////

class BaseTool : public QObject
{
    Q_OBJECT
public:
    BaseTool() :
        QObject(0),
        mEditor(0)
    {}

    virtual void setEditor(FloorEditor *editor)
    { mEditor = editor; }

    void setAction(QAction *action)
    { mAction = action; }

    QAction *action() const
    { return mAction; }

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) = 0;

public slots:
    virtual void activate() = 0;
    virtual void deactivate() = 0;

protected:
    FloorEditor *mEditor;
    QAction *mAction;
};

class PencilTool : public BaseTool
{
    Q_OBJECT
public:
    static PencilTool *instance();

    PencilTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

public slots:
    void activate();
    void deactivate();

private:
    static PencilTool *mInstance;
    bool mMouseDown;
};

class EraserTool : public BaseTool
{
    Q_OBJECT
public:
    static EraserTool *instance();

    EraserTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

public slots:
    void activate();
    void deactivate();

private:
    static EraserTool *mInstance;
    bool mMouseDown;
};

/////

class FloorEditor : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit FloorEditor(QWidget *parent = 0);

    QList<EditorFloor*> floors;
    EditorFloor *currentFloor;
    GraphicsFloorItem *mCurrentFloorItem;
    MetaBuilding *building;
    int Floor;

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void UpdateMetaBuilding();

    void UpFloor();
    void DownFloor();

    int posY, posX;
    int wallSnapX, wallSnapY;
    bool vertWall;
    BaseMapObject::Direction dir;

    void ProcessMove(int x, int y);

    void activateTool(BaseTool *tool);
    QPoint sceneToTile(const QPointF &scenePos);
    bool currentFloorContains(const QPoint &tilePos);

private:
    BaseTool *mCurrentTool;
};

class BuildingEditorWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit BuildingEditorWindow(QWidget *parent = 0);
    ~BuildingEditorWindow();

    static BuildingEditorWindow *instance;

    bool Startup();

    bool LoadBuildingTemplates();

    QString currentRoom() const;

private:
    Ui::BuildingEditorWindow *ui;
    FloorEditor *roomEditor;
    QComboBox *room;
};

} // namespace BuildingEditor

#endif // BUILDINGEDITORWINDOW_H
