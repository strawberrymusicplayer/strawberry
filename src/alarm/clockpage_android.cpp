// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSize>
#include <QStackedWidget>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>

#include "core/logging.h"
#include "widgets/androidwidgets.h"
#include "widgets/scrollarea_android.h"
#include "alarmbridge_android.h"
#include "clockpage_android.h"

using namespace Qt::Literals::StringLiterals;
using AndroidWidgets::ValueButton;

namespace {

constexpr char kBreadcrumbProperty[] = "strawberry_breadcrumb";
constexpr int kAnalog = 1;
constexpr int kBrightnessLevels[] = { 5, 10, 25, 50, 100 };

AndroidWidgets::ScrollPage CreatePage() {

  AndroidWidgets::ScrollPage page = AndroidWidgets::CreateScrollPage(nullptr);
  page.layout->setContentsMargins(0, 0, 0, 0);
  AndroidScrollArea::EnableForContents(page.area);
  return page;

}

QPushButton *CreateIconButton(const QString &text, const QString &icon, QWidget *parent) {

  QPushButton *button = new QPushButton(text, parent);
  button->setMinimumHeight(AndroidWidgets::kMinimumTouchSize);
  button->setIconSize(QSize(AndroidWidgets::kMinimumTouchSize * 2 / 3, AndroidWidgets::kMinimumTouchSize * 2 / 3));
  AndroidWidgets::SetThemedIcon(button, icon);
  return button;

}

}  // namespace

QPointer<ClockPage> ClockPage::instance_;

ClockPage::ClockPage(QWidget *parent)
    : QWidget(parent),
      title_(new QLabel(this)),
      pages_(new QStackedWidget(this)),
      menu_(nullptr) {

  instance_ = this;
  const JNINativeMethod methods[] = {
    {const_cast<char *>("nativeOpenAlarmList"), const_cast<char *>("()Z"), reinterpret_cast<void *>(NativeOpenAlarmList)},
  };
  QJniEnvironment env;
  if (!env.registerNativeMethods("nl/dewitweb/strawberryflavor/ClockActivity", methods, 1)) {
    qLog(Error) << "Could not register clock navigation";
  }

  setWindowTitle(tr("Clock"));
  QVBoxLayout *root = new QVBoxLayout(this);
  title_->setTextFormat(Qt::PlainText);
  title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  title_->setText(tr("Tools > Clock"));
  QPushButton *back = AndroidWidgets::CreateBackButton(this);
  QObject::connect(back, &QPushButton::clicked, this, &ClockPage::Back);
  root->addLayout(AndroidWidgets::CreatePageHeader(title_, back));
  root->addWidget(pages_);

  const AndroidWidgets::ScrollPage menu = CreatePage();
  menu_ = menu.area;
  QPushButton *show_clock = CreateIconButton(tr("Show clock"), u":/icons/scalable/clock.svg"_s, menu.contents);
  QObject::connect(show_clock, &QPushButton::clicked, this, []() { AlarmBridge::OpenClock(); });
  menu.layout->addWidget(show_clock);
  QPushButton *settings = CreateIconButton(tr("Clock settings"), u":/icons/scalable/clock-settings.svg"_s, menu.contents);
  QObject::connect(settings, &QPushButton::clicked, this, &ClockPage::ShowSettingsPage);
  menu.layout->addWidget(settings);
  menu.layout->addStretch();
  pages_->addWidget(menu_);

}

jboolean ClockPage::NativeOpenAlarmList(JNIEnv *, jclass) {

  QCoreApplication *application = QCoreApplication::instance();
  if (!application) return JNI_FALSE;
  return QMetaObject::invokeMethod(application, []() {
    if (instance_) Q_EMIT instance_->OpenAlarmList();
    else qLog(Error) << "Could not open the alarm list: clock page is gone";
  }, Qt::QueuedConnection) ? JNI_TRUE : JNI_FALSE;

}

void ClockPage::ShowMenu() {

  while (pages_->currentWidget() != menu_) Back();

}

void ClockPage::showEvent(QShowEvent *event) {

  QWidget::showEvent(event);
  if (pages_->currentWidget() == settings_) RefreshSettings();

}

void ClockPage::keyPressEvent(QKeyEvent *event) {

  if (event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape) {
    Back();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);

}

