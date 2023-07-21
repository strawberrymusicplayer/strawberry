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

#ifndef SMARTPLAYLISTSEARCHTERMWIDGET_H
#define SMARTPLAYLISTSEARCHTERMWIDGET_H

#include "config.h"

#include <QWidget>
#include <QPushButton>

#include "core/shared_ptr.h"
#include "smartplaylistsearchterm.h"

class QPropertyAnimation;
class QEvent;
class QShowEvent;
class QEnterEvent;
class QResizeEvent;

class CollectionBackend;
class Ui_SmartPlaylistSearchTermWidget;

class SmartPlaylistSearchTermWidget : public QWidget {
  Q_OBJECT

  Q_PROPERTY(float overlay_opacity READ overlay_opacity WRITE set_overlay_opacity)

 public:
  explicit SmartPlaylistSearchTermWidget(SharedPtr<CollectionBackend> collection_backend, QWidget *parent);
  ~SmartPlaylistSearchTermWidget() override;

  void SetActive(const bool active);

  float overlay_opacity() const;
  void set_overlay_opacity(const float opacity);

  void SetTerm(const SmartPlaylistSearchTerm &term);
  SmartPlaylistSearchTerm Term() const;

 signals:
  void Clicked();
  void RemoveClicked();

  void Changed();

 protected:
  void showEvent(QShowEvent*) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  void enterEvent(QEnterEvent*) override;
#else
  void enterEvent(QEvent*) override;
#endif
  void leaveEvent(QEvent*) override;
  void resizeEvent(QResizeEvent*) override;

 private slots:
  void FieldChanged(const int index);
  void OpChanged(const int idx);
  void RelativeValueChanged();
  void Grab();

 private:
  class Overlay;
  friend class Overlay;

  Ui_SmartPlaylistSearchTermWidget *ui_;
  SharedPtr<CollectionBackend> collection_backend_;

  Overlay *overlay_;
  QPropertyAnimation *animation_;
  bool active_;
  bool initialized_;

  SmartPlaylistSearchTerm::Type current_field_type_;
};

#endif  // SMARTPLAYLISTSEARCHTERMWIDGET_H
