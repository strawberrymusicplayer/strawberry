/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>

#include <QtGlobal>
#include <QWidget>
#include <QDialog>
#include <QDataStream>
#include <QList>
#include <QVariant>
#include <QString>
#include <QFrame>
#include <QKeySequence>
#include <QLayout>
#include <QLineEdit>
#include <QShortcut>
#include <QSlider>
#include <QMessageBox>
#include <QInputDialog>
#include <QToolButton>
#include <QCheckBox>
#include <QComboBox>
#include <QBoxLayout>
#include <QSettings>
#ifdef Q_OS_ANDROID
#  include <QKeyEvent>
#  include <QLabel>
#  include <QPushButton>
#  include <QRadioButton>
#  include <QScrollArea>
#  include <QScrollBar>
#  include <QScroller>
#  include <QSignalBlocker>
#  include <QStackedWidget>
#  include <QStyle>
#  include <QTimer>
#  include "settings/androidsettingspage.h"
#endif

#include "core/iconloader.h"
#include "core/settings.h"
#include "equalizer.h"
#include "equalizerslider.h"
#include "ui_equalizer.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "Equalizer";
constexpr char kPresets[] = "presets";
constexpr char kName[] = "name";
constexpr char kParams[] = "params";
constexpr char kSelectedPreset[] = "selected_preset";
constexpr char kEnabled[] = "enabled";
constexpr char kEnableStereoBalancer[] = "enable_stereo_balancer";
constexpr char kStereoBalance[] = "stereo_balance";
constexpr char kCustom[] = "custom";
const char *kGainText[] = { "60", "170", "310", "600", "1k", "3k", "6k", "12k", "14k", "16k" };
}  // namespace

Equalizer::Equalizer(QWidget *parent)
    : QDialog(parent),
      ui_(new Ui_Equalizer),
      loading_(false) {

  ui_->setupUi(this);

  // Icons
  ui_->preset_del->setIcon(IconLoader::Load(u"list-remove"_s));
#ifdef Q_OS_ANDROID
  ui_->preset_del->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
#endif
  ui_->preset_save->setIcon(IconLoader::Load(u"document-save"_s));

  preamp_ = AddSlider(tr("Pre-amp"), true);

#ifndef Q_OS_ANDROID
  QFrame *line = new QFrame(ui_->slider_container);
  line->setFrameShape(QFrame::VLine);
  line->setFrameShadow(QFrame::Sunken);
  ui_->slider_container->layout()->addWidget(line);
#endif

  for (int i = 0; i < kBands; ++i) gain_[i] = AddSlider(QString::fromLatin1(kGainText[i]));
#ifdef Q_OS_ANDROID
  SetupAndroidLayout();
#endif

  // Must be done before the signals are connected
  ReloadSettings();
#ifdef Q_OS_ANDROID
  UpdateAndroidPreset();
#endif

  QObject::connect(ui_->enable_equalizer, &QCheckBox::toggled, this, &Equalizer::EqualizerEnabledChangedSlot);

  QObject::connect(ui_->preset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, QOverload<int>::of(&Equalizer::PresetChanged));
  QObject::connect(ui_->preset_save, &QToolButton::clicked, this, &Equalizer::SavePreset);
  QObject::connect(ui_->preset_del, &QToolButton::clicked, this, &Equalizer::DelPreset);

  QObject::connect(ui_->enable_stereo_balancer, &QCheckBox::toggled, this, &Equalizer::StereoBalancerEnabledChangedSlot);
  QObject::connect(ui_->stereo_balance_slider, &QSlider::valueChanged, this, &Equalizer::StereoBalanceSliderChanged);

  QShortcut *close = new QShortcut(QKeySequence::Close, this);
  QObject::connect(close, &QShortcut::activated, this, &Equalizer::close);

}

Equalizer::~Equalizer() {
  delete ui_;
}

