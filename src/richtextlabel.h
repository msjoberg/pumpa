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

#ifndef _RICHTEXTLABEL_H_
#define _RICHTEXTLABEL_H_

#include <QFrame>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

#include "qactivitystreams.h"
#include "filedownloader.h"

//------------------------------------------------------------------------------

class RichTextLabel : public QLabel {
  Q_OBJECT

public:
  RichTextLabel(QWidget* parent = 0, bool singleLine = false);

  virtual void resizeEvent(QResizeEvent*);

private:
  bool m_singleLine;
};

#endif /* _RICHTEXTLABEL_H_ */
