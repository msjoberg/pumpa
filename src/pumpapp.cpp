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

#include <QStatusBar>

#include "pumpapp.h"

#include "json.h"
#include "util.h"
#include "messagewindow.h"
#include "filedownloader.h"

//------------------------------------------------------------------------------

PumpApp::PumpApp(QString settingsFile, QWidget* parent) : 
  QMainWindow(parent),
  m_wiz(NULL),
  m_trayIcon(NULL),
  m_requests(0)
{
  m_s = new PumpaSettings(settingsFile, this);
  resize(m_s->size());
  move(m_s->pos());

  m_settingsDialog = new PumpaSettingsDialog(m_s, this);
  connect(m_settingsDialog, SIGNAL(newAccount()),
          this, SLOT(launchOAuthWizard()));
  
  oaRequest = new KQOAuthRequest(this);
  oaManager = new KQOAuthManager(this);
  connect(oaManager, SIGNAL(authorizedRequestReady(QByteArray, int)),
          this, SLOT(onAuthorizedRequestReady(QByteArray, int)));

  createActions();
  createMenu();

#ifdef USE_DBUS
  m_dbus = new QDBusInterface("org.freedesktop.Notifications",
                              "/org/freedesktop/Notifications",
                              "org.freedesktop.Notifications");
  if (!m_dbus->isValid()) {
    qDebug() << "Unable to to connect to org.freedesktop.Notifications "
      "dbus service.";
    m_dbus = NULL;
  }
#endif

  updateTrayIcon();
  connect(m_s, SIGNAL(trayIconChanged()), this, SLOT(updateTrayIcon()));

  m_notifyMap = new QSignalMapper(this);

  m_inboxWidget = new CollectionWidget(this);
  connectCollection(m_inboxWidget);

  m_inboxMinorWidget = new CollectionWidget(this, true);
  connectCollection(m_inboxMinorWidget);

  m_directMajorWidget = new CollectionWidget(this);
  connectCollection(m_directMajorWidget);

  m_directMinorWidget = new CollectionWidget(this);
  connectCollection(m_directMinorWidget);

  m_tabWidget = new TabWidget(this);
  connect(m_tabWidget, SIGNAL(currentChanged(int)),
          this, SLOT(tabSelected(int)));
  m_tabWidget->addTab(m_inboxWidget, "&inbox");
  m_tabWidget->addTab(m_directMinorWidget, "&mentions");
  m_tabWidget->addTab(m_directMajorWidget, "&direct");
  m_tabWidget->addTab(m_inboxMinorWidget, "mean&while");

  m_notifyMap->setMapping(m_inboxWidget, FEED_INBOX);
  m_notifyMap->setMapping(m_directMinorWidget, FEED_MENTIONS);
  m_notifyMap->setMapping(m_directMajorWidget, FEED_DIRECT);
  m_notifyMap->setMapping(m_inboxMinorWidget, FEED_MEANWHILE);

  connect(m_notifyMap, SIGNAL(mapped(int)),
          this, SLOT(timelineHighlighted(int)));

  setWindowTitle(CLIENT_FANCY_NAME);
  setWindowIcon(QIcon(CLIENT_ICON));
  setCentralWidget(m_tabWidget);

  // oaRequest->setEnableDebugOutput(true);
  syncOAuthInfo();

  m_timerId = -1;

  if (!haveOAuth())
    launchOAuthWizard();
  else
    startPumping();
}

//------------------------------------------------------------------------------

PumpApp::~PumpApp() {
  m_s->size(size());
  m_s->pos(pos());
}

//------------------------------------------------------------------------------

void PumpApp::launchOAuthWizard() {
  if (!m_wiz) {
    m_wiz = new OAuthWizard(this);
    connect(m_wiz, SIGNAL(clientRegistered(QString, QString, QString, QString)),
            this, SLOT(onClientRegistered(QString, QString, QString, QString)));
    connect(m_wiz, SIGNAL(accessTokenReceived(QString, QString)),
            this, SLOT(onAccessTokenReceived(QString, QString)));
    connect(m_wiz, SIGNAL(rejected()), this, SLOT(exit()));
    connect(m_wiz, SIGNAL(accepted()), this, SLOT(show()));
  }
  m_wiz->show();
}

//------------------------------------------------------------------------------

