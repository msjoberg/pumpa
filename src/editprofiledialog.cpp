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

#include "qasactor.h"
#include "editprofiledialog.h"

//------------------------------------------------------------------------------

EditProfileDialog::EditProfileDialog(QWidget* parent):
  QDialog(parent)
{
  m_layout = new QVBoxLayout;

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
}

//------------------------------------------------------------------------------

void EditProfileDialog::onOKClicked() {
  m_profile->setDisplayName(m_realNameEdit->text());
  m_profile->setLocation(m_hometownEdit->text());
  m_profile->setSummary(m_bioEdit->toPlainText());
  
  accept();
  emit profileEdited(m_profile);
}
