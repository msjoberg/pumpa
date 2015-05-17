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

#include "qasobject.h"

#include "qasactor.h"
#include "qasobjectlist.h"
#include "qasactorlist.h"
#include "util.h"

#include <QDebug>
#include <QList>
#include <QtAlgorithms>
#include <QRegExp>

//------------------------------------------------------------------------------

QMap<QString, QASObject*> QASObject::s_objects;

void QASObject::clearCache() { deleteMap<QASObject*>(s_objects); }

int QASObject::objectsUnconnected() {
  int noConnections = 0;
  QMap<int, int> hist;
  for (QMap<QString, QASObject*>::const_iterator it = s_objects.begin();
       it != s_objects.end(); ++it) {
    int n = it.value()->connections();
    hist[n] = hist.value(n, 0) + 1;
    if (n == 0)
      noConnections++;
  }

  qDebug() << "QASObject connections:";
  QList<int> keys = hist.keys();
  qSort(keys);
  for (int i=0; i<keys.count(); ++i) {
    const int& k = keys[i];
    qDebug() << "   " << k << hist[k];
  }

  return noConnections;
}

int QASObject::connections() const {
  return receivers(SIGNAL(changed()));
}

//------------------------------------------------------------------------------

QASLocation::QASLocation(QObject* parent) :
  QObject(parent),
  m_latitude(-1.0),
  m_longitude(-1.0),
  m_hasPosition(false) {}

//------------------------------------------------------------------------------

void QASLocation::update(QVariantMap json) {
  bool ch;
  QASAbstractObject::updateVar(json, m_displayName, "displayName", ch);

  if (json.contains("position"))
    updatePosition(json["position"].toMap());
  else
    updatePosition(json);
}  

//------------------------------------------------------------------------------

void QASLocation::updatePosition(QVariantMap json) {
  if (json.contains("latitude") && json.contains("longitude")) {
    m_hasPosition = true;
    bool ch;
    QASAbstractObject::updateVar(json, m_longitude, "longitude", ch);
    QASAbstractObject::updateVar(json, m_latitude, "latitude", ch);
  } else {
    m_hasPosition = false;
  }
}

//------------------------------------------------------------------------------

QASObject::QASObject(QString id, QObject* parent) :
  QASAbstractObject(QAS_OBJECT, parent),
  m_id(id),
  m_liked(false),
  m_shared(false),
  m_inReplyTo(NULL),
  m_author(NULL),
  m_replies(NULL),
  m_likes(NULL),
  m_shares(NULL),
  m_postingActivity(NULL)
{
#ifdef DEBUG_QAS
  qDebug() << "new Object" << m_id;
#endif
  m_location = new QASLocation(this);
}

//------------------------------------------------------------------------------

void QASObject::update(QVariantMap json, bool ignoreLike) {
#ifdef DEBUG_QAS
  qDebug() << "updating Object" << m_id;
#endif
  bool ch = false;
  bool wasDeleted = isDeleted();

  m_json = json;

  updateVar(json, m_objectType, "objectType", ch);
  updateVar(json, m_url, "url", ch);
  updateVar(json, m_content, "content", ch);
  if (!ignoreLike)
    updateVar(json, m_liked, "liked", ch);
  updateVar(json, m_displayName, "displayName", ch);
  updateVar(json, m_shared, "pump_io", "shared", ch);

  if (m_objectType == "image" && json.contains("image")) {
    updateUrlOrProxy(json["image"].toMap(), m_imageUrl, ch);

    if (json.contains("fullImage"))
      updateVar(json["fullImage"].toMap(), m_fullImageUrl, "url", ch);
  }

  updateVar(json, m_published, "published", ch);
  updateVar(json, m_updated, "updated", ch);
  updateVar(json, m_deleted, "deleted", ch);

  updateVar(json, m_apiLink, "links", "self", "href", ch);  
  updateVar(json, m_proxyUrl, "pump_io", "proxyURL", ch);
  updateVar(json, m_streamUrl, "stream", "url", ch);
  updateVar(json, m_streamUrlProxy, "stream", "pump_io", "proxyURL", ch);

  updateVar(json, m_fileUrl, "fileUrl", ch);
  updateVar(json, m_mimeType, "mimeType", ch);

  if (json.contains("location"))
    m_location->update(json["location"].toMap());
 
  if (json.contains("inReplyTo")) {
    m_inReplyTo = QASObject::getObject(json["inReplyTo"].toMap(), parent());
    //connectSignals(m_inReplyTo, true, true);
  }

  if (json.contains("author")) {
    m_author = QASActor::getActor(json["author"].toMap(), parent());
    //connectSignals(m_author);
  }

  if (json.contains("replies")) {
    QVariantMap repliesMap = json["replies"].toMap();

    // don't replace a list with an empty one...
    size_t rs = repliesMap["items"].toList().size();
    if (!m_replies || rs>0) {
      m_replies = QASObjectList::getObjectList(repliesMap, parent());
      m_replies->isReplies(true);
      // connectSignals(m_replies);
    }
  }

  if (json.contains("likes")) {
    m_likes = QASActorList::getActorList(json["likes"].toMap(), parent());
    connectSignals(m_likes);
  }

  if (json.contains("shares")) {
    m_shares = QASActorList::getActorList(json["shares"].toMap(), parent());
    connectSignals(m_shares);
  }

  if (isDeleted()) {
    m_content = "";
    m_displayName = "";
    if (!wasDeleted)
      ch = true;
  }

  if (ch)
    emit changed();
}

