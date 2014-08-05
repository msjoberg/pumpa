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

#ifndef MESSAGE_WINDOW_H
#define MESSAGE_WINDOW_H

#include <QDialog>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QScrollArea>
#include <QSplitter>

#include "messageedit.h"
#include "qactivitystreams.h"
#include "pumpasettings.h"
#include "texttoolbutton.h"
#include "richtextlabel.h"
#include "messagerecipients.h"

//------------------------------------------------------------------------------

class PictureLabel : public QLabel {
  Q_OBJECT

 public:
  PictureLabel(QWidget* parent);
  void setOriginalPixmap(QPixmap);
  
 protected:
  virtual void resizeEvent(QResizeEvent* event);
  
  QPixmap m_originalPixmap;
};

//------------------------------------------------------------------------------

class MessageWindow : public QDialog {
  Q_OBJECT

public:
  MessageWindow(PumpaSettings* s, const RecipientList* rl, QWidget* parent=0);
  virtual void accept();

  void newMessage(QASObject* obj, QASObjectList* to, QASObjectList* cc);
  void editMessage(QASObject* obj);
  void clear();
  void setCompletions(const MessageEdit::completion_t* completions);

protected:
  virtual void showEvent(QShowEvent*);

signals:
  void sendMessage(QString, QString, RecipientList, RecipientList);
  void sendImage(QString, QString, QString, RecipientList, RecipientList);
  void sendReply(QASObject*, QString, RecipientList, RecipientList);
  void sendEdit(QASObject*, QString, QString);

private slots:
  void onAddPicture();
  void onRemovePicture();
  void togglePreview();
  void updatePreview(bool force=false);
  void onAddRecipient(QASActor*);
  void onAddTo();
  void onAddCc();
  void onMarkdownChecked(int);

private:
  void initWindow(QString title, QString buttonText, bool showRecipients);
  void addRecipientWindow(MessageRecipients*, QString);
  void updateAddPicture();

  void copyRecipients(MessageRecipients*, QASObjectList*);
  void setDefaultRecipients(MessageRecipients*, int);
  void addToRecipientList(QString, QASObject*);

  QVBoxLayout* m_layout;

  QLabel* m_infoLabel;
  QLabel* m_markupLabel;
  QHBoxLayout* m_infoLayout;

  QCheckBox* m_markdownCheckBox;

  QFormLayout* m_addressLayout;

  MessageEdit* m_textEdit;
  QHBoxLayout* m_buttonLayout;

  QHBoxLayout* m_pictureButtonLayout;
  TextToolButton* m_addPictureButton;
  TextToolButton* m_removePictureButton;
  TextToolButton* m_addToButton;
  TextToolButton* m_addCcButton;

  QLabel* m_toLabel;
  QLabel* m_ccLabel;

  RichTextLabel* m_previewLabel;
  QScrollArea* m_previewArea;
  QSplitter* m_splitter;

  PictureLabel* m_pictureLabel;
  QLineEdit* m_title;

  QPushButton* m_cancelButton;
  QPushButton* m_sendButton;
  QPushButton* m_previewButton;

  QString m_imageFileName;

  MessageRecipients* m_toRecipients;
  MessageRecipients* m_ccRecipients;

  QMap<QString, QASObject*> m_recipientSelection;
  QStringList m_recipientList;

  bool m_editing;
  bool m_isReply;

  QASObject* m_obj;
  PumpaSettings* m_s;
  const RecipientList* m_rl;
};

#endif
