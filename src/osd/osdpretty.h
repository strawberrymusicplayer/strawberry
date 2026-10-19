/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <chrono>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QMap>
#include <QPointer>
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
class QGraphicsOpacityEffect;

class Ui_OSDPretty;

#ifdef HAVE_LAYERSHELLQT
namespace LayerShellQt {
class Window;
}
#endif

class OSDPretty : public QWidget {
  Q_OBJECT

 public:
  enum class Mode {
    Popup,
    Draggable
  };

  explicit OSDPretty(const Mode mode, QWidget *parent = nullptr);
  ~OSDPretty() override;

  bool IsTransparencyAvailable() const;

  // Wayland compositors don't let windows position themselves, so on Wayland the OSD is shown as a layer shell surface, which not all compositors support.
  static bool IsLayerShellAvailable();

  void SetMessage(const QString &summary, const QString &message, const QImage &image);
  void ShowMessage(const QString &summary, const QString &message, const QImage &image);

  // Popup duration in milliseconds.  Only used in Mode::Popup.
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
  QFont osd_font() const { return font_; }
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
  void enterEvent(QEnterEvent *e) override;
  void leaveEvent(QEvent *e) override;
  void mousePressEvent(QMouseEvent *e) override;
  void showEvent(QShowEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;

 private:
  void Reposition();
  void UpdateMaximumSize();
  void SetOpacity(const qreal opacity);
#ifdef HAVE_LAYERSHELLQT
  void InitLayerShell();
  void SetLayerShellPosition(QScreen *screen, const QPoint &pos, const bool right_edge, const bool bottom_edge);
  void RecreateLayerShellSurface();
  QPoint LayerShellPopupPos(const QRect &geometry, const QPoint &pos) const;
#endif
  QRect PopupGeometry(QScreen *screen) const;
  void Load();

  QRect BoxBorder() const;

 private Q_SLOTS:
  void FaderValueChanged(const qreal value);
  void FaderFinished();
  void ScreenAdded(QScreen *screen);
  void ScreenRemoved(QScreen *screen);
  void ScreenGeometryChanged(QScreen *screen);

 private:
  Ui_OSDPretty *ui_;

  Mode mode_;

  // Settings loaded from Settings
  QColor foreground_color_;
  QColor background_color_;
  qreal background_opacity_;
  QString popup_screen_name_;
  QPoint popup_pos_;
  QPointer<QScreen> popup_screen_;
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

#ifdef HAVE_LAYERSHELLQT
  // The layer shell surface on Wayland, it is positioned by its position on the screen, since the position of the window and the global mouse position are not known.
  LayerShellQt::Window *layer_shell_window_;
  QPoint layer_shell_pos_;
  QPointer<QScreen> layer_shell_committed_screen_;
  QPoint layer_shell_committed_pos_;
  QGraphicsOpacityEffect *opacity_effect_;
  bool layer_shell_dragging_;
#endif

  // For timeout of notification
  // The duration is kept separately from the interval of the timer, since the timer is also started with the remaining time when the surface is created again.
  QTimer *timeout_;
  std::chrono::milliseconds popup_duration_;

  // For fading
  bool fading_enabled_;
  QTimeLine *fader_;

  // Toggling requested, we have to show or hide the OSD
  bool toggle_mode_;

  QMap<QString, QPointer<QScreen>> screens_;
};

#endif  // OSDPRETTY_H
