/*******************************************************************************
** Qt Advanced Docking System
** Copyright (C) 2017 Uwe Kindler
** 
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License as published by the Free Software Foundation; either
** version 2.1 of the License, or (at your option) any later version.
** 
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Lesser General Public License for more details.
** 
** You should have received a copy of the GNU Lesser General Public
** License along with this library; If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/


//============================================================================
/// \file   DockWidget.cpp
/// \author Uwe Kindler
/// \date   26.02.2017
/// \brief  Implementation of CDockWidget class
//============================================================================


//============================================================================
//                                   INCLUDES
//============================================================================
#include <DockWidgetTab.h>
#include "DockWidget.h"

#include <QBoxLayout>
#include <QAction>
#include <QSplitter>
#include <QStack>
#include <QScrollArea>
#include <QTextStream>
#include <QPointer>
#include <QEvent>
#include <QDebug>
#include <QToolBar>
#include <QXmlStreamWriter>

#include "DockContainerWidget.h"
#include "DockAreaWidget.h"
#include "DockManager.h"
#include "FloatingDockContainer.h"
#include "DockSplitter.h"
#include "ads_globals.h"


namespace ads
{
/**
 * Private data class of CDockWidget class (pimpl)
 */
struct DockWidgetPrivate
{
	CDockWidget* _this = nullptr;
	QBoxLayout* Layout = nullptr;
	QWidget* Widget = nullptr;
	CDockWidgetTab* TabWidget = nullptr;
	CDockWidget::DockWidgetFeatures Features = CDockWidget::AllDockWidgetFeatures;
	CDockManager* DockManager = nullptr;
	CDockAreaWidget* DockArea = nullptr;
	QAction* ToggleViewAction = nullptr;
	bool Closed = false;
	QScrollArea* ScrollArea = nullptr;
	QToolBar* ToolBar = nullptr;
	Qt::ToolButtonStyle ToolBarStyleDocked = Qt::ToolButtonIconOnly;
	Qt::ToolButtonStyle ToolBarStyleFloating = Qt::ToolButtonTextUnderIcon;
	QSize ToolBarIconSizeDocked = QSize(16, 16);
	QSize ToolBarIconSizeFloating = QSize(24, 24);
	bool IsFloatingTopLevel = false;

	/**
	 * Private data constructor
	 */
	DockWidgetPrivate(CDockWidget* _public);

	/**
	 * Show dock widget
	 */
	void showDockWidget();

	/**
	 * Hide dock widget.
	 */
	void hideDockWidget();

	/**
	 * Hides a dock area if all dock widgets in the area are closed.
	 * This function updates the current selected tab and hides the parent
	 * dock area if it is empty
	 */
	void updateParentDockArea();

	/**
	 * Setup the top tool bar
	 */
	void setupToolBar();

	/**
	 * Setup the main scroll area
	 */
	void setupScrollArea();
};
// struct DockWidgetPrivate

//============================================================================
DockWidgetPrivate::DockWidgetPrivate(CDockWidget* _public) :
	_this(_public)
{

}


//============================================================================
void DockWidgetPrivate::showDockWidget()
{
	if (!DockArea)
	{
		CFloatingDockContainer* FloatingWidget = new CFloatingDockContainer(_this);
		FloatingWidget->resize(_this->size());
		FloatingWidget->show();
	}
	else
	{
		DockArea->toggleView(true);
		DockArea->setCurrentDockWidget(_this);
		TabWidget->show();
		QSplitter* Splitter = internal::findParent<QSplitter*>(DockArea);
		while (Splitter && !Splitter->isVisible())
		{
			Splitter->show();
			Splitter = internal::findParent<QSplitter*>(Splitter);
		}

		CDockContainerWidget* Container = DockArea->dockContainer();
		if (Container->isFloating())
		{
			CFloatingDockContainer* FloatingWidget = internal::findParent<
					CFloatingDockContainer*>(Container);
			FloatingWidget->show();
		}
	}
}


//============================================================================
void DockWidgetPrivate::hideDockWidget()
{
	TabWidget->hide();
	updateParentDockArea();
}


//============================================================================
void DockWidgetPrivate::updateParentDockArea()
{
	if (!DockArea)
	{
		return;
	}

	auto NextDockWidget = DockArea->nextOpenDockWidget(_this);
	if (NextDockWidget)
	{
		DockArea->setCurrentDockWidget(NextDockWidget);
	}
	else
	{
		DockArea->hideAreaWithNoVisibleContent();
	}
}


