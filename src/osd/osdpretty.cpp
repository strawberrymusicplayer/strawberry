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

#include "config.h"

#include <chrono>

#include <QApplication>
#include <QGuiApplication>
#include <QWindow>
#include <QScreen>
#include <QWidget>
#include <QString>
#include <QImage>
#include <QPixmap>
#include <QBitmap>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QColor>
#include <QBrush>
#include <QCursor>
#include <QPen>
#include <QRect>
#include <QPoint>
#include <QFont>
#include <QTimer>
#include <QTimeLine>
#include <QTransform>
#include <QBoxLayout>
#include <QLinearGradient>
#include <QSettings>
#include <QFlags>
#include <QGraphicsOpacityEffect>
#include <QtEvents>

#ifdef HAVE_QPA_QPLATFORMNATIVEINTERFACE
#  include <qpa/qplatformnativeinterface.h>
#endif

#include "core/settings.h"
#include "constants/notificationssettings.h"

#ifdef HAVE_LAYERSHELLQT
#  include <wayland-client.h>
#  include <LayerShellQt/Window>
#endif

#include "osdpretty.h"
#include "ui_osdpretty.h"

#ifdef Q_OS_WIN32
#  include <windows.h>
#  include <dwmapi.h>
#endif

#ifdef Q_OS_WIN32
#  include "utilities/winutils.h"
#endif

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;

namespace {

constexpr int kDropShadowSize = 13;
constexpr int kBorderRadius = 10;
constexpr int kMaxIconSize = 100;
constexpr int kSnapProximity = 20;

#ifdef HAVE_LAYERSHELLQT
void LayerShellRegistryGlobal(void *data, wl_registry *registry, const uint32_t name, const char *interface, const uint32_t version) {

  Q_UNUSED(registry)
  Q_UNUSED(name)
  Q_UNUSED(version)

  if (qstrcmp(interface, "zwlr_layer_shell_v1") == 0) {
    *static_cast<bool*>(data) = true;
  }

}

void LayerShellRegistryGlobalRemove(void *data, wl_registry *registry, const uint32_t name) {

  Q_UNUSED(data)
  Q_UNUSED(registry)
  Q_UNUSED(name)

}

constexpr wl_registry_listener kLayerShellRegistryListener = { LayerShellRegistryGlobal, LayerShellRegistryGlobalRemove };
#endif

}  // namespace

bool OSDPretty::IsLayerShellAvailable() {

#ifdef HAVE_LAYERSHELLQT
  // The compositor doesn't change while running, so only check once.
  static const bool available = []() {
    if (!qGuiApp || !QGuiApplication::platformName().startsWith("wayland"_L1)) return false;
    QNativeInterface::QWaylandApplication *wayland_application = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!wayland_application || !wayland_application->display()) return false;
    wl_display *display = wayland_application->display();

    // List the globals of the compositor on an own event queue, since Qt dispatches the default queue.
    bool layer_shell_found = false;
    wl_event_queue *queue = wl_display_create_queue(display);
    wl_display *display_wrapper = static_cast<wl_display*>(wl_proxy_create_wrapper(display));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(display_wrapper), queue);
    wl_registry *registry = wl_display_get_registry(display_wrapper);
    wl_registry_add_listener(registry, &kLayerShellRegistryListener, &layer_shell_found);
    wl_display_roundtrip_queue(display, queue);
    wl_registry_destroy(registry);
    wl_proxy_wrapper_destroy(display_wrapper);
    wl_event_queue_destroy(queue);

    return layer_shell_found;
  }();

  return available;
#else
  return false;
#endif

}

OSDPretty::OSDPretty(const Mode mode, QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_OSDPretty),
      mode_(mode),
      background_color_(OSDPrettySettings::kPresetBlue),
      background_opacity_(0.85),
      popup_screen_(nullptr),
      disable_duration_(false),
#ifdef HAVE_LAYERSHELLQT
      layer_shell_window_(nullptr),
      opacity_effect_(nullptr),
      layer_shell_dragging_(false),
