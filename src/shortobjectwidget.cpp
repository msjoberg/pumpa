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

#include "shortobjectwidget.h"
#include "util.h"

#include <QVBoxLayout>

//------------------------------------------------------------------------------

ShortObjectWidget::ShortObjectWidget(QASObject* obj, QWidget* parent) :
  QFrame(parent),
  m_object(NULL),
  m_actor(NULL)
{
#ifdef DEBUG_WIDGETS
  qDebug() << "Creating ShortObjectWidget";
#endif
  m_textLabel = new RichTextLabel(this, true);

  m_actorWidget = new ActorWidget(NULL, this, true);
  connect(m_actorWidget, SIGNAL(follow(QString, bool)),
          this, SIGNAL(follow(QString, bool)));
  connect(m_actorWidget, SIGNAL(moreClicked()), this, SIGNAL(moreClicked()));

  m_moreButton = new TextToolButton("+", this);
  connect(m_moreButton, SIGNAL(clicked()), this, SIGNAL(moreClicked()));
  
  QHBoxLayout* acrossLayout = new QHBoxLayout;
  // acrossLayout->setSpacing(10);
  acrossLayout->setContentsMargins(0, 0, 0, 0);
  acrossLayout->addWidget(m_actorWidget, 0, Qt::AlignVCenter);
  acrossLayout->addWidget(m_textLabel, 0, Qt::AlignVCenter);
  acrossLayout->addWidget(m_moreButton, 0, Qt::AlignVCenter);

  changeObject(obj);
  
  setLayout(acrossLayout);
}

//------------------------------------------------------------------------------

ShortObjectWidget::~ShortObjectWidget() {
#ifdef DEBUG_WIDGETS
  qDebug() << "Deleting ShortObjectWidget" << m_object->id();
#endif
}

//------------------------------------------------------------------------------

void ShortObjectWidget::changeObject(QASAbstractObject* obj) {
  if (m_object != NULL)
    disconnect(m_object, SIGNAL(changed()), this, SLOT(onChanged()));

  m_object = qobject_cast<QASObject*>(obj);
  if (!m_object)
    return;

  connect(m_object, SIGNAL(changed()), this, SLOT(onChanged()));

  if (m_actor != NULL) 
    disconnect(m_actor, SIGNAL(changed()),
              m_actorWidget, SLOT(updateMenu()));

  QASActor* m_actor = m_object->asActor();
  if (!m_actor) {
    m_actor = m_object->author();
    if (m_actor)
      connect(m_actor, SIGNAL(changed()),
              m_actorWidget, SLOT(updateMenu()));
  }
  m_actorWidget->setActor(m_actor);

  static QSet<QString> expandableTypes;
  if (expandableTypes.isEmpty())
    expandableTypes << "person" << "note" << "comment" << "image" << "video"
                    << "audio";

  m_moreButton->setVisible(expandableTypes.contains(m_object->type())
			   && !m_object->isDeleted());

  updateText();
}

//------------------------------------------------------------------------------

void ShortObjectWidget::updateText() {
  if (!m_object)
    return;

  QString content = m_object->excerpt();

  QString text;
  QASActor* author = m_object->author();
  if (author && !author->displayName().isEmpty())
    text = author->displayName();
  if (!content.isEmpty())
    text += (text.isEmpty() ? "" : ": ") + content;

  m_textLabel->setText(text);
}

//------------------------------------------------------------------------------

void ShortObjectWidget::onChanged() {
  updateText();
  m_actorWidget->updateMenu();
}

//------------------------------------------------------------------------------