//============================================================================
void DockWidgetPrivate::setupToolBar()
{
	ToolBar = new QToolBar(_this);
	ToolBar->setObjectName("dockWidgetToolBar");
	Layout->insertWidget(0, ToolBar);
	ToolBar->setIconSize(QSize(16, 16));
	ToolBar->toggleViewAction()->setEnabled(false);
	ToolBar->toggleViewAction()->setVisible(false);
	_this->connect(_this, SIGNAL(topLevelChanged(bool)), SLOT(setToolbarFloatingStyle(bool)));
}



//============================================================================
void DockWidgetPrivate::setupScrollArea()
{
	ScrollArea = new QScrollArea(_this);
	ScrollArea->setObjectName("dockWidgetScrollArea");
	ScrollArea->setWidgetResizable(true);
	Layout->addWidget(ScrollArea);
}


//============================================================================
CDockWidget::CDockWidget(const QString &title, QWidget *parent) :
	QFrame(parent),
	d(new DockWidgetPrivate(this))
{
	d->Layout = new QBoxLayout(QBoxLayout::TopToBottom);
	d->Layout->setContentsMargins(0, 0, 0, 0);
	d->Layout->setSpacing(0);
	setLayout(d->Layout);
	setWindowTitle(title);
	setObjectName(title);

	d->TabWidget = new CDockWidgetTab(this);
    d->ToggleViewAction = new QAction(title, nullptr);
	d->ToggleViewAction->setCheckable(true);
	connect(d->ToggleViewAction, SIGNAL(triggered(bool)), this,
		SLOT(toggleView(bool)));
	setToolbarFloatingStyle(false);
}

//============================================================================
CDockWidget::~CDockWidget()
{
	qDebug() << "~CDockWidget()";
	delete d;
}


//============================================================================
void CDockWidget::setToggleViewActionChecked(bool Checked)
{
	QAction* Action = d->ToggleViewAction;
	Action->blockSignals(true);
	Action->setChecked(Checked);
	Action->blockSignals(false);
}


//============================================================================
void CDockWidget::setWidget(QWidget* widget, eInsertMode InsertMode)
{
	QScrollArea* ScrollAreaWidget = qobject_cast<QScrollArea*>(widget);
	if (ScrollAreaWidget || ForceNoScrollArea == InsertMode)
	{
		d->Layout->addWidget(widget);
		if (ScrollAreaWidget && ScrollAreaWidget->viewport())
		{
			ScrollAreaWidget->viewport()->setProperty("dockWidgetContent", true);
		}
	}
	else
	{
		d->setupScrollArea();
		d->ScrollArea->setWidget(widget);
	}

	d->Widget = widget;
	d->Widget->setProperty("dockWidgetContent", true);
}


//============================================================================
QWidget* CDockWidget::takeWidget()
{
	d->ScrollArea->takeWidget();
	d->Layout->removeWidget(d->Widget);
	d->Widget->setParent(nullptr);
    return d->Widget;
}


//============================================================================
QWidget* CDockWidget::widget() const
{
	return d->Widget;
}


//============================================================================
CDockWidgetTab* CDockWidget::tabWidget() const
{
	return d->TabWidget;
}


//============================================================================
void CDockWidget::setFeatures(DockWidgetFeatures features)
{
	d->Features = features;
}


//============================================================================
void CDockWidget::setFeature(DockWidgetFeature flag, bool on)
{
#if QT_VERSION >= 0x050700
	d->Features.setFlag(flag, on);
#else
    if(on)
    {
        d->Features |= flag;
    }
    else
    {
        d->Features &= ~flag;
    }
#endif
}


//============================================================================
CDockWidget::DockWidgetFeatures CDockWidget::features() const
{
	return d->Features;
}


//============================================================================
CDockManager* CDockWidget::dockManager() const
{
	return d->DockManager;
}


//============================================================================
void CDockWidget::setDockManager(CDockManager* DockManager)
{
	d->DockManager = DockManager;
}


//============================================================================
CDockContainerWidget* CDockWidget::dockContainer() const
{
	if (d->DockArea)
	{
		return d->DockArea->dockContainer();
	}
	else
	{
		return 0;
	}
}


//============================================================================
CDockAreaWidget* CDockWidget::dockAreaWidget() const
{
	return d->DockArea;
}


//============================================================================
bool CDockWidget::isFloating() const
{
	if (!isInFloatingContainer())
	{
		return false;
	}

	return dockContainer()->topLevelDockWidget() == this;
}