#endif
      timeout_(new QTimer(this)),
      popup_duration_(5s),
      fading_enabled_(false),
      fader_(new QTimeLine(300, this)),
      toggle_mode_(false) {

  setWindowTitle(u"OSDPretty"_s);
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);

  setAttribute(Qt::WA_TranslucentBackground, true);
  setAttribute(Qt::WA_X11NetWmWindowTypeNotification, true);
  setAttribute(Qt::WA_ShowWithoutActivating, true);

  ui_->setupUi(this);

  // Let the mouse events through to the OSD, the rich text labels accept mouse moves, so the OSD could not be dragged by its text.
  ui_->icon->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  ui_->summary->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  ui_->message->setAttribute(Qt::WA_TransparentForMouseEvents, true);

#ifdef Q_OS_WIN32
  // Don't show the window in the taskbar.  Qt::ToolTip does this too, but it adds an extra ugly shadow.
  int ex_style = GetWindowLong(reinterpret_cast<HWND>(winId()), GWL_EXSTYLE);
  ex_style |= WS_EX_NOACTIVATE;
  SetWindowLong(reinterpret_cast<HWND>(winId()), GWL_EXSTYLE, ex_style);
#endif

  // Mode settings
  switch (mode_) {
    case Mode::Popup:
      setCursor(QCursor(Qt::ArrowCursor));
      break;

    case Mode::Draggable:
      setCursor(QCursor(Qt::OpenHandCursor));
      break;
  }

  // Timeout
  timeout_->setSingleShot(true);
  QObject::connect(timeout_, &QTimer::timeout, this, &OSDPretty::hide);

  ui_->icon->setMaximumSize(kMaxIconSize, kMaxIconSize);

  // Fader
  QObject::connect(fader_, &QTimeLine::valueChanged, this, &OSDPretty::FaderValueChanged);
  QObject::connect(fader_, &QTimeLine::finished, this, &OSDPretty::FaderFinished);

  // Load the show edges and corners
  QImage shadow_edge(u":/pictures/osd_shadow_edge.png"_s);
  QImage shadow_corner(u":/pictures/osd_shadow_corner.png"_s);
  for (int i = 0; i < 4; ++i) {
    QTransform rotation = QTransform().rotate(90 * i);
    shadow_edge_[i] = QPixmap::fromImage(shadow_edge.transformed(rotation));
    shadow_corner_[i] = QPixmap::fromImage(shadow_corner.transformed(rotation));
  }
  background_ = QPixmap(u":/pictures/osd_background.png"_s);

  // Set the margins to allow for the drop shadow
  QBoxLayout *l = qobject_cast<QBoxLayout*>(layout());
  QMargins margin = l->contentsMargins();
  margin.setTop(margin.top() + kDropShadowSize);
  margin.setBottom(margin.bottom() + kDropShadowSize);
  margin.setLeft(margin.left() + kDropShadowSize);
  margin.setRight(margin.right() + kDropShadowSize);
  l->setContentsMargins(margin);

  const QList<QScreen*> screens = QGuiApplication::screens();
  for (QScreen *screen : screens) {
    screens_.insert(screen->name(), screen);
    QObject::connect(screen, &QScreen::geometryChanged, this, [this, screen]() { ScreenGeometryChanged(screen); });
    QObject::connect(screen, &QScreen::availableGeometryChanged, this, [this, screen]() { ScreenGeometryChanged(screen); });
  }
  QObject::connect(qApp, &QApplication::screenAdded, this, &OSDPretty::ScreenAdded);
  QObject::connect(qApp, &QApplication::screenRemoved, this, &OSDPretty::ScreenRemoved);

#ifdef HAVE_LAYERSHELLQT
  InitLayerShell();
#endif

  Load();

}

OSDPretty::~OSDPretty() {
  delete ui_;
}

#ifdef HAVE_LAYERSHELLQT
void OSDPretty::InitLayerShell() {

  if (!IsLayerShellAvailable()) return;

  // The layer shell surface is created for the platform window, so create it before the window is shown.
  winId();
  if (!windowHandle()) return;

  layer_shell_window_ = LayerShellQt::Window::get(windowHandle());
  if (!layer_shell_window_) return;

  // Show the OSD above other windows without taking the keyboard focus, and without reserving space on the screen.
  // An exclusive zone of -1 places the OSD relative to the whole screen and not next to panels, so the margins are in the same coordinates as the screen geometry, which PopupGeometry() returns.
  // Qt doesn't know the space panels reserve on Wayland, so with an exclusive zone of 0 the compositor would place the OSD next to them, off by their size from the calculated position.
  layer_shell_window_->setScope(u"strawberry-osd"_s);
  layer_shell_window_->setLayer(LayerShellQt::Window::LayerOverlay);
  layer_shell_window_->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
  layer_shell_window_->setExclusiveZone(-1);

  if (!opacity_effect_) {
    opacity_effect_ = new QGraphicsOpacityEffect(this);
    opacity_effect_->setEnabled(false);
    setGraphicsEffect(opacity_effect_);
  }

}

