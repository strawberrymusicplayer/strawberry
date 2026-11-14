/*
 * Strawberry Music Player
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

// Android-only parts of Equalizer, kept apart from equalizer.cpp to reduce conflicts when merging upstream.

#include <array>
#include <utility>

#include <QtGlobal>
#include <QCheckBox>
#include <QDialog>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QStackedWidget>
#include <QString>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "core/settings.h"
#include "widgets/androidwidgets.h"
#include "widgets/gridflowlayout.h"
#include "equalizer.h"
#include "equalizerslider.h"
#include "ui_equalizer.h"

using namespace Qt::Literals::StringLiterals;

namespace {
// Must match the settings group in equalizer.cpp.
constexpr char kSettingsGroup[] = "Equalizer";
constexpr char kAndroidFoldoutMode[] = "android_foldout_mode";
constexpr char kAndroidSectionOpenPrefix[] = "android_section_";
constexpr int kAndroidBandSpacing = 4;
constexpr int kAndroidMaximumBandColumns = 4;

QString AndroidSectionOpenKey(const int section) {
  return QLatin1String(kAndroidSectionOpenPrefix) + QString::number(section);
}

const QString kAndroidScrollBarStyle =
    u"QScrollBar:vertical { width: 8px; margin: 0px; background: transparent; }"
     "QScrollBar::handle:vertical { min-height: 48px; border-radius: 4px; background: palette(highlight); }"
     "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
     "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"_s;
}  // namespace

void Equalizer::SetupAndroidLayout() {

  QLayout *old_layout = layout();
  old_layout->removeItem(ui_->slider_label_layout);
  ui_->slider_label_layout->setParent(nullptr);
  while (QLayoutItem *item = old_layout->takeAt(0)) delete item;
  delete old_layout;
  QVBoxLayout *root = new QVBoxLayout(this);
  root->setSizeConstraint(QLayout::SetNoConstraint);
  setMinimumSize(0, 0);
  android_title_ = new QLabel(tr("Tools > Equalizer"), this);
  android_title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  QPushButton *back = AndroidWidgets::CreateBackButton(this);
  root->addLayout(AndroidWidgets::CreatePageHeader(android_title_, back));
  android_pages_ = new QStackedWidget(this);
  root->addWidget(android_pages_);
  android_main_ = new QScrollArea(android_pages_);
  android_main_->setWidgetResizable(true);
  android_main_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  android_main_->setFrameShape(QFrame::NoFrame);
  android_main_->setStyleSheet(u"QSlider::groove:horizontal { height: 6px; border-radius: 3px; background: palette(mid); }"
                               "QSlider::sub-page:horizontal { border-radius: 3px; background: palette(highlight); }"
                               "QSlider::handle:horizontal { width: 24px; margin: -9px 0; border-radius: 2px; background: palette(highlight); }"
                               "QSlider::sub-page:horizontal:disabled, QSlider::handle:horizontal:disabled { background: palette(mid); }"_s);
  QScroller::grabGesture(android_main_->viewport(), QScroller::LeftMouseButtonGesture);
  android_main_->verticalScrollBar()->setStyleSheet(kAndroidScrollBarStyle);
  QWidget *content = new QWidget;
  QVBoxLayout *controls = new QVBoxLayout(content);
  // Keep slider endpoints outside Android's edge-Back gesture region.
  controls->setContentsMargins(24, 8, 24, 8);
  controls->setSpacing(8);
  controls->setSizeConstraint(QLayout::SetMinimumSize);
  AndroidWidgets::AdaptCheckBox(ui_->enable_equalizer);
  QFont enable_font = ui_->enable_equalizer->font();
  enable_font.setBold(true);
  enable_font.setPointSizeF(enable_font.pointSizeF() * 1.2);
  ui_->enable_equalizer->setFont(enable_font);
  controls->addWidget(ui_->enable_equalizer);
  QLabel *disabled_hint = new QLabel(tr("Enable equalizer first to adjust the gain controls."), content);
  disabled_hint->setWordWrap(true);
  disabled_hint->setVisible(!ui_->enable_equalizer->isChecked());
  controls->addWidget(disabled_hint);
  QObject::connect(ui_->enable_equalizer, &QCheckBox::toggled, disabled_hint, [disabled_hint](const bool enabled) {
    disabled_hint->setVisible(!enabled);
  });
  QLabel *restart = new QLabel(ui_->enable_equalizer->toolTip(), content);
  restart->setWordWrap(true);
  controls->addWidget(restart);
  const AndroidWidgets::LabeledValueButton preset = AndroidWidgets::CreateLabeledValueButton(tr("Preset"), content);
  controls->addWidget(preset.label);
  QHBoxLayout *preset_section = new QHBoxLayout;
  controls->addLayout(preset_section);
  android_preset_ = preset.button;
  UpdateAndroidPreset();
  preset_section->addWidget(android_preset_);
  for (QToolButton *button : {ui_->preset_save, ui_->preset_del}) {
    button->setMinimumSize(48, 48);
    button->setAccessibleName(button->toolTip());
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setAutoRaise(true);
    button->setIconSize(QSize(28, 28));
    preset_section->addWidget(button);
  }
  const AndroidWidgets::LabeledValueButton foldout_mode = AndroidWidgets::CreateLabeledValueButton(tr("Collapse behaviour of sections"), content);
  controls->addWidget(foldout_mode.label);
  android_foldout_mode_ = foldout_mode.button;
  controls->addWidget(android_foldout_mode_);
  Settings foldout_settings;
  foldout_settings.beginGroup(kSettingsGroup);
  android_foldout_mode_value_ = static_cast<AndroidFoldoutMode>(qBound(static_cast<int>(AndroidFoldoutMode::AllExpanded), foldout_settings.value(kAndroidFoldoutMode, static_cast<int>(AndroidFoldoutMode::AllCollapsed)).toInt(), static_cast<int>(AndroidFoldoutMode::OneAtATime)));
  const auto add_foldout = [this, &foldout_settings](QVBoxLayout *parent, const AndroidSection section, const QString &title) {
    Q_ASSERT(android_sections_.count() == static_cast<int>(section));
    QFrame *card = new QFrame;
    card->setObjectName(u"equalizerFoldout"_s);
    card->setStyleSheet(u"QFrame#equalizerFoldout { background: palette(base); border: 1px solid palette(mid); border-radius: 12px; }"_s);
    QVBoxLayout *outer = new QVBoxLayout(card);
    outer->setContentsMargins(8, 4, 8, 4);
    QToolButton *heading = new QToolButton(card);
    heading->setText(title);
    heading->setCheckable(true);
    heading->setChecked(foldout_settings.value(AndroidSectionOpenKey(static_cast<int>(section)), android_foldout_mode_value_ == AndroidFoldoutMode::AllExpanded).toBool());
    heading->setAutoRaise(true);
    heading->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    heading->setMinimumHeight(48);
    heading->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QFont font = heading->font();
    font.setBold(true);
    heading->setFont(font);
    heading->setAccessibleName(title);
    QWidget *body = new QWidget(card);
    QVBoxLayout *inner = new QVBoxLayout(body);
    inner->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(heading);
    outer->addWidget(body);
    android_sections_.append(heading);
    android_section_contents_.append(body);
    QObject::connect(heading, &QToolButton::clicked, this, [this, heading](const bool open) {
      if (open && android_foldout_mode_value_ == AndroidFoldoutMode::OneAtATime) {
        for (QToolButton *other : android_sections_) if (other != heading) other->setChecked(false);
      }
      UpdateAndroidFoldouts();
      SaveAndroidFoldouts();
    });
    parent->addWidget(card);
    return inner;
  };
  QLayout *slider_layout = ui_->slider_container->layout();
  while (QLayoutItem *item = slider_layout->takeAt(0)) delete item;
  delete slider_layout;
  QVBoxLayout *sliders = new QVBoxLayout(ui_->slider_container);
  sliders->setContentsMargins(0, 0, 0, 0);
  sliders->setSpacing(8);
  add_foldout(sliders, AndroidSection::Preamp, tr("Pre-amp"))->addWidget(preamp_);
  QVBoxLayout *bass = add_foldout(sliders, AndroidSection::Bass, tr("Bass"));
  QVBoxLayout *midrange = add_foldout(sliders, AndroidSection::Midrange, tr("Midrange"));
  QVBoxLayout *treble = add_foldout(sliders, AndroidSection::Treble, tr("Treble"));
  GridFlowLayout *bass_bands = new GridFlowLayout(kAndroidBandSpacing, kAndroidMaximumBandColumns);
  GridFlowLayout *midrange_bands = new GridFlowLayout(kAndroidBandSpacing, kAndroidMaximumBandColumns);
  GridFlowLayout *treble_bands = new GridFlowLayout(kAndroidBandSpacing, kAndroidMaximumBandColumns);
  bass->addLayout(bass_bands);
  midrange->addLayout(midrange_bands);
  treble->addLayout(treble_bands);
  for (int i = 0; i < kBands; ++i) {
    (i < 3 ? bass_bands : i < 6 ? midrange_bands : treble_bands)->addWidget(gain_[i]);
  }
  controls->addWidget(ui_->slider_container);
  QVBoxLayout *stereo = add_foldout(controls, AndroidSection::StereoBalance, tr("Stereo balance"));
  ui_->enable_stereo_balancer->setMinimumHeight(48);
  stereo->addWidget(ui_->enable_stereo_balancer);
  stereo->addLayout(ui_->slider_label_layout);
  stereo->addWidget(ui_->stereo_balance_slider);
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
  QObject::connect(android_foldout_mode_, &QPushButton::clicked, this, &Equalizer::ShowAndroidFoldoutModes);
  UpdateAndroidFoldouts();

}

void Equalizer::UpdateAndroidPreset() {

  if (!android_preset_) return;
  const bool edited = presets_.value(ui_->preset->currentData().toString()) != current_params();
  android_preset_->SetValue(ui_->preset->currentText() + (edited ? u" *"_s : QString()));
  const QString selected = ui_->preset->currentData().toString();
  ui_->preset_del->setEnabled(selected != QLatin1String(kCustomPreset));
  ui_->preset_save->setEnabled(edited || selected == QLatin1String(kCustomPreset));

}

void Equalizer::SetAndroidPage(QWidget *page, const QString &title) {

  android_pages_->addWidget(page);
  android_pages_->setCurrentWidget(page);
  android_title_->setText(tr("Tools > Equalizer") + u" > "_s + title);

}

QString Equalizer::AndroidFoldoutModeName(const AndroidFoldoutMode mode) {

  switch (mode) {
    case AndroidFoldoutMode::AllExpanded:
      return tr("All expanded");
    case AndroidFoldoutMode::AllCollapsed:
      return tr("All collapsed");
    case AndroidFoldoutMode::OneAtATime:
      return tr("One section at a time");
  }
  return QString();

}

void Equalizer::UpdateAndroidFoldouts() {

  android_foldout_mode_->SetValue(AndroidFoldoutModeName(android_foldout_mode_value_));
  for (int i = 0; i < android_sections_.count(); ++i) {
    const bool open = android_sections_.at(i)->isChecked();
    android_sections_.at(i)->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    android_section_contents_.at(i)->setVisible(open);
  }

}

void Equalizer::SaveAndroidFoldouts() {

  Settings settings;
  settings.beginGroup(kSettingsGroup);
  settings.setValue(kAndroidFoldoutMode, static_cast<int>(android_foldout_mode_value_));
  for (int i = 0; i < android_sections_.count(); ++i) settings.setValue(AndroidSectionOpenKey(i), android_sections_.at(i)->isChecked());

}

void Equalizer::ShowAndroidFoldoutModes() {

  QWidget *page = new QWidget;
  QVBoxLayout *layout = new QVBoxLayout(page);
  constexpr std::array modes = {AndroidFoldoutMode::AllExpanded, AndroidFoldoutMode::AllCollapsed, AndroidFoldoutMode::OneAtATime};
  for (const AndroidFoldoutMode mode : modes) {
    const QString mode_name = AndroidFoldoutModeName(mode);
    QRadioButton *choice = AndroidWidgets::CreateChoiceRow(mode_name, page);
    choice->setChecked(mode == android_foldout_mode_value_);
    layout->addWidget(choice);
    QObject::connect(choice, &QRadioButton::clicked, this, [this, mode]() {
      android_foldout_mode_value_ = mode;
      for (QToolButton *section : std::as_const(android_sections_)) section->setChecked(mode == AndroidFoldoutMode::AllExpanded);
      UpdateAndroidFoldouts();
      SaveAndroidFoldouts();
      AndroidBack();
    });
  }
  layout->addStretch();
  SetAndroidPage(page, tr("Collapse behaviour of sections"));

}

void Equalizer::ShowAndroidPresets() {

  QScrollArea *area = new QScrollArea;
  area->setWidgetResizable(true);
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  QScroller::grabGesture(area->viewport(), QScroller::LeftMouseButtonGesture);
  area->verticalScrollBar()->setStyleSheet(kAndroidScrollBarStyle);
  QWidget *content = new QWidget;
  QVBoxLayout *layout = new QVBoxLayout(content);
  QPushButton *create = new QPushButton(tr("New"), content);
  create->setMinimumHeight(48);
  layout->addWidget(create);
  QObject::connect(create, &QPushButton::clicked, this, [this]() {
    AndroidBack();
    ShowAndroidPresetEditor(false);
  });
  QWidget *selected_choice = nullptr;
  for (int i = 0; i < ui_->preset->count(); ++i) {
    const QString name = ui_->preset->itemData(i).toString();
    QRadioButton *choice = AndroidWidgets::CreateChoiceRow(ui_->preset->itemText(i), content);
    choice->setChecked(i == ui_->preset->currentIndex());
    if (choice->isChecked()) selected_choice = choice;
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
  QLineEdit *edit = new QLineEdit(page);
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
      if (presets_.contains(entered)) {
        error->setText(tr("A preset with this name already exists."));
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
