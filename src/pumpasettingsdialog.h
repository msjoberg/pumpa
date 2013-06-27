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

#ifndef _PUMPASETTINGSDIALOG_H_
#define _PUMPASETTINGSDIALOG_H_

#include <QtGui>

class PumpaSettingsDialog : public QDialog {
  Q_OBJECT

public:
  PumpaSettingsDialog(QSettings* settings, QWidget* parent=0);

  static int comboIndexToFeedInt(int i) {
    return comboIndexConverter(i, false); 
  }
  static int feedIntToComboIndex(int i) { 
    return comboIndexConverter(i, true); 
  }

signals:
  void settingsChanged();

// private slots:
//   void on_buttonBox_accepted();

protected:
  void setVisible(bool visible);

private slots:
  void onAuthButtonClicked();

private:
  static int comboIndexConverter(int i, bool backwards=false);
  void updateUI();
  QSettings* s;

  QLabel* m_currentAccountLabel;
  QPushButton* m_authButton;
  QSpinBox* m_updateTimeSpinBox;
  QCheckBox* m_useIconCheckBox;
  QDialogButtonBox* m_buttonBox;
  QComboBox* m_highlightComboBox;
  QComboBox* m_popupComboBox;

  QFormLayout* m_formLayout;
  QVBoxLayout* m_layout;
};

#endif /* _PUMPASETTINGSDIALOG_H_ */
