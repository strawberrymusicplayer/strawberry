/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QTimer>
#include <QIODevice>
#include <QTextStream>
#include <QFile>
#include <QString>
#include <QPalette>
#include <QColor>
#include <QEvent>

#include "includes/shared_ptr.h"
#include "logging.h"
#include "stylesheetloader.h"

using namespace Qt::Literals::StringLiterals;

using std::make_shared;

StyleSheetLoader::StyleSheetLoader(QObject *parent) : QObject(parent) {}

void StyleSheetLoader::SetStyleSheet(QWidget *widget, const QString &filename) {

  // Load the file
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) {
    qLog(Error) << "Could not open stylesheet file" << filename << "for reading:" << file.errorString();
    return;
  }
  QTextStream stream(&file);
  QString stylesheet;
  Q_FOREVER {
    QString line = stream.readLine();
    stylesheet.append(line);
    if (stream.atEnd()) break;
  }
  file.close();

  SharedPtr<StyleSheetData> styledata = make_shared<StyleSheetData>();
  styledata->filename_ = filename;
  styledata->stylesheet_template_ = stylesheet;
  styledata->stylesheet_current_ = widget->styleSheet();
  styledata_.insert(widget, styledata);

  widget->installEventFilter(this);
  UpdateStyleSheet(widget, styledata);

}

void StyleSheetLoader::UpdateStyleSheet(QWidget *widget, SharedPtr<StyleSheetData> styledata) {

  QString stylesheet = styledata->stylesheet_template_;

  // Replace %palette-role with actual colours
  QPalette p(widget->palette());

  QColor color_altbase = p.color(QPalette::AlternateBase);
#ifdef Q_OS_MACOS
  color_altbase.setAlpha(color_altbase.alpha() >= 180 ? (color_altbase.lightness() > 180 ? 130 : 16) : color_altbase.alpha());
#else
  color_altbase.setAlpha(color_altbase.alpha() >= 180 ? 116 : color_altbase.alpha());
#endif
  stylesheet.replace("%palette-alternate-base"_L1, QStringLiteral("rgba(%1,%2,%3,%4)").arg(color_altbase.red()).arg(color_altbase.green()).arg(color_altbase.blue()).arg(color_altbase.alpha()));

  ReplaceColor(&stylesheet, u"Window"_s, p, QPalette::Window);
  ReplaceColor(&stylesheet, u"Background"_s, p, QPalette::Window);
  ReplaceColor(&stylesheet, u"WindowText"_s, p, QPalette::WindowText);
  ReplaceColor(&stylesheet, u"Base"_s, p, QPalette::Base);
  ReplaceColor(&stylesheet, u"AlternateBase"_s, p, QPalette::AlternateBase);
  ReplaceColor(&stylesheet, u"ToolTipBase"_s, p, QPalette::ToolTipBase);
  ReplaceColor(&stylesheet, u"ToolTipText"_s, p, QPalette::ToolTipText);
  ReplaceColor(&stylesheet, u"Text"_s, p, QPalette::Text);
  ReplaceColor(&stylesheet, u"Button"_s, p, QPalette::Button);
  ReplaceColor(&stylesheet, u"ButtonText"_s, p, QPalette::ButtonText);
  ReplaceColor(&stylesheet, u"BrightText"_s, p, QPalette::BrightText);
  ReplaceColor(&stylesheet, u"Light"_s, p, QPalette::Light);
  ReplaceColor(&stylesheet, u"Midlight"_s, p, QPalette::Midlight);
  ReplaceColor(&stylesheet, u"Dark"_s, p, QPalette::Dark);
  ReplaceColor(&stylesheet, u"Mid"_s, p, QPalette::Mid);
  ReplaceColor(&stylesheet, u"Shadow"_s, p, QPalette::Shadow);
  ReplaceColor(&stylesheet, u"Highlight"_s, p, QPalette::Highlight);
  ReplaceColor(&stylesheet, u"HighlightedText"_s, p, QPalette::HighlightedText);
  ReplaceColor(&stylesheet, u"Link"_s, p, QPalette::Link);
  ReplaceColor(&stylesheet, u"LinkVisited"_s, p, QPalette::LinkVisited);

#ifdef Q_OS_MACOS
  stylesheet.replace(QLatin1String("macos"), QLatin1String("*"));
#endif

  if (stylesheet != styledata->stylesheet_current_) {
    styledata->stylesheet_current_ = stylesheet;
    widget->setStyleSheet(stylesheet);
  }

}

void StyleSheetLoader::ReplaceColor(QString *css, const QString &name, const QPalette &palette, const QPalette::ColorRole role) {

  css->replace(u"%palette-"_s + name + u"-lighter"_s, palette.color(role).lighter().name(), Qt::CaseInsensitive);
  css->replace(u"%palette-"_s + name + u"-darker"_s, palette.color(role).darker().name(), Qt::CaseInsensitive);
  css->replace(u"%palette-"_s + name, palette.color(role).name(), Qt::CaseInsensitive);

}

bool StyleSheetLoader::eventFilter(QObject *obj, QEvent *event) {

  if (event->type() == QEvent::PaletteChange) {
    QWidget *widget = qobject_cast<QWidget*>(obj);
    if (widget && styledata_.contains(widget)) {
      UpdateStyleSheet(widget, styledata_[widget]);
    }
  }

  return false;

}
