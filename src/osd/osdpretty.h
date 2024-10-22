/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef OSDPRETTY_H
#define OSDPRETTY_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QMap>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QColor>
#include <QFont>
#include <QPoint>
#include <QRect>
#include <QRgb>

class QScreen;
class QTimer;
class QTimeLine;
class QEvent;
class QMouseEvent;
class QPaintEvent;
class QShowEvent;
class QEnterEvent;

class Ui_OSDPretty;

class OSDPretty : public QWidget {
  Q_OBJECT

 public:
  enum class Mode {
    Popup,
    Draggable
  };

  explicit OSDPretty(const Mode mode, QWidget *parent = nullptr);
  ~OSDPretty() override;

  bool IsTransparencyAvailable();

  void SetMessage(const QString &summary, const QString &message, const QImage &image);
  void ShowMessage(const QString &summary, const QString &message, const QImage &image);

  // Popup duration in seconds.  Only used in Mode_Popup.
  void set_popup_duration(const int msec);

  // These will get overwritten when ReloadSettings() is called
  void set_foreground_color(const QRgb color);
  void set_background_color(const QRgb color);
  void set_background_opacity(const qreal opacity);
  void set_font(const QFont &font);

  QRgb foreground_color() const { return foreground_color_.rgb(); }
  QRgb background_color() const { return background_color_.rgb(); }
  qreal background_opacity() const { return background_opacity_; }
  QString popup_screen() const { return popup_screen_name_; }
  QPoint popup_pos() const { return popup_pos_; }
  QFont font() const { return font_; }
  bool disable_duration() const { return disable_duration_; }
  bool fading() const { return fading_enabled_; }

  // When the user has been moving the popup, use these to get its current position and screen.
  // Note that these return invalid values if the popup is hidden.
  QScreen *current_screen() const;
  QScreen *current_screen(const QPoint pos) const;
  QPoint current_pos() const;

  // QWidget
  void setVisible(bool visible) override;

  bool toggle_mode() const { return toggle_mode_; }
  void set_toggle_mode(const bool toggle_mode) { toggle_mode_ = toggle_mode; }

 Q_SIGNALS:
  void PositionChanged();

 public Q_SLOTS:
  void ReloadSettings();

 protected:
  void paintEvent(QPaintEvent *e) override;
  void enterEvent(QEnterEvent*) override;
  void leaveEvent(QEvent*) override;
  void mousePressEvent(QMouseEvent *e) override;
  void showEvent(QShowEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;

 private:
  void Reposition();
  void Load();

  QRect BoxBorder() const;

 private Q_SLOTS:
  void FaderValueChanged(const qreal value);
  void FaderFinished();
  void ScreenAdded(QScreen *screen);
  void ScreenRemoved(QScreen *screen);

 private:
  Ui_OSDPretty *ui_;

  Mode mode_;

  // Settings loaded from Settings
  QColor foreground_color_;
  QColor background_color_;
  qreal background_opacity_;
  QString popup_screen_name_;
  QPoint popup_pos_;
  QScreen *popup_screen_;
  QFont font_;
  // The OSD is kept always on top until you click (no timer)
  bool disable_duration_;

  // Cached pixmaps
  QPixmap shadow_edge_[4];
  QPixmap shadow_corner_[4];
  QPixmap background_;

  // For dragging the OSD
  QPoint original_window_pos_;
  QPoint drag_start_pos_;

  // For timeout of notification
  QTimer *timeout_;

  // For fading
  bool fading_enabled_;
  QTimeLine *fader_;

  // Toggling requested, we have to show or hide the OSD
  bool toggle_mode_;

  QMap<QString, QScreen*> screens_;
};

#endif  // OSDPRETTY_H
