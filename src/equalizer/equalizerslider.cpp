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

#include "config.h"

#include <cmath>
#include <limits>

#include <QtGlobal>
#include <QWidget>
#include <QString>
#include <QLabel>
#include <QFont>
#include <QFontMetrics>
#include <QPalette>
#include <QEvent>
#ifdef Q_OS_ANDROID
#  include <QDoubleSpinBox>
#  include <QKeyEvent>
#  include <QLineEdit>
#  include <QLocale>
#  include <QSignalBlocker>
#  include <QToolButton>
#  include <QStyle>
#endif

#include "widgets/stickyslider.h"
#include "core/iconloader.h"
#include "equalizerslider.h"
#include "ui_equalizerslider.h"

namespace {
constexpr int kPreampMutedValue = -100;
}  // namespace

#ifdef Q_OS_ANDROID
namespace {
constexpr char kGainPositiveColor[] = "#008000";
constexpr char kGainNegativeColor[] = "#ff0000";
constexpr char kGainZeroColor[] = "#0000ff";
constexpr char kGainSelectionColor[] = "#e0f2ff";
constexpr char kGainFocusBorderColor[] = "#87cefa";
}  // namespace

// Editable dB value, colored by the sign of the gain.
class EqualizerGainSpinBox : public QDoubleSpinBox {
 public:
  explicit EqualizerGainSpinBox(QWidget *parent) : QDoubleSpinBox(parent) {
    lineEdit()->setStyleSheet(QStringLiteral(
        "QLineEdit { background: transparent; border: 1px solid transparent; border-radius: 8px; selection-background-color: %1; }"
        "QLineEdit:focus, QLineEdit[gainSelected=\"true\"] { background: %1; border-color: %2; }").arg(QLatin1String(kGainSelectionColor), QLatin1String(kGainFocusBorderColor)));
    QObject::connect(lineEdit(), &QLineEdit::selectionChanged, this, [this]() {
      lineEdit()->setProperty("gainSelected", lineEdit()->hasSelectedText());
      lineEdit()->style()->unpolish(lineEdit());
      lineEdit()->style()->polish(lineEdit());
      lineEdit()->update();
      setProperty("gainSelected", lineEdit()->hasSelectedText());
      style()->unpolish(this);
      style()->polish(this);
      UpdateColor();
      update();
    });
    QObject::connect(this, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { UpdateColor(); });
    UpdateColor();
  }

  void UpdateColor() {
    const QColor color(QLatin1String(value() > 0 ? kGainPositiveColor : value() < 0 ? kGainNegativeColor : kGainZeroColor));
    QPalette palette = lineEdit()->palette();
    palette.setColor(QPalette::All, QPalette::Text, color);
    palette.setColor(QPalette::All, QPalette::HighlightedText, color);
    palette.setColor(QPalette::All, QPalette::Highlight, QColor(QLatin1String(kGainSelectionColor)));
    lineEdit()->setPalette(palette);
  }

 protected:
  QString textFromValue(const double value) const override {
    const QString text = QDoubleSpinBox::textFromValue(value);
    return value > 0 ? QStringLiteral("+") + text : text;
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
      interpretText();
      event->accept();
      return;
    }
    QDoubleSpinBox::keyPressEvent(event);
  }
};
#endif