void PumpApp::startPumping() {
  resetActivityStreams();

  // Setup endpoints for our timeline widgets
  m_inboxWidget->setEndpoint(inboxEndpoint("major"));
  m_inboxMinorWidget->setEndpoint(inboxEndpoint("minor"));
  m_directMajorWidget->setEndpoint(inboxEndpoint("direct/major"));
  m_directMinorWidget->setEndpoint(inboxEndpoint("direct/minor"));

  show();
  request("/api/user/" + m_s->userName(), QAS_SELF_PROFILE);
  fetchAll();

  resetTimer();
}

//------------------------------------------------------------------------------

void PumpApp::connectCollection(CollectionWidget* w) {
  connect(w, SIGNAL(request(QString, int)), this, SLOT(request(QString, int)));
  connect(w, SIGNAL(newReply(QASObject*)), this, SLOT(newNote(QASObject*)));
  connect(w, SIGNAL(linkHovered(const QString&)),
          this, SLOT(statusMessage(const QString&)));
  connect(w, SIGNAL(like(QASObject*)), this, SLOT(onLike(QASObject*)));
  connect(w, SIGNAL(share(QASObject*)), this, SLOT(onShare(QASObject*)));
  connect(w, SIGNAL(highlightMe()), m_notifyMap, SLOT(map()));
}

//------------------------------------------------------------------------------

void PumpApp::onClientRegistered(QString userName, QString siteUrl,
                                 QString clientId, QString clientSecret) {
  m_s->userName(userName);
  m_s->siteUrl(siteUrl);
  m_s->clientId(clientId);
  m_s->clientSecret(clientSecret);
}

//------------------------------------------------------------------------------

void PumpApp::onAccessTokenReceived(QString token, QString tokenSecret) {
  m_s->token(token);
  m_s->tokenSecret(tokenSecret);
  syncOAuthInfo();

  startPumping();
}

//------------------------------------------------------------------------------

bool PumpApp::haveOAuth() {
  return !m_s->clientId().isEmpty() &&
    !m_s->clientSecret().isEmpty() &&
    !m_s->token().isEmpty() &&
    !m_s->tokenSecret().isEmpty();
}

//------------------------------------------------------------------------------

void PumpApp::tabSelected(int index) {
  m_tabWidget->deHighlightTab(index);
  resetNotifications();
}

//------------------------------------------------------------------------------

void PumpApp::timerEvent(QTimerEvent* event) {
  if (event->timerId() != m_timerId)
    return;
  m_timerCount++;

  if (m_timerCount >= m_s->reloadTime()) {
    m_timerCount = 0;
    fetchAll();
  }
  refreshTimeLabels();
}

//------------------------------------------------------------------------------

void PumpApp::resetTimer() {
  if (m_timerId != -1)
    killTimer(m_timerId);
  m_timerId = startTimer(60*1000); // one minute timer
  m_timerCount = 0;
}

//------------------------------------------------------------------------------

void PumpApp::refreshTimeLabels() {
  m_inboxWidget->refreshTimeLabels();
  m_directMinorWidget->refreshTimeLabels();
  m_directMajorWidget->refreshTimeLabels();
  m_inboxMinorWidget->refreshTimeLabels();
}

//------------------------------------------------------------------------------

void PumpApp::syncOAuthInfo() {
  FileDownloader::setOAuthInfo(m_s->siteUrl(), m_s->clientId(),
                               m_s->clientSecret(),
                               m_s->token(), m_s->tokenSecret());
}

//------------------------------------------------------------------------------

void PumpApp::statusMessage(const QString& msg) {
  statusBar()->showMessage(msg);
}

//------------------------------------------------------------------------------

void PumpApp::notifyMessage(QString msg) {
  statusMessage(msg);
  // qDebug() << "[STATUS]:" << msg;
}

//------------------------------------------------------------------------------

void PumpApp::timelineHighlighted(int feed) {
  if ((feed & m_s->highlightFeeds()) && m_trayIcon)
    m_trayIcon->setIcon(QIcon(":/images/pumpa_glow.png"));

  if (feed & m_s->popupFeeds())
    sendNotification(CLIENT_FANCY_NAME, "New messages.");
}

//------------------------------------------------------------------------------

void PumpApp::resetNotifications() {
  if (m_trayIcon)
    m_trayIcon->setIcon(QIcon(CLIENT_ICON));
  m_tabWidget->deHighlightTab();
}

