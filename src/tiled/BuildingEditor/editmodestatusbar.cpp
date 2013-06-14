#include "editmodestatusbar.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingdocumentmgr.h"
#include "buildingtemplates.h"
#include "buildingtools.h"

#include <QAction>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>

using namespace BuildingEditor;

EditModeStatusBar::EditModeStatusBar(const QString &prefix, QObject *parent) :
    QObject(parent)
{
    setObjectName(prefix + QLatin1String("object"));

    statusBarLayout = new QHBoxLayout();
    statusBarLayout->setObjectName(prefix + QLatin1String("statusBarLayout"));

    coordLabel = new QLabel();
    coordLabel->setObjectName(prefix + QLatin1String("coordLabel"));
    coordLabel->setAlignment(Qt::AlignCenter);
    statusBarLayout->addWidget(coordLabel);

    statusLabel = new QLabel();
    statusLabel->setObjectName(prefix + QLatin1String("statusLabel"));
    statusBarLayout->addWidget(statusLabel);

    spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    statusBarLayout->addItem(spacer);

    editorScaleComboBox = new QComboBox();
    editorScaleComboBox->setObjectName(prefix + QLatin1String("editorScaleComboBox"));
    statusBarLayout->addWidget(editorScaleComboBox);

    connect(BuildingDocumentMgr::instance(), SIGNAL(currentDocumentChanged(BuildingDocument*)),
            SLOT(resizeCoordsLabel()));
    connect(ToolManager::instance(), SIGNAL(statusTextChanged(BaseTool*)),
            SLOT(updateToolStatusText()));
    connect(ToolManager::instance(), SIGNAL(currentToolChanged(BaseTool*)),
            SLOT(currentToolChanged(BaseTool*)));
}

void EditModeStatusBar::resizeCoordsLabel()
{
    int width = 999, height = 999;
    if (BuildingDocument *doc = BuildingDocumentMgr::instance()->currentDocument()) {
        width = qMax(width, doc->building()->width());
        height = qMax(height, doc->building()->height());
    }
    QFontMetrics fm = coordLabel->fontMetrics();
    QString coordString = QString(QLatin1String("%1,%2")).arg(width).arg(height);
    coordLabel->setMinimumWidth(fm.width(coordString) + 8);
}

void EditModeStatusBar::mouseCoordinateChanged(const QPoint &tilePos)
{
    coordLabel->setText(tr("%1,%2").arg(tilePos.x()).arg(tilePos.y()));
}

void EditModeStatusBar::currentToolChanged(BaseTool *tool)
{
    Q_UNUSED(tool)
    updateToolStatusText();
}

void EditModeStatusBar::updateToolStatusText()
{
    if (BaseTool *tool = ToolManager::instance()->currentTool()) {
        statusLabel->setText(tool->statusText());
    } else {
        statusLabel->clear();
    }
}
