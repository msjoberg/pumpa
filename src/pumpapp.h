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

#ifndef _PUMPAPP_H_
#define _PUMPAPP_H_

#include <QObject>
#include <QDebug>
#include <QDesktopServices>
#include <QApplication>
#include <QMainWindow>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QProgressDialog>
#include <QMovie>
#include <QSslError>

#ifdef USE_DBUS
#include <QDBusInterface>
#endif

#include "QtKOAuth"

#include "pumpa_defines.h"
#include "qactivitystreams.h"
#include "collectionwidget.h"
#include "oauthwizard.h"
#include "tabwidget.h"
#include "pumpasettingsdialog.h"
#include "pumpasettings.h"
#include "contextwidget.h"
#include "objectlistwidget.h"
#include "messagewindow.h"
#include "filedownloader.h"
#include "editprofiledialog.h"

//------------------------------------------------------------------------------

class PumpApp : public QMainWindow {
  Q_OBJECT

public:
  PumpApp(PumpaSettings* settings, QString locale="", QWidget* parent=0);
  virtual ~PumpApp();                            

signals:
  void userAuthorizationStarted();
                    
private slots:
  void onSslErrors(QNetworkReply*, QList<QSslError>);

  void userTestDoneAndFollow();

  void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void updateTrayIcon();
  void toggleVisible();
  void timelineHighlighted(int);
  void onNewMinorObjects();

  void followDialog();
  void editProfile();
  void editProfileDialog();
  void onLike(QASObject* obj);
  void onShare(QASObject* obj);
  void postNote(QString note, QString title,
                RecipientList to, RecipientList cc);
  void postImage(QString msg, QString title, QString imageFile,
                 RecipientList to, RecipientList cc);
  void postAvatarImage(QString imageFile);
  void postReply(QASObject* replyToObj, QString content,
                 RecipientList to, RecipientList cc);
  void postEdit(QASObject* obj, QString content, QString title);
  void follow(QString acctId, bool follow);
  void onDeleteObject(QASObject* obj);
  void onEditObject(QASObject* obj);
  void onProfileEdited(QASActor* profile, QString newImageFile);

  void errorMessage(QString msg);
  void notifyMessage(QString msg);
  void statusMessage(const QString& msg);
  void onShowContext(QASObject*);
  void tabSelected(int index);

  void onClientRegistered(QString, QString, QString, QString);
  void onAccessTokenReceived(QString token, QString tokenSecret);

  void onAuthorizedRequestReady(QByteArray response, int id);

  void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
  void uploadCanceled(bool abortRequest=true);
  
  void request(QString endpoint, int response_id,
               KQOAuthRequest::RequestHttpMethod method = KQOAuthRequest::GET,
               QVariantMap data=QVariantMap());

  void exit();
  void about();
  void reportBug();
  void preferences();
  void newNote(QASObject* obj = NULL, QASObjectList* to = NULL,
               QASObjectList* cc = NULL, bool edit = false);
  void reload();
  void loadOlder();

  void startPumping();

  void launchOAuthWizard();
  void wizardCancelled();

  void debugAction();

  void showFollowers();
  void showFollowing();
  void showFavourites();
  void showUserActivities();
  void showFirehose();
  void closeTab();

protected:
  void timerEvent(QTimerEvent*);
  virtual bool event(QEvent* e) {
    if (e->type() == QEvent::WindowActivate)
      resetNotifications();
    return QMainWindow::event(e);
  }
  void closeEvent(QCloseEvent* e) {
    m_showHideAction->setText(showHideText(false));
    QMainWindow::closeEvent(e);
  }

private:
  void addPublicRecipient(RecipientList& rl);

  void uploadProfile();

  bool tabShown(ASWidget* aw) const;

  bool isShown(QASAbstractObject* obj);

  KQOAuthRequest* initRequest(QString endpoint,
                              KQOAuthRequest::RequestHttpMethod method);
  QNetworkReply* executeRequest(KQOAuthRequest* request, int response_id);