void Equalizer::ReloadSettings() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  presets_.clear();
  ui_->preset->clear();

  const int count = s.beginReadArray(kPresets);
  for (int i = 0; i < count; ++i) {
    s.setArrayIndex(i);
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    AddPreset(s.value(kName).toString(), s.value(kParams).value<Equalizer::Params>());
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif
  }
  s.endArray();

  if (count == 0) LoadDefaultPresets();

  const QString selected_preset = s.value(kSelectedPreset, QLatin1String(kCustom)).toString();
  const QString selected_preset_display_name = tr(qUtf8Printable(selected_preset));
  const int selected_index = ui_->preset->findText(selected_preset_display_name);
  if (selected_index != -1) ui_->preset->setCurrentIndex(selected_index);

  ui_->enable_equalizer->setChecked(s.value(kEnabled, false).toBool());
  ui_->slider_container->setEnabled(ui_->enable_equalizer->isChecked());

  ui_->enable_stereo_balancer->setChecked(s.value(kEnableStereoBalancer, false).toBool());
#ifdef Q_OS_ANDROID
  for (QLabel *label : {ui_->label_left, ui_->label_balance, ui_->label_right}) label->setEnabled(ui_->enable_stereo_balancer->isChecked());
#else
  ui_->slider_label_layout->setEnabled(ui_->enable_stereo_balancer->isChecked());
#endif
  ui_->stereo_balance_slider->setEnabled(ui_->enable_stereo_balancer->isChecked());

  const int stereo_balance = s.value(kStereoBalance, 0).toInt();
  ui_->stereo_balance_slider->setValue(stereo_balance);
  StereoBalanceSliderChanged(stereo_balance);

  PresetChanged(selected_preset);

}

void Equalizer::LoadDefaultPresets() {

  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Custom")),             Params(0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Classical")),          Params(0, 0, 0, 0, 0, 0, -40, -40, -40, -50));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Club")),               Params(0, 0, 20, 30, 30, 30, 20, 0, 0, 0));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Dance")),              Params(50, 35, 10, 0, 0, -30, -40, -40, 0, 0));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Full Bass")),          Params(70, 70, 70, 40, 20, -45, -50, -55, -55, -55));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Full Treble")),        Params(-50, -50, -50, -25, 15, 55, 80, 80, 80, 85));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Full Bass + Treble")), Params(35, 30, 0, -40, -25, 10, 45, 55, 60, 60));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Laptop/Headphones")),  Params(25, 50, 25, -20, 0, -30, -40, -40, 0, 0));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Large Hall")),         Params(50, 50, 30, 30, 0, -25, -25, -25, 0, 0));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Live")),               Params(-25, 0, 20, 25, 30, 30, 20, 15, 15, 10));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Party")),              Params(35, 35, 0, 0, 0, 0, 0, 0, 35, 35));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Pop")),                Params(-10, 25, 35, 40, 25, -5, -15, -15, -10, -10));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Reggae")),             Params(0, 0, -5, -30, 0, -35, -35, 0, 0, 0));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Rock")),               Params(40, 25, -30, -40, -20, 20, 45, 55, 55, 55));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Soft")),               Params(25, 10, -5, -15, -5, 20, 45, 50, 55, 60));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Ska")),                Params(-15, -25, -25, -5, 20, 30, 45, 50, 55, 50));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Soft Rock")),          Params(20, 20, 10, -5, -25, -30, -20, -5, 15, 45));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Techno")),             Params(40, 30, 0, -30, -25, 0, 40, 50, 50, 45));
  AddPreset(QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Zero")),               Params(0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

}

void Equalizer::AddPreset(const QString &name, const Params &params) {

  QString name_displayed = tr(qUtf8Printable(name));
  presets_[name] = params;

  if (ui_->preset->findText(name_displayed) == -1) {
    ui_->preset->addItem(name_displayed, QVariant(name));
  }

}

void Equalizer::PresetChanged(const int index) {

  PresetChanged(ui_->preset->itemData(index).toString());
}

void Equalizer::PresetChanged(const QString &name) {

#ifndef Q_OS_ANDROID
  if (presets_.contains(last_preset_)) {
    if (presets_[last_preset_] != current_params()) {
      SaveCurrentPreset();
    }
  }
#endif
  last_preset_ = name;

  const Params p = presets_.value(name);

  loading_ = true;
  preamp_->set_value(p.preamp);
  for (int i = 0; i < kBands; ++i) gain_[i]->set_value(p.gain[i]);
  loading_ = false;

  EqualizerParametersChangedSlot();
#ifdef Q_OS_ANDROID
  UpdateAndroidPreset();
#endif
  Save();

}

