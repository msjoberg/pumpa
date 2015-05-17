/*
  Copyright 2013-2015 Mats Sjöberg
  
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

#ifndef _ASWIDGET_H_
#define _ASWIDGET_H_

#include "qactivitystreams.h"
#include "objectwidgetwithsignals.h"

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>

//------------------------------------------------------------------------------

class ASWidget : public QScrollArea {
  Q_OBJECT

public:
  ASWidget(QWidget* parent, int widgetLimit=-1, int purgeWait=10);
  virtual void refreshTimeLabels();
  virtual void fetchNewer();
  virtual void fetchOlder(int count=-1);
  void refresh();
  void setEndpoint(QString endpoint, QObject* parent, int asMode=-1);
  QString url() const { return m_list->url(); }

  int count() const { return m_object_set.size(); }
  const QList<QASAbstractObject*>& newObjects() { return m_newObjects; }
  virtual bool hasObject(QASAbstractObject* obj) { 
    return m_object_set.contains(obj);
  }

  bool linksInitialised() const { return !m_list->firstTime(); }

signals:
  void highlightMe();  
  void hasNewObjects();
  void request(QString, int);
  void newReply(QASObject*, QASObjectList*, QASObjectList*);
  void linkHovered(const QString&);
  void like(QASObject*);
  void share(QASObject*);
  void showContext(QASObject*);
  void follow(QString, bool);
  void deleteObject(QASObject*);
  void editObject(QASObject*);

protected slots:
  virtual void update();

protected:
  virtual QASAbstractObjectList* initList(QString endpoint, QObject* parent);

  QASAbstractObject* objectAt(int idx);
  ObjectWidgetWithSignals* widgetAt(int idx);
  virtual ObjectWidgetWithSignals* createWidget(QASAbstractObject*);
  virtual void changeWidgetObject(ObjectWidgetWithSignals*, QASAbstractObject*);
  virtual bool countAsNew(QASAbstractObject*) { return true; }

  void keyPressEvent(QKeyEvent* event);
  virtual void clear();

  void refreshObject(QASAbstractObject* obj);

  QVBoxLayout* m_itemLayout;
  QWidget* m_listContainer;
  bool m_firstTime;

  QSet<QASAbstractObject*> m_object_set;
  QASAbstractObjectList* m_list;

  int m_asMode;

  bool m_reuseWidgets;
  int m_purgeWait;
  int m_purgeCounter;
  int m_widgetLimit;

  QList<QASAbstractObject*> m_newObjects;
};

#endif /* _ASWIDGET_H_ */