//------------------------------------------------------------------------------

bool PumpApp::sendNotification(QString summary, QString text) {
#ifdef USE_DBUS
  if (m_dbus && m_dbus->isValid()) {

    // https://developer.gnome.org/notification-spec/
    QList<QVariant> args;
    args.append(CLIENT_NAME);       // Application Name
    args.append(0123U);         // Replaces ID (0U)
    args.append(QString());     // Notification Icon
    args.append(summary);       // Summary
    args.append(text);          // Body
    args.append(QStringList()); // Actions

    QVariantMap hints;
    // for hints to make icon, see
    // https://dev.visucore.com/bitcoin/doxygen/notificator_8cpp_source.html
    args.append(hints);
    args.append(3000);

    m_dbus->callWithArgumentList(QDBus::NoBlock, "Notify", args);
    return true;
  }
#endif

  if (QSystemTrayIcon::supportsMessages() && m_trayIcon) {
    m_trayIcon->showMessage(CLIENT_FANCY_NAME, summary+" "+text);
    return true;
  }
  
  qDebug() << "[NOTIFY]" << summary << text;
  return false;
}

//------------------------------------------------------------------------------

void PumpApp::errorMessage(QString msg) {
  statusMessage("Error: " + msg);
  qDebug() << "[ERROR]:" << msg;
}

//------------------------------------------------------------------------------

void PumpApp::updateTrayIcon() {
  bool useTray = m_s->useTrayIcon() && QSystemTrayIcon::isSystemTrayAvailable();

  if (useTray) {
    qApp->setQuitOnLastWindowClosed(false);
    if (!m_trayIcon)
      createTrayIcon();
    else
      m_trayIcon->show();
  } else {
    qApp->setQuitOnLastWindowClosed(true);
    if (m_trayIcon)
      m_trayIcon->hide();
  }
}

//------------------------------------------------------------------------------

void PumpApp::createTrayIcon() {
  m_trayIconMenu = new QMenu(this);
  m_trayIconMenu->addAction(newNoteAction);
  m_trayIconMenu->addAction(newPictureAction);
  m_trayIconMenu->addSeparator();
  m_trayIconMenu->addAction(m_showHideAction);
  m_trayIconMenu->addAction(exitAction);
  
  m_trayIcon = new QSystemTrayIcon(QIcon(CLIENT_ICON));
  connect(m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
          this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
  m_trayIcon->setContextMenu(m_trayIconMenu);
  m_trayIcon->setToolTip(CLIENT_FANCY_NAME);
  m_trayIcon->show();
}

//------------------------------------------------------------------------------

void PumpApp::trayIconActivated(QSystemTrayIcon::ActivationReason reason) {
  if (reason == QSystemTrayIcon::Trigger) {
    m_trayIcon->setIcon(QIcon(CLIENT_ICON));
    toggleVisible();
  }
}

//------------------------------------------------------------------------------

QString PumpApp::showHideText(bool visible) {
  return QString("%1 &Window").arg(visible ? "Hide" : "Show" );
}

//------------------------------------------------------------------------------

void PumpApp::toggleVisible() {
  setVisible(!isVisible());
  m_showHideAction->setText(showHideText());
}

//------------------------------------------------------------------------------

void PumpApp::createActions() {
  exitAction = new QAction(tr("E&xit"), this);
  exitAction->setShortcut(tr("Ctrl+Q"));
  connect(exitAction, SIGNAL(triggered()), this, SLOT(exit()));

  openPrefsAction = new QAction(tr("Preferences"), this);
  connect(openPrefsAction, SIGNAL(triggered()), this, SLOT(preferences()));

  reloadAction = new QAction(tr("&Reload timeline"), this);
  reloadAction->setShortcut(tr("Ctrl+R"));
  connect(reloadAction, SIGNAL(triggered()), 
          this, SLOT(reload()));

  loadOlderAction = new QAction(tr("Load &older in timeline"), this);
  loadOlderAction->setShortcut(tr("Ctrl+O"));
  connect(loadOlderAction, SIGNAL(triggered()), this, SLOT(loadOlder()));

  // pauseAct = new QAction(tr("&Pause home timeline"), this);
  // pauseAct->setShortcut(tr("Ctrl+P"));
  // pauseAct->setCheckable(true);
  // connect(pauseAct, SIGNAL(triggered()), this, SLOT(pauseTimeline()));

  aboutAction = new QAction(tr("&About"), this);
  connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));

  aboutQtAction = new QAction("About &Qt", this);
  connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

  newNoteAction = new QAction(tr("New &Note"), this);
  newNoteAction->setShortcut(tr("Ctrl+N"));
  connect(newNoteAction, SIGNAL(triggered()), this, SLOT(newNote()));

  newPictureAction = new QAction(tr("New &Picture"), this);
  newPictureAction->setShortcut(tr("Ctrl+P"));
  newPictureAction->setEnabled(false);
  connect(newPictureAction, SIGNAL(triggered()), this, SLOT(newPicture()));

  m_showHideAction = new QAction(showHideText(true), this);
  connect(m_showHideAction, SIGNAL(triggered()), this, SLOT(toggleVisible()));
}

