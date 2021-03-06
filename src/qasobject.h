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

#ifndef _QASOBJECT_H_
#define _QASOBJECT_H_

#include "qasabstractobject.h"

class QASActor;
class QASActorList;
class QASObjectList;
class QASActivity;

//------------------------------------------------------------------------------

class QASLocation : public QObject {
  Q_OBJECT

public:
  QASLocation(QObject* parent);

  bool isEmpty() const { return m_displayName.isEmpty(); }
  void update(QVariantMap json);

  QString osmURL(int level) const { 
    return QString("http://www.openstreetmap.org/#map=%1/%2/%3").
      arg(level).arg(m_latitude).arg(m_longitude);
  }

  bool hasPosition() const { return m_hasPosition; }
  double latitude() const { return m_latitude; }
  double longitude() const { return m_longitude; }
  QString displayName() const { return m_displayName; }

private:
  void updatePosition(QVariantMap json);

  double m_latitude;
  double m_longitude;
  QString m_displayName;

  bool m_hasPosition;
};

//------------------------------------------------------------------------------

class QASObject : public QASAbstractObject {
  Q_OBJECT

protected:
  QASObject(QString id, QObject* parent);

public:
  static void clearCache();
  static int cacheItems() { return s_objects.count(); }
  static int objectsUnconnected();

  int connections() const;

  static QASObject* getObject(QVariantMap json, QObject* parent,
                              bool ignoreLike=false);
  static QASObject* getObject(QString id) { 
    return s_objects.contains(id) ? s_objects[id] : NULL;
  }

  virtual void update(QVariantMap json, bool ignoreLike);
  virtual void update(QVariantMap json) { update(json, false); }

  QASActor* asActor();

  qint64 sortInt() const { return sortIntByDateTime(m_updated); }
  QDateTime updatedDate() const { return m_updated; }
  
  QString id() const { return m_id; }
  QString content() const;
  QString type() const { return m_objectType; }
  QString url() const { return m_url; }
  QString imageUrl() const { return m_imageUrl; }
  QString fullImageUrl() const { return m_fullImageUrl; }
  QString displayName() const { return m_displayName; }
  virtual QString apiLink() const;
  QString proxyUrl() const { return m_proxyUrl; }
  QString urlOrProxy() const {
    return m_proxyUrl.isEmpty() ? m_url : m_proxyUrl; 
  }
  QString fileUrl() const { return m_fileUrl; }
  QString mimeType() const { return m_mimeType; }

  QASLocation* location() const { return m_location; }
  QString streamUrl(bool orig=false) const {
    return m_streamUrlProxy.isEmpty() || orig ? m_streamUrl : m_streamUrlProxy; 
  }

  QDateTime published() const { return m_published; }

  void toggleLiked();
  bool liked() const { return m_liked; }
  size_t numLikes() const;
  void addLike(QASActor* actor, bool like);
  QASActorList* likes() const { return m_likes; }

  bool shared() const { return m_shared; }
  size_t numShares() const;
  void addShare(QASActor* actor);
  QASActorList* shares() const { return m_shares; }

  size_t numReplies() const;
  QASObjectList* replies() const { return m_replies; }
  void addReply(QASObject* obj);

  QASActor* author() const { return m_author; }
  void setAuthor(QASActor* a) { m_author = a; }
  QASObject* inReplyTo() const { return m_inReplyTo; }

  // currently just a minimal variant needed for the API e.g. when
  // favouriting the object
  QVariantMap toJson() const;

  virtual bool isDeleted() const { return !m_deleted.isNull(); }
  QDateTime deletedDate() const { return m_deleted; }

  QString excerpt() const;

  QASActivity* postingActivity() const { return m_postingActivity; }
  void setPostingActivity(QASActivity* a) { m_postingActivity = a; }

protected:
  QString deletedText() const;

  QString m_id;
  QString m_content;
  bool m_liked;
  bool m_shared;
  QString m_objectType;
  QString m_url;
  QString m_imageUrl;
  QString m_fullImageUrl;
  QString m_displayName;
  QString m_apiLink;
  QString m_proxyUrl;
  QString m_streamUrl;
  QString m_streamUrlProxy;
  QString m_fileUrl;
  QString m_mimeType;

  QDateTime m_published;
  QDateTime m_updated;
  QDateTime m_deleted;

  QASLocation* m_location;

  QASObject* m_inReplyTo;
  QASActor* m_author;
  QASObjectList* m_replies;
  QASActorList* m_likes;
  QASActorList* m_shares;

  QASActivity* m_postingActivity;

  static QMap<QString, QASObject*> s_objects;
};

typedef QList<QASObject*> RecipientList;

#endif /* _QASOBJECT_H_ */
