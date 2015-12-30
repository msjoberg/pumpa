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

#ifndef _EDITPROFILEDIALOG_H_
#define _EDITPROFILEDIALOG_H_

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "qasactor.h"
#include "texttoolbutton.h"

class EditProfileDialog : public QDialog {
  Q_OBJECT

public:
  EditProfileDialog(QWidget* parent=0);

  void setProfile(QASActor* profile);

signals:
  void profileEdited(QASActor*, QString);

private slots:
  void onOKClicked();
  void updateImage();
  void onChangeImage();

private:
  QString m_imageFileName;
  
  QLabel* m_imageLabel;
  TextToolButton* m_changeImageButton;

  QHBoxLayout* m_imageLayout;
  
  QLabel* m_realNameLabel;
  QLineEdit* m_realNameEdit;

  QLabel* m_hometownLabel;
  QLineEdit* m_hometownEdit;

  QLabel* m_bioLabel;
  QTextEdit* m_bioEdit;
  
  QDialogButtonBox* m_buttonBox;

  QVBoxLayout* m_layout;

  QASActor* m_profile;
};

#endif /* _EDITPROFILEDIALOG_H_ */
