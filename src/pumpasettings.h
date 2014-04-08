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

#ifndef _PUMPASETTINGS_H_
#define _PUMPASETTINGS_H_

#include <QObject>
#include <QString>
#include <QSettings>
#include <QSize>
#include <QPoint>

#include "pumpa_defines.h"

class PumpaSettings : public QObject {
  Q_OBJECT

public:
  PumpaSettings(QString filename="", QObject* parent=0);

  bool firstStart() const { return m_firstStart; }

  bool contains(QString key) const { return m_s->contains(key); }

  // getters
  QString siteUrl() const;
  QString userName() const { 
    return getValue("username", "", "Account").toString(); 
  }

  QString clientId() const {
    return getValue("oauth_client_id", "", "Account").toString();
  }
  QString clientSecret() const {
    return getValue("oauth_client_secret", "", "Account").toString();
  }

  QString token() const {
    return getValue("oauth_token", "", "Account").toString();
  }
  QString tokenSecret() const {
    return getValue("oauth_token_secret", "", "Account").toString();
  }

  int reloadTime() const;

  bool useTrayIcon() const {
    return getValue("use_tray_icon", false).toBool();
  }

  int highlightFeeds() const;
  int popupFeeds() const;

  QSize size() const {
    return getValue("size", QSize(550, 500), "MainWindow").toSize();
  }

  QPoint pos() const {
    return getValue("pos", QPoint(0, 0), "MainWindow").toPoint();
  }

  int defaultToAddress() const {
    return getValue("default_to", RECIPIENT_PUBLIC).toInt(); 
  }

  int defaultCcAddress() const {
    return getValue("default_cc", RECIPIENT_FOLLOWERS).toInt(); 
  }

  bool commentOnComments() const {
    return getValue("comment_on_comments", false).toBool();
  }

  bool useMarkdown() const {
    return getValue("use_markdown", false).toBool();
  }

  bool ignoreSslErrors() const {
    return getValue("ignore_ssl_errors", false).toBool();
  }

  QString locale() const { return getValue("locale", ""). toString(); }

  QString linkColor() const { return getValue("link_color", "").toString(); }

  QString firehoseUrl() const { 
    return getValue("firehose_url",
                    "https://ofirehose.com/feed.json").toString(); 
  }

  int maxTimelineItems() const {
    return getValue("max_timeline_items", 80).toInt();
  }

  int maxFirehoseItems() const {
    return getValue("max_timeline_items", 40).toInt();
  }

  // setters
  void siteUrl(QString s) { setValue("site_url", s, "Account"); }
  void userName(QString s) { setValue("username", s, "Account"); }

  void clientId(QString s) { setValue("oauth_client_id", s, "Account"); }
  void clientSecret(QString s) {
    setValue("oauth_client_secret", s, "Account");
  }

  void token(QString s) { setValue("oauth_token", s, "Account"); }
  void tokenSecret(QString s) { setValue("oauth_token_secret", s, "Account"); }

  void reloadTime(int i) { setValue("reload_time", i); }

  void useTrayIcon(bool b);

  void highlightFeeds(int i) { setValue("highlight_feeds", i); }
  void popupFeeds(int i) { setValue("popup_feeds", i); }

  void size(QSize s) { setValue("size", s, "MainWindow"); }
  void pos(QPoint p) { setValue("pos", p, "MainWindow"); }

  void defaultToAddress(int i) { setValue("default_to", i); }
  void defaultCcAddress(int i) { setValue("default_cc", i); }

  void commentOnComments(bool b) { setValue("comment_on_comments", b); }

  void useMarkdown(bool b) { setValue("use_markdown", b); }

  void ignoreSslErrors(bool b) { setValue("ignore_ssl_errors", b); }

signals:
  void trayIconChanged();

private:
  QVariant getValue(QString name, QVariant defaultValue=QVariant(),
                    QString group="General") const;
  void setValue(QString name, QVariant value, QString group="General");

  void readSettings();

  bool m_firstStart;

  QSettings* m_s;
};

#endif /* _PUMPASETTINGS_H_ */
