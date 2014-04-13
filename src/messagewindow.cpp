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

#include "messagewindow.h"
#include "pumpa_defines.h"
#include "pumpasettings.h"
#include "util.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>

//------------------------------------------------------------------------------

const int max_picture_size = 160;

//------------------------------------------------------------------------------

MessageWindow::MessageWindow(PumpaSettings* s, const RecipientList* rl,
                             QWidget* parent) :
  QDialog(parent),
  m_addressLayout(NULL),
  m_obj(NULL),
  m_s(s),
  m_rl(rl)
{
  setMinimumSize(QSize(400,400));
  setWindowTitle(CLIENT_FANCY_NAME);

  m_infoLabel = new QLabel(this);

  m_markdownCheckBox = new QCheckBox(tr("Use Markdown"), this);
  connect(m_markdownCheckBox, SIGNAL(stateChanged(int)),
          this, SLOT(onMarkdownChecked(int)));

  m_markupLabel = new QLabel(this);
  m_markupLabel->setText(QString("<a href=\"%2\">" + tr("[help]") + "</a>").
                       arg(MARKUP_DOC_URL));
  m_markupLabel->setOpenExternalLinks(true);
  m_markupLabel->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                       Qt::LinksAccessibleByMouse);

  m_infoLayout = new QHBoxLayout;
  m_infoLayout->addWidget(m_infoLabel);
  m_infoLayout->addStretch();
  m_infoLayout->addWidget(m_markdownCheckBox);
  m_infoLayout->addWidget(m_markupLabel);

  m_toRecipients = new MessageRecipients(this);
  m_ccRecipients = new MessageRecipients(this);

  m_toLabel = new QLabel(tr("To:"));
  m_ccLabel = new QLabel(tr("Cc:"));

  m_addressLayout = new QFormLayout;
  m_addressLayout->addRow(m_toLabel, m_toRecipients);
  m_addressLayout->addRow(m_ccLabel, m_ccRecipients);
  m_addressLayout->setContentsMargins(0, 0, 0, 0);

  m_addPictureButton = new TextToolButton(this);
  connect(m_addPictureButton, SIGNAL(clicked()), this, SLOT(onAddPicture()));

  m_removePictureButton = new TextToolButton(tr("&Remove picture"), this);
  connect(m_removePictureButton, SIGNAL(clicked()),
          this, SLOT(onRemovePicture()));

  m_addToButton = new TextToolButton(tr("+ &To"), this);
  connect(m_addToButton, SIGNAL(clicked()), this, SLOT(onAddTo()));

  m_addCcButton = new TextToolButton(tr("+ &Cc"), this);
  connect(m_addCcButton, SIGNAL(clicked()), this, SLOT(onAddCc()));

  m_pictureButtonLayout = new QHBoxLayout;
  m_pictureButtonLayout->addWidget(m_addPictureButton, 0, Qt::AlignTop);
  m_pictureButtonLayout->addWidget(m_removePictureButton, 0, Qt::AlignTop);
  m_pictureButtonLayout->addStretch();
  m_pictureButtonLayout->addWidget(m_addToButton, 0, Qt::AlignTop);
  m_pictureButtonLayout->addWidget(m_addCcButton, 0, Qt::AlignTop);

  m_pictureLabel = new QLabel(this);
  m_pictureLabel->setScaledContents(true);
  m_pictureLabel->setMaximumSize(max_picture_size, max_picture_size);
  m_pictureLabel->setFocusPolicy(Qt::NoFocus);
  m_pictureLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);

  m_title = new QLineEdit(this);
#if QT_VERSION >= 0x040700
  m_title->setPlaceholderText(tr("Title (optional)"));
