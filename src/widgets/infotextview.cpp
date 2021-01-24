/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <QWidget>
#include <QApplication>
#include <QMenu>
#include <QWheelEvent>
#include <QRegularExpression>
#include <QtDebug>

#include "core/logging.h"
#include "infotextview.h"

InfoTextView::InfoTextView(QWidget *parent) : QTextBrowser(parent), last_width_(-1), recursion_filter_(false) {
    
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setOpenExternalLinks(true);

}

void InfoTextView::resizeEvent(QResizeEvent *e) {

  const int w = qMax(100, width());
  if (w == last_width_) return;
  last_width_ = w;

  document()->setTextWidth(w);
  setMinimumHeight(document()->size().height());

  QTextBrowser::resizeEvent(e);

}

QSize InfoTextView::sizeHint() const { return minimumSize(); }

void InfoTextView::wheelEvent(QWheelEvent *e) { e->ignore(); }

void InfoTextView::SetHtml(const QString &html) {

  QString copy(html.trimmed());

  // Simplify newlines
  copy.replace(QRegularExpression("\\r\\n?"), "\n");

  // Convert two or more newlines to <p>, convert single newlines to <br>
  copy.replace(QRegularExpression("([^>])([\\t ]*\\n){2,}"), "\\1<p>");
  copy.replace(QRegularExpression("([^>])[\\t ]*\\n"), "\\1<br>");

  // Strip any newlines from the end
  copy.replace(QRegularExpression("((<\\s*br\\s*/?\\s*>)|(<\\s*/?\\s*p\\s*/?\\s*>))+$"), "");

  setHtml(copy);

}

// Prevents QTextDocument from trying to load remote images before they are ready.
QVariant InfoTextView::loadResource(int type, const QUrl &name) {

  if (recursion_filter_) {
    recursion_filter_ = false;
    return QVariant();
  }
  recursion_filter_ = true;
  if (type == QTextDocument::ImageResource && name.scheme() == "http") {
    if (document()->resource(type, name).isNull()) {
      return QVariant();
    }
  }
  return QTextBrowser::loadResource(type, name);

}
