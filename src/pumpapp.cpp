/*
  Copyright 2013-2015 Mats Sjöberg
  
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
#include <QPalette>
#include <QInputDialog>
#include <QLineEdit>
#include <QClipboard>
#include <QDesktopServices>

#include "pumpapp.h"

#include "json.h"
#include "util.h"
#include "filedownloader.h"
#include "qaspell.h"
#include "editprofiledialog.h"

//------------------------------------------------------------------------------

PumpApp::PumpApp(PumpaSettings* settings, QString locale, QWidget* parent) : 
  QMainWindow(parent),
  m_nextRequestId(0),
  m_s(settings),
  m_isLoading(false),
  m_wiz(NULL),
  m_messageWindow(NULL),
  m_editProfileDialog(NULL),
  m_trayIcon(NULL),
  m_locale(locale),
  m_uploadDialog(NULL),
  m_uploadRequest(NULL)
{
  if (m_locale.isEmpty())
    m_locale = "en_US";
#ifdef USE_ASPELL
  QASpell::setLocale(m_locale);
#endif

  resize(m_s->size());
  move(m_s->pos());
  QASActor::setHiddenAuthors(m_s->hideAuthors());

  // for old users set use_markdown=true, false for new installs
  if (!m_s->firstStart() && !m_s->contains("General/use_markdown")) {
    qDebug() << "Setting Markdown on by default for old users.";
    m_s->useMarkdown(true);
  }

  m_settingsDialog = new PumpaSettingsDialog(m_s, this);
  connect(m_settingsDialog, SIGNAL(newAccount()),
          this, SLOT(launchOAuthWizard()));

  QString linkColorStr = m_s->linkColor();
  if (!linkColorStr.isEmpty()) {
    QColor linkColor(linkColorStr);
    if (linkColor.isValid()) {
      QPalette pal(qApp->palette());
      pal.setColor(QPalette::Link, linkColor);
      pal.setColor(QPalette::LinkVisited, linkColor);
      qApp->setPalette(pal);
    } else {
      qDebug() << "[ERROR] cannot parse link_color \"" + linkColorStr + "\"";
    }
  }

  m_nam = new QNetworkAccessManager(this);
  connect(m_nam, SIGNAL(sslErrors(QNetworkReply*, QList<QSslError>)), 
          this, SLOT(onSslErrors(QNetworkReply*, QList<QSslError>)));

  m_oam = new KQOAuthManager(this);
  connect(m_oam, SIGNAL(authorizedRequestReady(QByteArray, int)),
          this, SLOT(onAuthorizedRequestReady(QByteArray, int)));
  connect(m_oam, SIGNAL(sslErrors(QNetworkReply*, QList<QSslError>)), 
          this, SLOT(onSslErrors(QNetworkReply*, QList<QSslError>)));

  m_fdm = FileDownloadManager::getManager(this);

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

  int max_tl = m_s->maxTimelineItems();
  int max_fh = m_s->maxFirehoseItems();

  m_tabWidget = new TabWidget(this);
  m_tabWidget->setFocusPolicy(Qt::NoFocus);

  m_inboxWidget = new CollectionWidget(this, max_tl);
  connectCollection(m_inboxWidget);

  m_inboxMinorWidget = new CollectionWidget(this, max_tl);
  connectCollection(m_inboxMinorWidget);

  m_directMajorWidget = new CollectionWidget(this, max_tl);
  connectCollection(m_directMajorWidget);

  m_directMinorWidget = new CollectionWidget(this, max_tl);
  connectCollection(m_directMinorWidget);

  m_favouritesWidget = new ObjectListWidget(m_tabWidget);
  connectCollection(m_favouritesWidget);
  m_favouritesWidget->hide();

  m_followersWidget = new ObjectListWidget(m_tabWidget);
  connectCollection(m_followersWidget);
  m_followersWidget->hide();

  m_followingWidget = new ObjectListWidget(m_tabWidget);
  connectCollection(m_followingWidget, false);
  m_followingWidget->hide();

  m_userActivitiesWidget = new CollectionWidget(this, max_tl);
  connectCollection(m_userActivitiesWidget, false);
  m_userActivitiesWidget->hide();

  m_firehoseWidget = new CollectionWidget(this, max_fh, 0);
  connectCollection(m_firehoseWidget);
  m_firehoseWidget->hide();

  connect(m_inboxMinorWidget, SIGNAL(hasNewObjects()),
          this, SLOT(onNewMinorObjects()));
  connect(m_directMinorWidget, SIGNAL(hasNewObjects()),
          this, SLOT(onNewMinorObjects()));

  connect(m_tabWidget, SIGNAL(currentChanged(int)),
          this, SLOT(tabSelected(int)));

  m_tabWidget->addTab(m_inboxWidget, tr("&Inbox"));
  m_tabWidget->addTab(m_directMinorWidget, tr("&Mentions"));
  m_tabWidget->addTab(m_directMajorWidget, tr("&Direct"));
  m_tabWidget->addTab(m_inboxMinorWidget, tr("Mean&while"));
  // m_tabWidget->addTab(m_firehoseWidget, tr("Fi&rehose"), true, true);

  m_notifyMap->setMapping(m_inboxWidget, FEED_INBOX);
  m_notifyMap->setMapping(m_directMinorWidget, FEED_MENTIONS);
  m_notifyMap->setMapping(m_directMajorWidget, FEED_DIRECT);
  m_notifyMap->setMapping(m_inboxMinorWidget, FEED_MEANWHILE);

  connect(m_notifyMap, SIGNAL(mapped(int)),
          this, SLOT(timelineHighlighted(int)));

  m_loadIcon = new QLabel(this);
  m_loadMovie = new QMovie(":/images/loader.gif", QByteArray(), this);
  statusBar()->addPermanentWidget(m_loadIcon);

  setWindowTitle(CLIENT_FANCY_NAME);
  setWindowIcon(QIcon(CLIENT_ICON));
  setCentralWidget(m_tabWidget);

  // oaRequest->setEnableDebugOutput(true);
  // syncOAuthInfo();

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
  m_s->hideAuthors(QASActor::getHiddenAuthors());
}

//------------------------------------------------------------------------------

void PumpApp::launchOAuthWizard() {
  qApp->setQuitOnLastWindowClosed(false);

  if (!m_wiz) {
    m_wiz = new OAuthWizard(m_nam, m_oam, this);
    connect(m_wiz, SIGNAL(clientRegistered(QString, QString, QString, QString)),
            this, SLOT(onClientRegistered(QString, QString, QString, QString)));
    connect(m_wiz, SIGNAL(accessTokenReceived(QString, QString)),
            this, SLOT(onAccessTokenReceived(QString, QString)));
    connect(m_wiz, SIGNAL(accepted()), this, SLOT(show()));
    connect(m_wiz, SIGNAL(rejected()), this, SLOT(wizardCancelled()));
  }
  m_wiz->restart();
  m_wiz->show();
}

//------------------------------------------------------------------------------

QString certSubjectInfo(const QSslCertificate& cert) {
#ifdef QT5
  return cert.subjectInfo(QSslCertificate::CommonName).join(" ");
#else
  return cert.subjectInfo(QSslCertificate::CommonName);
#endif
}

QString certIssuerInfo(const QSslCertificate& cert) {
#ifdef QT5
  return cert.issuerInfo(QSslCertificate::CommonName).join(" ");
#else
  return cert.issuerInfo(QSslCertificate::CommonName);
#endif
}

//------------------------------------------------------------------------------

void PumpApp::onSslErrors(QNetworkReply* reply, QList<QSslError> errors) {
  if (m_s->ignoreSslErrors()) {
    reply->ignoreSslErrors();
    return;
  }

  QString infoText;
  if (reply)
    infoText += "URL: " + reply->url().toString() + "\n";

  for (int i=0; i<errors.size(); i++) {
    infoText += tr("SSL Error: ") + errors[i].errorString() + ".\n";
  }
  infoText += 
    QString(tr("\n%1 is unable to verify the identity of the server. "
            "This error could mean that someone is trying to impersonate the "
            "server, or that the server's administrator has made an error.\n")).
    arg(CLIENT_FANCY_NAME);

  QString detailText;
  QSslCertificate cert = errors[0].certificate();
  if (!cert.isNull()) {
    detailText = tr("SSL Server certificate.\n") +
      tr("Issued to: ") + certSubjectInfo(cert) + "\n" +
      tr("Issued by: ") + certIssuerInfo(cert) + "\n" +
      tr("Effective: ") + cert.effectiveDate().toString() + "\n" +
      tr("Expires: ") + cert.expiryDate().toString() + "\n" +
      tr("MD5 digest: ") + cert.digest().toHex() + "\n";
  }

  qDebug() << infoText;
  qDebug() << detailText;

  QMessageBox msgBox;
  msgBox.setText(tr("<b>Untrusted SSL connection!</b>"));
  msgBox.setIcon(QMessageBox::Critical);
  msgBox.setInformativeText(infoText);
  if (!detailText.isEmpty())
    msgBox.setDetailedText(detailText);
  msgBox.setStandardButtons(QMessageBox::Ignore | QMessageBox::Abort);
  msgBox.setDefaultButton(QMessageBox::Abort);

  if (msgBox.exec() == QMessageBox::Ignore) {
    reply->ignoreSslErrors();
    return;
  }
}

//------------------------------------------------------------------------------

void PumpApp::startPumping() {
  resetActivityStreams();

  QString webFinger = siteUrlToAccountId(m_s->userName(), m_s->siteUrl());

  setWindowTitle(QString("%1 - %2").arg(CLIENT_FANCY_NAME).arg(webFinger));

  // Setup endpoints for our timeline widgets
  m_inboxWidget->setEndpoint(inboxEndpoint("major"), this, QAS_FOLLOW);
  m_inboxMinorWidget->setEndpoint(inboxEndpoint("minor"), this);
  m_directMajorWidget->setEndpoint(inboxEndpoint("direct/major"), this);
  m_directMinorWidget->setEndpoint(inboxEndpoint("direct/minor"), this);
  m_followersWidget->setEndpoint(apiUrl(apiUser("followers")), this);
  m_followingWidget->setEndpoint(apiUrl(apiUser("following")), this,
                                 QAS_FOLLOW);
  m_favouritesWidget->setEndpoint(apiUrl(apiUser("favorites")), this);
  m_firehoseWidget->setEndpoint(m_s->firehoseUrl(), this);
  m_userActivitiesWidget->setEndpoint(apiUrl(apiUser("feed")), this);
  show();

  m_recipientLists.clear();

  addPublicRecipient(m_recipientLists);

  QVariantMap followersJson;
  followersJson["displayName"] = tr("Followers");
  followersJson["objectType"] = "collection";
  followersJson["id"] = apiUrl(apiUser("followers"));
  m_recipientLists.append(QASObject::getObject(followersJson, this));

  request(apiUser("profile"), QAS_SELF_PROFILE);
  request(apiUser("lists/person"), QAS_SELF_LISTS);
  fetchAll(true);

  resetTimer();
}

//------------------------------------------------------------------------------

void PumpApp::connectCollection(ASWidget* w, bool highlight) {
  connect(w, SIGNAL(request(QString, int)), this, SLOT(request(QString, int)));
  connect(w, SIGNAL(newReply(QASObject*, QASObjectList*, QASObjectList*)),
          this, SLOT(newNote(QASObject*, QASObjectList*, QASObjectList*)));
  connect(w, SIGNAL(linkHovered(const QString&)),
          this, SLOT(statusMessage(const QString&)));
  connect(w, SIGNAL(like(QASObject*)), this, SLOT(onLike(QASObject*)));
  connect(w, SIGNAL(share(QASObject*)), this, SLOT(onShare(QASObject*)));
  if (highlight)
    connect(w, SIGNAL(highlightMe()), m_notifyMap, SLOT(map()));
  connect(w, SIGNAL(showContext(QASObject*)), 
          this, SLOT(onShowContext(QASObject*)));
  connect(w, SIGNAL(follow(QString, bool)), this, SLOT(follow(QString, bool)));
  connect(w, SIGNAL(deleteObject(QASObject*)),
          this, SLOT(onDeleteObject(QASObject*)));
  connect(w, SIGNAL(editObject(QASObject*)),
	  this, SLOT(onEditObject(QASObject*)));
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
  // syncOAuthInfo();

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
  m_closeTabAction->setEnabled(m_tabWidget->closable(index));
}

//------------------------------------------------------------------------------

void PumpApp::timerEvent(QTimerEvent* event) {
  if (event->timerId() != m_timerId)
    return;
  m_timerCount++;

  if (m_timerCount >= m_s->reloadTime()) {
    m_timerCount = 0;
    fetchAll(false);
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

void PumpApp::debugAction() {
  checkMemory("debug");
  qDebug() << "inbox" << m_inboxWidget->count();
  qDebug() << "meanwhile" << m_inboxMinorWidget->count();
  qDebug() << "firehose" << m_firehoseWidget->count();

  m_fdm->dumpStats();
}

//------------------------------------------------------------------------------

void PumpApp::refreshTimeLabels() {
  m_inboxWidget->refreshTimeLabels();
  m_directMinorWidget->refreshTimeLabels();
  m_directMajorWidget->refreshTimeLabels();
  m_inboxMinorWidget->refreshTimeLabels();
  m_firehoseWidget->refreshTimeLabels();
  for (int i=0; i<m_contextWidgets.size(); ++i)
    m_contextWidgets[i]->refreshTimeLabels();
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
  bool doTrayIcon = (feed & m_s->highlightFeeds()) && m_trayIcon;
  bool doPopup = feed & m_s->popupFeeds();

  // If we don't do any notifications don't even bother...
  if (!doTrayIcon && !doPopup)
    return;

  // We highlight the tray icon and generate popups only on certain
  // actions, so we first need to filter the list of new activities.
  QList<QASActivity*> acts;

  CollectionWidget* cw =
    qobject_cast<CollectionWidget*>(m_notifyMap->mapping(feed));
  if (!cw) // if it wasn't a regular timeline we ignore it
    return;

  // We just keep posts, i.e. new notes or comments.
  // Other possibilities would be: follow favorite like
  QStringList keepVerbs;
  keepVerbs << "post";

  // Filter: keep only activities that have a verb in keepVerbs
  const QList<QASAbstractObject*>& ol = cw->newObjects();
  for (int i=0; i<ol.count(); ++i) {
    QASActivity* act = qobject_cast<QASActivity*>(ol.at(0));
    if (act && keepVerbs.contains(act->verb()))
      acts.push_back(act);
  }

  if (acts.isEmpty())
    return;

  // Highlight tray icon.
  if (doTrayIcon)
    m_trayIcon->setIcon(QIcon(":/images/pumpa_glow.png"));

  // Popup notifications.
  if (doPopup) {
    int actsCount = 0;
    for (int i=0; i<acts.size(); i++)
      if (!acts.at(i)->skipNotify())
	actsCount++;

    if (actsCount == 0)
      return;

    QString msg =
      QString(tr("You have %Ln new notification(s).", 0, actsCount));

    // If there's only a single post activity we'll make the
    // notification more informative.
    QASActivity* act = acts.at(0);
    QASObject* obj = act->object();
    QASActor* actor = act->actor();
    if (actsCount == 1 && act->verb() == "post" && obj && actor) {
      QString actorName = actor->displayNameOrWebFinger();
      if (obj->type() == "comment")
        msg = QString(tr("%1 commented: ")).arg(actorName);
      else 
        msg = QString(tr("%1 wrote: ")).arg(actorName);
      msg += "\"" + obj->excerpt() + "\"";
    }
    sendNotification(CLIENT_FANCY_NAME, msg);
  }
}

//------------------------------------------------------------------------------

void PumpApp::onNewMinorObjects() {
  CollectionWidget* cw = qobject_cast<CollectionWidget*>(sender());
  if (!cw)
    return;

  const QList<QASAbstractObject*>& newObjects = cw->newObjects();
  cw->url();

  for (int i=0; i<newObjects.size(); ++i) {
    QASActivity* act = qobject_cast<QASActivity*>(newObjects.at(i));
    if (act && act->object() && act->object()->inReplyTo()) {
      QASObject* irtObj = act->object()->inReplyTo();
      if (irtObj->url().isEmpty() || isShown(irtObj))
        refreshObject(irtObj);
    }
  }
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
  statusMessage(tr("Error: ") + msg);
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

    if (m_trayIcon) {
      QString toolTip = CLIENT_FANCY_NAME;
      if (!m_s->userName().isEmpty())
        toolTip += " - " + siteUrlToAccountId(m_s->userName(), m_s->siteUrl());
      m_trayIcon->setToolTip(toolTip);
    }
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
  // m_trayIconMenu->addAction(newPictureAction);
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
  return QString(tr("%1 &Window")).arg(visible ? tr("Hide") : tr("Show") );
}

//------------------------------------------------------------------------------

void PumpApp::toggleVisible() {
  setVisible(!isVisible());
  m_showHideAction->setText(showHideText());
  if (isVisible())
    activateWindow();
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

  loadOlderAction = new QAction(tr("Load older in timeline"), this);
  loadOlderAction->setShortcut(tr("Ctrl+O"));
  connect(loadOlderAction, SIGNAL(triggered()), this, SLOT(loadOlder()));

  followAction = new QAction(tr("F&ollow an account"), this);
  followAction->setShortcut(tr("Ctrl+L"));
  connect(followAction, SIGNAL(triggered()), this, SLOT(followDialog()));

  profileAction = new QAction(tr("Your &profile"), this);
  connect(profileAction, SIGNAL(triggered()), this, SLOT(editProfile()));

  aboutAction = new QAction(tr("&About"), this);
  connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));

  aboutQtAction = new QAction(tr("About &Qt"), this);
  connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

  reportBugAction = new QAction(tr("Report &bug online"), this);
  connect(reportBugAction, SIGNAL(triggered()), this, SLOT(reportBug()));

  newNoteAction = new QAction(tr("New &Note"), this);
  newNoteAction->setShortcut(tr("Ctrl+N"));
  connect(newNoteAction, SIGNAL(triggered()), this, SLOT(newNote()));

  m_debugAction = new QAction("Debug", this);
  m_debugAction->setShortcut(tr("Ctrl+D"));
  connect(m_debugAction, SIGNAL(triggered()), this, SLOT(debugAction()));
  addAction(m_debugAction);
  
  m_closeTabAction = new QAction(tr("Close tab"), this);
  m_closeTabAction->setShortcut(tr("Ctrl+W"));
  connect(m_closeTabAction, SIGNAL(triggered()), this, SLOT(closeTab()));
  m_closeTabAction->setEnabled(false);

  m_firehoseAction = new QAction(tr("Firehose"), this);
  connect(m_firehoseAction, SIGNAL(triggered()), this, SLOT(showFirehose()));

  m_followersAction = new QAction(tr("Followers"), this);
  connect(m_followersAction, SIGNAL(triggered()), this, SLOT(showFollowers()));

  m_followingAction = new QAction(tr("Following"), this);
  connect(m_followingAction, SIGNAL(triggered()), this, SLOT(showFollowing()));

  m_favouritesAction = new QAction(tr("Favorites"), this);
  connect(m_favouritesAction, SIGNAL(triggered()),
          this, SLOT(showFavourites()));

  m_userActivitiesAction = new QAction(tr("Activities"), this);
  connect(m_userActivitiesAction, SIGNAL(triggered()),
          this, SLOT(showUserActivities()));

  m_showHideAction = new QAction(showHideText(true), this);
  connect(m_showHideAction, SIGNAL(triggered()), this, SLOT(toggleVisible()));
}

//------------------------------------------------------------------------------

void PumpApp::createMenu() {
  fileMenu = new QMenu(tr("&Pumpa"), this);
  fileMenu->addAction(newNoteAction);
  fileMenu->addSeparator();
  fileMenu->addAction(followAction);
  fileMenu->addAction(profileAction);
  fileMenu->addAction(reloadAction);
  fileMenu->addAction(loadOlderAction);
  fileMenu->addSeparator();
  fileMenu->addAction(openPrefsAction);
  fileMenu->addSeparator();
  fileMenu->addAction(exitAction);
  menuBar()->addMenu(fileMenu);

  m_tabsMenu = new QMenu(tr("&Tabs"), this);
  m_tabsMenu->addAction(m_userActivitiesAction);
  m_tabsMenu->addAction(m_favouritesAction);
  m_tabsMenu->addAction(m_followersAction);
  m_tabsMenu->addAction(m_followingAction);
  m_tabsMenu->addAction(m_firehoseAction);
  m_tabsMenu->addAction(m_closeTabAction);
  menuBar()->addMenu(m_tabsMenu);

  helpMenu = new QMenu(tr("&Help"), this);
  helpMenu->addAction(aboutAction);
  helpMenu->addAction(aboutQtAction);
  helpMenu->addSeparator();
  helpMenu->addAction(reportBugAction);
  menuBar()->addMenu(helpMenu);
}

//------------------------------------------------------------------------------

void PumpApp::preferences() {
  m_settingsDialog->exec();
}

//------------------------------------------------------------------------------

void PumpApp::wizardCancelled() {
  qApp->setQuitOnLastWindowClosed(true);
  if (!haveOAuth())
    exit();
}

//------------------------------------------------------------------------------

void PumpApp::exit() {
  qApp->exit();
}

//------------------------------------------------------------------------------

void PumpApp::about() { 
  static const QString GPL = 
    tr("<p>Pumpa is free software: you can redistribute it and/or modify it "
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
       "</p>");
  static const QString credits = 
    tr("<p>The <a href=\"https://github.com/kypeli/kQOAuth\">kQOAuth library"
       "</a> is copyrighted by <a href=\"http://www.johanpaul.com/\">Johan "
       "Paul</a> and licensed under LGPL 2.1.</p>"
       "<p>The <a href=\"https://github.com/vmg/sundown\">sundown Markdown "
       "library</a> is copyrighted by Natacha Port&eacute;, Vicent Marti and "
       "others, and <a href=\"https://github.com/vmg/sundown#license\">"
       "permissively licensed</a>.</p>"
       "<p>The Pumpa logo was "
       "<a href=\"http://opengameart.org/content/fruit-and-veggie-inventory\">"
       "created by Joshua Taylor</a> for the "
       "<a href=\"http://lpc.opengameart.org/\">Liberated Pixel Cup</a>."
       "The logo is copyrighted by the artist and is dual licensed under the "
       "CC-BY-SA 3.0 license and the GNU GPL 3.0.");
  
  QString mainText = 
    QString("<p><b>%1 %2</b> - %3<br/><a href=\"%4\">%4</a><br/>"
            + tr("Copyright &copy; 2013-2015 Mats Sj&ouml;berg")
            + " - <a href=\"https://pump.saz.im/sazius\">sazius@pump.saz.im</a>."
            "</p>"
            + tr("<p>Report bugs and feature requests at "
                 "<a href=\"%5\">%5</a>.</p>"))
    .arg(CLIENT_FANCY_NAME)
    .arg(CLIENT_VERSION)
    .arg(tr("A simple Qt-based pump.io client."))
    .arg(WEBSITE_URL)
    .arg(BUGTRACKER_URL);
  
  QMessageBox::about(this, QString(tr("About %1")).arg(CLIENT_FANCY_NAME),
                     mainText + GPL + credits);
}

//------------------------------------------------------------------------------

void PumpApp::reportBug() {
  QDesktopServices::openUrl(QString(BUGTRACKER_URL));
}

//------------------------------------------------------------------------------

void PumpApp::addPublicRecipient(RecipientList& rl) {
  QVariantMap publicJson;
  publicJson["displayName"] = tr("Public");
  publicJson["objectType"] = "collection";
  publicJson["id"] = PUBLIC_RECIPIENT_ID;
  rl.append(QASObject::getObject(publicJson, this));
}

//------------------------------------------------------------------------------

void PumpApp::newNote(QASObject* obj, QASObjectList* to, QASObjectList* cc,
		      bool edit) {
  if (!m_messageWindow) {
    m_messageWindow = new MessageWindow(m_s, &m_recipientLists, this);
    connect(m_messageWindow,
            SIGNAL(sendMessage(QString, QString, RecipientList, RecipientList)),
            this,
            SLOT(postNote(QString, QString, RecipientList, RecipientList)));
    connect(m_messageWindow, SIGNAL(sendImage(QString, QString, QString,
                                              RecipientList, RecipientList)),
            this, SLOT(postImage(QString, QString, QString,
                                 RecipientList, RecipientList)));
    connect(m_messageWindow, SIGNAL(sendReply(QASObject*, QString,
                                              RecipientList, RecipientList)),
            this, SLOT(postReply(QASObject*, QString,
                                 RecipientList, RecipientList)));
    connect(m_messageWindow, SIGNAL(sendEdit(QASObject*, QString, QString)),
	    this, SLOT(postEdit(QASObject*, QString, QString)));
    m_messageWindow->setCompletions(&m_completions);
  }
  if (edit)
    m_messageWindow->editMessage(obj);
  else
    m_messageWindow->newMessage(obj, to, cc);
  m_messageWindow->show();
}

//------------------------------------------------------------------------------

void PumpApp::reload() {
  fetchAll(true);
  refreshTimeLabels();
}

//------------------------------------------------------------------------------

void PumpApp::fetchAll(bool all) {
  m_inboxWidget->fetchNewer();
  if (m_inboxWidget->linksInitialised())
    m_inboxWidget->refresh();
  m_directMinorWidget->fetchNewer();
  m_directMajorWidget->fetchNewer();
  m_inboxMinorWidget->fetchNewer();

  if (tabShown(m_firehoseWidget))
    m_firehoseWidget->fetchNewer();

  for (int i=0; i<m_contextWidgets.size(); ++i)
    m_contextWidgets[i]->fetchNewer();

  // These will be reloaded even if not shown, if all=true
  if (all || tabShown(m_followersWidget))
    m_followersWidget->fetchNewer();
  if (all || tabShown(m_followingWidget))
    m_followingWidget->fetchNewer();
  if (all || tabShown(m_favouritesWidget))
    m_favouritesWidget->fetchNewer();
  if (all || tabShown(m_userActivitiesWidget))
    m_userActivitiesWidget->fetchNewer();
}

//------------------------------------------------------------------------------

void PumpApp::loadOlder() {
  ASWidget* cw = 
    qobject_cast<ASWidget*>(m_tabWidget->currentWidget());
  if (cw)
    cw->fetchOlder();
}

//------------------------------------------------------------------------------

bool PumpApp::isShown(QASAbstractObject* obj) {
  // check context widgets first
  for (int i=0; i<m_contextWidgets.size(); ++i)
    if (m_contextWidgets[i]->hasObject(obj))
      return true;

  // check all other tab widgets
  return m_inboxWidget->hasObject(obj) ||
    m_directMinorWidget->hasObject(obj) ||
    m_directMajorWidget->hasObject(obj) ||
    m_inboxMinorWidget->hasObject(obj) ||
    (tabShown(m_firehoseWidget) && m_firehoseWidget->hasObject(obj)) ||
    (tabShown(m_followersWidget) && m_followersWidget->hasObject(obj)) ||
    (tabShown(m_followingWidget) && m_followingWidget->hasObject(obj)) ||
    (tabShown(m_favouritesWidget) && m_favouritesWidget->hasObject(obj)) ||
    (tabShown(m_userActivitiesWidget) && 
     m_userActivitiesWidget->hasObject(obj));
}

//------------------------------------------------------------------------------

QString PumpApp::inboxEndpoint(QString path) {
  if (m_s->siteUrl().isEmpty()) {
    errorMessage(tr("Site not configured yet!"));
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

void PumpApp::errorBox(QString msg) {
  QMessageBox::critical(this, CLIENT_FANCY_NAME, msg, QMessageBox::Ok);
}

//------------------------------------------------------------------------------

bool PumpApp::webFingerFromString(QString text, QString& username,
                                  QString& server) {
  if (text.startsWith("https://") || text.startsWith("http://")) {
    int slashPos = text.lastIndexOf('/');
    if (slashPos > 0)
      text = siteUrlToAccountId(text.mid(slashPos+1), text.left(slashPos));
  }

  return splitWebfingerId(text, username, server);
}

//------------------------------------------------------------------------------

void PumpApp::followDialog() {
  bool ok;

  QString defaultText = "evan@e14n.com";
  QString cbText = QApplication::clipboard()->text();
  if (cbText.contains('@') || cbText.startsWith("https://") ||
      cbText.startsWith("http://")) 
    defaultText = cbText;

  QString text =
    QInputDialog::getText(this, tr("Follow pump.io user"),
                          tr("Enter webfinger ID of person to follow: "),
                          QLineEdit::Normal, defaultText, &ok);

  if (!ok || text.isEmpty())
    return;

  QString username, server;
  QString error;

  if (!webFingerFromString(text, username, server))
    error = tr("Sorry, that doesn't even look like a webfinger ID!");

  QASObject* obj = QASObject::getObject("acct:" + username + "@" + server);
  QASActor* actor = obj ? obj->asActor() : NULL;
  if (actor && actor->followed())
    error = tr("Sorry, you are already following that person!");

  if (!error.isEmpty())
    return errorBox(error);

  testUserAndFollow(username, server);
}

//------------------------------------------------------------------------------

void PumpApp::editProfile() {
  request(apiUser("profile"), QAS_EDIT_PROFILE);
}

//------------------------------------------------------------------------------

void PumpApp::editProfileDialog() {
  if (!m_editProfileDialog) {
    m_editProfileDialog = new EditProfileDialog(this);
    connect(m_editProfileDialog, SIGNAL(profileEdited(QASActor*, QString)), 
            this, SLOT(onProfileEdited(QASActor*, QString)));
  }
  m_editProfileDialog->setProfile(m_selfActor);
  m_editProfileDialog->show();
}

//------------------------------------------------------------------------------

void PumpApp::onProfileEdited(QASActor* profile, QString newImageFile) {
  m_profile.clear();

  m_profile["objectType"] = "person";
  m_profile["displayName"] = profile->displayName();
  m_profile["summary"] = profile->summary();
  
  QVariantMap jsonLoc;
  jsonLoc["objectType"] = "place";
  jsonLoc["displayName"] = profile->location();
  m_profile["location"] = jsonLoc;

  if (newImageFile.isEmpty()) {
    uploadProfile();
  } else {
    postAvatarImage(newImageFile);
  }
}

//------------------------------------------------------------------------------

void PumpApp::uploadProfile() {
  request(apiUser("profile"), QAS_SELF_PROFILE | QAS_REFRESH, 
          KQOAuthRequest::PUT, m_profile);
}

//------------------------------------------------------------------------------

void PumpApp::testUserAndFollow(QString username, QString server) {
  QString fingerUrl = QString("%1/.well-known/webfinger?resource=%2@%1").
    arg(server).arg(username);

  QNetworkRequest rec(QUrl("https://" + fingerUrl));
  QNetworkReply* reply = m_nam->head(rec);
  connect(reply, SIGNAL(finished()), this, SLOT(userTestDoneAndFollow()));
  qDebug() << "testUserAndFollow" << fingerUrl;
  
  // isn't this an ugly yet fancy hack? :-)
  reply->setProperty("pumpa_redirects", 0);
}

//------------------------------------------------------------------------------

void PumpApp::userTestDoneAndFollow() {
  QString error;

  QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
  QUrl url = reply->url();

#ifdef QT5
  QUrlQuery replyQuery(url.query());
  QString userId = replyQuery.queryItemValue("resource");
#else
  QString userId = url.queryItemValue("resource");
#endif

  int redirs = reply->property("pumpa_redirects").toInt();
#ifdef DEBUG_NET
  qDebug() << "userTestDoneAndFollow" << url << redirs;
#endif

  if (reply->error() != QNetworkReply::NoError) {
    if (redirs == 0) {
      url.setScheme("http");
      QNetworkRequest rec(url);
      QNetworkReply* r = m_nam->head(rec);
      r->setProperty("pumpa_redirects", ++redirs);
      connect(r, SIGNAL(finished()), this, SLOT(userTestDoneAndFollow()));
      return;
    } else {
      return errorBox(tr("Invalid user: ") + userId);
    }
  }

  QUrl loc = reply->header(QNetworkRequest::LocationHeader).toUrl();
  if (loc.isValid()) {
    if (redirs > 5)
      return errorBox(tr("Invalid user (cannot check site): ") + userId);
    reply->deleteLater();
    
    QNetworkRequest rec(loc);
    QNetworkReply* r = m_nam->head(rec);
    r->setProperty("pumpa_redirects", ++redirs);
    connect(r, SIGNAL(finished()), this, SLOT(userTestDoneAndFollow()));
    return;
  }
  
  follow("acct:" + userId, true);
}

//------------------------------------------------------------------------------

bool PumpApp::tabShown(ASWidget* aw) const {
  return aw && m_tabWidget->indexOf(aw) != -1;
}

//------------------------------------------------------------------------------

void PumpApp::onShowContext(QASObject* obj) {
  ContextWidget* cw = new ContextWidget(this);
  connectCollection(cw);

  m_tabWidget->addTab(cw, tr("&Context"), true, true);
  cw->setObject(obj);
  m_tabWidget->setCurrentWidget(cw);

  m_contextWidgets.append(cw);
}

//------------------------------------------------------------------------------

void PumpApp::closeTab() {
  ContextWidget* cw = 
    qobject_cast<ContextWidget*>(m_tabWidget->closeCurrentTab());
  if (cw) {
    int i = m_contextWidgets.indexOf(cw);
    if (i != -1)
      m_contextWidgets.removeAt(i);
    delete cw;
  }
}

//------------------------------------------------------------------------------

void PumpApp::showFirehose() {
  if (!tabShown(m_firehoseWidget))
    m_tabWidget->addTab(m_firehoseWidget, tr("Fi&rehose"), true, true);
  m_tabWidget->setCurrentWidget(m_firehoseWidget);
  m_firehoseWidget->fetchNewer();
}

//------------------------------------------------------------------------------

void PumpApp::showFollowers() {
  if (!tabShown(m_followersWidget))
    m_tabWidget->addTab(m_followersWidget, tr("&Followers"), true, true);
  m_tabWidget->setCurrentWidget(m_followersWidget);
  m_followersWidget->fetchNewer();
}

//------------------------------------------------------------------------------

void PumpApp::showFollowing() {
  if (!tabShown(m_followingWidget))
    m_tabWidget->addTab(m_followingWidget, tr("F&ollowing"), false, true);
  m_tabWidget->setCurrentWidget(m_followingWidget);
  m_followingWidget->fetchNewer();
}

//------------------------------------------------------------------------------

void PumpApp::showFavourites() {
  if (!tabShown(m_favouritesWidget))
    m_tabWidget->addTab(m_favouritesWidget, tr("F&avorites"), false, true);
  m_tabWidget->setCurrentWidget(m_favouritesWidget);
  m_favouritesWidget->fetchNewer();
}

//------------------------------------------------------------------------------

void PumpApp::showUserActivities() {
  if (!tabShown(m_userActivitiesWidget))
    m_tabWidget->addTab(m_userActivitiesWidget, tr("A&ctivities"), false, true);
  m_tabWidget->setCurrentWidget(m_userActivitiesWidget);
  m_userActivitiesWidget->fetchNewer();
}

//------------------------------------------------------------------------------

void PumpApp::postNote(QString content, QString title,
                       RecipientList to, RecipientList cc) {
  if (content.isEmpty())
    return;

  QVariantMap obj;
  obj["objectType"] = "note";
  obj["content"] = addTextMarkup(content, m_s->useMarkdown());

  QString ptitle = processTitle(title, false);
  if (!ptitle.isEmpty())
    obj["displayName"] = ptitle;

  feed("post", obj, QAS_OBJECT | QAS_REFRESH | QAS_POST, to, cc);
}

//------------------------------------------------------------------------------

void PumpApp::postEdit(QASObject* obj, QString content, QString title) {
  QVariantMap json;
  json["id"] = obj->id();
  json["objectType"] = obj->type();
  json["content"] = addTextMarkup(content, m_s->useMarkdown());

  QString ptitle = processTitle(title, false);
  if (!ptitle.isEmpty())
    json["displayName"] = ptitle;

  feed("update", json, QAS_OBJECT | QAS_REFRESH | QAS_POST);
}

//------------------------------------------------------------------------------

void PumpApp::postImage(QString msg,
                        QString title,
                        QString imageFile,
                        RecipientList to,
                        RecipientList cc) {
  m_imageObject.clear();
  m_imageObject["content"] = addTextMarkup(msg, m_s->useMarkdown());
  m_imageObject["displayName"] = processTitle(title, false);

  m_imageTo = to;
  m_imageCc = cc;

  uploadFile(imageFile);
}

//------------------------------------------------------------------------------

void PumpApp::postAvatarImage(QString imageFile) {
  m_imageObject.clear();

  m_imageTo = RecipientList();
  m_imageCc = RecipientList();

  addPublicRecipient(m_imageCc);

  uploadFile(imageFile, QAS_AVATAR_UPLOAD);
}

//------------------------------------------------------------------------------

void PumpApp::uploadFile(QString filename, int flags) {
  QString lcfn = filename.toLower();
  QString mimeType;
  if (lcfn.endsWith(".jpg") || lcfn.endsWith(".jpeg"))
    mimeType = "image/jpeg";
  else if (lcfn.endsWith(".png"))
    mimeType = "image/png";
  else if (lcfn.endsWith(".gif"))
    mimeType = "image/gif";
  else {
    qDebug() << "Cannot determine mime type of file" << filename;
    return;
  }

  QFile fp(filename);
  if (!fp.open(QIODevice::ReadOnly)) {
    qDebug() << "Unable to read file" << filename;
      return;
  }

  QByteArray ba = fp.readAll();
  
  KQOAuthRequest* oaRequest = initRequest(apiUrl(apiUser("uploads")),
                                          KQOAuthRequest::POST);
  oaRequest->setContentType(mimeType);
  oaRequest->setContentLength(ba.size());
  oaRequest->setRawData(ba);

  if (m_uploadDialog == NULL) {
    m_uploadDialog = new QProgressDialog("Uploading image...", "Abort", 0, 100,
                                         this);
    m_uploadDialog->setWindowModality(Qt::WindowModal);
    connect(m_uploadDialog, SIGNAL(canceled()), this, SLOT(uploadCanceled()));
  } else {
    m_uploadDialog->reset();
  }
  m_uploadDialog->setValue(0);
  m_uploadDialog->show();

  flags = QAS_IMAGE_UPLOAD | flags;
  m_uploadRequest = executeRequest(oaRequest, flags);
  connect(m_uploadRequest, SIGNAL(uploadProgress(qint64, qint64)),
          this, SLOT(uploadProgress(qint64, qint64)));
}

//------------------------------------------------------------------------------

void PumpApp::updatePostedImage(QVariantMap obj, int flags) {
  m_imageObject.unite(obj);

  // Work-around for https://github.com/e14n/pump.io/issues/885
  // Thanks to Owen Shepherd for pointing this out!
  RecipientList to;
  to.append(m_selfActor);

  feed("update", m_imageObject, QAS_IMAGE_UPDATE | flags, to);
}

//------------------------------------------------------------------------------

void PumpApp::postImageActivity(QVariantMap, int flags) {
  if (flags == 0)
    flags = QAS_REFRESH;

  flags |= QAS_ACTIVITY | QAS_POST;
  
  feed("post", m_imageObject, flags, m_imageTo, m_imageCc);
}

//------------------------------------------------------------------------------

void PumpApp::uploadProgress(qint64 bytesSent, qint64 bytesTotal) {
  if (!m_uploadDialog || bytesTotal <= 0)
    return;

  m_uploadDialog->setValue((100*bytesSent)/bytesTotal);
}

//------------------------------------------------------------------------------

void PumpApp::uploadCanceled(bool abortRequest) {
  if (m_uploadRequest && abortRequest) {
#ifdef DEBUG_NET
    qDebug() << "[DEBUG] aborting upload...";
#endif
    m_uploadRequest->abort();
  }
  m_uploadRequest = NULL;

  m_imageObject.clear();
  m_uploadDialog->reset();
}

//------------------------------------------------------------------------------

void PumpApp::postReply(QASObject* replyToObj, QString content,
                        RecipientList to, RecipientList cc) {
  if (content.isEmpty())
    return;

  QVariantMap obj;
  obj["objectType"] = "comment";
  obj["content"] = addTextMarkup(content, m_s->useMarkdown());

  QVariantMap noteObj;
  noteObj["id"] = replyToObj->id();
  noteObj["objectType"] = replyToObj->type();
  obj["inReplyTo"] = noteObj;

  feed("post", obj, QAS_ACTIVITY | QAS_REFRESH | QAS_POST, to, cc);
}

//------------------------------------------------------------------------------

void PumpApp::follow(QString acctId, bool follow) {
  QVariantMap obj;
  obj["id"] = acctId;
  obj["objectType"] = "person";

  int mode = QAS_ACTIVITY;
  if (follow)
    mode |= QAS_FOLLOW;
  else
    mode |= QAS_UNFOLLOW;

  feed(follow ? "follow" : "stop-following", obj, mode);
}

//------------------------------------------------------------------------------

void PumpApp::onDeleteObject(QASObject* obj) {
  QVariantMap json;
  json["id"] = obj->id();
  json["objectType"] = obj->type();

  feed("delete", json, QAS_ACTIVITY);
}

//------------------------------------------------------------------------------

void PumpApp::onEditObject(QASObject* obj) {
  newNote(obj, NULL, NULL, true);
}

//------------------------------------------------------------------------------

void PumpApp::addRecipient(QVariantMap& data, QString name, RecipientList to) {
  if (to.isEmpty())
    return;

  QVariantList recList;

  for (int i=0; i<to.size(); ++i) {
    QASObject* obj = to.at(i);

    QVariantMap rec;
    rec["objectType"] = obj->type();
    rec["id"] = obj->id();
    if (!obj->proxyUrl().isEmpty()) {
      QVariantMap pump_io;
      pump_io["proxyURL"] = obj->proxyUrl();
      rec["pump_io"] = pump_io;
    }

    recList.append(rec);
  }

  data[name] = recList;
}

//------------------------------------------------------------------------------

void PumpApp::feed(QString verb, QVariantMap object, int response_id,
                   RecipientList to, RecipientList cc) {
  QString endpoint = "api/user/" + m_s->userName() + "/feed";

  QVariantMap data;
  data["verb"] = verb;
  data["object"] = object;

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

QString PumpApp::apiUser(QString path) {
  return QString("api/user/%1/%2").arg(m_s->userName()).arg(path);
}

//------------------------------------------------------------------------------

KQOAuthRequest* PumpApp::initRequest(QString endpoint,
                                     KQOAuthRequest::RequestHttpMethod method) {
  KQOAuthRequest* oaRequest = new KQOAuthRequest(this);
  oaRequest->initRequest(KQOAuthRequest::AuthorizedRequest, QUrl(endpoint));
  oaRequest->setConsumerKey(m_s->clientId());
  oaRequest->setConsumerSecretKey(m_s->clientSecret());
  oaRequest->setToken(m_s->token());
  oaRequest->setTokenSecret(m_s->tokenSecret());
  oaRequest->setHttpMethod(method); 
  oaRequest->setTimeout(60000); // one minute time-out
  return oaRequest;
}

//------------------------------------------------------------------------------

void PumpApp::request(QString endpoint, int response_id,
                      KQOAuthRequest::RequestHttpMethod method,
                      QVariantMap data) {
  endpoint = apiUrl(endpoint);

  bool firehose = (endpoint == m_s->firehoseUrl());
  if (!endpoint.startsWith(m_s->siteUrl()) && !firehose) {
#ifdef DEBUG_NET
    qDebug() << "[DEBUG] dropping request for" << endpoint;
#endif
    return;
  }

#ifdef DEBUG_NET
  qDebug() << (method == KQOAuthRequest::GET  ? "[GET]" :
               method == KQOAuthRequest::POST ? "[POST]" : "[PUT]") 
           << response_id << ":" << endpoint;
#endif

  QStringList epl = endpoint.split("?");
  KQOAuthRequest* oaRequest = initRequest(epl[0], method);

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
  
  if (method == KQOAuthRequest::POST || method == KQOAuthRequest::PUT) {
    QByteArray ba = serializeJson(data);
    oaRequest->setRawData(ba);
    oaRequest->setContentType("application/json");
    oaRequest->setContentLength(ba.size());
#ifdef DEBUG_NET
    qDebug() << "DATA" << oaRequest->rawData();
#endif
  }

  executeRequest(oaRequest, response_id);

  if (!m_isLoading)
    notifyMessage(tr("Loading ..."));
  setLoading(true);
}

//------------------------------------------------------------------------------

QNetworkReply* PumpApp::executeRequest(KQOAuthRequest* request,
                                       int response_id) {
  int id = m_nextRequestId++;

  if (m_nextRequestId > 32000) { // bound to be smaller than any MAX_INT
    m_nextRequestId = 0;
    while (m_requestMap.contains(m_nextRequestId))
      m_nextRequestId++;
  }

  m_requestMap.insert(id, qMakePair(request, response_id));
  m_oam->executeAuthorizedRequest(request, id);

  return m_oam->getReply(request);
}

//------------------------------------------------------------------------------

void PumpApp::followActor(QASActor* actor, bool doFollow) {
  actor->setFollowed(doFollow);

  QString from = QString("%1 (%2)").arg(actor->displayName()).
    arg(actor->webFinger());

  if (from.isEmpty() || from.startsWith("http://") ||
      from.startsWith("https://"))
    return;

  if (doFollow)
    m_completions.insert(from, actor);
  else
    m_completions.remove(from);
}    

//------------------------------------------------------------------------------

void PumpApp::onAuthorizedRequestReady(QByteArray response, int rid) {
  KQOAuthManager::KQOAuthError lastError = m_oam->lastError();

  QPair<KQOAuthRequest*, int> rp = m_requestMap.take(rid);
  KQOAuthRequest* request = rp.first;
  int id = rp.second;
  QString reqUrl = request->requestEndpoint().toString();

#ifdef DEBUG_NET_MOAR
  qDebug() << "[DEBUG] request done [" << rid << id << "]" << reqUrl
           << response.count() << "bytes";
#endif
#ifdef DEBUG_NET_EVEN_MOAR
  qDebug() << "[DEBUG]" << response;
#endif

  request->deleteLater();

  if (m_requestMap.isEmpty()) {
    setLoading(false);
    notifyMessage(tr("Ready!"));
  } 
#ifdef DEBUG_NET_MOAR
  else {
    qDebug() << "[DEBUG] Still waiting for requests:";
    QMapIterator<int, requestInfo_t> i(m_requestMap);
    while (i.hasNext()) {
      i.next();
      requestInfo_t ri = i.value();
      qDebug() << "   " << ri.first->requestEndpoint() << ri.second;
    }    
  }
#endif

  int sid = id & 0xFF;

  if (lastError) {
    if (id & QAS_POST) {
      errorMessage(tr("Unable to post message!"));
      m_messageWindow->show();
    } else if (sid == QAS_IMAGE_UPLOAD) {
      uploadCanceled(false);
      errorMessage(tr("Unable to upload image!"));
    } else if (sid == QAS_OBJECT) {
      qDebug() << "[WARNING] unable to fetch context for object.";
    } else {
      errorMessage(QString(tr("Network or authorisation error [%1/%2] %3.")).
                   arg(m_oam->lastError()).arg(id).arg(reqUrl));
    }
#ifdef DEBUG_NET
    qDebug() << "[ERROR]" << response;
#endif
    return;
  }

  QVariantMap json = parseJson(response);
  if (sid == QAS_NULL)
    return;

  if (sid == QAS_COLLECTION) {
    QASCollection::getCollection(json, this, id);
  } else if (sid == QAS_ACTIVITY) {
    QASActivity* act = QASActivity::getActivity(json, this);
    if (act) { // if not a broken activity
      QASObject* obj = act->object();

      if ((id & QAS_AVATAR_UPLOAD) && obj) {

        QVariantMap jsonImage;
        jsonImage["url"] = obj->imageUrl();
        jsonImage["width"] = 96;
        jsonImage["height"] = 96;
        m_profile["image"] = jsonImage;

        uploadProfile();
      }

      if ((id & QAS_TOGGLE_LIKE) && obj)
	obj->toggleLiked();

      if ((id & QAS_FOLLOW) || (id & QAS_UNFOLLOW)) {
	QASActor* actor = obj ? obj->asActor() : NULL;
	if (actor) {
	  bool doFollow = (id & QAS_FOLLOW);
	  followActor(actor, doFollow);
	  notifyMessage(QString(doFollow ? tr("Successfully followed ") :
				tr("Successfully unfollowed ")) +
			actor->displayNameOrWebFinger());
	}
      }
    }
  } else if (sid == QAS_OBJECTLIST) {
    QASObjectList* ol = QASObjectList::getObjectList(json, this, id);
    if (ol && (id & QAS_FOLLOW)) {
      for (size_t i=0; i<ol->size(); ++i) {
        QASActor* actor = ol->at(i)->asActor();
        if (actor)
          followActor(actor);
      }

      if (ol->nextLink().isEmpty())
        QASActor::setFollowedKnown();
    }
  } else if (sid == QAS_OBJECT) {
    QASObject::getObject(json, this);
  } else if (sid == QAS_ACTORLIST) {
    QASActorList::getActorList(json, this);
  } else if (sid == QAS_SELF_PROFILE || sid == QAS_EDIT_PROFILE) {
    m_selfActor = QASActor::getActor(json, this);
    m_selfActor->setYou();
    if (sid == QAS_EDIT_PROFILE)
      editProfileDialog();
  } else if (sid == QAS_SELF_LISTS) {
    QASObjectList* lists = QASObjectList::getObjectList(json, this, id);
    for (size_t i=0; i<lists->size(); ++i) {
      m_recipientLists.append(lists->at(i));
    }
  } else if (sid == QAS_IMAGE_UPLOAD) {
    m_uploadDialog->reset();
    updatePostedImage(json, id & QAS_AVATAR_UPLOAD);
  } else if (sid == QAS_IMAGE_UPDATE) {
    postImageActivity(json, id & QAS_AVATAR_UPLOAD);
  }

  if ((id & QAS_POST) && m_messageWindow && !m_messageWindow->isVisible())
    m_messageWindow->clear();

  if (id & QAS_REFRESH) { 
    fetchAll(false);
  }
}

//------------------------------------------------------------------------------
// FIXME: this shouldn't be implemented in millions of places

void PumpApp::refreshObject(QASAbstractObject* obj) {
  if (!obj)
    return;
  
  QDateTime now = QDateTime::currentDateTime();
  QDateTime lr = obj->lastRefreshed();

  if (lr.isNull() || lr.secsTo(now) > 10) {
    obj->lastRefreshed(now);
    request(obj->apiLink(), obj->asType());
  }
}
 
//------------------------------------------------------------------------------

void PumpApp::setLoading(bool on) {
  if (!m_loadIcon || m_isLoading == on)
    return;

  m_isLoading = on;

  if (!on) {
    m_loadIcon->setMovie(NULL);
    m_loadIcon->setPixmap(QPixmap(":/images/empty.gif"));
  } else if (m_loadMovie->isValid()) {
    // m_loadIcon->setPixmap(QPixmap());
    m_loadIcon->setMovie(m_loadMovie);
    m_loadMovie->start();
  }
}