//------------------------------------------------------------------------------

QASObject* QASObject::getObject(QVariantMap json, QObject* parent,
                                bool ignoreLike) {
  QString id = json["id"].toString();

  if (id.isEmpty()) // some objects from Mediagoblin seem to be without "id"
    id = json["url"].toString();

  if (id.isEmpty()) {
    qDebug() << "WARNING: null object";
    qDebug() << debugDumpJson(json, "object");
    return NULL;
  }

  if (json["objectType"] == "person")
    return QASActor::getActor(json, parent);

  QASObject* obj = s_objects.contains(id) ?  s_objects[id] :
    new QASObject(id, parent);
  s_objects.insert(id, obj);

  obj->update(json, ignoreLike);
  return obj;
}

//------------------------------------------------------------------------------

void QASObject::addReply(QASObject* obj) {
  if (!m_replies) {
    m_replies = QASObjectList::initObjectList(id() + "/replies", parent());
    m_replies->isReplies(true);
    // connectSignals(m_replies);
  }
  m_replies->addObject(obj);
#ifdef DEBUG_QAS
  qDebug() << "addReply" << obj->id() << "to" << id();
#endif
  emit changed();
}

//------------------------------------------------------------------------------

void QASObject::toggleLiked() { 
  m_liked = !m_liked; 
  emit changed();
}

//------------------------------------------------------------------------------

size_t QASObject::numLikes() const {
  return m_likes ? m_likes->size() : 0;
}

//------------------------------------------------------------------------------

void QASObject::addLike(QASActor* actor, bool like) {
  if (!m_likes)
    return;
  if (like)
    m_likes->addActor(actor);
  else
    m_likes->removeActor(actor);
}

//------------------------------------------------------------------------------

void QASObject::addShare(QASActor* actor) {
  if (!m_shares)
    return;
  m_shares->addActor(actor);
}

//------------------------------------------------------------------------------

size_t QASObject::numShares() const {
  return m_shares ? m_shares->size() : 0;
}

//------------------------------------------------------------------------------

size_t QASObject::numReplies() const {
  return m_replies ? m_replies->size() : 0;
}

//------------------------------------------------------------------------------

QString QASObject::apiLink() const {
  return 
    !m_proxyUrl.isEmpty() ? m_proxyUrl :
    !m_apiLink.isEmpty() ? m_apiLink :
    m_id;
}

//------------------------------------------------------------------------------

QVariantMap QASObject::toJson() const {
  QVariantMap obj;

  obj["id"] = m_id;

  addVar(obj, m_content, "content");
  addVar(obj, m_objectType, "objectType");
  addVar(obj, m_url, "url");
  addVar(obj, m_displayName, "displayName");
  // addVar(obj, m_, "");

  return obj;
}

//------------------------------------------------------------------------------

QASActor* QASObject::asActor() {
  return qobject_cast<QASActor*>(this);
}

//------------------------------------------------------------------------------

QString QASObject::deletedText() const {
  return "<i>[" + QString(tr("Deleted %1")).arg(deletedDate().toString()) + 
    "]</i>";
}

//------------------------------------------------------------------------------

QString QASObject::content() const {
  return isDeleted() ? deletedText() : m_content;
}

//------------------------------------------------------------------------------

QString QASObject::excerpt() const {
  if (isDeleted())
    return deletedText();

  QString text = displayName();
  if (text.isEmpty()) {
    text = content();
  }
  if (!text.isEmpty()) {
    text.replace(QRegExp(HTML_TAG_REGEX), " ");
  } else {
    QString t = type();
    text = (t == "image" ? "an " : "a ") + t;
  }
  return text.trimmed();
}

