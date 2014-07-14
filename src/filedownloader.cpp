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

#include "filedownloader.h"
#include "pumpa_defines.h"
#include "pumpasettings.h"
#include "util.h"

#ifdef QT5
#include <QStandardPaths>
#else
#include <QDesktopServices>
#endif

#include <QCryptographicHash>

//------------------------------------------------------------------------------

FileDownloadManager* FileDownloadManager::s_instance = NULL;

//------------------------------------------------------------------------------

FileDownloadManager::FileDownloadManager(QObject* parent) : QObject(parent) {
  m_nextRequestId = 0;

  m_nam = new QNetworkAccessManager(this);
  // connect(m_nam, SIGNAL(sslErrors(QNetworkReply*, QList<QSslError>)), 
  //         this, SLOT(onSslErrors(QNetworkReply*, QList<QSslError>)));

  m_oam = new KQOAuthManager(this);
  connect(m_oam, SIGNAL(authorizedRequestReady(QByteArray, int)),
          this, SLOT(onAuthorizedRequestReady(QByteArray, int)));
  connect(m_oam, SIGNAL(sslErrors(QNetworkReply*, QList<QSslError>)), 
          this, SLOT(onSslErrors(QNetworkReply*, QList<QSslError>)));
}

//------------------------------------------------------------------------------

FileDownloadManager* FileDownloadManager::getManager(QObject* parent) {
  if (s_instance == NULL && parent != 0)
    s_instance = new FileDownloadManager(parent);

  return s_instance;
}

//------------------------------------------------------------------------------

bool FileDownloadManager::hasFile(QString url) {
  return !fileName(url).isEmpty();
}

//------------------------------------------------------------------------------

QString FileDownloadManager::fileName(QString url) {
  QString fn = urlToPath(url);
  return QFile::exists(fn) ? fn : "";
}

//------------------------------------------------------------------------------

QPixmap FileDownloadManager::pixmap(QString url, QString brokenImage) {
  QString fn = urlToPath(url);

  QPixmap pix(fn);

  // Sometimes files are given with the wrong file ending, or Qt is
  // unable to guess the format so we try to force them into popular
  // formats.

  if (pix.isNull()) 
    pix.load(fn, "JPEG");

  if (pix.isNull())
    pix.load(fn, "PNG");

  if (pix.isNull())
    pix.load(fn, "GIF");

  // OK, we give up and return a broken image icon.
  if (pix.isNull())  
    pix.load(brokenImage);

  return pix;
}

//------------------------------------------------------------------------------

QMovie* FileDownloadManager::movie(QString url) {
  QString fn = urlToPath(url);

  QMovie* mov = new QMovie(fn, QByteArray(), this);
  mov->setCacheMode(QMovie::CacheAll);  

  return mov;
}

//------------------------------------------------------------------------------

bool FileDownloadManager::supportsAnimation(QString url) {
  QString fn = urlToPath(url);

  QImageReader r(fn);
  return r.supportsAnimation();
}

//------------------------------------------------------------------------------

QString FileDownloadManager::urlToPath(QString url) {
  if (m_urlMap.contains(url))
    return m_urlMap[url];

  static QCryptographicHash hash(QCryptographicHash::Md5);
  static QStringList knownEndings;
  if (knownEndings.isEmpty())
    knownEndings << ".png" << ".jpeg" << ".jpg" << ".gif";

  if (m_cacheDir.isEmpty()) {
    m_cacheDir = 
#ifdef QT5
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#else
      QDesktopServices::storageLocation(QDesktopServices::CacheLocation);
#endif
    if (m_cacheDir.isEmpty())
      m_cacheDir = slashify(QDir::homePath())+".cache/";
    else
      m_cacheDir = slashify(m_cacheDir);
    m_cacheDir += "pumpa/";
  }
  QString path = m_cacheDir;
  QDir d;
  d.mkpath(path);

  QString ending;
  for (int i=0; i<knownEndings.count() && ending.isEmpty(); i++)
    if (url.endsWith(knownEndings[i]))
      ending = knownEndings[i];
  if (ending.isEmpty())
    ending = ".png";

  hash.reset();
  hash.addData(url.toUtf8());

  QString hashStr = hash.result().toHex();

  QString ret = path + hashStr + ending;
  m_urlMap.insert(url, ret);
  return ret;
}

//------------------------------------------------------------------------------

FileDownloader* FileDownloadManager::download(QString url) {
  if (m_inProgress.contains(url))
    return m_inProgress[url];

  qDebug() << "[DOWNLOAD]" << url;

  FileDownloader* fd = new FileDownloader(url, this);
  m_inProgress.insert(url, fd);
  connect(fd, SIGNAL(fileReady()), this, SLOT(onFileReady()));

  return fd;
}