void Equalizer::SavePreset() {

#ifdef Q_OS_ANDROID
  ShowAndroidPresetEditor(false);
#else
  QString name = SaveCurrentPreset();
  if (!name.isEmpty()) {
    last_preset_ = name;
    ui_->preset->setCurrentIndex(ui_->preset->findText(tr(qUtf8Printable(name))));
  }
#endif

}

QString Equalizer::SaveCurrentPreset() {

#ifdef Q_OS_ANDROID
  const QString name = QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Custom"));
  const QSignalBlocker blocker(ui_->preset);
  AddPreset(name, current_params());
  last_preset_ = name;
  ui_->preset->setCurrentIndex(ui_->preset->findData(name));
  UpdateAndroidPreset();
  Save();
  return name;
#else
  QString name = QInputDialog::getText(this, tr("Save preset"), tr("Name"), QLineEdit::Normal, tr(qUtf8Printable(last_preset_)));
  if (name.isEmpty()) {
    return QString();
  }

  AddPreset(name, current_params());
  Save();
  return name;
#endif

}

void Equalizer::DelPreset() {

#ifdef Q_OS_ANDROID
  ShowAndroidPresetEditor(true);
#else
  QString name = ui_->preset->itemData(ui_->preset->currentIndex()).toString();
  QString name_displayed = ui_->preset->currentText();
  if (!presets_.contains(name) || name.isEmpty()) return;

  const int ret = QMessageBox::question( this, tr("Delete preset"), tr("Are you sure you want to delete the \"%1\" preset?").arg(name_displayed), QMessageBox::Yes, QMessageBox::No);
  if (ret == QMessageBox::No) return;

  presets_.remove(name);
  ui_->preset->removeItem(ui_->preset->currentIndex());
  Save();
#endif

}

EqualizerSlider *Equalizer::AddSlider(const QString &label, const bool preamp) {

  EqualizerSlider *ret = new EqualizerSlider(label, ui_->slider_container, preamp);
  ui_->slider_container->layout()->addWidget(ret);
  QObject::connect(ret, &EqualizerSlider::ValueChanged, this, &Equalizer::EqualizerParametersChangedSlot);

  return ret;

}

bool Equalizer::is_stereo_balancer_enabled() const {
  return ui_->enable_stereo_balancer->isChecked();
}

bool Equalizer::is_equalizer_enabled() const {
  return ui_->enable_equalizer->isChecked();
}

int Equalizer::preamp_value() const {
  return preamp_->value();
}

QList<int> Equalizer::gain_values() const {

  QList<int> ret;
  ret.reserve(kBands);
  for (int i = 0; i < kBands; ++i) {
    ret << gain_[i]->value();
  }
  return ret;

}

Equalizer::Params Equalizer::current_params() const {

  QList<int> gains = gain_values();

  Params ret;
  ret.preamp = preamp_value();
  std::copy(gains.begin(), gains.end(), ret.gain);
  return ret;

}

float Equalizer::stereo_balance() const {
  return qBound(-1.0F, static_cast<float>(ui_->stereo_balance_slider->value()) / 100.0F, 1.0F);
}

void Equalizer::StereoBalancerEnabledChangedSlot(const bool enabled) {

  if (!enabled) {
    ui_->stereo_balance_slider->setValue(0);
    Q_EMIT StereoBalanceChanged(stereo_balance());
  }
  ui_->stereo_balance_slider->setEnabled(enabled);
#ifdef Q_OS_ANDROID
  for (QLabel *label : {ui_->label_left, ui_->label_balance, ui_->label_right}) label->setEnabled(enabled);
#endif
  Q_EMIT StereoBalancerEnabledChanged(enabled);
  Save();

}

void Equalizer::StereoBalanceSliderChanged(const int value) {

  Q_UNUSED(value)

  Q_EMIT StereoBalanceChanged(stereo_balance());
  Save();

}

void Equalizer::EqualizerEnabledChangedSlot(const bool enabled) {

  Q_EMIT EqualizerEnabledChanged(enabled);
  ui_->slider_container->setEnabled(enabled);
  Save();

}

void Equalizer::EqualizerParametersChangedSlot() {

  if (loading_) return;
#ifdef Q_OS_ANDROID
  // Preset loading emits the same parameter signal but must not replace the chosen preset.
  if (android_pages_ && current_params() != presets_.value(last_preset_)) SaveCurrentPreset();
#endif
  Q_EMIT EqualizerParametersChanged(preamp_value(), gain_values());

}