//============================================================================
bool CDockWidget::isInFloatingContainer() const
{
	auto Container = dockContainer();
	if (!Container)
	{
		return false;
	}

	if (!Container->isFloating())
	{
		return false;
	}

	return true;
}


//============================================================================
bool CDockWidget::isClosed() const
{
	return d->Closed;
}


//============================================================================
QAction* CDockWidget::toggleViewAction() const
{
	return d->ToggleViewAction;
}


//============================================================================
void CDockWidget::setToggleViewActionMode(eToggleViewActionMode Mode)
{
	if (ActionModeToggle == Mode)
	{
		d->ToggleViewAction->setCheckable(true);
		d->ToggleViewAction->setIcon(QIcon());
	}
	else
	{
		d->ToggleViewAction->setCheckable(false);
		d->ToggleViewAction->setIcon(d->TabWidget->icon());
	}
}


//============================================================================
void CDockWidget::toggleView(bool Open)
{
	// If the toggle view action mode is ActionModeShow, then Open is always
	// true if the sender is the toggle view action
	QAction* Sender = qobject_cast<QAction*>(sender());
	if (Sender == d->ToggleViewAction && !d->ToggleViewAction->isCheckable())
	{
		Open = true;
	}
	// If the dock widget state is different, then we really need to toggle
	// the state. If we are in the right state, then we simply make this
	// dock widget the current dock widget
	if (d->Closed != !Open)
	{
		toggleViewInternal(Open);
	}
	else if (Open && d->DockArea)
	{
		d->DockArea->setCurrentDockWidget(this);
	}
}


//============================================================================
void CDockWidget::toggleViewInternal(bool Open)
{
	CDockContainerWidget* DockContainer = dockContainer();
	CDockWidget* TopLevelDockWidgetBefore = DockContainer
		? DockContainer->topLevelDockWidget() : nullptr;

	if (Open)
	{
		d->showDockWidget();
	}
	else
	{
		d->hideDockWidget();
	}
	d->Closed = !Open;
	d->ToggleViewAction->blockSignals(true);
	d->ToggleViewAction->setChecked(Open);
	d->ToggleViewAction->blockSignals(false);
	if (d->DockArea)
	{
		d->DockArea->toggleDockWidgetView(this, Open);
	}

	if (Open && TopLevelDockWidgetBefore)
	{
		CDockWidget::emitTopLevelEventForWidget(TopLevelDockWidgetBefore, false);
	}

	// Here we need to call the dockContainer() function again, because if
	// this dock widget was unassigned before the call to showDockWidget() then
	// it has a dock container now
	DockContainer = dockContainer();
	CDockWidget* TopLevelDockWidgetAfter = DockContainer
		? DockContainer->topLevelDockWidget() : nullptr;
	CDockWidget::emitTopLevelEventForWidget(TopLevelDockWidgetAfter, true);
	CFloatingDockContainer* FloatingContainer = DockContainer->floatingWidget();
	if (FloatingContainer)
	{
		FloatingContainer->updateWindowTitle();
	}

	if (!Open)
	{
		emit closed();
	}
	emit viewToggled(Open);
}


//============================================================================
void CDockWidget::setDockArea(CDockAreaWidget* DockArea)
{
	d->DockArea = DockArea;
	d->ToggleViewAction->setChecked(DockArea != nullptr && !this->isClosed());
}


//============================================================================
void CDockWidget::saveState(QXmlStreamWriter& s) const
{
	s.writeStartElement("Widget");
	s.writeAttribute("Name", objectName());
	s.writeAttribute("Closed", QString::number(d->Closed ? 1 : 0));
	s.writeEndElement();
}


//============================================================================
void CDockWidget::flagAsUnassigned()
{
	d->Closed = true;
	setParent(d->DockManager);
	setVisible(false);
	setDockArea(nullptr);
	tabWidget()->setParent(this);
}


//============================================================================
bool CDockWidget::event(QEvent *e)
{
	if (e->type() == QEvent::WindowTitleChange)
	{
		const auto title = windowTitle();
		if (d->TabWidget)
		{
			d->TabWidget->setText(title);
		}
		if (d->ToggleViewAction)
		{
			d->ToggleViewAction->setText(title);
		}
		if (d->DockArea)
		{
			d->DockArea->markTitleBarMenuOutdated();//update tabs menu
		}
		emit titleChanged(title);
	}
	return Super::event(e);
}


