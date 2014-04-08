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

#include "objectwidget.h"

//------------------------------------------------------------------------------

ObjectWidget::ObjectWidget(QASObject* obj, QWidget* parent) : 
  ObjectWidgetWithSignals(parent),
  m_objectWidget(NULL),
  m_shortObjectWidget(NULL),
  m_contextLabel(NULL),
  m_contextButton(NULL),
  m_topLayout(NULL),
  m_object(NULL),
  m_irtObject(NULL),
  m_short(false)
{
#ifdef DEBUG_WIDGETS
  qDebug() << "Creating ObjectWidget";
#endif

  m_layout = new QVBoxLayout;
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(0);

  // Add label with context "Re:" text and "show context" button for
  // replies.
  m_topLayout = new QHBoxLayout;
  m_topLayout->setContentsMargins(0, 0, 0, 0);

  m_contextLabel = new RichTextLabel(this, true);
  m_topLayout->addWidget(m_contextLabel, 0, Qt::AlignVCenter);

  m_topLayout->addSpacing(10);
  m_contextButton = new TextToolButton(this);
  connect(m_contextButton, SIGNAL(clicked()), this, SLOT(onShowContext()));
  m_topLayout->addWidget(m_contextButton, 0, Qt::AlignVCenter);

  m_layout->addLayout(m_topLayout);

  m_objectWidget = new FullObjectWidget(m_object, this);
  ObjectWidgetWithSignals::connectSignals(m_objectWidget, this);
  connect(m_objectWidget, SIGNAL(lessClicked()),
          this, SLOT(showLess()));
  m_layout->addWidget(m_objectWidget);

  m_shortObjectWidget = new ShortObjectWidget(m_object, this);
  connect(m_shortObjectWidget, SIGNAL(moreClicked()),
          this, SLOT(showMore()));
  m_layout->addWidget(m_shortObjectWidget);
  
  changeObject(obj);

  setLayout(m_layout);
}

//------------------------------------------------------------------------------

ObjectWidget::~ObjectWidget() {
#ifdef DEBUG_WIDGETS
  qDebug() << "Deleting ObjectWidget" << m_object->id();
#endif
}

//------------------------------------------------------------------------------

void ObjectWidget::changeObject(QASAbstractObject* obj, bool fullObject) {
  if (m_object)
    disconnect(m_object, SIGNAL(changed()), this, SLOT(onChanged()));
  if (m_irtObject)
    disconnect(m_irtObject, SIGNAL(changed()),
               this, SLOT(updateContextLabel()));
  m_irtObject = NULL;

  m_object = qobject_cast<QASObject*>(obj);
  if (!obj)
    return;
  m_short = !fullObject;

  connect(m_object, SIGNAL(changed()), this, SLOT(onChanged()));

  m_objectWidget->changeObject(obj);
  m_shortObjectWidget->changeObject(obj);

  m_contextLabel->setVisible(false);
  m_contextButton->setVisible(false);
  if (m_object->type() == "comment" && m_object->inReplyTo()) {
    m_irtObject = m_object->inReplyTo();
    connect(m_irtObject, SIGNAL(changed()), this, SLOT(updateContextLabel()));

    if (!m_irtObject->url().isEmpty())
      updateContextLabel();
  }

  if (m_short) {
    m_contextLabel->setVisible(false);
    m_contextButton->setVisible(false);
    m_objectWidget->setVisible(false);
    m_shortObjectWidget->setVisible(true);
  } else {
    m_shortObjectWidget->setVisible(false);
    m_objectWidget->setVisible(true);
  }

  QASActor* author = m_object->author();
  if (author && author->url().isEmpty())
    refreshObject(m_object);
}

//------------------------------------------------------------------------------

void ObjectWidget::showMore() {
  if (!m_short || !m_shortObjectWidget)
    return;

  m_short = false;
  m_shortObjectWidget->setVisible(false);
  m_objectWidget->setVisible(true);
  if (m_irtObject && !m_irtObject->url().isEmpty()) {
    m_contextLabel->setVisible(true);
    m_contextButton->setVisible(true);
  }
  emit moreClicked();
}
  
//------------------------------------------------------------------------------

void ObjectWidget::showLess() {
  if (m_short || !m_objectWidget)
    return;

  m_short = true;
  m_objectWidget->setVisible(false);
  m_shortObjectWidget->setVisible(true);
  m_contextLabel->setVisible(false);
  m_contextButton->setVisible(false);
  emit lessClicked();
}
  
//------------------------------------------------------------------------------

void ObjectWidget::onChanged() {
  setVisible(!m_object->url().isEmpty() && !m_object->isDeleted());
}

//------------------------------------------------------------------------------

void ObjectWidget::updateContextLabel() {
  if (!m_irtObject || !m_contextLabel)
    return;

  QString text = m_irtObject->excerpt();
  m_contextLabel->setText(tr("Re: ") + text);
  m_contextButton->setText(tr("show context"));
  if (!m_short && !m_irtObject->url().isEmpty()) {
    m_contextLabel->setVisible(true);
    m_contextButton->setVisible(true);
  }
}

//------------------------------------------------------------------------------

void ObjectWidget::onShowContext() {
  if (!m_irtObject || !m_topLayout)
    return;

  emit showContext(m_irtObject);
}
    
//------------------------------------------------------------------------------

void ObjectWidget::refreshTimeLabels() {
  if (m_objectWidget)
    m_objectWidget->refreshTimeLabels();
  if (m_shortObjectWidget)
    m_shortObjectWidget->refreshTimeLabels();
}

//------------------------------------------------------------------------------

void ObjectWidget::disableLessButton() {
  if (!m_short)
    m_objectWidget->disableLessButton();
}
