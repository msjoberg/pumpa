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

#ifndef _ACTORWIDGET_H_
#define _ACTORWIDGET_H_

#include <QWidget>
#include <QToolButton>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>

#include "qactivitystreams.h"

//------------------------------------------------------------------------------

class ActorWidget : public QToolButton {
  Q_OBJECT
public:
  ActorWidget(QASActor* a, QWidget* parent = 0, bool small=false);
  virtual ~ActorWidget();

  void setActor(QASActor* a);

signals:
  void follow(QString, bool);
  void lessClicked();
  void moreClicked();

public slots:
  void onImageChanged();
  void updatePixmap();
  void updateMenu();

private slots:
  void onFollowAuthor();
  void onHideAuthor();
  void onMenuTitle();

private:
  void createMenu();

  QASActor* m_actor;
  QString m_url;
  QString m_localFile;

  QMenu* m_menu;
  QAction* m_menuTitleAction;
  QAction* m_followAction;
  QAction* m_hideAuthorAction;
};

#endif /* _ACTORWIDGET_H_ */
