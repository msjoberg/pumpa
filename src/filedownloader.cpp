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

#ifdef QT5
#include <QStandardPaths>
#else
#include <QDesktopServices>
#endif

#include <QCryptographicHash>

//------------------------------------------------------------------------------

QString FileDownloader::m_cacheDir;
QMap<QString, FileDownloader*> FileDownloader::m_downloading;

QString FileDownloader::s_siteUrl;
QString FileDownloader::s_clientId;
QString FileDownloader::s_clientSecret;
QString FileDownloader::s_token;
QString FileDownloader::s_tokenSecret;

//------------------------------------------------------------------------------

QString slashify(const QString& url) {
  QString ret = url;
  if (!ret.endsWith('/'))
    ret.append('/');
  return ret;
}

//------------------------------------------------------------------------------

void FileDownloader::setOAuthInfo(QString siteUrl,
                                  QString clientId,
                                  QString clientSecret,
                                  QString token,
                                  QString tokenSecret) {
  s_siteUrl = siteUrl;
  s_clientId = clientId;
  s_clientSecret = clientSecret;
  s_token = token;
  s_tokenSecret = tokenSecret;
}

//------------------------------------------------------------------------------
FileDownloader::FileDownloader(const QString& url) :
  m_downloadingUrl(url),
  m_downloadStarted(false)
{
  QString fn = urlToPath(m_downloadingUrl);

  if (QFile::exists(fn)) {
    m_cachedFile = fn;
  } else {
    m_cachedFile = "";
    m_downloading.insert(m_downloadingUrl, this);

    oaRequest = new KQOAuthRequest(this);
    oaManager = new KQOAuthManager(this);
    connect(oaManager, SIGNAL(authorizedRequestReady(QByteArray, int)),
            this, SLOT(onAuthorizedRequestReady(QByteArray, int)));
    connect(oaManager, SIGNAL(errorMessage(QString)),
            this, SIGNAL(networkError(QString)));

    m_nam = new QNetworkAccessManager(this);
    connect(m_nam, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(replyFinished(QNetworkReply*)));
    connect(m_nam, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError>&)),
            this, SLOT(onSslErrors(QNetworkReply*, const QList<QSslError>&)));
  }
}

//------------------------------------------------------------------------------

FileDownloader* FileDownloader::get(const QString& url, bool download) {
  if (m_downloading.contains(url))
    return m_downloading[url];

  FileDownloader* fd = new FileDownloader(url);
  if (download && !fd->ready())
    fd->download();
  return fd;
}

//------------------------------------------------------------------------------

void FileDownloader::download() {
  if (m_downloadStarted)
    return;

  if (m_downloadingUrl.startsWith(s_siteUrl)) {
    oaRequest->initRequest(KQOAuthRequest::AuthorizedRequest,
                           QUrl(m_downloadingUrl));

    oaRequest->setConsumerKey(s_clientId);
    oaRequest->setConsumerSecretKey(s_clientSecret);

    oaRequest->setToken(s_token);
    oaRequest->setTokenSecret(s_tokenSecret);

    oaRequest->setHttpMethod(KQOAuthRequest::GET); 

    oaManager->executeAuthorizedRequest(oaRequest, 0);
  } else {
    m_nam->get(QNetworkRequest(QUrl(m_downloadingUrl)));
  }
  
  m_downloadStarted = true;
}

//------------------------------------------------------------------------------

QString FileDownloader::fileName() const {
  return ready() ? m_cachedFile : urlToPath(m_downloadingUrl);
}

//------------------------------------------------------------------------------

QString FileDownloader::fileName(QString defaultImage) const {
  return ready() ? m_cachedFile : defaultImage;
}

//------------------------------------------------------------------------------

QPixmap FileDownloader::pixmap(QString defaultImage) const {
  QString fn = fileName(defaultImage);
  QPixmap pix(fn);

  if (pix.isNull())
    pix.load(fn,"JPEG");

  if (pix.isNull())
    pix.load(fn,"PNG");

  if (pix.isNull())
    pix.load(defaultImage);

  return pix;
}

//------------------------------------------------------------------------------

QMovie* FileDownloader::movie(QString defaultImage) {
  QString fn = fileName(defaultImage);
  QMovie* mov = new QMovie(fn, QByteArray(), this);
  mov->setCacheMode(QMovie::CacheAll);  
  return mov;
}

//------------------------------------------------------------------------------

bool FileDownloader::supportsAnimation() const {
  if (!ready())
    return false;

  QString fn = fileName();
  QImageReader r(fn);
  return r.supportsAnimation();
}

//------------------------------------------------------------------------------

void FileDownloader::onSslErrors(QNetworkReply* reply,
                                 const QList<QSslError>&) {
  reply->ignoreSslErrors();
}

//------------------------------------------------------------------------------

void FileDownloader::replyFinished(QNetworkReply* nr) {
  if (nr->error()) {
    emit networkError(tr("Network error: ")+nr->errorString());
    return;
  }
  onAuthorizedRequestReady(nr->readAll(), 0);
  nr->deleteLater();
}

//------------------------------------------------------------------------------

void FileDownloader::onAuthorizedRequestReady(QByteArray response, int) {
  m_downloading.remove(m_downloadingUrl);

  if (oaManager->lastError()) {
    emit networkError(QString(tr("Unable to download %1 (Error #%2)."))
                      .arg(m_downloadingUrl)
                      .arg(oaManager->lastError()));
    return;
  }

  QString fn = urlToPath(m_downloadingUrl);
  QFile* fp = new QFile(fn);
  if (!fp->open(QIODevice::WriteOnly)) {
    emit networkError(QString(tr("Could not open file %1 for writing: ")).
                      arg(fn) + fp->errorString());
    return;
  }
  fp->write(response);
  fp->close();

  QPixmap pix = pixmap(fn);
  resizeImage(pix, fn);
  
  emit fileReady(fn);
  emit fileReady();
}

//------------------------------------------------------------------------------

void FileDownloader::resizeImage(QPixmap pix, QString fn) {
  if (pix.isNull())
    return;

  int w = pix.width();
  int h = pix.height();
  
  if (w < IMAGE_MAX_WIDTH && h < IMAGE_MAX_HEIGHT)
    return;

  QPixmap newPix;
  if (w > h) 
    newPix = pix.scaledToWidth(IMAGE_MAX_WIDTH);
  else
    newPix = pix.scaledToHeight(IMAGE_MAX_HEIGHT);
  newPix.save(fn);
}

//------------------------------------------------------------------------------

QString FileDownloader::urlToPath(const QString& url) {
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

  return path + hashStr + ending;
}
