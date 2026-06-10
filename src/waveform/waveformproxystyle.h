/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry contributors
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

#ifndef WAVEFORMPROXYSTYLE_H
#define WAVEFORMPROXYSTYLE_H

#include <QtGlobal>
#include <QObject>
#include <QProxyStyle>
#include <QByteArray>
#include <QPixmap>
#include <QPalette>
#include <QRect>
#include <QPoint>
#include <QStyle>

class QPainter;
class QSlider;
class QStyleOptionComplex;
class QStyleOptionSlider;
class QTimeLine;
class QWidget;
class QEvent;

// A QProxyStyle that intercepts QSlider groove painting to draw a whole-track amplitude waveform, a played/unplayed color split and a playhead cursor line.
//
// Built unconditionally (no compile gate): the waveform has no FFTW3 dependency.
// All rendering is driven by WaveformRenderer::RenderToPixmap; this class owns the lazy pixmap cache and the fade state machine.
// The right-click seekbar menu is owned by TrackSlider, which calls SetShowWaveform directly.
// Mutual exclusivity with the moodbar is coordinated by TrackSlider via the WaveformShow signal, so this class never references MoodbarProxyStyle.
class WaveformProxyStyle : public QProxyStyle {
  Q_OBJECT

 public:
  explicit WaveformProxyStyle(QSlider *slider, QObject *parent = nullptr);

  // Margin inset applied to the groove rect when the waveform is active.
  // Public so tests can assert the groove geometry against the symbol, not a literal.
  static constexpr int kWaveformMarginSize = 3;

  void ReloadSettings();

  // Whether the waveform would paint.
  // Lets TrackSlider pick the active seekbar renderer and reflect the current mode.
  bool is_showing() const { return show_; }

  // QProxyStyle
  void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const override;
  QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *widget) const override;

  // QObject
  bool eventFilter(QObject *object, QEvent *event) override;

 public Q_SLOTS:
  // An empty byte array means there's no waveform, so just show a normal slider.
  void SetWaveformData(const QByteArray &data);

  // If the waveform is disabled then a normal slider will always be shown.
  // Emits WaveformShow when the state changes so TrackSlider can enforce mutual exclusivity against the moodbar, keeping the same mainwindow:954 connection.
  void SetShowWaveform(const bool show);

 Q_SIGNALS:
  // Emitted by SetShowWaveform when the show state changes, so TrackSlider can enforce mutual exclusivity against the moodbar via mainwindow:954.
  void WaveformShow(const bool show);

 private:
  enum class State {
    WaveformOn,
    WaveformOff,
    FadingToOn,
    FadingToOff
  };

 private:
  void NextState();

  void Render(const ComplexControl control, const QStyleOptionSlider *option, QPainter *painter, const QWidget *widget);
  void EnsureWaveformRendered();

 private Q_SLOTS:
  void FaderValueChanged(const qreal value);

 private:
  QSlider *slider_;

  bool show_;
  QColor color_;
  QByteArray data_;

  State state_;
  QTimeLine *fade_timeline_;

  QPixmap fade_source_;
  QPixmap fade_target_;

  bool waveform_pixmap_dirty_;
  // Two position-independent bar pixmaps composited around the live split: the unplayed pixmap (Highlight) and the played pixmap (Highlight at reduced alpha).
  // Both are rendered at the device-pixel-ratio size.
  QPixmap waveform_pixmap_unplayed_;
  QPixmap waveform_pixmap_played_;
};

#endif  // WAVEFORMPROXYSTYLE_H