#ifndef QT_NO_TOOLTIP
//============================================================================
void CDockWidget::setTabToolTip(const QString &text)
{
	if (d->TabWidget)
	{
		d->TabWidget->setToolTip(text);
	}
	if (d->ToggleViewAction)
	{
		d->ToggleViewAction->setToolTip(text);
	}
	if (d->DockArea)
	{
		d->DockArea->markTitleBarMenuOutdated();//update tabs menu
	}
}
#endif


//============================================================================
void CDockWidget::setIcon(const QIcon& Icon)
{
	d->TabWidget->setIcon(Icon);
	if (!d->ToggleViewAction->isCheckable())
	{
		d->ToggleViewAction->setIcon(Icon);
	}
}


//============================================================================
QIcon CDockWidget::icon() const
{
	return d->TabWidget->icon();
}


//============================================================================
QToolBar* CDockWidget::toolBar() const
{
	return d->ToolBar;
}


//============================================================================
QToolBar* CDockWidget::createDefaultToolBar()
{
	if (!d->ToolBar)
	{
		d->setupToolBar();
	}

	return d->ToolBar;
}


//============================================================================
void CDockWidget::setToolBar(QToolBar* ToolBar)
{
	if (d->ToolBar)
	{
		delete d->ToolBar;
	}

	d->ToolBar = ToolBar;
	d->Layout->insertWidget(0, d->ToolBar);
	this->connect(this, SIGNAL(topLevelChanged(bool)), SLOT(setToolbarFloatingStyle(bool)));
	setToolbarFloatingStyle(isFloating());
}


//============================================================================
void CDockWidget::setToolBarStyle(Qt::ToolButtonStyle Style, eState State)
{
	if (StateFloating == State)
	{
		d->ToolBarStyleFloating = Style;
	}
	else
	{
		d->ToolBarStyleDocked = Style;
	}

	setToolbarFloatingStyle(isFloating());
}


//============================================================================
Qt::ToolButtonStyle CDockWidget::toolBarStyle(eState State) const
{
	if (StateFloating == State)
	{
		return d->ToolBarStyleFloating;
	}
	else
	{
		return d->ToolBarStyleDocked;
	}
}


//============================================================================
void CDockWidget::setToolBarIconSize(const QSize& IconSize, eState State)
{
	if (StateFloating == State)
	{
		d->ToolBarIconSizeFloating = IconSize;
	}
	else
	{
		d->ToolBarIconSizeDocked = IconSize;
	}

	setToolbarFloatingStyle(isFloating());
}


//============================================================================
QSize CDockWidget::toolBarIconSize(eState State) const
{
	if (StateFloating == State)
	{
		return d->ToolBarIconSizeFloating;
	}
	else
	{
		return d->ToolBarIconSizeDocked;
	}
}


//============================================================================
void CDockWidget::setToolbarFloatingStyle(bool Floating)
{
	if (!d->ToolBar)
	{
		return;
	}

	auto IconSize = Floating ? d->ToolBarIconSizeFloating : d->ToolBarIconSizeDocked;
	if (IconSize != d->ToolBar->iconSize())
	{
		d->ToolBar->setIconSize(IconSize);
	}

	auto ButtonStyle = Floating ? d->ToolBarStyleFloating : d->ToolBarStyleDocked;
	if (ButtonStyle != d->ToolBar->toolButtonStyle())
	{
		d->ToolBar->setToolButtonStyle(ButtonStyle);
	}
}


//============================================================================
void CDockWidget::emitTopLevelEventForWidget(CDockWidget* TopLevelDockWidget, bool Floating)
{
	if (TopLevelDockWidget)
	{
		TopLevelDockWidget->dockAreaWidget()->updateTitleBarVisibility();
		TopLevelDockWidget->emitTopLevelChanged(Floating);
	}
}


//============================================================================
void CDockWidget::emitTopLevelChanged(bool Floating)
{
	if (Floating != d->IsFloatingTopLevel)
	{
		d->IsFloatingTopLevel = Floating;
		emit topLevelChanged(d->IsFloatingTopLevel);
	}
}


//============================================================================
void CDockWidget::setClosedState(bool Closed)
{
	d->Closed = Closed;
}


//============================================================================
QSize CDockWidget::minimumSizeHint() const
{
	return QSize(60, 40);
}


} // namespace ads

//---------------------------------------------------------------------------
// EOF DockWidget.cpp
