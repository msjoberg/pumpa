/*
  Copyright 2015 Mats Sjöberg
  
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

#include <QFileDialog>
#include <QMessageBox>

#include "editprofiledialog.h"
#include "qasactor.h"
#include "filedownloader.h"

//------------------------------------------------------------------------------

EditProfileDialog::EditProfileDialog(QWidget* parent):
  QDialog(parent),
  m_profile(NULL)
{
  m_imageFileName = "";
  
  m_layout = new QVBoxLayout;

  m_imageLayout = new QHBoxLayout;
  
  m_imageLabel = new QLabel();
  m_imageLayout->addWidget(m_imageLabel);

  m_changeImageButton = new TextToolButton(tr("&Change picture"));
  m_imageLayout->addWidget(m_changeImageButton);

  connect(m_changeImageButton, SIGNAL(clicked()), this, SLOT(onChangeImage()));

  m_layout->addLayout(m_imageLayout);

  m_realNameLabel = new QLabel(tr("Real name"));
  m_layout->addWidget(m_realNameLabel);
  
  m_realNameEdit = new QLineEdit;
  m_layout->addWidget(m_realNameEdit);

  m_hometownLabel = new QLabel(tr("Hometown"));
  m_layout->addWidget(m_hometownLabel);

  m_hometownEdit = new QLineEdit;
  m_layout->addWidget(m_hometownEdit);

  m_bioLabel = new QLabel(tr("Bio"));
  m_layout->addWidget(m_bioLabel);

  m_bioEdit = new QTextEdit;
  m_layout->addWidget(m_bioEdit);

  m_buttonBox = new QDialogButtonBox(this);
  m_buttonBox->setOrientation(Qt::Horizontal);
  m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | 
                                  QDialogButtonBox::Cancel);
  connect(m_buttonBox, SIGNAL(accepted()), this, SLOT(onOKClicked()));
  connect(m_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

  m_layout->addWidget(m_buttonBox);
  
  setLayout(m_layout);
}

//------------------------------------------------------------------------------

void EditProfileDialog::setProfile(QASActor* profile) {
  m_realNameEdit->setText(profile->displayName());
  m_hometownEdit->setText(profile->location());
  m_bioEdit->setText(profile->summary());
  
  m_profile = profile;
  m_imageFileName = "";

  updateImage();
}

//------------------------------------------------------------------------------

void EditProfileDialog::updateImage() {
  if (m_profile == NULL)
    return;

  QPixmap pix;
  if (!m_imageFileName.isEmpty()) {
    pix.load(m_imageFileName);
  } else {
    FileDownloadManager* fdm = FileDownloadManager::getManager();
    QString imgSrc = m_profile->imageUrl();

    if (fdm->hasFile(imgSrc)) {
      pix = fdm->pixmap(imgSrc, ":/images/image_broken.png");
    } else if (!imgSrc.isEmpty()) {
      FileDownloader* fd = fdm->download(imgSrc);
      connect(fd, SIGNAL(fileReady()), this, SLOT(updateImage()), Qt::UniqueConnection);
    }
  }

  if (!pix.isNull()) {
    m_imageLabel->setPixmap(pix);
    m_imageLabel->setFixedSize(pix.size());
  }
}

//------------------------------------------------------------------------------

void EditProfileDialog::onOKClicked() {
  QString newRealName = m_realNameEdit->text();
  if (newRealName.isEmpty()) // to avoid having empty real name
    newRealName = m_profile->preferredUsername();
  
  m_profile->setDisplayName(newRealName);
  m_profile->setLocation(m_hometownEdit->text());
  m_profile->setSummary(m_bioEdit->toPlainText());
  
  accept();
  emit profileEdited(m_profile, m_imageFileName);
}

//------------------------------------------------------------------------------

void EditProfileDialog::onChangeImage() {
  QString fileName =
    QFileDialog::getOpenFileName(this, tr("Select Image"), "",
                                 tr("Image files (*.png *.jpg *.jpeg *.gif)"
                                    ";;All files (*.*)"));

  if (!fileName.isEmpty()) {

    QPixmap p;
    p.load(fileName);
    if (p.isNull()) {
      QMessageBox::critical(this, tr("Sorry!"),
                            tr("That file didn't appear to be an image."));
    } else {
      m_imageFileName = fileName;
      updateImage();
    }
  }
}