void Equalizer::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  s.beginWriteArray(kPresets, static_cast<int>(presets_.count()));
  int i = 0;
  for (auto it = presets_.cbegin(); it != presets_.cend(); ++it) {
    s.setArrayIndex(i++);
    s.setValue(kName, it.key());
    s.setValue(kParams, QVariant::fromValue(it.value()));
  }
  s.endArray();

  s.setValue(kSelectedPreset, ui_->preset->itemData(ui_->preset->currentIndex()).toString());
  s.setValue(kEnabled, ui_->enable_equalizer->isChecked());
  s.setValue(kEnableStereoBalancer, ui_->enable_stereo_balancer->isChecked());
  s.setValue(kStereoBalance, ui_->stereo_balance_slider->value());

}

void Equalizer::closeEvent(QCloseEvent *e) {

  Q_UNUSED(e)

  const QString name = ui_->preset->itemData(ui_->preset->currentIndex()).toString();
  if (!presets_.contains(name)) return;

  if (presets_[name] == current_params()) return;

  SavePreset();

}

#ifdef Q_OS_ANDROID
void Equalizer::SetupAndroidLayout() {

  setWindowModality(Qt::ApplicationModal);
  QLayout *old_layout = layout();
  old_layout->removeItem(ui_->slider_label_layout);
  ui_->slider_label_layout->setParent(nullptr);
  while (QLayoutItem *item = old_layout->takeAt(0)) delete item;
  delete old_layout;
  QVBoxLayout *root = new QVBoxLayout(this);
  root->setSizeConstraint(QLayout::SetNoConstraint);
  setMinimumSize(0, 0);
  QHBoxLayout *navigation = new QHBoxLayout;
  android_title_ = new QLabel(tr("Tools > Equalizer"), this);
  android_title_->setWordWrap(true);
  android_title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  QPushButton *back = new QPushButton(tr("Back"), this);
  back->setMinimumSize(48, 48);
  navigation->addWidget(android_title_, 1);
  navigation->addWidget(back);
  root->addLayout(navigation);
  android_pages_ = new QStackedWidget(this);
  root->addWidget(android_pages_);
  android_main_ = new QScrollArea(android_pages_);
  android_main_->setWidgetResizable(true);
  android_main_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  QScroller::grabGesture(android_main_->viewport(), QScroller::LeftMouseButtonGesture);
  android_main_->verticalScrollBar()->setStyleSheet(u"QScrollBar:vertical { width: 24px; }"_s);
  QWidget *content = new QWidget;
  QVBoxLayout *controls = new QVBoxLayout(content);
  // Keep slider endpoints outside Android's edge-Back gesture region.
  controls->setContentsMargins(32, 6, 32, 6);
  controls->setSizeConstraint(QLayout::SetMinimumSize);
  android_preset_ = new AndroidSettingsButton<QPushButton>(content);
  android_preset_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  UpdateAndroidPreset();
  controls->addWidget(android_preset_);
  QHBoxLayout *preset_actions = new QHBoxLayout;
  for (QToolButton *button : {ui_->preset_save, ui_->preset_del}) {
    button->setMinimumSize(48, 48);
    button->setAccessibleName(button->toolTip());
    preset_actions->addWidget(button);
  }
  controls->addLayout(preset_actions);
  ui_->enable_equalizer->setMinimumHeight(48);
  controls->addWidget(ui_->enable_equalizer);
  QLabel *restart = new QLabel(ui_->enable_equalizer->toolTip(), content);
  restart->setWordWrap(true);
  controls->addWidget(restart);
  QLayout *slider_layout = ui_->slider_container->layout();
  while (QLayoutItem *item = slider_layout->takeAt(0)) delete item;
  delete slider_layout;
  QVBoxLayout *sliders = new QVBoxLayout(ui_->slider_container);
  sliders->setContentsMargins(0, 0, 0, 0);
  sliders->addWidget(preamp_);
  for (EqualizerSlider *slider : gain_) sliders->addWidget(slider);
  controls->addWidget(ui_->slider_container);
  ui_->enable_stereo_balancer->setMinimumHeight(48);
  controls->addWidget(ui_->enable_stereo_balancer);
  controls->addLayout(ui_->slider_label_layout);
  controls->addWidget(ui_->stereo_balance_slider);
  ui_->stereo_balance_slider->setMinimumHeight(48);
  ui_->stereo_balance_slider->setAccessibleName(tr("Balance"));
  controls->addStretch();
  android_main_->setWidget(content);
  android_pages_->addWidget(android_main_);
  ui_->preset->hide();
  ui_->label_preset->hide();
  ui_->line->hide();
  QObject::connect(back, &QPushButton::clicked, this, &Equalizer::AndroidBack);
  QObject::connect(android_preset_, &QPushButton::clicked, this, &Equalizer::ShowAndroidPresets);

}