//------------------------------------------------------------------------------

void PumpApp::createMenu() {
  fileMenu = new QMenu(tr("&Pumpa"), this);
  fileMenu->addAction(newNoteAction);
  fileMenu->addAction(newPictureAction);
  // fileMenu->addSeparator();
  fileMenu->addAction(reloadAction);
  fileMenu->addAction(loadOlderAction);
  // fileMenu->addAction(pauseAct);
  fileMenu->addSeparator();
  fileMenu->addAction(openPrefsAction);
  fileMenu->addSeparator();
  fileMenu->addAction(exitAction);
  menuBar()->addMenu(fileMenu);

  helpMenu = new QMenu(tr("&Help"), this);
  helpMenu->addAction(aboutAction);
  helpMenu->addAction(aboutQtAction);
  menuBar()->addMenu(helpMenu);
}

//------------------------------------------------------------------------------

void PumpApp::preferences() {
  m_settingsDialog->exec();
}

//------------------------------------------------------------------------------

void PumpApp::exit() { 
  qApp->exit();
}

//------------------------------------------------------------------------------

void PumpApp::about() { 
  static const QString GPL = 
    "<p>Pumpa is free software: you can redistribute it and/or modify it "
    "under the terms of the GNU General Public License as published by "
    "the Free Software Foundation, either version 3 of the License, or "
    "(at your option) any later version.</p>"
    "<p>Pumpa is distributed in the hope that it will be useful, but "
    "WITHOUT ANY WARRANTY; without even the implied warranty of "
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU "
    "General Public License for more details.</p>"
    "<p>You should have received a copy of the GNU General Public License "
    "along with Pumpa.  If not, see "
    "<a href=\"http://www.gnu.org/licenses/\">http://www.gnu.org/licenses/</a>."
    "</p>"
    "<p>The <a href=\"https://github.com/kypeli/kQOAuth\">kQOAuth library</a> "
    "is copyrighted by <a href=\"http://www.johanpaul.com/\">Johan Paul</a> "
    "and licensed under LGPL 2.1.</p>"
    "<p>The Pumpa logo was "
    "<a href=\"http://opengameart.org/content/fruit-and-veggie-inventory\">"
    "created by Joshua Taylor</a> for the "
    "<a href=\"http://lpc.opengameart.org/\">Liberated Pixel Cup</a>."
    "The logo is copyrighted by the artist and is dual licensed under the "
    "CC-BY-SA 3.0 license and the GNU GPL 3.0.";

  QString mainText = QString("<p><b>%1 %2</b> - %3<br/>"
                             "<a href=\"%4\">%4</a><br/>"
                             "Copyright &copy; 2013 Mats Sj&ouml;berg.</p>")
    .arg(CLIENT_FANCY_NAME)
    .arg(CLIENT_VERSION)
    .arg("A simple Qt-based pump.io client.")
    .arg(WEBSITE_URL);

  QMessageBox::about(this, tr("About " CLIENT_FANCY_NAME), 
                     mainText + GPL);
}

//------------------------------------------------------------------------------

void PumpApp::newNote(QASObject* obj) {
  QASObject* irtObj = obj ? obj->inReplyTo() : NULL;
  if (irtObj) {
    obj = irtObj;
    qDebug() << "[DEBUG] Opening reply window to" << obj->type() << obj->id();
  }

  MessageWindow* w = new MessageWindow(obj, m_s, this);
  connect(w, SIGNAL(sendMessage(QString, int, int)),
          this, SLOT(postNote(QString, int, int)));
  connect(w, SIGNAL(sendReply(QASObject*, QString)),
          this, SLOT(postReply(QASObject*, QString)));
  w->show();
}

