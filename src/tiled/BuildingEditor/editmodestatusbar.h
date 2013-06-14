#ifndef EDITMODESTATUSBAR_H
#define EDITMODESTATUSBAR_H

#include <QObject>
#include <QPoint>

class QHBoxLayout;
class QLabel;
class QSpacerItem;
class QLabel;
class QComboBox;

namespace BuildingEditor {

class BaseTool;

class EditModeStatusBar : public QObject
{
    Q_OBJECT
public:
    EditModeStatusBar(const QString &prefix, QObject *parent = 0);

public slots:
    void currentToolChanged(BaseTool *tool);
    void mouseCoordinateChanged(const QPoint &tilePos);
    void updateToolStatusText();
    void resizeCoordsLabel();

public:
    QHBoxLayout *statusBarLayout;
    QLabel *coordLabel;
    QSpacerItem *spacer;
    QLabel *statusLabel;
    QComboBox *editorScaleComboBox;
};

}

#endif // EDITMODESTATUSBAR_H
