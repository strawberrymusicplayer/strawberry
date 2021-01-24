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

#ifndef COLLAPSIBLEINFOHEADER_H
#define COLLAPSIBLEINFOHEADER_H

#include <QWidget>
#include <QString>
#include <QIcon>

class QPropertyAnimation;
class QEnterEvent;
class QEvent;
class QPaintEvent;
class QMouseEvent;

class CollapsibleInfoHeader : public QWidget {
  Q_OBJECT
  Q_PROPERTY(float opacity READ opacity WRITE set_opacity)

 public:
  CollapsibleInfoHeader(QWidget *parent = nullptr);

  static const int kHeight;
  static const int kIconSize;

  bool expanded() const { return expanded_; }
  bool hovering() const { return hovering_; }
  const QString &title() const { return title_; }
  const QIcon &icon() const { return icon_; }

  float opacity() const { return opacity_; }
  void set_opacity(const float opacity);

 public slots:
  void SetExpanded(const bool expanded);
  void SetTitle(const QString &title);
  void SetIcon(const QIcon &icon);

 signals:
  void Expanded();
  void Collapsed();
  void ExpandedToggled(const bool expanded);

 protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  void enterEvent(QEnterEvent*) override;
#else
  void enterEvent(QEvent*) override;
#endif
  void leaveEvent(QEvent*) override;
  void paintEvent(QPaintEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;

 private:
  bool expanded_;
  bool hovering_;
  QString title_;
  QIcon icon_;

  QPropertyAnimation *animation_;
  float opacity_;
};

#endif  // COLLAPSIBLEINFOHEADER_H
