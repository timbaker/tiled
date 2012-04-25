/*
 * zmapsdock.cpp
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "zmapsdock.hpp"

#include "mainwindow.h"
#include "preferences.h"
#include "utils.h"

#include <QBoxLayout>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QMenu>
#include <QSlider>
#include <QToolBar>
#include <QUrl>

using namespace Tiled;
using namespace Tiled::Internal;

ZMapsDock::ZMapsDock(MainWindow *mainWindow, QWidget *parent)
    : QDockWidget(parent)
	, mMapsView(new ZMapsView(mainWindow))
{
    setObjectName(QLatin1String("ZMapsDock"));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setMargin(5);

    QHBoxLayout *dirLayout = new QHBoxLayout;
	QLabel *label = new QLabel(tr("Folder:"));
	QLineEdit *edit = mDirectoryEdit = new QLineEdit();
	QToolButton *button = new QToolButton();
	button->setIcon(QIcon(QLatin1String(":/images/16x16/document-properties.png")));
	button->setToolTip(tr("Choose Folder"));
	dirLayout->addWidget(label);
	dirLayout->addWidget(edit);
	dirLayout->addWidget(button);

	layout->addWidget(mMapsView);
	layout->addLayout(dirLayout);

    setWidget(widget);
    retranslateUi();

	connect(button, SIGNAL(clicked()), this, SLOT(browse()));

	Preferences *prefs = Preferences::instance();
	connect(prefs, SIGNAL(lotDirectoryChanged()), this, SLOT(onLotDirectoryChanged()));
	edit->setText(prefs->lotDirectory());
	connect(edit, SIGNAL(returnPressed()), this, SLOT(editedLotDirectory()));

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, SIGNAL(visibilityChanged(bool)),
            mMapsView, SLOT(setVisible(bool)));
}

void ZMapsDock::browse()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Choose the Maps Folder"), mDirectoryEdit->text());
    if (!f.isEmpty()) {
		Preferences *prefs = Preferences::instance();
		prefs->setLotDirectory(f);
    }
}

void ZMapsDock::editedLotDirectory()
{
	Preferences *prefs = Preferences::instance();
	prefs->setLotDirectory(mDirectoryEdit->text());
}

void ZMapsDock::onLotDirectoryChanged()
{
	Preferences *prefs = Preferences::instance();
	mDirectoryEdit->setText(prefs->lotDirectory());
}

void ZMapsDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void ZMapsDock::retranslateUi()
{
    setWindowTitle(tr("Maps"));
}

///// ///// ///// ///// /////

/*
class CustomFileSystemModel : public QFileSystemModel
{
public:
	 QVariant headerData(int section, Qt::Orientation orientation, int role) const
	 {
		 if (role == Qt::TextAlignmentRole)
		 {
			 if (orientation == Qt::Horizontal) {
				 switch (section)
				 {
				 case 1:
					 return Qt::AlignRight;
				 }
			 }
		 }
		 return QFileSystemModel::headerData(section, orientation, role);
	 }
};
*/

ZMapsView::ZMapsView(MainWindow *mainWindow, QWidget *parent)
    : QTreeView(parent)
	, mMainWindow(mainWindow)
{
    setRootIsDecorated(false);
    setHeaderHidden(false);
    setItemsExpandable(false);
    setUniformRowHeights(true);
	setDragEnabled(true);
	setDefaultDropAction(Qt::MoveAction);

	Preferences *prefs = Preferences::instance();
	connect(prefs, SIGNAL(lotDirectoryChanged()), this, SLOT(onLotDirectoryChanged()));

	QDir lotDirectory(prefs->lotDirectory());

	QFileSystemModel *model = new /*CustomFileSystemModel*/QFileSystemModel;
	model->setRootPath(lotDirectory.absolutePath());

	model->setFilter(QDir::Files);
	model->setNameFilters(QStringList(QLatin1String("*.tmx")));
	model->setNameFilterDisables(false); // hide filtered files

	setModel(model);

	QHeaderView* hHeader = header();
	hHeader->hideSection(2);
	hHeader->hideSection(3);

	setRootIndex(model->index(lotDirectory.absolutePath()));
	
	//resizeColumnToContents(0);
	header()->setStretchLastSection(false);
	header()->setResizeMode(0, QHeaderView::Stretch);
	header()->setResizeMode(1, QHeaderView::ResizeToContents);

//	model->setHeaderData(1, Qt::Horizontal, Qt::AlignRight, Qt::TextAlignmentRole);

	connect(this, SIGNAL(activated(QModelIndex)), SLOT(onActivated(QModelIndex)));
}

QSize ZMapsView::sizeHint() const
{
    return QSize(130, 100);
}

void ZMapsView::onLotDirectoryChanged()
{
	Preferences *prefs = Preferences::instance();
	QDir lotDirectory(prefs->lotDirectory());
	setRootIndex(((QFileSystemModel*)model())->index(lotDirectory.absolutePath()));
}

void ZMapsView::mousePressEvent(QMouseEvent *event)
{
	QTreeView::mousePressEvent(event);
}

void ZMapsView::mouseMoveEvent(QMouseEvent *event)
{
#if 0
    if (!(event->buttons() & Qt::LeftButton))
         return;
     if ((event->pos() - mDragStartPosition).manhattanLength() < QApplication::startDragDistance())
         return;

     QDrag *drag = new QDrag(this);
     QMimeData *mimeData = new QMimeData;
	 QUrl
	 mimeData->setUrls(QList<QUrl>(QUrl()));
     drag->setMimeData(mimeData);
     Qt::DropAction dropAction = drag->exec(Qt::CopyAction | Qt::MoveAction);
#endif
	 QTreeView::mouseMoveEvent(event);
}

void ZMapsView::onActivated(const QModelIndex &index)
{
	QString path = ((QFileSystemModel*)model())->filePath(index);
	mMainWindow->openFile(path);
}

void ZMapsView::currentRowChanged(const QModelIndex &index)
{
    Q_UNUSED(index)
}

void ZMapsView::contextMenuEvent(QContextMenuEvent *event)
{
    Q_UNUSED(event)
}

void ZMapsView::keyPressEvent(QKeyEvent *event)
{
    QTreeView::keyPressEvent(event);
}