void ClockPage::SetPage(QWidget *page, const QString &title) {

  // Like Settings, a fourth-level title starts on a separate line below its parent's breadcrumb.
  const QString breadcrumb = title_->text() + (pages_->count() == 2 ? u"\n"_s : u" > "_s) + title;
  page->setProperty(kBreadcrumbProperty, breadcrumb);
  pages_->addWidget(page);
  pages_->setCurrentWidget(page);
  title_->setText(breadcrumb);
  // Hiding the focused control would move the focus out of the page, and Back would then quit the app.
  setFocus(Qt::OtherFocusReason);

}

void ClockPage::Back() {

  QWidget *page = pages_->currentWidget();
  if (page == menu_) {
    close();
    return;
  }
  pages_->removeWidget(page);
  page->deleteLater();
  QWidget *parent = pages_->widget(pages_->count() - 1);
  pages_->setCurrentWidget(parent);
  title_->setText(parent == menu_ ? tr("Tools > Clock") : parent->property(kBreadcrumbProperty).toString());
  if (parent == settings_) RefreshSettings();
  setFocus(Qt::OtherFocusReason);

}

void ClockPage::ShowSettingsPage() {

  const AndroidWidgets::ScrollPage page = CreatePage();
  settings_ = page.area;
  SetPage(page.area, tr("Clock settings"));
  RefreshSettings();

}

void ClockPage::RefreshSettings() {

  QScrollArea *area = qobject_cast<QScrollArea*>(settings_.data());
  if (!area) return;
  const int scroll_position = area->verticalScrollBar()->value();

  QWidget *contents = new QWidget;
  contents->setBackgroundRole(QPalette::Base);
  contents->setAutoFillBackground(true);
  QVBoxLayout *layout = new QVBoxLayout(contents);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  const AndroidWidgets::LabeledValueButton type = AndroidWidgets::CreateLabeledValueButton(tr("Clock type"), contents);
  layout->addWidget(type.label);
  layout->addWidget(type.button);
  const int current_type = AlarmBridge::ClockType();
  type.button->SetValue(ClockTypeText(current_type));
  QObject::connect(type.button, &QPushButton::clicked, this, [this, current_type]() {
    ShowChoicePage(tr("Clock type"), {{0, ClockTypeText(0)}, {kAnalog, ClockTypeText(kAnalog)}}, current_type, [](const int value) { AlarmBridge::SetClockType(value); });
  });

  const AndroidWidgets::LabeledValueButton brightness = AndroidWidgets::CreateLabeledValueButton(tr("Screen brightness while the clock is shown"), contents);
  layout->addWidget(brightness.label);
  layout->addWidget(brightness.button);
  const int current_brightness = AlarmBridge::ClockBrightness();
  brightness.button->SetValue(BrightnessText(current_brightness));
  QObject::connect(brightness.button, &QPushButton::clicked, this, [this, current_brightness]() {
    QList<Choice> choices = {{0, BrightnessText(0)}};
    bool included = current_brightness <= 0;
    for (const int percent : kBrightnessLevels) {
      if (!included && current_brightness < percent) {
        choices << Choice(current_brightness, BrightnessText(current_brightness));
        included = true;
      }
      choices << Choice(percent, BrightnessText(percent));
      if (current_brightness == percent) included = true;
    }
    ShowChoicePage(tr("Screen brightness while the clock is shown"), choices, current_brightness, [](const int value) { AlarmBridge::SetClockBrightness(value); });
  });

  layout->addStretch();
  area->setWidget(contents);
  QTimer::singleShot(0, area, [area, scroll_position]() { area->verticalScrollBar()->setValue(scroll_position); });

}

void ClockPage::ShowChoicePage(const QString &title, const QList<Choice> &choices, const int current, const std::function<void(int)> &chosen) {

  const AndroidWidgets::ScrollPage page = CreatePage();
  for (const Choice &choice : choices) {
    QRadioButton *row = AndroidWidgets::CreateChoiceRow(choice.second, page.contents);
    row->setChecked(choice.first == current);
    const int value = choice.first;
    QObject::connect(row, &QRadioButton::clicked, this, [this, chosen, value]() {
      chosen(value);
      Back();
    });
    page.layout->addWidget(row);
  }
  page.layout->addStretch();
  SetPage(page.area, title);

}

QString ClockPage::ClockTypeText(const int type) {

  return type == kAnalog ? tr("Analog") : tr("Digital");

}

QString ClockPage::BrightnessText(const int percent) {

  return percent <= 0 ? tr("System brightness") : tr("%1%").arg(percent);

}