#endif

  m_previewLabel = new RichTextLabel(this);
  m_previewLabel->setLineWidth(1);
  m_previewLabel->setFrameStyle(QFrame::Box);
  m_previewLabel->hide();

  m_textEdit = new MessageEdit(m_s, this);
  connect(m_textEdit, SIGNAL(ready()), this, SLOT(accept()));
  connect(m_textEdit, SIGNAL(textChanged()), this, SLOT(updatePreview()));
  connect(m_textEdit, SIGNAL(addRecipient(QASActor*)),
          this, SLOT(onAddRecipient(QASActor*)));

  m_layout = new QVBoxLayout;
  m_layout->addLayout(m_infoLayout);
  m_layout->addLayout(m_addressLayout);
  m_layout->addLayout(m_pictureButtonLayout);
  m_layout->addWidget(m_pictureLabel, 0, Qt::AlignHCenter);
  m_layout->addWidget(m_title);
  m_layout->addWidget(m_textEdit);
  m_layout->addWidget(m_previewLabel);

  m_cancelButton = new QPushButton(tr("Cancel"));
  connect(m_cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  m_previewButton = new QPushButton(tr("Preview"));
  connect(m_previewButton, SIGNAL(clicked()), this, SLOT(togglePreview()));
  
  m_sendButton = new QPushButton(tr("Send message"));
  connect(m_sendButton, SIGNAL(clicked()), this, SLOT(accept()));
  m_sendButton->setDefault(true);

  m_buttonLayout = new QHBoxLayout;
  m_buttonLayout->addWidget(m_cancelButton);
  m_buttonLayout->addWidget(m_previewButton);
  m_buttonLayout->addWidget(m_sendButton);
  m_layout->addLayout(m_buttonLayout);
  
  setLayout(m_layout);

  m_textEdit->setFocus(Qt::OtherFocusReason);

  QTextCursor cursor = m_textEdit->textCursor();
  cursor.movePosition(QTextCursor::End);
  m_textEdit->setTextCursor(cursor);
}

//------------------------------------------------------------------------------

void MessageWindow::onMarkdownChecked(int state) {
  if (state == Qt::Unchecked) {
    m_s->useMarkdown(false);
  } else {
    m_s->useMarkdown(true);
  }
  updatePreview();
}

//------------------------------------------------------------------------------

void MessageWindow::setCompletions(const MessageEdit::completion_t*
                                   completions) {
  if (m_textEdit) 
    m_textEdit->setCompletions(completions); 
}

//------------------------------------------------------------------------------

void MessageWindow::addToRecipientList(QString name, QASObject* obj) {
  m_recipientList.append(name);
  if (!m_recipientSelection.contains(name))
    m_recipientSelection.insert(name, obj);
}

//------------------------------------------------------------------------------

void MessageWindow::addRecipientWindow(MessageRecipients* mr, QString title) {
  QInputDialog* dialog = new QInputDialog(this);
  // dialog->setOption(QInputDialog::UseListViewForComboBoxItems);
  dialog->setComboBoxItems(m_recipientList);
  dialog->setComboBoxEditable(true);
  dialog->setLabelText(title);
  dialog->setInputMode(QInputDialog::TextInput);
  dialog->setWindowTitle(CLIENT_FANCY_NAME);

  dialog->exec();

  if (dialog->result() == QDialog::Accepted) {
    QString text = dialog->textValue();
    if (m_recipientSelection.contains(text))
      mr->addRecipient(m_recipientSelection[text]);
  }
}

//------------------------------------------------------------------------------

void MessageWindow::onAddTo() { 
  addRecipientWindow(m_toRecipients, tr("Select recipient (To)")); 
}

//------------------------------------------------------------------------------

void MessageWindow::onAddCc() { 
  addRecipientWindow(m_ccRecipients, tr("Select recipient (Cc)")); 
}

//------------------------------------------------------------------------------

void MessageWindow::onAddRecipient(QASActor* actor) {
  m_toRecipients->addRecipient(actor);
}

//------------------------------------------------------------------------------

void MessageWindow::newMessage(QASObject* obj, QASObjectList* to,
                               QASObjectList* cc) {
  QASObject* origObj = obj;
  if (obj && !m_s->commentOnComments() && obj->inReplyTo())
    obj = obj->inReplyTo();

  bool isReply = (obj != NULL);
  m_obj = obj;

  QString title = isReply ? tr("Post a reply") : tr("Post a note");
  setWindowTitle(QString(CLIENT_FANCY_NAME) + " - " + title);

  m_markdownCheckBox->setChecked(m_s->useMarkdown());

  m_recipientList.clear();
  for (int i=0; i<m_rl->size(); ++i) {
    QASObject* obj = m_rl->at(i);
    addToRecipientList(obj->displayName() + " (list)", obj);
  }

  const MessageEdit::completion_t* completions = m_textEdit->getCompletions();
  MessageEdit::completion_t::const_iterator it = completions->constBegin();
  for (; it != completions->constEnd(); ++it)
    addToRecipientList(it.key(), it.value());

  m_infoLabel->setText(title);

  if (!isReply) {
    // A new post, use default recipients
    setDefaultRecipients(m_toRecipients, m_s->defaultToAddress());
    setDefaultRecipients(m_ccRecipients, m_s->defaultCcAddress());
  } else {
    // For a reply, copy recipients from parent
    copyRecipients(m_toRecipients, to);
    copyRecipients(m_ccRecipients, cc);

    // add original post author to recipients
    if (obj->author())
      m_toRecipients->addRecipient(obj->author());

    // if this is a reply to a comment add comment author to
    // recipients as well
    if (origObj != obj && origObj->author())
      m_toRecipients->addRecipient(origObj->author());
  }
  
  // m_toRecipients->setVisible(!isReply || hasInitialTo);
  // m_addressLayout->labelForField(m_toRecipients)->
  //   setVisible(!isReply || hasInitialTo);

  // m_ccRecipients->setVisible(!isReply);
  // m_addressLayout->labelForField(m_ccRecipients)->setVisible(!isReply);

  // m_addToButton->setVisible(!isReply);
  // m_addCcButton->setVisible(!isReply);

  updateAddPicture();
}

