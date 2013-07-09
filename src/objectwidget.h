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

#ifndef _OBJECTWIDGET_H_
#define _OBJECTWIDGET_H_

#include <QFrame>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QStackedLayout>

#include "qactivitystreams.h"
#include "richtextlabel.h"
#include "shortobjectwidget.h"

//------------------------------------------------------------------------------

class ImageLabel : public QLabel {
  Q_OBJECT
  
public:
  ImageLabel(QWidget* parent=0);

signals:
  void clicked();

protected:
  virtual void mousePressEvent(QMouseEvent*);
};

//------------------------------------------------------------------------------

class ObjectWidget : public QFrame {
  Q_OBJECT

public:
  ObjectWidget(QASObject* obj, QWidget* parent = 0, bool childWidget=false);

  QASObject* object() const { return m_object; }

signals:
  void linkHovered(const QString&);
  void like(QASObject*);
  void share(QASObject*);
  void newReply(QASObject*);

private slots:
  void onChanged();
  void updateImage();
  void imageClicked();
  void onHasMoreClicked();

  void favourite();
  void repeat();
  void reply();

private:
  void setText(QString text);
  void setInfo(QString text);

  void updateLikes();
  void updateShares();

  QString recipientsToString(QASObjectList* rec);
  QString processText(QString old_text, bool getImages=false);

  void addHasMoreButton(QASObjectList* ol, int li);
  void updateFavourButton(bool wait = false);
  void updateShareButton(bool wait = false);
  void addObjectList(QASObjectList* ol);

  QString m_imageUrl;
  QString m_localFile;

  RichTextLabel* m_textLabel;
  ImageLabel* m_imageLabel;

  RichTextLabel* m_infoLabel;
  RichTextLabel* m_likesLabel;
  RichTextLabel* m_sharesLabel;
  QLabel* m_titleLabel;
  QPushButton* m_hasMoreButton;

  QToolButton* m_favourButton;
  QToolButton* m_shareButton;
  QToolButton* m_commentButton;

  QVBoxLayout* m_contentLayout;
  QHBoxLayout* m_buttonLayout;
  QVBoxLayout* m_commentsLayout;

  QASObject* m_object;

  QList<QASObject*> m_repliesList;
  QSet<QString> m_repliesMap;

  bool m_childWidget;
};

class WrappedObjectWidget : public QFrame {
  Q_OBJECT

public:
  WrappedObjectWidget(QASObject* obj, QWidget* parent = 0,
                      bool shortWidget=false);

  QASObject* object() const { return m_object; }

signals:
  void linkHovered(const QString&);
  void like(QASObject*);
  void share(QASObject*);
  void newReply(QASObject*);
  void moreClicked();
                          
private slots:
  void showMore();
  void onChanged();

private:
  ObjectWidget* m_objectWidget;
  ShortObjectWidget* m_shortObjectWidget;

  QVBoxLayout* m_layout;

  QASObject* m_object;
  bool m_short;
};
  

#endif /* _OBJECTWIDGET_H_ */