EqualizerSlider::EqualizerSlider(const QString &label, QWidget *parent, const bool preamp)
    : QWidget(parent),
      ui_(new Ui_EqualizerSlider),
      preamp_(preamp) {

  ui_->setupUi(this);
  ui_->band->setText(label);

  QFontMetrics fm = ui_->gain->fontMetrics();
  ui_->gain->setMinimumWidth(fm.horizontalAdvance(tr("%1 dB").arg(-99.99)));
  ui_->gain->setText(tr("%1 dB").arg(0));  // Gain [dB]

  ui_->slider->setValue(0);
#ifdef Q_OS_ANDROID
  while (QLayoutItem *item = ui_->horizontalLayout->takeAt(0)) delete item;
  ui_->slider->hide();
  ui_->gain->hide();
  ui_->verticalLayout->removeWidget(ui_->band);
  ui_->verticalLayout->removeWidget(ui_->gain);
  ui_->verticalLayout->setSpacing(4);
  gain_editor_ = new EqualizerGainSpinBox(this);
  gain_editor_->setDecimals(2);
  gain_editor_->setRange(preamp_ ? -41.0 : -24.0, preamp_ ? 20.0 * std::log10(2.0) : 12.0);
  gain_editor_->setSingleStep(1.0);
  if (preamp_) gain_editor_->setSpecialValueText(tr("Muted"));
  gain_editor_->setKeyboardTracking(false);
  gain_editor_->setAccessibleName(label);
  gain_editor_->setAlignment(Qt::AlignCenter);
  gain_editor_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  gain_editor_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  gain_editor_->setStyleSheet(QStringLiteral(
      "QDoubleSpinBox { padding: 2px; border: 1px solid transparent; border-radius: 8px; background: transparent; }"
      "QDoubleSpinBox:focus, QDoubleSpinBox[gainSelected=\"true\"] { border-color: #87cefa; background: #e0f2ff; }"
      "QDoubleSpinBox QLineEdit { background: transparent; selection-background-color: #e0f2ff; }"));
  QToolButton *up = new QToolButton(this);
  up->setIcon(IconLoader::Load(QStringLiteral("go-up")));
  up->setAccessibleName(tr("Increase %1").arg(label));
  QToolButton *down = new QToolButton(this);
  down->setIcon(IconLoader::Load(QStringLiteral("go-down")));
  down->setAccessibleName(tr("Decrease %1").arg(label));
  for (QToolButton *button : {up, down}) {
    button->setMinimumSize(48, 48);
    button->setAutoRaise(true);
    button->setIconSize(QSize(36, 36));
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setAutoRepeat(true);
  }
  ui_->verticalLayout->addWidget(up);
  ui_->verticalLayout->addWidget(gain_editor_);
  QLabel *unit = new QLabel(tr("dB"), this);
  unit->setAlignment(Qt::AlignCenter);
  QFont unit_font = unit->font();
  unit_font.setBold(false);
  unit->setFont(unit_font);
  QPalette unit_palette = unit->palette();
  unit_palette.setColor(QPalette::All, QPalette::WindowText, Qt::black);
  unit->setPalette(unit_palette);
  ui_->verticalLayout->addWidget(unit);
  ui_->verticalLayout->addWidget(down);
  ui_->verticalLayout->addWidget(ui_->band);
  QObject::connect(up, &QToolButton::clicked, gain_editor_, &QDoubleSpinBox::stepUp);
  QObject::connect(down, &QToolButton::clicked, gain_editor_, &QDoubleSpinBox::stepDown);
  QObject::connect(gain_editor_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EqualizerSlider::set_gain_db);
  ui_->band->setWordWrap(true);
  QFont band_font = ui_->band->font();
  band_font.setBold(true);
  ui_->band->setFont(band_font);
  ui_->band->setForegroundRole(QPalette::Text);
  QPalette band_palette = ui_->band->palette();
  band_palette.setColor(QPalette::All, QPalette::Text, Qt::black);
  ui_->band->setPalette(band_palette);
  QFont font = gain_editor_->font();
  font.setBold(true);
  font.setPointSizeF(font.pointSizeF() * 1.3);
  gain_editor_->setFont(font);
  const QFontMetrics editor_metrics = gain_editor_->fontMetrics();
  const int text_width = qMax(editor_metrics.horizontalAdvance(gain_editor_->locale().toString(-40.0, 'f', 2) + gain_editor_->suffix()),
                             editor_metrics.horizontalAdvance(tr("Muted")));
  gain_editor_->setMinimumSize(qMax(48, text_width + 4), 48);
  setMinimumHeight(0);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
#endif

  QObject::connect(ui_->slider, &QSlider::valueChanged, this, &EqualizerSlider::OnValueChanged);
  setToolTip(isEnabled() ? QString() : tr("Enable equalizer first"));

}

EqualizerSlider::~EqualizerSlider() {
  delete ui_;
}

void EqualizerSlider::changeEvent(QEvent *event) {

  QWidget::changeEvent(event);
  if (event->type() == QEvent::EnabledChange) {
    setToolTip(isEnabled() ? QString() : tr("Enable equalizer first"));
  }

}

void EqualizerSlider::OnValueChanged(const int value) {

  // The conversion to dB matches GstEnginePipeline::UpdateEqualizer().
  const double gain = gain_db();
  if (preamp_ && value == kPreampMutedValue) {
    ui_->gain->setText(tr("Muted"));
  }
  else {
    ui_->gain->setText(tr("%1 dB").arg(preamp_ ? std::round(gain * 10.0) / 10.0 : gain));
  }
#ifdef Q_OS_ANDROID
  UpdateGainEditor();
#endif
  Q_EMIT ValueChanged(value);

}

int EqualizerSlider::value() const {
  return ui_->slider->value();
}

void EqualizerSlider::set_value(const int value) {
  ui_->slider->setValue(value);
}

double EqualizerSlider::gain_db() const {

  if (preamp_) {
    return value() == kPreampMutedValue ? -std::numeric_limits<double>::infinity() : 20.0 * std::log10((value() + 100) * 0.01);
  }
  return value() * (value() < 0 ? 0.24 : 0.12);

}

void EqualizerSlider::set_gain_db(const double gain) {

  const int engine_value = preamp_ ? (gain <= -41.0 ? kPreampMutedValue : qRound(100.0 * std::pow(10.0, gain / 20.0)) - 100)
                                   : qRound(gain / (gain < 0 ? 0.24 : 0.12));
  set_value(engine_value);
#ifdef Q_OS_ANDROID
  // An unchanged engine value emits no change, but the editor must still show the rounded gain.
  UpdateGainEditor();
#endif

}

#ifdef Q_OS_ANDROID
void EqualizerSlider::UpdateGainEditor() {

  const QSignalBlocker blocker(gain_editor_);
  gain_editor_->setValue(preamp_ && value() == kPreampMutedValue ? gain_editor_->minimum() : gain_db());
  gain_editor_->UpdateColor();

}
#endif