void OSDPretty::SetLayerShellPosition(QScreen *screen, const QPoint &pos, const bool right_edge, const bool bottom_edge) {

  layer_shell_pos_ = pos;

  // The layer shell surface is placed on the screen of the window, which can only be changed before it is shown.
  if (screen && windowHandle() && windowHandle()->screen() != screen) {
    // A shown surface can't be moved to another screen, for example when the screen is changed in the settings or added back while the OSD is shown, so create it again, which places it on this screen.
    if (windowHandle()->isExposed()) {
      RecreateLayerShellSurface();
      return;
    }
    windowHandle()->setScreen(screen);
  }

  // Anchor to the right and bottom edges for the right and bottom edge positions, so the compositor keeps the OSD at the edge.
  LayerShellQt::Window::Anchors anchors;
  QMargins margins;
  if (right_edge) {
    anchors |= LayerShellQt::Window::AnchorRight;
  }
  else {
    anchors |= LayerShellQt::Window::AnchorLeft;
    margins.setLeft(pos.x());
  }
  if (bottom_edge) {
    anchors |= LayerShellQt::Window::AnchorBottom;
  }
  else {
    anchors |= LayerShellQt::Window::AnchorTop;
    margins.setTop(pos.y());
  }
  layer_shell_window_->setAnchors(anchors);
  layer_shell_window_->setMargins(margins);

  // The compositor applies the anchors and margins with the next commit of the surface, so repaint the OSD to commit them.
  if (isVisible()) update();

}

void OSDPretty::RecreateLayerShellSurface() {

  // A layer shell surface can't be moved to another screen, so hide the OSD and show it again, which creates the surface on popup_screen_ at popup_pos_.
  // Keep the time left before the OSD is hidden, and the opacity if it is fading, since showing it again starts them over.
  const bool timeout_active = timeout_->isActive();
  const int timeout_remaining = timeout_->remainingTime();
  const bool fader_running = fader_->state() == QTimeLine::Running;

  // Show it again without fading in, since it is already shown.
  const bool fading_enabled = fading_enabled_;
  fading_enabled_ = false;
  QWidget::setVisible(false);

  // Also create the platform window again, so it gets a new surface, otherwise the compositor can keep sending mouse positions relative to the surface on the other screen.
  // The layer shell is only set for the platform window when the layer shell window is created, so create that again too.
  delete layer_shell_window_;
  layer_shell_window_ = nullptr;
  if (windowHandle()) {
    windowHandle()->destroy();
  }
  InitLayerShell();

  QWidget::setVisible(true);
  fading_enabled_ = fading_enabled;

  if (timeout_active) {
    timeout_->start(timeout_remaining);
  }
  else {
    timeout_->stop();
  }
  if (fader_running) {
    SetOpacity(fader_->currentValue());
  }

}

QPoint OSDPretty::LayerShellPopupPos(const QRect &geometry, const QPoint &pos) const {

  // -1 means the right or bottom edge, like current_pos(), so the OSD stays at the edge when its size changes.
  return QPoint(pos.x() >= geometry.width() - width() ? -1 : pos.x(), pos.y() >= geometry.height() - height() ? -1 : pos.y());

}
#endif

QRect OSDPretty::PopupGeometry(QScreen *screen) const {

#ifdef HAVE_LAYERSHELLQT
  // The layer shell surface is placed relative to the whole screen, see InitLayerShell().
  if (layer_shell_window_) return screen->geometry();
#endif

  return screen->availableGeometry();

}