//------------------------------------------------------------------------------

void PumpApp::newPicture() {
}

//------------------------------------------------------------------------------

void PumpApp::reload() {
  fetchAll();
}

//------------------------------------------------------------------------------

void PumpApp::fetchAll() {
  m_inboxWidget->fetchNewer();
  m_directMinorWidget->fetchNewer();
  m_directMajorWidget->fetchNewer();
  m_inboxMinorWidget->fetchNewer();
}

//------------------------------------------------------------------------------

void PumpApp::loadOlder() {
  CollectionWidget* cw = 
    qobject_cast<CollectionWidget*>(m_tabWidget->currentWidget());
  cw->fetchOlder();
}

//------------------------------------------------------------------------------

QString PumpApp::inboxEndpoint(QString path) {
  if (m_s->siteUrl().isEmpty()) {
    errorMessage("Site not configured yet!");
    return "";
  }
  return m_s->siteUrl() + "/api/user/" + m_s->userName() + "/inbox/" + path;
}

//------------------------------------------------------------------------------

void PumpApp::onLike(QASObject* obj) {
  feed(obj->liked() ? "unlike" : "like", obj->toJson(),
       QAS_ACTIVITY | QAS_TOGGLE_LIKE);
}

//------------------------------------------------------------------------------

void PumpApp::onShare(QASObject* obj) {
  feed("share", obj->toJson(), QAS_ACTIVITY | QAS_REFRESH);
}

//------------------------------------------------------------------------------

QString PumpApp::addTextMarkup(QString text) {
  QString oldText = text;

  qDebug() << "\n[DEBUG] MARKUP\n" << text;

  // Remove any inline HTML tags
  // text.replace(QRegExp(HTML_TAG_REGEX), "&lt;\\1&gt;");
  QRegExp rx(HTML_TAG_REGEX);
  QRegExp urlRx(URL_REGEX);
  int pos = 0;
  
  while ((pos = rx.indexIn(text, pos)) != -1) {
    int len = rx.matchedLength();
    QString tag = rx.cap(1);
    if (urlRx.exactMatch(tag)) {
      pos += len;
    } else {
      QString newText = "&lt;" + tag + "&gt;";
      text.replace(pos, len, newText);
      pos += newText.length();
    }
  }
  qDebug() << "\n[DEBUG] MARKUP (clean inline HTML)\n" << text;

  text = markDown(text);

  qDebug() << "\n[DEBUG] MARKUP (apply Markdown)\n" << text;

  text = linkifyUrls(text);

  qDebug() << "\n[DEBUG] MARKUP (linkify plain URLs)\n" << text;

  // text.replace("\n", "<br/>");
  
  // text = changePairedTags(text, "\\*\\*", "\\*\\*",
  //                            "<strong>", "</strong>");
  // text = changePairedTags(text, "\\*", "\\*", "<em>", "</em>");

  // text = changePairedTags(text, "__", "__", "<strong>", "</strong>");
  // text = changePairedTags(text, "_", "_", "<em>", "</em>");

  // text = changePairedTags(text, "``", "``", "<pre>", "</pre>", "`");
  // text = changePairedTags(text, "`", "`", "<code>", "</code>");
  
  return text;
}

//------------------------------------------------------------------------------

void PumpApp::postNote(QString content, int to, int cc) {
  if (content.isEmpty())
    return;

  QVariantMap obj;
  obj["objectType"] = "note";
  obj["content"] = addTextMarkup(content);

  feed("post", obj, QAS_OBJECT | QAS_REFRESH, to, cc);
}

//------------------------------------------------------------------------------

void PumpApp::postReply(QASObject* replyToObj, QString content) {
  if (content.isEmpty())
    return;

  QVariantMap obj;
  obj["objectType"] = "comment";
  obj["content"] = addTextMarkup(content);

  QVariantMap noteObj;
  noteObj["id"] = replyToObj->id();
  noteObj["objectType"] = replyToObj->type();
  obj["inReplyTo"] = noteObj;

  feed("post", obj, QAS_ACTIVITY | QAS_REFRESH);
}

//------------------------------------------------------------------------------

