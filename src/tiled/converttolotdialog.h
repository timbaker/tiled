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
    
    QString filePath()
    { return mMapPath; }

    Tiled::ObjectGroup *objectGroup()
    { return mObjectGroup; }

    bool eraseSource()
    { return mEraseSource; }

    bool emptyLevels()
    { return mEmptyLevels; }

private:
    void accept();

signals:
    
public slots:
    
private:
    Ui::ConvertToLotDialog *ui;
    QString mMapPath;
    Tiled::ObjectGroup *mObjectGroup;
    QList<Tiled::ObjectGroup*> mObjectGroups;
    bool mEraseSource;
    bool mEmptyLevels;
};

#endif // CONVERTTOLOTDIALOG_H