void OSDPretty::showEvent(QShowEvent *e) {

  // The settings are not loaded here, they are loaded with ReloadSettings(), otherwise unsaved changes to the preview in the settings dialog would be lost.

  UpdateMaximumSize();

  SetOpacity(fading_enabled_ ? 0.0 : 1.0);

  QWidget::showEvent(e);

  Reposition();

  if (fading_enabled_) {
    fader_->setDirection(QTimeLine::Forward);
    fader_->start();  // Timeout will be started in FaderFinished
  }
  else if (mode_ == Mode::Popup) {
    if (!disable_duration()) {
      timeout_->start(popup_duration_);
    }
    // Ensures it is above when showing the preview
    raise();
  }

}

void OSDPretty::ScreenAdded(QScreen *screen) {

  screens_.insert(screen->name(), screen);
  QObject::connect(screen, &QScreen::geometryChanged, this, [this, screen]() { ScreenGeometryChanged(screen); });
  QObject::connect(screen, &QScreen::availableGeometryChanged, this, [this, screen]() { ScreenGeometryChanged(screen); });

  // Show the OSD on the configured screen again when it is added back, also when it is shown right now.
  if (screen->name() == popup_screen_name_) {
    popup_screen_ = screen;
    if (isVisible()) {
      UpdateMaximumSize();
      Reposition();
    }
  }

}

void OSDPretty::ScreenRemoved(QScreen *screen) {

  // Remove the screen by the pointer and not by the name, since the name of a screen can change after it was added, and the screen would be kept, also remove screens which were already deleted.
  for (QMap<QString, QPointer<QScreen>>::iterator it = screens_.begin(); it != screens_.end();) {
    if (it.value().isNull() || it.value() == screen) {
      it = screens_.erase(it);
    }
    else {
      ++it;
    }
  }
  // Fall back to another screen, but keep the configured screen name, so the OSD is shown on it again when it is added back.
  if (screen == popup_screen_) {
    popup_screen_ = current_screen();
    if (isVisible()) {
      UpdateMaximumSize();
      Reposition();
    }
  }

}

void OSDPretty::ScreenGeometryChanged(QScreen *screen) {

  // Position the OSD again when the size of its screen changes, so it stays on the screen and at the right or bottom edge.
  // This also updates the position of the layer shell surface used for dragging, which the compositor moves when it's anchored to the right or bottom edge.
  if (screen == popup_screen_ && isVisible()) {
    UpdateMaximumSize();
    Reposition();
  }

}

bool OSDPretty::IsTransparencyAvailable() const {

  if (qApp) {
    const QString platform = QGuiApplication::platformName();
#ifdef HAVE_QPA_QPLATFORMNATIVEINTERFACE
    if (platform == "xcb"_L1) {
      // On X11 the window can only be translucent while a compositing manager is running.
      QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
      QScreen *screen = popup_screen_.isNull() ? QGuiApplication::primaryScreen() : popup_screen_.data();
      if (native && screen) {
        return native->nativeResourceForScreen(QByteArray("compositingEnabled"), screen);
      }
    }
#endif
    // Wayland compositors always composite the scene, so translucency is always available.
    if (platform.startsWith("wayland"_L1)) {
      return true;
    }
  }

#ifdef Q_OS_WIN32
  // On Windows translucency requires Desktop Window Manager composition to be enabled.
  BOOL composition_enabled = FALSE;
  if (SUCCEEDED(DwmIsCompositionEnabled(&composition_enabled))) {
    return composition_enabled != FALSE;
  }
#endif

  // For macOS (Quartz) and any other platform we assume translucency is available.
  return true;

}