//------------------------------------------------------------------------------

void FileDownloadManager::executeAuthorizedRequest(KQOAuthRequest* oar,
						   FileDownloader* fd) {
  int id = m_nextRequestId++;

  if (m_nextRequestId > 32000) { // bound to be smaller than any MAX_INT
    m_nextRequestId = 0;
    while (m_requestMap.contains(m_nextRequestId))
      m_nextRequestId++;
  }

  m_requestMap.insert(id, qMakePair(oar, fd));
  m_oam->executeAuthorizedRequest(oar, id);
}


//------------------------------------------------------------------------------

void FileDownloadManager::onSslErrors(QNetworkReply* nr, QList<QSslError>) {
  qDebug() << "FileDownloadManager SSL ERROR" << nr->url();
}

//------------------------------------------------------------------------------

void FileDownloadManager::onAuthorizedRequestReady(QByteArray response,
						   int id) {
  QPair<KQOAuthRequest*, FileDownloader*> rp = m_requestMap.take(id);
  KQOAuthRequest* oar = rp.first;
  FileDownloader* fd = rp.second;

  fd->requestReady(response, oar);
}

//------------------------------------------------------------------------------

void FileDownloadManager::onFileReady() {
  FileDownloader *fd = qobject_cast<FileDownloader*>(sender());  

  if (!fd || !m_inProgress.contains(fd->url())) {
    qDebug() << "ERROR: file downloader returning with non-existing request.";
    return;
  }

  fd->deleteLater();
}

//------------------------------------------------------------------------------

FileDownloader::FileDownloader(QString url, FileDownloadManager* fdm) :
  QObject(fdm),
  m_url(url),
  m_oar(NULL),
  m_fdm(fdm)
{
  PumpaSettings* ps = PumpaSettings::getSettings();

  if (ps && m_url.startsWith(ps->siteUrl())) {
    m_oar = new KQOAuthRequest(this);
    m_oar->initRequest(KQOAuthRequest::AuthorizedRequest, QUrl(m_url));

    m_oar->setConsumerKey(ps->clientId());
    m_oar->setConsumerSecretKey(ps->clientSecret());
    m_oar->setToken(ps->token());
    m_oar->setTokenSecret(ps->tokenSecret());

    m_oar->setHttpMethod(KQOAuthRequest::GET); 
    m_oar->setTimeout(60000); // one minute time-out

    m_fdm->executeAuthorizedRequest(m_oar, this);
  } else {
    QNetworkReply* nr = m_fdm->m_nam->get(QNetworkRequest(QUrl(m_url)));
    connect(nr, SIGNAL(finished()), this, SLOT(replyFinished()));
  }
}

//------------------------------------------------------------------------------

void FileDownloader::replyFinished() {
  QNetworkReply *nr = qobject_cast<QNetworkReply *>(sender());  

  if (nr->error()) {
    emit networkError(tr("Network error: ")+nr->errorString());
    return;
  }

  requestReady(nr->readAll(), NULL);
  nr->deleteLater();
}

//------------------------------------------------------------------------------

void FileDownloader::requestReady(QByteArray response, KQOAuthRequest* oar) {
  if (oar != NULL && m_fdm->m_oam->lastError()) {
    emit networkError(QString(tr("Unable to download %1 (Error #%2)."))
                      .arg(m_url)
                      .arg(m_fdm->m_oam->lastError()));
    return;
  }

  QString fn = m_fdm->urlToPath(m_url);

  QFile* fp = new QFile(fn);
  if (!fp->open(QIODevice::WriteOnly)) {
    emit networkError(QString(tr("Could not open file %1 for writing: ")).
                      arg(fn) + fp->errorString());
    return;
  }
  fp->write(response);
  fp->close();

  // QPixmap pix = pixmap(fn);
  // resizeImage(pix, fn);
  
  // emit fileReady(fn);
  emit fileReady();
}

//------------------------------------------------------------------------------

// void FileDownloader::resizeImage(QPixmap pix, QString fn) {
//   if (pix.isNull())
//     return;

//   int w = pix.width();
//   int h = pix.height();
  
//   if (w < IMAGE_MAX_WIDTH && h < IMAGE_MAX_HEIGHT)
//     return;

//   QPixmap newPix;
//   if (w > h) 
//     newPix = pix.scaledToWidth(IMAGE_MAX_WIDTH);
//   else
//     newPix = pix.scaledToHeight(IMAGE_MAX_HEIGHT);
//   newPix.save(fn);
// }
