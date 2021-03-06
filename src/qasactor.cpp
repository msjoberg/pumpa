/*
  Copyright 2013-2015 Mats Sjöberg
  
  This file is part of the Pumpa programme.

  Pumpa is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Pumpa is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Pumpa.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "qasactor.h"

#include "util.h"

#include <QRegExp>
#include <QDebug>

//------------------------------------------------------------------------------

QMap<QString, QASActor*> QASActor::s_actors;
QSet<QString> QASActor::s_hiddenAuthors;

bool QASActor::s_followed_known = false;

void QASActor::clearCache() { deleteMap<QASActor*>(s_actors); }

//------------------------------------------------------------------------------

QASActor::QASActor(QString id, QObject* parent) :
  QASObject(id, parent),
  m_followed(false),
  m_followed_json(false),
  m_followed_set(false),
  m_isYou(false)
{
#ifdef DEBUG_QAS
  qDebug() << "new Actor" << m_id;
#endif
}

//------------------------------------------------------------------------------

void QASActor::update(QVariantMap json) {
#ifdef DEBUG_QAS
  qDebug() << "updating Actor" << m_id;
#endif
  bool ch = false;
  bool dummy = false;

  m_json = json;

  m_author = NULL;

  updateVar(json, m_url, "url", ch); 
  updateVar(json, m_displayName, "displayName", ch);
  updateVar(json, m_objectType, "objectType", ch);
  updateVar(json, m_preferredUsername, "preferredUsername", ch);

  updateVar(json, m_published, "published", ch);
  updateVar(json, m_updated, "updated", ch);

  m_webFinger = m_id;
  if (m_webFinger.startsWith("http://") || m_webFinger.startsWith("https://"))
    m_webFinger = m_preferredUsername;

  if (m_webFinger.startsWith("acct:"))
    m_webFinger.remove(0, 5);

  // this seems to be unreliable
  updateVar(json, m_followed_json, "pump_io", "followed", dummy);

  updateVar(json, m_summary, "summary", ch);
  updateVar(json, m_location, "location", "displayName", ch);

  QString oldUrl = m_imageUrl;
  if (json.contains("image")) {
    QVariantMap im = json["image"].toMap();
    if (json.contains("status_net"))
      updateVar(im, m_imageUrl, "url", ch);
    else
      updateUrlOrProxy(im, m_imageUrl, ch);
  }

  if (ch)
    emit changed();
}

//------------------------------------------------------------------------------

QASActor* QASActor::getActor(QVariantMap json, QObject* parent) {
  QString id = json["id"].toString();
  Q_ASSERT_X(!id.isEmpty(), "getActor", serializeJsonC(json));

  QASActor* act = s_actors.contains(id) ? s_actors[id] :
    new QASActor(id, parent);
  s_actors.insert(id, act);

  act->update(json);
  return act;
}

//------------------------------------------------------------------------------

bool QASActor::followed() const {
  return m_followed;
}

//------------------------------------------------------------------------------

void QASActor::setFollowed(bool b) {
  if (b != m_followed) {
    m_followed_set = true;
    m_followed = b;
    emit changed();
  }
}

//------------------------------------------------------------------------------

QString QASActor::displayNameOrWebFinger() const {
  if (displayName().isEmpty())
    return webFinger();
  return displayName();
}

//------------------------------------------------------------------------------

bool QASActor::isHidden() const {
  return s_hiddenAuthors.contains(m_id);
}

//------------------------------------------------------------------------------

void QASActor::setHidden(bool b) {
  if (b)
    s_hiddenAuthors.insert(m_id);
  else
    s_hiddenAuthors.remove(m_id);
  emit changed();
}

//------------------------------------------------------------------------------

void QASActor::setHiddenAuthors(QStringList sl) { 
  s_hiddenAuthors = sl.toSet(); 
}

//------------------------------------------------------------------------------

void QASActor::setYou() {
  m_isYou = true;
  emit changed();
}

//------------------------------------------------------------------------------

void QASActor::followedIsKnown() {
  emit changed();
}

//------------------------------------------------------------------------------

void QASActor::setFollowedKnown() {
  s_followed_known = true;

  QMap<QString, QASActor*>::const_iterator it = s_actors.constBegin();
  while (it != s_actors.constEnd()) {
    it.value()->followedIsKnown();
    ++it;
  }
}
