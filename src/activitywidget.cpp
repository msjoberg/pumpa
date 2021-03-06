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

#include "activitywidget.h"
#include "texttoolbutton.h"
#include "pumpa_defines.h"

#include <QDebug>

//------------------------------------------------------------------------------

ActivityWidget::ActivityWidget(QASActivity* a, bool fullObject,
                               QWidget* parent) :
  ObjectWidgetWithSignals(parent),
  m_objectWidget(NULL),
  m_activity(NULL)
{
#ifdef DEBUG_WIDGETS
  qDebug() << "Creating ActivityWidget";
#endif

  m_textLabel = new RichTextLabel(this);
  connect(m_textLabel, SIGNAL(linkHovered(const QString&)),
          this,  SIGNAL(linkHovered(const QString&)));

  m_objectWidget = new ObjectWidget(NULL, this);
  ObjectWidgetWithSignals::connectSignals(m_objectWidget, this);

  connect(m_objectWidget, SIGNAL(showContext(QASObject*)),
          this, SIGNAL(showContext(QASObject*)));

  QVBoxLayout* layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_textLabel, 0, Qt::AlignTop);

  layout->addWidget(m_objectWidget, 0, Qt::AlignTop);
  layout->addWidget(new QLabel("<hr />"));

  changeObject(a, fullObject);

  setLayout(layout);
}

//------------------------------------------------------------------------------

ActivityWidget::~ActivityWidget() {
#ifdef DEBUG_WIDGETS
  qDebug() << "Deleting ActivityWidget" << m_activity->id();
#endif
}

//------------------------------------------------------------------------------

void ActivityWidget::changeObject(QASAbstractObject* aObj, bool fullObject) {
  m_activity = qobject_cast<QASActivity*>(aObj);

  if (!m_activity)
    return;

  const QString verb = m_activity->verb();
  QASObject* obj = m_activity->object();
  QString objType = obj->type();

  m_objectWidget->changeObject(obj, fullObject);
  m_objectWidget->setActivity(m_activity);

  bool objectVisible = !obj->content().isEmpty() ||
    !obj->displayName().isEmpty()
    || (objType == "image" && !obj->imageUrl().isEmpty())
    || obj->isDeleted();

  m_objectWidget->setVisible(objectVisible);

  updateText();
}

//------------------------------------------------------------------------------

void ActivityWidget::onObjectChanged() {
  updateText();
}

//------------------------------------------------------------------------------

void ActivityWidget::refreshTimeLabels() {
  if (m_objectWidget)
    m_objectWidget->refreshTimeLabels();
}

//------------------------------------------------------------------------------

void ActivityWidget::updateText() {
  QString verb = m_activity->verb();
  QString text = m_activity->content();
  QString objType = m_activity->object()->type();

  QString generatorName = m_activity->generatorName();
  if (!generatorName.isEmpty() && (verb != "share"))
    text += QString(tr(" via %1")).arg(generatorName);

  if (verb == "share")
    text = QString::fromUtf8("\342\231\273") + " " + text;

  if (verb == "post") {
    QString toStr = recipientsToString(m_activity->to());
    QString ccStr = recipientsToString(m_activity->cc());
    if (!toStr.isEmpty())
      text += " " + tr("To:") +" " + toStr;
    
    if (!ccStr.isEmpty())
      text += " " + tr("CC:") + " " + ccStr;
  }

  m_textLabel->setText(text);
}
 
//------------------------------------------------------------------------------

QString ActivityWidget::recipientsToString(QASObjectList* rec) {
  if (!rec)
    return "";

  QStringList ret;

  for (size_t i=0; i<rec->size(); ++i) {
    QASObject* r = rec->at(i);
    if (r->type() == "collection" && r->id() == PUBLIC_RECIPIENT_ID) {
      ret << tr("Public");
    } else {
      QString name = r->displayName();
      QString url = r->url();

      if (name.isEmpty())
        continue;

      if (url.isEmpty())
        ret << name;
      else
        ret << QString("<a href=\"%1\">%2</a>").arg(url).arg(name);
    }
  }

  return ret.join(", ");
}
