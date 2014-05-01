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

#include "util.h"

#include <QTextStream>
#include <QStringList>
#include <QRegExp>
#include <QObject>
#include <QDebug>
#include <QFile>

#ifdef DEBUG_MEMORY
#include <sys/resource.h>
#include <unistd.h>
#endif

#include "sundown/markdown.h"
#include "sundown/html.h"
#include "sundown/buffer.h"

#ifdef USE_TIDY
#include <tidy/tidy.h>
#include <tidy/buffio.h>
#endif

//------------------------------------------------------------------------------

QString markDown(QString text) {
  struct sd_callbacks callbacks;
  struct html_renderopt options;
  sdhtml_renderer(&callbacks, &options, 0);

  struct sd_markdown* markdown = sd_markdown_new(0, 16, &callbacks, &options);

  struct buf* ob = bufnew(64);
  // QByteArray ba = text.toLocal8Bit();
  QByteArray ba = text.toUtf8();

  sd_markdown_render(ob, (const unsigned char*)ba.constData(), ba.size(),
                     markdown);
  sd_markdown_free(markdown);

  // QString ret = QString::fromLocal8Bit((char*)ob->data, ob->size);
  QString ret = QString::fromUtf8((char*)ob->data, ob->size);
  bufrelease(ob);

  return ret;
}

//------------------------------------------------------------------------------

QString siteUrlFixer(QString url, bool useSsl) {
  if (!url.startsWith("http://") && !url.startsWith("https://"))
    url = (useSsl ? "https://" : "http://") + url;

  if (url.endsWith('/'))
    url.chop(1);

  return url;
}

//------------------------------------------------------------------------------

QString linkifyUrls(QString text, bool useMarkdown) {
  QRegExp rx(QString("(^|\\s)%1([\\s\\.\\,\\!\\?\\)]|$)").arg(URL_REGEX_STRICT));

  QStringList lines = text.split('\n');

  for (int i=0; i<lines.size(); ++i) {
    QString line = lines.at(i);
    int pos = 0;
    while ((pos = rx.indexIn(line, pos)) != -1) {
      int len = rx.matchedLength();
      QString before = rx.cap(1);
      QString url = rx.cap(2);
      QString after = rx.cap(3);

      QString newText = useMarkdown ?
        QString("%1<%2>%3").arg(before).arg(url).arg(after) :
        QString("%1<a href=\"%2\">%2</a>%3").arg(before).arg(url).arg(after);

      line.replace(pos, len, newText);
      pos += newText.count();
    }
    lines[i] = line;
  }
  return lines.join("\n");
}

//------------------------------------------------------------------------------

QString changePairedTags(QString text, 
                         QString begin, QString end,
                         QString newBegin, QString newEnd,
                         QString nogoItems) {
  QRegExp rx(QString(MD_PAIR_REGEX).arg(begin).arg(end).arg(nogoItems));
  int pos = 0;
  while ((pos = rx.indexIn(text, pos)) != -1) {
    int len = rx.matchedLength();
    QString newText = QString(newBegin + rx.cap(1) + newEnd);
    text.replace(pos, len, newText);
    pos += newText.count();
  }
  return text;
}

//------------------------------------------------------------------------------

QString siteUrlToAccountId(QString username, QString url) {
  if (url.startsWith("http://"))
    url.remove(0, 7);
  if (url.startsWith("https://"))
    url.remove(0, 8);

  if (url.endsWith('/'))
    url.chop(1);
 
  return username + "@" + url;
}

//------------------------------------------------------------------------------

QString relativeFuzzyTime(QDateTime sTime) {
  QString dateStr = sTime.toString("ddd d MMMM yyyy");

  int secs = sTime.secsTo(QDateTime::currentDateTime().toUTC());
  if (secs < 0)
    secs = 0;
  int mins = qRound((float)secs/60);
  int hours = qRound((float)secs/60/60);
    
  if (secs < 60) { 
    dateStr = QObject::tr("a few seconds ago");
  } else if (mins == 1) {
    dateStr = QObject::tr("one minute ago");
  } else if (mins < 60) {
    dateStr = QObject::tr("%n minute(s) ago", 0, mins);
  } else if (hours >= 1 && hours < 24) {
    dateStr = QObject::tr("%n hour(s) ago", 0, hours);
  }
  return dateStr;
}

//------------------------------------------------------------------------------

bool splitWebfingerId(QString accountId, QString& username, QString& server) {
  static QRegExp rx("^([\\w\\._-+]+)@([\\w\\._-+]+)$");
  if (!rx.exactMatch(accountId.trimmed()))
    return false;

  username = rx.cap(1);
  server = rx.cap(2);
  return true;
}

//------------------------------------------------------------------------------

long getMaxRSS() {
#ifdef DEBUG_MEMORY
  struct rusage rusage;
  getrusage(RUSAGE_SELF, &rusage);
  return rusage.ru_maxrss;
#else
  return 0;
#endif
}

//------------------------------------------------------------------------------