void OSDPretty::Load() {

  Settings s;
  s.beginGroup(OSDPrettySettings::kSettingsGroup);
  foreground_color_ = QColor(static_cast<QRgb>(s.value(OSDPrettySettings::kForegroundColor, OSDPrettySettings::kDefaultForegroundColor).toInt()));
  background_color_ = QColor(static_cast<QRgb>(s.value(OSDPrettySettings::kBackgroundColor, OSDPrettySettings::kPresetBlue).toInt()));
  background_opacity_ = s.value(OSDPrettySettings::kBackgroundOpacity, OSDPrettySettings::kDefaultBackgroundOpacity).toFloat();
  font_.fromString(s.value(OSDPrettySettings::kFont, QLatin1String(OSDPrettySettings::kDefaultFont)).toString());
  disable_duration_ = s.value(OSDPrettySettings::kDisableDuration, OSDPrettySettings::kDefaultDisableDuration).toBool();
#ifdef Q_OS_WIN32
  fading_enabled_ = s.value(OSDPrettySettings::kFading, true).toBool();
#else
  fading_enabled_ = s.value(OSDPrettySettings::kFading, false).toBool();
#endif

  if (s.contains(OSDPrettySettings::kPopupScreen)) {
    popup_screen_name_ = s.value(OSDPrettySettings::kPopupScreen).toString();
    // The screen can have been deleted without being removed, then it's nullptr.
    if (screens_.contains(popup_screen_name_) && !screens_.value(popup_screen_name_).isNull()) {
      popup_screen_ = screens_.value(popup_screen_name_);
    }
    else {
      // Fall back to another screen, but keep the configured screen name, so ScreenAdded() shows the OSD on it again when it is added back.
      popup_screen_ = current_screen();
    }
  }
  else {
    popup_screen_ = current_screen();
    if (current_screen()) popup_screen_name_ = current_screen()->name();
  }

  if (s.contains(OSDPrettySettings::kPopupPos)) {
    popup_pos_ = s.value(OSDPrettySettings::kPopupPos).toPoint();
  }
  else {
    // Top right corner by default, -1 means the right edge, so it does not depend on the size of the OSD, which is not known before a message is set.
    popup_pos_.setX(-1);
    popup_pos_.setY(0);
  }

  // The screen can have changed while the OSD is shown, update the size limits before set_font() repositions it.
  UpdateMaximumSize();

  set_font(font_);
  set_foreground_color(foreground_color());

  s.endGroup();

}

void OSDPretty::ReloadSettings() {
  Load();
  if (isVisible()) update();
}

QRect OSDPretty::BoxBorder() const {
  return rect().adjusted(kDropShadowSize, kDropShadowSize, -kDropShadowSize, -kDropShadowSize);
}

void OSDPretty::paintEvent(QPaintEvent *e) {

  Q_UNUSED(e)

#ifdef HAVE_LAYERSHELLQT
  // The painted buffer is committed with the pending screen, anchors and margins, so mouse positions are relative to this position from now on.
  layer_shell_committed_screen_ = popup_screen_;
  layer_shell_committed_pos_ = layer_shell_pos_;
#endif

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  QRect box(BoxBorder());

  // Shadow corners
  const int kShadowCornerSize = kDropShadowSize + kBorderRadius;
  p.drawPixmap(0, 0, shadow_corner_[0]);
  p.drawPixmap(width() - kShadowCornerSize, 0, shadow_corner_[1]);
  p.drawPixmap(width() - kShadowCornerSize, height() - kShadowCornerSize, shadow_corner_[2]);
  p.drawPixmap(0, height() - kShadowCornerSize, shadow_corner_[3]);

  // Shadow edges
  p.drawTiledPixmap(kShadowCornerSize, 0, width() - kShadowCornerSize * 2, kDropShadowSize, shadow_edge_[0]);
  p.drawTiledPixmap(width() - kDropShadowSize, kShadowCornerSize, kDropShadowSize, height() - kShadowCornerSize * 2, shadow_edge_[1]);
  p.drawTiledPixmap(kShadowCornerSize, height() - kDropShadowSize, width() - kShadowCornerSize * 2, kDropShadowSize, shadow_edge_[2]);
  p.drawTiledPixmap(0, kShadowCornerSize, kDropShadowSize, height() - kShadowCornerSize * 2, shadow_edge_[3]);

  // Box background
  p.setBrush(background_color_);
  p.setPen(QPen());
  p.setOpacity(background_opacity_);
  p.drawRoundedRect(box, kBorderRadius, kBorderRadius);

  // Background pattern
  QPainterPath background_path;
  background_path.addRoundedRect(box, kBorderRadius, kBorderRadius);
  p.setClipPath(background_path);
  p.setOpacity(1.0);
  p.drawPixmap(box.right() - background_.width(), box.bottom() - background_.height(), background_);
  p.setClipping(false);

  // Gradient overlay
  QLinearGradient gradient(0, 0, 0, height());
  gradient.setColorAt(0, QColor(255, 255, 255, 130));
  gradient.setColorAt(1, QColor(255, 255, 255, 50));
  p.setBrush(gradient);
  p.drawRoundedRect(box, kBorderRadius, kBorderRadius);

  // Box border
  p.setBrush(QBrush());
  p.setPen(QPen(background_color_.darker(150), 2));
  p.drawRoundedRect(box, kBorderRadius, kBorderRadius);

}

