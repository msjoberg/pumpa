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

#include "actorwidget.h"
#include "filedownloader.h"

#include <QMessageBox>
#include <QDesktopServices>

//------------------------------------------------------------------------------

ActorWidget::ActorWidget(QASActor* a, QWidget* parent, bool small) :
  QToolButton(parent), m_actor(a)
{
#ifdef DEBUG_WIDGETS
  qDebug() << "Creating ActorWidget" << (m_actor ? m_actor->id() : "NULL");
#endif
  int max_size = small ? 32 : 64;

  setStyleSheet("QToolButton::menu-indicator { "
                "subcontrol-origin: padding; "
                "subcontrol-position: bottom right; "
                "}");
  setIconSize(QSize(max_size, max_size));
  setFocusPolicy(Qt::NoFocus);
  setPopupMode(QToolButton::InstantPopup);
  setAutoRaise(true);

  m_menu = new QMenu(this);
  m_menuTitleAction = new QAction(this);
  connect(m_menuTitleAction, SIGNAL(triggered()), this, SLOT(onMenuTitle()));

  m_followAction = new QAction(this);
  connect(m_followAction, SIGNAL(triggered()), this, SLOT(onFollowAuthor()));

  m_hideAuthorAction = new QAction(this);
  connect(m_hideAuthorAction, SIGNAL(triggered()), this, SLOT(onHideAuthor()));

  createMenu();

  onImageChanged();
}

//------------------------------------------------------------------------------

ActorWidget::~ActorWidget() {
#ifdef DEBUG_WIDGETS
  qDebug() << "Deleting ActorWidget" << m_actor->id();
#endif
}

//------------------------------------------------------------------------------

void ActorWidget::setActor(QASActor* a) {
  if (m_actor == a)
    return;

  m_actor = a;
  onImageChanged();
  createMenu();
}

//------------------------------------------------------------------------------

void ActorWidget::onImageChanged() {
  m_url = m_actor ? m_actor->imageUrl() : "";
  updatePixmap();
}

//------------------------------------------------------------------------------

void ActorWidget::updatePixmap() {
  if (m_url.isEmpty()) {
    setIcon(QIcon(":/images/default.png"));
    return;
  }

  FileDownloader* fd = FileDownloader::get(m_url, true);
  connect(fd, SIGNAL(fileReady()), this, SLOT(updatePixmap()),
          Qt::UniqueConnection);
  setIcon(QIcon(fd->pixmap(":/images/default.png")));
}

//------------------------------------------------------------------------------

void ActorWidget::createMenu() {
  if (!m_actor) {
    setMenu(NULL);
    return;
  } 
  
  bool isYou = m_actor->isYou();

  m_menuTitleAction->setText(m_actor->displayName());
  m_menu->clear();
  m_menu->addAction(m_menuTitleAction);
  m_menu->addSeparator();

  if (!isYou)
    m_menu->addAction(m_followAction);

  m_menu->addAction(m_hideAuthorAction);
  
  updateMenu();

  setMenu(m_menu);
}

//------------------------------------------------------------------------------

void ActorWidget::updateMenu() {
  if (!m_actor)
    return;

  m_followAction->setText(m_actor->followed() ? tr("stop following") : 
                          tr("follow"));
  m_hideAuthorAction->setText(m_actor->isHidden() ? 
                              tr("stop minimising posts") :
                              tr("auto-minimise posts"));
}

//------------------------------------------------------------------------------

void ActorWidget::onFollowAuthor() {
  bool doFollow = !m_actor->followed();
  if (!doFollow) {
    QString msg = QString(tr("Are you sure you want to stop following %1?")).
      arg(m_actor->displayNameOrWebFinger());
    int ret = QMessageBox::warning(this, CLIENT_FANCY_NAME, msg,
                                   QMessageBox::Cancel | QMessageBox::Yes,
                                   QMessageBox::Cancel);
    if (ret != QMessageBox::Yes)
      return;
  }

  updateMenu();
  if (!m_actor->isYou())
    emit follow(m_actor->id(), doFollow);
}

//------------------------------------------------------------------------------

void ActorWidget::onHideAuthor() {
  bool doHide = !m_actor->isHidden();
  m_actor->setHidden(doHide);

  updateMenu();
  if (doHide)
    emit lessClicked();
  else
    emit moreClicked();
}

//------------------------------------------------------------------------------

void ActorWidget::onMenuTitle() {
  if (m_actor && !m_actor->url().isEmpty())
    QDesktopServices::openUrl(m_actor->url());
}
