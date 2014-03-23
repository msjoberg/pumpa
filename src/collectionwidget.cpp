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

#include "collectionwidget.h"
#include "pumpa_defines.h"
#include "activitywidget.h"

#include <QDebug>

//------------------------------------------------------------------------------

CollectionWidget::CollectionWidget(QWidget* parent, int widgetLimit,
                                   int purgeWait) :
  ASWidget(parent, widgetLimit, purgeWait),
  m_loadOlderButton(NULL)
{}

//------------------------------------------------------------------------------

QASAbstractObjectList* CollectionWidget::initList(QString endpoint,
                                                  QObject* parent) {
  m_asMode = QAS_COLLECTION;
  return QASCollection::initCollection(endpoint, parent);
}

//------------------------------------------------------------------------------

void CollectionWidget::clear() {
  ASWidget::clear();

  m_loadOlderButton = new QPushButton(this);
  m_loadOlderButton->setFocusPolicy(Qt::NoFocus);
  connect(m_loadOlderButton, SIGNAL(clicked()),
          this, SLOT(onLoadOlderClicked()));
  m_loadOlderButton->setVisible(false);
  m_itemLayout->addWidget(m_loadOlderButton);
}

//------------------------------------------------------------------------------

void CollectionWidget::onLoadOlderClicked() {
  updateLoadOlderButton(true);
  fetchOlder();
}

//------------------------------------------------------------------------------

void CollectionWidget::updateLoadOlderButton(bool wait) {
  if (!m_list->size() || m_list->nextLink().isEmpty()) {
    m_loadOlderButton->setVisible(false);
    return;
  }
  QString text = tr("Load older");
  if (wait)
    text = "...";

  m_loadOlderButton->setText(text);
  m_loadOlderButton->setVisible(true);
}

//------------------------------------------------------------------------------

void CollectionWidget::update() {
  ASWidget::update();
  updateLoadOlderButton();
}

//------------------------------------------------------------------------------

ObjectWidgetWithSignals*
CollectionWidget::createWidget(QASAbstractObject* aObj) {
  QASActivity* act = qobject_cast<QASActivity*>(aObj);
  if (!act) {
    qDebug() << "ERROR CollectionWidget::createWidget passed non-activity";
    return NULL;
  }

  ActivityWidget* aw = new ActivityWidget(act, isFullObject(act), this);
  connect(aw, SIGNAL(showContext(QASObject*)),
          this, SIGNAL(showContext(QASObject*)));

  // Add new object to shown objects set
  QASObject* obj = act->object();
  if (obj)
    m_objects_shown.insert(obj);

  return aw;
}

//------------------------------------------------------------------------------

void CollectionWidget::changeWidgetObject(ObjectWidgetWithSignals* ow,
                                          QASAbstractObject* aObj) {
  QASActivity* act = qobject_cast<QASActivity*>(aObj);
  ActivityWidget* aw = qobject_cast<ActivityWidget*>(ow);
  if (!act || !aw) {
    if (!act)
      qDebug() << "[ERROR] CollectionWidget::changeWidgetObject, bad object";
    if (!aw)
      qDebug() << "[ERROR] CollectionWidget::changeWidgetObject, bad widget";
    return ASWidget::changeWidgetObject(ow, aObj);
  }

  aw->changeObject(act, isFullObject(act));

  // Remove old object from shown objects set
  QASActivity* oldAct = aw->activity();
  QASObject* oldObj = oldAct->object();
  if (oldObj)
    m_objects_shown.remove(oldObj);

  // Add new object to shown objects set
  QASObject* obj = act->object();
  if (obj)
    m_objects_shown.insert(obj);
}

//------------------------------------------------------------------------------

bool CollectionWidget::isFullObject(QASActivity* act) {
  QString verb = act->verb();
  QASObject* obj = act->object();
  bool objAlreadyShown = obj && m_objects_shown.contains(obj);

  return (verb == "post" ||
          (verb == "share" && !objAlreadyShown));
}

//------------------------------------------------------------------------------

bool CollectionWidget::countAsNew(QASAbstractObject* aObj) {
  QASActivity* act = qobject_cast<QASActivity*>(aObj);
  if (!act)
    return false;
  
  return !act->actor()->isYou();
}
  