//------------------------------------------------------------------------------

void MessageWindow::copyRecipients(MessageRecipients* mr, QASObjectList* ol) {
  mr->clear();
  if (ol) {
    RecipientList rl = ol->toRecipientList();
    for (int i=0; i<rl.size(); ++i)
      mr->addRecipient(rl[i]);
  }
}

//------------------------------------------------------------------------------

void MessageWindow::setDefaultRecipients(MessageRecipients* mr,
                                         int defAddress) {
  mr->clear();
  if (defAddress == RECIPIENT_PUBLIC)
    mr->addRecipient(m_rl->at(0));
  if (defAddress == RECIPIENT_FOLLOWERS)
    mr->addRecipient(m_rl->at(1));
}

//------------------------------------------------------------------------------

void MessageWindow::clear() {
  m_imageFileName = "";
  m_textEdit->clear();
  m_title->clear();
  m_previewLabel->clear();
}

//------------------------------------------------------------------------------

void MessageWindow::showEvent(QShowEvent*) {
  m_textEdit->setFocus(Qt::OtherFocusReason);
  m_textEdit->selectAll();
  activateWindow();
}

//------------------------------------------------------------------------------

void MessageWindow::accept() {
  m_textEdit->hideCompletion();
  QString msg = m_textEdit->toPlainText();

  RecipientList to = m_toRecipients->recipients();
  RecipientList cc = m_ccRecipients->recipients();

  if (m_obj == NULL) {
    QString title = m_title->text();

    if (m_imageFileName.isEmpty()) {
      emit sendMessage(msg, title, to, cc);
    } else {
      emit sendImage(msg, title, m_imageFileName, to, cc);
    }
  } else {
    emit sendReply(m_obj, msg, to, cc);
  }

  QDialog::accept();
}

//------------------------------------------------------------------------------

void MessageWindow::onAddPicture() {
  QString fileName =
    QFileDialog::getOpenFileName(this, tr("Select Image"), "",
                                 tr("Image files (*.png *.jpg *.jpeg *.gif)"
                                    ";;All files (*.*)"));

  if (!fileName.isEmpty()) {
    m_imageFileName = fileName;
    updateAddPicture();
  }
}

//------------------------------------------------------------------------------

void MessageWindow::onRemovePicture() {
  m_imageFileName = "";
  updateAddPicture();
}

//------------------------------------------------------------------------------

void MessageWindow::updateAddPicture() {
  if (m_obj != NULL) { // if reply just hide everything
    m_addPictureButton->setVisible(false);
    m_removePictureButton->setVisible(false);
    m_pictureLabel->setVisible(false);
    m_title->setVisible(false);
    m_removePictureButton->setVisible(false);
    return;
  }

  QPixmap p;
  if (!m_imageFileName.isEmpty()) {
    p.load(m_imageFileName);
    if (p.isNull()) {
      QMessageBox::critical(this, tr("Sorry!"),
                            tr("That file didn't appear to be an image."));
      m_imageFileName = "";
    }
  }

  m_addPictureButton->setVisible(true);
  m_title->setVisible(true);
  if (m_imageFileName.isEmpty()) {
    m_addPictureButton->setText(tr("&Add picture"));
    m_removePictureButton->setVisible(false);
    m_pictureLabel->setVisible(false);
  } else {
    m_pictureLabel->setPixmap(p);
    m_addPictureButton->setText(tr("&Change picture"));
    m_removePictureButton->setVisible(true);
    m_pictureLabel->setVisible(true);
  }
}

//------------------------------------------------------------------------------

void MessageWindow::updatePreview() {
  if (m_previewLabel->isVisible()) 
    m_previewLabel->setText(addTextMarkup(m_textEdit->toPlainText(),
                                          m_s->useMarkdown()));
}

//------------------------------------------------------------------------------

void MessageWindow::togglePreview() {
  m_previewLabel->setVisible(!m_previewLabel->isVisible());
  updatePreview();
}
