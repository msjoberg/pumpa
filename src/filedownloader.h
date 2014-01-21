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

#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QtCore>
#include <QtNetwork>
#include <QPixmap>
#include <QMovie>

#include "QtKOAuth"

class FileDownloader : public QObject {
  Q_OBJECT

public:
  static void setOAuthInfo(QString siteUrl,
                           QString clientId, QString clientSecret,
                           QString token, QString tokenSecret);

  static FileDownloader* get(const QString& url, bool download=false);

  void download();

  bool downloading() const { return m_downloadStarted; }

  bool ready() const { return !m_cachedFile.isEmpty(); }
  QString fileName() const;
  QString fileName(QString defaultImage) const;

  bool supportsAnimation() const;
  QPixmap pixmap(QString defaultImage=":/images/broken_image.png") const;
  QMovie* movie(QString defaultImage=":/images/broken_image.png");

  static QString getCacheDir() { return m_cacheDir; }
  
  static QString urlToPath(const QString& url);
  
signals:
  void networkError(const QString&);
  void fileReady(const QString&);
  void fileReady();

private slots:
  void onAuthorizedRequestReady(QByteArray response, int id);
  void onSslErrors(QNetworkReply* reply, const QList<QSslError>&);
  void replyFinished(QNetworkReply* nr);

private:
  FileDownloader();
  FileDownloader(const QString&);

  static void resizeImage(QPixmap pix, QString fn);

  KQOAuthManager *oaManager;
  KQOAuthRequest *oaRequest;
  QNetworkAccessManager* m_nam;

  QString m_downloadingUrl;
  QString m_cachedFile;

  bool m_downloadStarted;

  static QString m_cacheDir;
  static QMap<QString, FileDownloader*> m_downloading;

  static QString s_siteUrl;
  static QString s_clientId;
  static QString s_clientSecret;
  static QString s_token;
  static QString s_tokenSecret;
};

#endif
