/*
  Copyright 2013 Mats Sjöberg
  
  This file is part of the Pumpa programme.

  Pumpa is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Pumpa is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU General Public License
  along with Pumpa.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tabwidget.h"

#include <QDebug>

//------------------------------------------------------------------------------

TabWidget::TabWidget(QWidget* parent) : QTabWidget(parent) {
  sMap = new QSignalMapper(this);
  connect(sMap, SIGNAL(mapped(int)), this, SLOT(highlightTab(int)));
  setTabsClosable(true);

  connect(tabBar(), SIGNAL(tabCloseRequested(int)),
          this, SLOT(closeTab(int)));
}

//------------------------------------------------------------------------------

int TabWidget::addTab(QWidget* page, const QString& label, 
                      bool highlight, bool closable) {
  int index = QTabWidget::addTab(page, label);

  if (highlight)
    addHighlightConnection(page, index);

  if (!closable) {
    tabBar()->setTabButton(index, QTabBar::LeftSide, 0);
    tabBar()->setTabButton(index, QTabBar::RightSide, 0);
  } else {
    okToClose.insert(index);
  }

  return index;
}

//------------------------------------------------------------------------------

void TabWidget::closeTab(int index) {
  if (!okToClose.contains(index)) {
    qDebug() << "[ERROR] Tried to close unclosable tab" << index;
    return;
  }

  removeTab(index);
}

//------------------------------------------------------------------------------

void TabWidget::highlightTab(int index) {
  if (index == -1)
    index = currentIndex();
  tabBar()->setTabTextColor(index, Qt::red);
}

//------------------------------------------------------------------------------

void TabWidget::deHighlightTab(int index) {
  if (index == -1)
    index = currentIndex();
  QPalette pal;
  tabBar()->setTabTextColor(index, pal.color(foregroundRole()));
}

//------------------------------------------------------------------------------

void TabWidget::addHighlightConnection(QWidget* page, int index) {
  sMap->setMapping(page, index);
  connect(page, SIGNAL(highlightMe()), sMap, SLOT(map()));
}

//------------------------------------------------------------------------------

void TabWidget::keyPressEvent(QKeyEvent* event) {
  int keyn = event->key() - Qt::Key_0;
  if ((event->modifiers() == Qt::AltModifier) && // alt
      (keyn >= 1 && keyn <= 9 && keyn <= count())) { // + 0 .. 9
    setCurrentIndex(keyn-1);
  } else {
    QTabWidget::keyPressEvent(event);
  }
}