void Equalizer::UpdateAndroidPreset() {

  if (!android_preset_) return;
  if (QLayout *old_layout = android_preset_->layout()) {
    while (QLayoutItem *item = old_layout->takeAt(0)) {
      delete item->widget();
      delete item;
    }
  }
  delete android_preset_->layout();
  QVBoxLayout *layout = new QVBoxLayout(android_preset_);
  QLabel *title = new QLabel(tr("Preset") + u" >"_s, android_preset_);
  title->setAttribute(Qt::WA_TransparentForMouseEvents);
  title->setWordWrap(true);
  layout->addWidget(title);
  QLabel *value = new QLabel(ui_->preset->currentText(), android_preset_);
  value->setAttribute(Qt::WA_TransparentForMouseEvents);
  value->setTextFormat(Qt::PlainText);
  value->setWordWrap(true);
  QFont font = value->font();
  font.setBold(true);
  value->setFont(font);
  QPalette palette = value->palette();
  palette.setColor(QPalette::WindowText, palette.color(QPalette::Link));
  value->setPalette(palette);
  layout->addWidget(value);
  android_preset_->setAccessibleName(tr("Preset") + u": "_s + ui_->preset->currentText());
  ui_->preset_del->setEnabled(ui_->preset->currentData().toString() != QStringLiteral(QT_TRANSLATE_NOOP("Equalizer", "Custom")));

}

void Equalizer::SetAndroidPage(QWidget *page, const QString &title) {

  android_pages_->addWidget(page);
  android_pages_->setCurrentWidget(page);
  android_title_->setText(tr("Tools > Equalizer") + u" > "_s + title);

}

void Equalizer::ShowAndroidPresets() {

  QScrollArea *area = new QScrollArea;
  area->setWidgetResizable(true);
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  QScroller::grabGesture(area->viewport(), QScroller::LeftMouseButtonGesture);
  area->verticalScrollBar()->setStyleSheet(u"QScrollBar:vertical { width: 24px; }"_s);
  QWidget *content = new QWidget;
  QVBoxLayout *layout = new QVBoxLayout(content);
  QWidget *selected_choice = nullptr;
  for (int i = 0; i < ui_->preset->count(); ++i) {
    const QString name = ui_->preset->itemData(i).toString();
    QRadioButton *choice = new AndroidSettingsButton<QRadioButton>(content);
    choice->setStyleSheet(u"QRadioButton::indicator { image: none; border: none; background: transparent; width: 0px; height: 0px; }"_s);
    choice->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout *row = new QVBoxLayout(choice);
    QLabel *label = new QLabel(ui_->preset->itemText(i), choice);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    row->addWidget(label);
    choice->setAccessibleName(ui_->preset->itemText(i));
    choice->setChecked(i == ui_->preset->currentIndex());
    if (choice->isChecked()) {
      selected_choice = choice;
      choice->setAutoFillBackground(true);
      QPalette palette = choice->palette();
      palette.setColor(QPalette::Window, palette.color(QPalette::Highlight));
      palette.setColor(QPalette::WindowText, palette.color(QPalette::HighlightedText));
      choice->setPalette(palette);
    }
    layout->addWidget(choice);
    QObject::connect(choice, &QRadioButton::clicked, this, [this, name]() {
      ui_->preset->setCurrentIndex(ui_->preset->findData(name));
      AndroidBack();
    });
  }
  layout->addStretch();
  area->setWidget(content);
  SetAndroidPage(area, tr("Preset"));
  if (selected_choice) {
    QTimer::singleShot(0, area, [area, selected_choice]() { area->ensureWidgetVisible(selected_choice); });
  }

}

