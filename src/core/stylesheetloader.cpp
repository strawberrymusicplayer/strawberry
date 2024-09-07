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
#include <QColor>
#include <QPalette>
#include <QEvent>

#include "shared_ptr.h"
#include "core/logging.h"
#include "stylesheetloader.h"

using namespace Qt::StringLiterals;

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

  {
    QColor alt = p.color(QPalette::AlternateBase);
#ifdef Q_OS_MACOS
    if (alt.lightness() > 180) {
      alt.setAlpha(130);
    }
    else {
      alt.setAlpha(16);
    }
#else
    alt.setAlpha(130);
#endif
    stylesheet.replace("%palette-alternate-base"_L1, QStringLiteral("rgba(%1,%2,%3,%4)").arg(alt.red()).arg(alt.green()).arg(alt.blue()).arg(alt.alpha()));
  }

  ReplaceColor(&stylesheet, QStringLiteral("Window"), p, QPalette::Window);
  ReplaceColor(&stylesheet, QStringLiteral("Background"), p, QPalette::Window);
  ReplaceColor(&stylesheet, QStringLiteral("WindowText"), p, QPalette::WindowText);
  ReplaceColor(&stylesheet, QStringLiteral("Base"), p, QPalette::Base);
  ReplaceColor(&stylesheet, QStringLiteral("AlternateBase"), p, QPalette::AlternateBase);
  ReplaceColor(&stylesheet, QStringLiteral("ToolTipBase"), p, QPalette::ToolTipBase);
  ReplaceColor(&stylesheet, QStringLiteral("ToolTipText"), p, QPalette::ToolTipText);
  ReplaceColor(&stylesheet, QStringLiteral("Text"), p, QPalette::Text);
  ReplaceColor(&stylesheet, QStringLiteral("Button"), p, QPalette::Button);
  ReplaceColor(&stylesheet, QStringLiteral("ButtonText"), p, QPalette::ButtonText);
  ReplaceColor(&stylesheet, QStringLiteral("BrightText"), p, QPalette::BrightText);
  ReplaceColor(&stylesheet, QStringLiteral("Light"), p, QPalette::Light);
  ReplaceColor(&stylesheet, QStringLiteral("Midlight"), p, QPalette::Midlight);
  ReplaceColor(&stylesheet, QStringLiteral("Dark"), p, QPalette::Dark);
  ReplaceColor(&stylesheet, QStringLiteral("Mid"), p, QPalette::Mid);
  ReplaceColor(&stylesheet, QStringLiteral("Shadow"), p, QPalette::Shadow);
  ReplaceColor(&stylesheet, QStringLiteral("Highlight"), p, QPalette::Highlight);
  ReplaceColor(&stylesheet, QStringLiteral("HighlightedText"), p, QPalette::HighlightedText);
  ReplaceColor(&stylesheet, QStringLiteral("Link"), p, QPalette::Link);
  ReplaceColor(&stylesheet, QStringLiteral("LinkVisited"), p, QPalette::LinkVisited);

#ifdef Q_OS_MACOS
  stylesheet.replace(QLatin1String("macos"), QLatin1String("*"));
#endif

  if (stylesheet != styledata->stylesheet_current_) {
    styledata->stylesheet_current_ = stylesheet;
    widget->setStyleSheet(stylesheet);
  }

}

void StyleSheetLoader::ReplaceColor(QString *css, const QString &name, const QPalette &palette, const QPalette::ColorRole role) {

  css->replace(QStringLiteral("%palette-") + name + QStringLiteral("-lighter"), palette.color(role).lighter().name(), Qt::CaseInsensitive);
  css->replace(QStringLiteral("%palette-") + name + QStringLiteral("-darker"), palette.color(role).darker().name(), Qt::CaseInsensitive);
  css->replace(QStringLiteral("%palette-") + name, palette.color(role).name(), Qt::CaseInsensitive);

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