void OSDPretty::SetMessage(const QString &summary, const QString &message, const QImage &image) {

  if (!image.isNull()) {
    QImage scaled_image = image.scaled(static_cast<int>(kMaxIconSize * devicePixelRatioF()), static_cast<int>(kMaxIconSize * devicePixelRatioF()), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled_image.setDevicePixelRatio(devicePixelRatioF());
    ui_->icon->setPixmap(QPixmap::fromImage(scaled_image));
    ui_->icon->show();
  }
  else {
    ui_->icon->hide();
  }

  ui_->summary->setText(summary);
  ui_->message->setText(message);

  if (isVisible()) Reposition();

}

// Set the desired message and then show the OSD
void OSDPretty::ShowMessage(const QString &summary, const QString &message, const QImage &image) {

  SetMessage(summary, message, image);

  if (isVisible() && mode_ == Mode::Popup) {
    // The OSD is already visible, toggle or restart the timer
    if (toggle_mode()) {
      set_toggle_mode(false);
      // If timeout is disabled, timer hadn't been started
      if (!disable_duration()) {
        timeout_->stop();
      }
      hide();
    }
    else {
      if (!disable_duration()) {
        timeout_->start(popup_duration_);  // Restart the timer
      }
    }
  }
  else {
    if (toggle_mode()) {
      set_toggle_mode(false);
    }
    // The OSD is not visible, show it
    show();
  }

}

void OSDPretty::setVisible(bool visible) {

  if (!visible && fading_enabled_ && fader_->direction() == QTimeLine::Forward) {
    fader_->setDirection(QTimeLine::Backward);
    fader_->start();
  }
  else {
    QWidget::setVisible(visible);
  }

}

void OSDPretty::FaderFinished() {

  if (fader_->direction() == QTimeLine::Backward) {
    hide();
  }
  else if (mode_ == Mode::Popup && !disable_duration()) {
    timeout_->start(popup_duration_);
  }

}

void OSDPretty::FaderValueChanged(const qreal value) {
  SetOpacity(value);
}

void OSDPretty::SetOpacity(const qreal opacity) {

#ifdef HAVE_LAYERSHELLQT
  // Window opacity is not supported on Wayland, so draw the OSD and its children with the opacity instead.
  if (opacity_effect_) {
    opacity_effect_->setOpacity(opacity);
    opacity_effect_->setEnabled(opacity < 1.0);
    return;
  }
#endif

  setWindowOpacity(opacity);

}

void OSDPretty::UpdateMaximumSize() {

  // Get the resolution of the screen the OSD is shown on
  QScreen *screen = popup_screen_ ? popup_screen_.data() : current_screen();
  if (!screen) return;

  const QRect resolution = screen->availableGeometry();
  // Leave 200 px for icon
  ui_->summary->setMaximumWidth(resolution.width() - 200);
  ui_->message->setMaximumWidth(resolution.width() - 200);
  // Set maximum size for the OSD, a little margin here too
  setMaximumSize(resolution.width() - 100, resolution.height() - 100);

}

void OSDPretty::Reposition() {

  // The screen was deleted without being removed, use the configured screen if it's available, otherwise another screen.
  if (!popup_screen_) {
    const QPointer<QScreen> configured_screen = screens_.value(popup_screen_name_);
    popup_screen_ = configured_screen ? configured_screen.data() : current_screen();
    UpdateMaximumSize();
  }

  // Make the OSD the proper size
  layout()->activate();
  resize(sizeHint());

  // Work out where to place the OSD.  -1 for x or y means "on the right or bottom edge".
  if (popup_screen_) {

    QRect geometry = PopupGeometry(popup_screen_);

    int x = popup_pos_.x() < 0 ? geometry.right() + 1 - width() : geometry.left() + popup_pos_.x();
    int y = popup_pos_.y() < 0 ? geometry.bottom() + 1 - height() : geometry.top() + popup_pos_.y();

    // Keep it on the screen, screens left of or above the primary screen have negative coordinates.
    // QRect::right() and bottom() are the last pixel inside the screen, so add 1 for the OSD to reach the edge.
    // qMax() keeps the range valid if the OSD is still larger than the screen.
    x = qBound(geometry.left(), x, qMax(geometry.left(), geometry.right() + 1 - width()));
    y = qBound(geometry.top(), y, qMax(geometry.top(), geometry.bottom() + 1 - height()));
#ifdef HAVE_LAYERSHELLQT
    if (layer_shell_window_) {
      SetLayerShellPosition(popup_screen_, QPoint(x - geometry.left(), y - geometry.top()), popup_pos_.x() < 0, popup_pos_.y() < 0);
    }
    else
#endif
    {
      move(x, y);
    }
  }

  const bool transparency_available = IsTransparencyAvailable();

#ifndef Q_OS_WIN32
  // The mask is only needed without a compositing window manager, then it is set as XShape mask.
  if (transparency_available) {
    clearMask();
    return;
  }
#endif

  // Create a mask for the actual area of the OSD
  QBitmap mask(size());
  mask.clear();

  QPainter p(&mask);
  p.setBrush(Qt::color1);
  p.drawRoundedRect(BoxBorder().adjusted(-1, -1, 0, 0), kBorderRadius, kBorderRadius);
  p.end();

  if (transparency_available) {
    clearMask();
  }
  else {
    setMask(mask);
  }

  // On windows, enable blurbehind on the masked area
#ifdef Q_OS_WIN32
  Utilities::enableBlurBehindWindow(windowHandle(), QRegion(mask));
#endif

}

void OSDPretty::enterEvent(QEnterEvent *e) {

  Q_UNUSED(e)

  if (mode_ == Mode::Popup) {
    SetOpacity(0.25);
  }

}

void OSDPretty::leaveEvent(QEvent *e) {

  Q_UNUSED(e)

  SetOpacity(1.0);

}

void OSDPretty::mousePressEvent(QMouseEvent *e) {

  if (mode_ == Mode::Popup) {
    hide();
  }
  else {
#ifdef HAVE_LAYERSHELLQT
    // The global mouse position is not known for a layer shell surface, so drag by the position in the OSD.
    if (layer_shell_window_) {
      original_window_pos_ = layer_shell_pos_;
      drag_start_pos_ = e->position().toPoint();
      layer_shell_dragging_ = true;
      return;
    }
#endif
    original_window_pos_ = pos();
    drag_start_pos_ = e->globalPosition().toPoint();
  }

}

void OSDPretty::mouseMoveEvent(QMouseEvent *e) {

#ifdef HAVE_LAYERSHELLQT
  if (mode_ == Mode::Draggable && layer_shell_window_) {
    if (!layer_shell_dragging_) return;
    // The mouse position is relative to the position of the surface which was last committed, not to a position or screen which is not committed yet.
    QScreen *screen = popup_screen_ ? popup_screen_.data() : current_screen();
    if (!screen) return;
    QScreen *committed_screen = layer_shell_committed_screen_ ? layer_shell_committed_screen_.data() : screen;
    const QPoint global_pos = PopupGeometry(committed_screen).topLeft() + layer_shell_committed_pos_ + e->position().toPoint();

    // When dragged to another screen, update the size limits for it first, so the OSD fits on it before it is kept within its bounds.
    QScreen *new_screen = QGuiApplication::screenAt(global_pos);
    if (new_screen && new_screen != screen) {
      popup_screen_ = new_screen;
      popup_screen_name_ = new_screen->name();
      UpdateMaximumSize();
      layout()->activate();
      resize(sizeHint());
    }
    else {
      new_screen = screen;
    }

    const QRect geometry = PopupGeometry(new_screen);
    QPoint new_pos = global_pos - drag_start_pos_ - geometry.topLeft();
    new_pos.setX(qBound(0, new_pos.x(), qMax(0, geometry.width() - width())));
    new_pos.setY(qBound(0, new_pos.y(), qMax(0, geometry.height() - height())));

    // Snap to center
    const int snap_x = geometry.width() / 2 - width() / 2;
    if (new_pos.x() > snap_x - kSnapProximity && new_pos.x() < snap_x + kSnapProximity) {
      new_pos.setX(snap_x);
    }

    if (new_screen == screen) {
      SetLayerShellPosition(screen, new_pos, false, false);
    }
    else {
      // The surface is created again on the other screen, and the mouse positions the compositor sends for it after this depend on the compositor, so end the drag here.
      // The OSD is shown under the mouse on the other screen, where it can be dragged further.
      // Save the position like when the drag ends, at the edges as -1, so it doesn't depend on the size of the preview.
      layer_shell_dragging_ = false;
      popup_pos_ = LayerShellPopupPos(geometry, new_pos);
      RecreateLayerShellSurface();
      Q_EMIT PositionChanged();
    }
    return;
  }
#endif

  if (mode_ == Mode::Draggable) {
    QPoint delta = e->globalPosition().toPoint() - drag_start_pos_;
    QPoint new_pos = original_window_pos_ + delta;

    // Keep it to the bounds of the desktop
    QScreen *screen = current_screen(e->globalPosition().toPoint());
    if (!screen) return;

    // When dragged to another screen, update the size limits for it first, so the OSD fits on it before it is kept within its bounds.
    if (screen != popup_screen_) {
      popup_screen_ = screen;
      popup_screen_name_ = screen->name();
      UpdateMaximumSize();
      layout()->activate();
      resize(sizeHint());
    }

    QRect geometry = screen->availableGeometry();

    // qMax() keeps the range valid if the OSD is still larger than the screen.
    new_pos.setX(qBound(geometry.left(), new_pos.x(), qMax(geometry.left(), geometry.right() + 1 - width())));
    new_pos.setY(qBound(geometry.top(), new_pos.y(), qMax(geometry.top(), geometry.bottom() + 1 - height())));

    // Snap to center
    int snap_x = geometry.center().x() - width() / 2;
    if (new_pos.x() > snap_x - kSnapProximity && new_pos.x() < snap_x + kSnapProximity) {
      new_pos.setX(snap_x);
    }

    move(new_pos);
  }

}

void OSDPretty::mouseReleaseEvent(QMouseEvent *e) {

  Q_UNUSED(e)

#ifdef HAVE_LAYERSHELLQT
  if (mode_ == Mode::Draggable && layer_shell_window_) {
    if (!layer_shell_dragging_) return;
    layer_shell_dragging_ = false;
    QScreen *screen = popup_screen_ ? popup_screen_.data() : current_screen();
    if (!screen) return;
    popup_screen_ = screen;
    popup_screen_name_ = screen->name();
    popup_pos_ = LayerShellPopupPos(PopupGeometry(screen), layer_shell_pos_);
    Q_EMIT PositionChanged();
    return;
  }
#endif

  if (current_screen() && mode_ == Mode::Draggable) {
    popup_screen_ = current_screen();
    popup_screen_name_ = current_screen()->name();
    popup_pos_ = current_pos();
    Q_EMIT PositionChanged();
  }

}

QScreen *OSDPretty::current_screen(const QPoint pos) const {

  QScreen *screen = QGuiApplication::screenAt(pos);
  if (!screen) screen = QGuiApplication::primaryScreen();

  return screen;

}

QScreen *OSDPretty::current_screen() const { return current_screen(pos()); }

QPoint OSDPretty::current_pos() const {

  if (current_screen()) {
    QRect geometry = current_screen()->availableGeometry();

    int x = pos().x() >= geometry.right() + 1 - width() ? -1 : pos().x() - geometry.left();
    int y = pos().y() >= geometry.bottom() + 1 - height() ? -1 : pos().y() - geometry.top();

    return QPoint(x, y);
  }

  return QPoint(0, 0);

}

void OSDPretty::set_background_color(const QRgb color) {
  background_color_ = color;
  if (isVisible()) update();
}

void OSDPretty::set_background_opacity(const qreal opacity) {
  background_opacity_ = opacity;
  if (isVisible()) update();
}

void OSDPretty::set_foreground_color(const QRgb color) {

  foreground_color_ = QColor(color);

  QPalette p;
  p.setColor(QPalette::WindowText, foreground_color_);

  ui_->summary->setPalette(p);
  ui_->message->setPalette(p);

}

void OSDPretty::set_popup_duration(const int msec) {
  popup_duration_ = std::chrono::milliseconds(msec);
}

void OSDPretty::set_font(const QFont &font) {

  font_ = font;

  // Update the UI
  ui_->summary->setFont(font);
  ui_->message->setFont(font);
  // Now adjust OSD size so everything fits
  ui_->verticalLayout->activate();
  resize(sizeHint());
  // Update the position after font change
  Reposition();

}