void PumpApp::addRecipient(QVariantMap& data, QString name, int to) {
  if (to == RECIPIENT_EMPTY)
    return;

  QVariantList recList;

  QVariantMap rec;
  rec["objectType"] = "collection";
  if (to == RECIPIENT_PUBLIC)
    rec["id"] = PUBLIC_RECIPIENT_ID;
  else if (to == RECIPIENT_FOLLOWERS)
    rec["id"] = apiUrl("api/user/" + m_s->userName() + "/followers");

  recList.append(rec);

  data[name] = recList;
}
//------------------------------------------------------------------------------

void PumpApp::feed(QString verb, QVariantMap object, int response_id,
                   int to, int cc) {
  QString endpoint = "api/user/" + m_s->userName() + "/feed";

  QVariantMap data;
  if (!verb.isEmpty()) {
    data["verb"] = verb;
    data["object"] = object;
  }

  addRecipient(data, "to", to);
  addRecipient(data, "cc", cc);

  request(endpoint, response_id, KQOAuthRequest::POST, data);
}

//------------------------------------------------------------------------------

QString PumpApp::apiUrl(QString endpoint) {
  QString ret = endpoint;
  if (!ret.startsWith("http")) {
    if (ret[0] != '/')
      ret = '/' + ret;
    ret = m_s->siteUrl() + ret;
  }
  return ret;
}

//------------------------------------------------------------------------------

void PumpApp::request(QString endpoint, int response_id,
                      KQOAuthRequest::RequestHttpMethod method,
                      QVariantMap data) {
  endpoint = apiUrl(endpoint);

  if (!endpoint.startsWith(m_s->siteUrl())) {
    qDebug() << "[DEBUG] dropping request for" << endpoint;
    return;
  }

  qDebug() << (method == KQOAuthRequest::GET ? "[GET]" : "[POST]") 
           << response_id << ":" << endpoint;

  QStringList epl = endpoint.split("?");
  oaRequest->initRequest(KQOAuthRequest::AuthorizedRequest, 
                         QUrl(epl[0]));
  oaRequest->setConsumerKey(m_s->clientId());
  oaRequest->setConsumerSecretKey(m_s->clientSecret());

  // I have no idea why this is the only way that seems to
  // work. Incredibly frustrating and ugly :-/
  if (epl.size() > 1) {
    KQOAuthParameters params;
    QStringList parts = epl[1].split("&");
    for (int i=0; i<parts.size(); i++) {
      QStringList ps = parts[i].split("=");
      params.insert(ps[0], QUrl::fromPercentEncoding(ps[1].toLatin1()));
    }
    oaRequest->setAdditionalParameters(params);
  }
  
  oaRequest->setToken(m_s->token());
  oaRequest->setTokenSecret(m_s->tokenSecret());

  oaRequest->setHttpMethod(method); 

  if (method == KQOAuthRequest::POST) {
    oaRequest->setContentType("application/json");
    oaRequest->setRawData(serializeJson(data));
  }

  oaManager->executeAuthorizedRequest(oaRequest, response_id);
  
  notifyMessage("Loading ...");
  m_requests++;
}

//------------------------------------------------------------------------------

void PumpApp::onAuthorizedRequestReady(QByteArray response, int id) {
  m_requests--;
  if (!m_requests) 
    notifyMessage("Ready!");

  if (oaManager->lastError()) {
    errorMessage(QString("Network or authorisation error [id=%1].").
                 arg(oaManager->lastError()));
    return;
  }

  int sid = id & 0xFF;

  QVariantMap obj = parseJson(response);
  if (sid == QAS_NULL)
    return;

  if (sid == QAS_COLLECTION) {
    QASCollection::getCollection(obj, this, id);
  } else if (sid == QAS_ACTIVITY) {
    QASActivity* act = QASActivity::getActivity(obj, this);
    if ((id & QAS_TOGGLE_LIKE) && act->object())
      act->object()->toggleLiked();
  } else if (sid == QAS_OBJECTLIST) {
    QASObjectList::getObjectList(obj, this, id);
  } else if (sid == QAS_OBJECT) {
    QASObject::getObject(obj, this);
  } else if (sid == QAS_ACTORLIST) {
    QASActorList::getActorList(obj, this);
  } else if (sid == QAS_SELF_PROFILE) {
    m_selfActor = QASActor::getActor(obj["profile"].toMap(), this);
    m_selfActor->setYou();
  }

  if (id & QAS_REFRESH) 
    fetchAll();
}

