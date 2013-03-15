#ifndef CONVERTTOLOTDIALOG_H
#define CONVERTTOLOTDIALOG_H

#include <QDialog>

namespace Ui {
class ConvertToLotDialog;
}

namespace Tiled {
class ObjectGroup;

namespace Internal {
class MapDocument;
}
}

class ConvertToLotDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConvertToLotDialog(const Tiled::Internal::MapDocument *mapDocument, QWidget *parent = 0);
    
    QString filePath() const
    { return mMapPath; }

    Tiled::ObjectGroup *objectGroup() const
    { return mObjectGroup; }

    bool eraseSource() const
    { return mEraseSource; }

    bool emptyLevels() const
    { return mEmptyLevels; }

    bool levelIsometric() const
    { return mLevelIsometric; }

    bool openLot() const
    { return mOpenLot; }

private:
    void accept();

signals:
    
public slots:
    void mapBrowse();
    
private:
    Ui::ConvertToLotDialog *ui;
    QString mMapPath;
    Tiled::ObjectGroup *mObjectGroup;
    QList<Tiled::ObjectGroup*> mObjectGroups;
    bool mEraseSource;
    bool mEmptyLevels;
    bool mLevelIsometric;
    bool mOpenLot;
};

#endif // CONVERTTOLOTDIALOG_H