void Equalizer::ShowAndroidPresetEditor(const bool remove) {

  const QString name = ui_->preset->currentData().toString();
  QWidget *page = new QWidget;
  QVBoxLayout *layout = new QVBoxLayout(page);
  QLabel *label = new QLabel(remove ? tr("Are you sure you want to delete the \"%1\" preset?").arg(ui_->preset->currentText()) : tr("Name"), page);
  label->setTextFormat(Qt::PlainText);
  label->setWordWrap(true);
  layout->addWidget(label);
  QLineEdit *edit = new QLineEdit(ui_->preset->currentText(), page);
  edit->setMinimumHeight(48);
  edit->setAccessibleName(tr("Name"));
  edit->setVisible(!remove);
  layout->addWidget(edit);
  QLabel *error = new QLabel(page);
  error->setWordWrap(true);
  layout->addWidget(error);
  QPushButton *confirm = new QPushButton(remove ? tr("Delete preset") : tr("Save preset"), page);
  confirm->setMinimumHeight(48);
  layout->addWidget(confirm);
  QPushButton *cancel = new QPushButton(tr("Cancel"), page);
  cancel->setMinimumHeight(48);
  layout->addWidget(cancel);
  layout->addStretch();
  QObject::connect(cancel, &QPushButton::clicked, this, &Equalizer::AndroidBack);
  const auto perform = [this, remove, name, edit, error]() {
    if (remove) {
      const QSignalBlocker blocker(ui_->preset);
      presets_.remove(name);
      ui_->preset->removeItem(ui_->preset->findData(name));
      SaveCurrentPreset();
    }
    else {
      const QString entered = edit->text().trimmed();
      if (entered.isEmpty()) {
        error->setText(tr("Enter a preset name."));
        return;
      }
      const QSignalBlocker blocker(ui_->preset);
      AddPreset(entered, current_params());
      last_preset_ = entered;
      ui_->preset->setCurrentIndex(ui_->preset->findData(entered));
      UpdateAndroidPreset();
      Save();
    }
    AndroidBack();
  };
  QObject::connect(confirm, &QPushButton::clicked, this, perform);
  if (!remove) QObject::connect(edit, &QLineEdit::returnPressed, this, perform);
  SetAndroidPage(page, remove ? tr("Delete preset") : tr("Save preset"));

}

void Equalizer::AndroidBack() {

  if (android_pages_->currentWidget() == android_main_) {
    close();
    return;
  }
  QWidget *page = android_pages_->currentWidget();
  android_pages_->setCurrentWidget(android_main_);
  android_pages_->removeWidget(page);
  page->deleteLater();
  android_title_->setText(tr("Tools > Equalizer"));

}

void Equalizer::keyPressEvent(QKeyEvent *event) {

  if (event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape) {
    AndroidBack();
    event->accept();
    return;
  }
  QDialog::keyPressEvent(event);

}
#endif

Equalizer::Params::Params() : preamp(0) {
  for (int i = 0; i < Equalizer::kBands; ++i) gain[i] = 0;
}

Equalizer::Params::Params(int g0, int g1, int g2, int g3, int g4, int g5, int g6, int g7, int g8, int g9, int pre) : preamp(pre) {

  gain[0] = g0;
  gain[1] = g1;
  gain[2] = g2;
  gain[3] = g3;
  gain[4] = g4;
  gain[5] = g5;
  gain[6] = g6;
  gain[7] = g7;
  gain[8] = g8;
  gain[9] = g9;

}

bool Equalizer::Params::operator==(const Equalizer::Params &other) const {

  if (preamp != other.preamp) return false;
  for (int i = 0; i < Equalizer::kBands; ++i) {
    if (gain[i] != other.gain[i]) return false;
  }
  return true;

}

bool Equalizer::Params::operator!=(const Equalizer::Params &other) const {
  return !(*this == other);
}

QDataStream &operator<<(QDataStream &s, const Equalizer::Params &p) {

  s << p.preamp;
  for (int i = 0; i < Equalizer::kBands; ++i) s << p.gain[i];
  return s;

}

QDataStream &operator>>(QDataStream &s, Equalizer::Params &p) {

  s >> p.preamp;
  for (int i = 0; i < Equalizer::kBands; ++i) s >> p.gain[i];
  return s;

}