  typedef QPair<KQOAuthRequest*, int> requestInfo_t;
  QMap<int, requestInfo_t> m_requestMap;
  int m_nextRequestId;

  void setLoading(bool on);
  void refreshObject(QASAbstractObject* obj);

  void uploadFile(QString filename, int flags=0);

  void updatePostedImage(QVariantMap obj, int flags=0);
  void postImageActivity(QVariantMap obj, int flags=0);

  void errorBox(QString msg);

  bool webFingerFromString(QString text, QString& username, QString& server);

  void testUserAndFollow(QString username, QString server);

  QString apiUrl(QString endpoint);

  // constructs api/user/$username/$path
  QString apiUser(QString path);

  void addRecipient(QVariantMap& data, QString name, RecipientList to);

  void resetNotifications();

  void createTrayIcon();
  QString showHideText(bool);
  QString showHideText() { return showHideText(isVisible()); }

  void connectCollection(ASWidget* w, bool highlight=true);

  bool haveOAuth();

  void resetTimer();

  void refreshTimeLabels();

  void fetchAll(bool);
  QString inboxEndpoint(QString path);

  void feed(QString verb, QVariantMap object, int response_id,
            RecipientList to = RecipientList(),
            RecipientList cc = RecipientList());

  bool sendNotification(QString summary, QString text);
  
  PumpaSettingsDialog* m_settingsDialog;
  PumpaSettings* m_s;

  void followActor(QASActor* actor, bool doFollow=true);
  void addCompletion(QString from, QString to, bool add);

  void createActions();
  void createMenu();

  QAction* newNoteAction;
  // QAction* newPictureAction;
  QAction* reloadAction;
  QAction* followAction;
  QAction* profileAction;
  QAction* loadOlderAction;
  QAction* openPrefsAction;
  QAction* exitAction;
  QMenu* fileMenu;

  QAction* aboutAction;
  QAction* aboutQtAction;
  QAction* reportBugAction;
  QMenu* helpMenu;

  QAction* m_followersAction;
  QAction* m_followingAction;
  QAction* m_favouritesAction;
  QAction* m_userActivitiesAction;
  QAction* m_firehoseAction;
  QAction* m_closeTabAction;
  QMenu* m_tabsMenu;

  QAction* m_debugAction;

  KQOAuthManager *m_oam;

  FileDownloadManager* m_fdm;

  TabWidget* m_tabWidget;
  CollectionWidget* m_inboxWidget;
  CollectionWidget* m_directMajorWidget;
  CollectionWidget* m_directMinorWidget;
  CollectionWidget* m_inboxMinorWidget;
  CollectionWidget* m_firehoseWidget;
  QList<ContextWidget*> m_contextWidgets;
  ObjectListWidget* m_followersWidget;
  ObjectListWidget* m_followingWidget;
  ObjectListWidget* m_favouritesWidget;
  CollectionWidget* m_userActivitiesWidget;

  QLabel* m_loadIcon;
  QMovie* m_loadMovie;
  bool m_isLoading;

  QASActor* m_selfActor;

  OAuthWizard* m_wiz;

  MessageWindow* m_messageWindow;

  EditProfileDialog* m_editProfileDialog;

  QSystemTrayIcon* m_trayIcon;
  QMenu* m_trayIconMenu;
  QAction* m_showHideAction;

  QString m_locale;

  int m_timerId;
  int m_timerCount;

  QVariantMap m_imageObject;
  RecipientList m_imageTo;
  RecipientList m_imageCc;
  QVariantMap m_profile;

  RecipientList m_recipientLists;

  MessageEdit::completion_t m_completions;

  QProgressDialog* m_uploadDialog;
  QNetworkReply* m_uploadRequest;

  QNetworkAccessManager* m_nam;

  QSignalMapper* m_notifyMap;
#ifdef USE_DBUS
  QDBusInterface* m_dbus;
#endif
};

#endif /* _PUMPAPP_H_ */
