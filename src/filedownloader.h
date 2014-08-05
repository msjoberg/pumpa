/*
  Copyright 2014 Mats Sjöberg
  
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

//------------------------------------------------------------------------------

class FileDownloader;  // forward declaration

class FileDownloadManager : public QObject {
  Q_OBJECT

  FileDownloadManager(QObject*);
 public:

  static FileDownloadManager* getManager(QObject* = 0);

  bool hasFile(QString url);

  QString fileName(QString url);

  QPixmap pixmap(QString url, QString brokenImage = "");
  QMovie* movie(QString url);
  bool supportsAnimation(QString url);

  FileDownloader* download(QString url);

  void dumpStats();
  
 private slots:
  void onSslErrors(QNetworkReply*, QList<QSslError>);
  void onAuthorizedRequestReady(QByteArray response, int id);
  void onFileReady(QString = "");

 private:
  void executeAuthorizedRequest(KQOAuthRequest*, FileDownloader*);
  QString urlToPath(QString url);
 
  QNetworkAccessManager* m_nam;
  KQOAuthManager* m_oam;

  QMap<QString, FileDownloader*> m_inProgress;
  QMap<QString, QString> m_urlMap;
  QString m_cacheDir;

  int m_nextRequestId;

  typedef QPair<KQOAuthRequest*, FileDownloader*> requestData_t;
  QMap<int, requestData_t> m_requestMap;

  static FileDownloadManager* s_instance;

  friend class FileDownloader;
};
  
//------------------------------------------------------------------------------

class FileDownloader : public QObject {
  Q_OBJECT

 public:
  FileDownloader(QString url, FileDownloadManager* fdm);

  QString url() const { return m_url; }
  
 signals:
  void networkError(QString);
  void fileReady();
  
 private slots:
    void replyFinished();
  
 private:
  void requestReady(QByteArray response, KQOAuthRequest* oar);
  static void resizeImage(QPixmap pix, QString fn);

  QString m_url;
  KQOAuthRequest* m_oar;

  FileDownloadManager* m_fdm;

  friend class FileDownloadManager;
};

#endif
