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

#ifndef TABWIDGET_H
#define TABWIDGET_H

#include <QSet>
#include <QWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QSignalMapper>
#include <QKeyEvent>

class TabWidget : public QTabWidget {
  Q_OBJECT

public:
  TabWidget(QWidget* parent=0);

  int addTab(QWidget* page, const QString& label, bool highlight=true,
             bool closable=false);

  bool closable(int index) const { return m_okToClose.contains(index); }
  
  void closeCurrentTab();

public slots:
  void highlightTab(int index=-1);

  void deHighlightTab(int index=-1);

protected slots:
  void closeTab(int index);

protected:
  virtual void keyPressEvent(QKeyEvent* event);

  void addHighlightConnection(QWidget* page, int index);

  QSignalMapper* m_sMap;
  QSet<int> m_okToClose;
};

#endif