long getCurrentRSS() {
#ifdef DEBUG_MEMORY
  QFile fp("/proc/self/statm");
  if (!fp.open(QIODevice::ReadOnly))
    return -1;

  QTextStream in(&fp);
  QString line = in.readLine();
  QStringList parts = line.split(" ");

  return parts[1].toLong() * sysconf( _SC_PAGESIZE);
#else
  return 0;
#endif
}

//------------------------------------------------------------------------------

void checkMemory(QString desc) {
  static long oldMem = -1;
  
  long mem = getCurrentRSS();
  long diff = 0;
  if (oldMem > 0)
    diff = mem-oldMem;

  QString msg("RESIDENT MEMORY");
  if (!desc.isEmpty())
    msg += " (" + desc + ")";
  msg += QString(": %1 KB").arg((float)mem/1024.0, 0, 'f', 2);
  if (diff != 0)
    msg += QString(" (%2%1)").arg(diff).arg(diff > 0 ? '+' : '-');

  qDebug() << msg;
}

//------------------------------------------------------------------------------

QString removeHtml(QString origText) {
  QString text = origText;

  // Remove any inline HTML tags
  // text.replace(QRegExp(HTML_TAG_REGEX), "&lt;\\1&gt;");
  QRegExp rx(HTML_TAG_REGEX);
  QRegExp urlRx(URL_REGEX);
  int pos = 0;
  
  while ((pos = rx.indexIn(text, pos)) != -1) {
    int len = rx.matchedLength();
    QString tag = rx.cap(1);
    if (urlRx.exactMatch(tag)) {
      pos += len;
    } else {
      QString newText = "&lt;" + tag + "&gt;";
      text.replace(pos, len, newText);
      pos += newText.length();
    }
  }
  return text;
}

//------------------------------------------------------------------------------

QString tidyHtml(QString str, bool& ok) {
  QString res = str;
  ok = false;

#ifdef USE_TIDY
  TidyDoc tdoc = tidyCreate();
  TidyBuffer output = {0, 0, 0, 0, 0};
  TidyBuffer errbuf = {0, 0, 0, 0, 0};

  bool configOk = 
    tidyOptSetBool(tdoc, TidyXhtmlOut, yes) && 
    tidyOptSetBool(tdoc, TidyForceOutput, yes) &&
    tidyOptSetBool(tdoc, TidyMark, no) &&
    tidyOptSetInt(tdoc, TidyBodyOnly, yes) &&
    tidyOptSetInt(tdoc, TidyDoctypeMode, TidyDoctypeOmit);
    
  if (configOk &&
      (tidySetErrorBuffer(tdoc, &errbuf) >= 0) &&
      (tidyParseString(tdoc, str.toLatin1().data()) >= 0) &&
      (tidyCleanAndRepair(tdoc) >= 0) &&
      (tidyRunDiagnostics(tdoc) >= 0) &&
      (tidySaveBuffer(tdoc, &output) >= 0) &&
      (output.bp != 0 && output.size > 0)) {
    res = QString::fromUtf8((char*)output.bp, output.size);

    ok = true;
  }

#ifdef DEBUG_MARKUP
  if (errbuf.size > 0) {
    QString errStr =  QString::fromUtf8((char*)errbuf.bp, errbuf.size);
    qDebug() << "\n[DEBUG] MARKUP, libtidy errors and warnings:\n" << errStr;
  }
#endif

  if (output.bp != 0)
    tidyBufFree(&output);
  if (errbuf.bp != 0)
    tidyBufFree(&errbuf);
  tidyRelease(tdoc);
#endif

  return res;
}

//------------------------------------------------------------------------------

QString addTextMarkup(QString text, bool useMarkdown) {
  QString oldText = text;

#ifdef DEBUG_MARKUP
  qDebug() << "\n[DEBUG] MARKUP\n" << text;
#endif

#ifndef USE_TIDY
  text = removeHtml(text);
# ifdef DEBUG_MARKUP
  qDebug() << "\n[DEBUG] MARKUP (clean inline HTML)\n" << text;
# endif
#endif

  // linkify plain URLs
  text = linkifyUrls(text, useMarkdown);

#ifdef DEBUG_MARKUP
  qDebug() << "\n[DEBUG] MARKUP (linkify plain URLs)\n" << text;
#endif

  if (useMarkdown) {
    // apply markdown
    text = markDown(text);
  } else {
    text.replace("\n", "<br/>");
  }

#ifdef DEBUG_MARKUP
  qDebug() << "\n[DEBUG] MARKUP (apply "
           << (useMarkdown?"Markdown":"text conversion") <<  ")\n" << text;
#endif

#ifdef USE_TIDY
  bool tidyOk = false;
  text = tidyHtml(text, tidyOk);

# ifdef DEBUG_MARKUP
  qDebug() << "\n[DEBUG] MARKUP (libtidy)\n" << text;
# endif

#endif

  return text;
}

//------------------------------------------------------------------------------

QString processTitle(QString text) {
  text = removeHtml(text);
  text.replace("\n", " ");
  return text;
}
