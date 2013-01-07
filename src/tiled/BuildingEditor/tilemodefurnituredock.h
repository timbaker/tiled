#ifndef TILEMODEFURNITUREDOCK_H
#define TILEMODEFURNITUREDOCK_H

#include <QDockWidget>

class QListWidget;

namespace BuildingEditor {

class FurnitureGroup;
class FurnitureTile;
class FurnitureView;

class TileModeFurnitureDock : public QDockWidget
{
    Q_OBJECT

public:
    TileModeFurnitureDock(QWidget *parent = 0);

    void switchTo();

protected:
    void changeEvent(QEvent *event);

private:
    void retranslateUi();

    void setGroupsList();
    void setFurnitureList();

private slots:
    void currentGroupChanged(int row);
    void currentFurnitureChanged();

private:
    QListWidget *mGroupList;
    FurnitureView *mFurnitureView;
    FurnitureGroup *mCurrentGroup;
    FurnitureTile *mCurrentTile;
};

} // namespace BuildingEditor

#endif // TILEMODEFURNITUREDOCK_H
