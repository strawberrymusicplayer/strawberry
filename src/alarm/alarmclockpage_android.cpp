// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <memory>
#include <utility>

#include <QAbstractItemModel>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QFontInfo>
#include <QFrame>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMimeData>
#include <QModelIndex>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QRectF>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSize>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QVariantList>
#include <QtConcurrentRun>

#include "collection/collectionbackend.h"
#include "core/iconloader.h"
#include "core/song.h"
#include "playlist/playlistbackend.h"
#include "radios/radioservices.h"
#include "smartplaylists/smartplaylistsearch.h"
#include "smartplaylists/smartplaylistsearchterm.h"
#include "utilities/strutils.h"
#include "widgets/androidwidgets.h"
#include "widgets/scrollarea_android.h"
#include "widgets/timedial_android.h"
#include "alarmbridge_android.h"
#include "alarmclock_android.h"
#include "alarmclockpage_android.h"
#include "alarmtext_android.h"

using namespace Qt::Literals::StringLiterals;
using AndroidWidgets::ValueButton;

namespace {

constexpr char kBreadcrumbProperty[] = "strawberry_breadcrumb";
constexpr int kSaveDelayMsec = 400;
constexpr int kSongSearchLimit = 50;

const QString kSliderStyle =
    u"QSlider::groove:horizontal { height: 6px; border-radius: 3px; background: palette(mid); }"
     "QSlider::sub-page:horizontal { border-radius: 3px; background: palette(highlight); }"
     "QSlider::handle:horizontal { width: 24px; margin: -9px 0; border-radius: 2px; background: palette(highlight); }"_s;

QLabel *CreateWrappingLabel(const QString &text, QWidget *parent) {

  QLabel *label = new QLabel(text, parent);
  label->setTextFormat(Qt::PlainText);
  label->setWordWrap(true);
  return label;

}

QPushButton *CreateButton(const QString &text, QWidget *parent) {

  QPushButton *button = new QPushButton(text, parent);
  button->setMinimumHeight(AndroidWidgets::kMinimumTouchSize);
  return button;

}

// A green circle with a white plus, painted rather than themed, so it stays green and readable in light and dark themes.
void SetAddIcon(QPushButton *button) {

  const int size = AndroidWidgets::kMinimumTouchSize * 2 / 3;
  const qreal ratio = button->devicePixelRatioF();
  QPixmap pixmap(QSize(size, size) * ratio);
  pixmap.setDevicePixelRatio(ratio);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(0x2E, 0x9D, 0x46));
  painter.drawEllipse(QRectF(0, 0, size, size));
  const qreal arm = size * 0.28;
  const qreal center = size / 2.0;
  QPen pen(Qt::white, size * 0.12, Qt::SolidLine, Qt::RoundCap);
  painter.setPen(pen);
  painter.drawLine(QPointF(center - arm, center), QPointF(center + arm, center));
  painter.drawLine(QPointF(center, center - arm), QPointF(center, center + arm));
  painter.end();
  button->setIcon(QIcon(pixmap));
  button->setIconSize(QSize(size, size));

}

// Adds a value button with its title label above it, as on the Android settings pages.
ValueButton *AddValueButton(QVBoxLayout *layout, const QString &title) {

  const AndroidWidgets::LabeledValueButton labeled = AndroidWidgets::CreateLabeledValueButton(title, layout->parentWidget());
  layout->addWidget(labeled.label);
  layout->addWidget(labeled.button);
  return labeled.button;

}

AndroidWidgets::ScrollPage CreatePage() {

  AndroidWidgets::ScrollPage page = AndroidWidgets::CreateScrollPage(nullptr);
  page.layout->setContentsMargins(0, 0, 0, 0);
  AndroidScrollArea::EnableForContents(page.area);
  return page;

}

QString SongText(const Song &song) {

  QStringList parts;
  parts << (song.title().isEmpty() ? song.basefilename() : song.title());
  if (!song.artist().isEmpty()) parts << song.artist();
  return parts.join(" - "_L1);

}

}  // namespace

AlarmClockPage::AlarmClockPage(AlarmClock *alarm_clock, SharedPtr<RadioServices> radio_services, QWidget *parent)
    : QWidget(parent),
      alarm_clock_(alarm_clock),
      radio_services_(radio_services),
      title_(new QLabel(this)),
      pages_(new QStackedWidget(this)),
      overview_(nullptr),
      overview_contents_(nullptr),
      save_timer_(new QTimer(this)),
      countdown_timer_(new QTimer(this)) {

  setWindowTitle(tr("Alarm clock"));
  QVBoxLayout *root = new QVBoxLayout(this);
  title_->setTextFormat(Qt::PlainText);
  title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  title_->setText(tr("Tools > Alarm clock"));
  QPushButton *back = AndroidWidgets::CreateBackButton(this);
  QObject::connect(back, &QPushButton::clicked, this, &AlarmClockPage::Back);
  root->addLayout(AndroidWidgets::CreatePageHeader(title_, back));
  root->addWidget(pages_);

  const AndroidWidgets::ScrollPage overview = CreatePage();
  overview_ = overview.area;
  pages_->addWidget(overview_);

  save_timer_->setSingleShot(true);
  save_timer_->setInterval(kSaveDelayMsec);
  QObject::connect(save_timer_, &QTimer::timeout, this, &AlarmClockPage::Save);
  countdown_timer_->setSingleShot(true);
  QObject::connect(countdown_timer_, &QTimer::timeout, this, [this]() {
    if (isVisible() && pages_->currentWidget() == overview_) RefreshOverview();
  });

  // Permissions may be granted in Android's settings, and alarms may go off or be switched off while the app is in the background.
  QObject::connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](const Qt::ApplicationState state) {
    if (state != Qt::ApplicationActive || !isVisible()) return;
    if (pages_->currentWidget() == overview_) RefreshOverview();
    else if (pages_->currentWidget() == settings_) RefreshSettings();
    else if (pages_->currentWidget() == editor_) RefreshEditor();
  });
  QObject::connect(alarm_clock_, &AlarmClock::TracksUpdated, this, [this](const QString &id) {
    // The size of the copied tracks changed, which enables or disables deleting them.
    if (isVisible() && settings_ && pages_->currentWidget() == settings_) {
      QTimer::singleShot(0, this, [this]() {
        if (isVisible() && settings_ && pages_->currentWidget() == settings_) RefreshSettings();
      });
    }
    if (id != alarm_.id) return;
    const Alarm stored = AlarmBridge::FindAlarm(id);
    alarm_.tracks = stored.tracks;
    alarm_.tracks_updated = stored.tracks_updated;
  });

}

void AlarmClockPage::ShowOverview() {

  while (pages_->currentWidget() != overview_) Back();

}

void AlarmClockPage::showEvent(QShowEvent *event) {

  QWidget::showEvent(event);
  AlarmBridge::RequestRuntimePermissions();
  if (pages_->currentWidget() == overview_) RefreshOverview();
  else if (pages_->currentWidget() == settings_) RefreshSettings();
  else if (pages_->currentWidget() == editor_) RefreshEditor();

}

void AlarmClockPage::hideEvent(QHideEvent *event) {

  countdown_timer_->stop();
  if (save_timer_->isActive()) {
    save_timer_->stop();
    Save();
  }
  QWidget::hideEvent(event);

}

void AlarmClockPage::keyPressEvent(QKeyEvent *event) {

  if (event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape) {
    Back();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);

}

void AlarmClockPage::SetPage(QWidget *page, const QString &title) {

  // Like Settings, a fourth-level title starts on a separate line below its parent's breadcrumb.
  const QString breadcrumb = title_->text() + (pages_->count() == 2 ? u"\n"_s : u" > "_s) + title;
  page->setProperty(kBreadcrumbProperty, breadcrumb);
  pages_->addWidget(page);
  pages_->setCurrentWidget(page);
  title_->setText(breadcrumb);
  // Hiding the focused control would move the focus out of the page, and Back would then quit the app.
  setFocus(Qt::OtherFocusReason);

}

void AlarmClockPage::Back() {

  QWidget *page = pages_->currentWidget();
  if (page == overview_) {
    close();
    return;
  }
  if (page == editor_ && save_timer_->isActive()) {
    save_timer_->stop();
    Save();
  }
  pages_->removeWidget(page);
  page->deleteLater();
  QWidget *parent = pages_->widget(pages_->count() - 1);
  pages_->setCurrentWidget(parent);
  title_->setText(parent == overview_ ? tr("Tools > Alarm clock") : parent->property(kBreadcrumbProperty).toString());
  if (parent == overview_) RefreshOverview();
  else if (parent == settings_) RefreshSettings();
  else if (parent == editor_) RefreshEditor();
  setFocus(Qt::OtherFocusReason);

}

void AlarmClockPage::BackTo(QWidget *page) {

  while (pages_->count() > 1 && pages_->currentWidget() != page) Back();

}

void AlarmClockPage::RefreshOverview() {

  QScrollArea *area = qobject_cast<QScrollArea*>(overview_);
  const int scroll_position = area->verticalScrollBar()->value();

  QWidget *contents = new QWidget;
  contents->setBackgroundRole(QPalette::Base);
  contents->setAutoFillBackground(true);
  QVBoxLayout *layout = new QVBoxLayout(contents);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  AddPermissionWarnings(layout);

  AlarmList alarms = AlarmBridge::Alarms();
  std::stable_sort(alarms.begin(), alarms.end(), [](const Alarm &a, const Alarm &b) { return a.hour * 60 + a.minute < b.hour * 60 + b.minute; });
  if (alarms.isEmpty()) layout->addWidget(CreateWrappingLabel(tr("There are no alarms yet."), contents));

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  qint64 next_snooze = 0;
  for (const Alarm &alarm : std::as_const(alarms)) {
    QFrame *row = new QFrame(contents);
    row->setFrameShape(QFrame::StyledPanel);
    QSizePolicy row_policy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    row_policy.setHeightForWidth(true);
    row->setSizePolicy(row_policy);
    QHBoxLayout *row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(8, 4, 4, 4);
    row_layout->setSpacing(2);
    const QString time = TimeText(alarm.hour, alarm.minute);
    const QString frequency = AlarmText::Frequency(alarm.days);
    QLabel *heading = CreateWrappingLabel(QString(), row);
    heading->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    heading->setTextFormat(Qt::RichText);
    QStringList lines{AlarmText::Kind(alarm)};
    if (!alarm.name.isEmpty()) lines << alarm.name;
    const qint64 snooze = AlarmBridge::SnoozeUntilMsec(alarm.id);
    if (snooze > now) {
      lines << AlarmText::Snooze(QLocale().toString(QDateTime::fromMSecsSinceEpoch(snooze).time(), QLocale::ShortFormat), AlarmText::MinutesLeft(now, snooze));
      if (next_snooze <= 0 || snooze < next_snooze) next_snooze = snooze;
    }
    QString label_text = u"<span style=\"font-size:%1px; font-weight:bold\">%2</span> | %3"_s.arg(qRound(QFontInfo(heading->font()).pixelSize() * 1.3)).arg(time.toHtmlEscaped(), frequency.toHtmlEscaped());
    for (const QString &line : std::as_const(lines)) {
      label_text += u"<br>"_s + line.toHtmlEscaped();
    }
    heading->setText(label_text);
    row_layout->addWidget(heading, 1);
    // A switched-off alarm stays readable but dimmed.
    if (!alarm.enabled) {
      const QColor text = row->palette().color(QPalette::WindowText);
      const QColor background = row->palette().color(QPalette::Base);
      const QColor dimmed = QColor::fromRgbF((text.redF() * 3 + background.redF() * 2) / 5, (text.greenF() * 3 + background.greenF() * 2) / 5, (text.blueF() * 3 + background.blueF() * 2) / 5);
      heading->setStyleSheet(u"color: %1"_s.arg(dimmed.name()));
    }
    const QString description = time + u", "_s + frequency + u", "_s + lines.join(", "_L1);

    QWidget *actions = new QWidget(row);
    actions->setFixedWidth(AndroidWidgets::kMinimumTouchSize * 3 + 4);
    QHBoxLayout *actions_layout = new QHBoxLayout(actions);
    actions_layout->setContentsMargins(0, 0, 0, 0);
    actions_layout->setSpacing(2);
    row_layout->addWidget(actions);

    QPushButton *edit = CreateButton(QString(), actions);
    edit->setFixedWidth(AndroidWidgets::kMinimumTouchSize);
    edit->setIcon(IconLoader::Load(u"edit-rename"_s));
    edit->setIconSize(QSize(AndroidWidgets::kNavigationIconSize, AndroidWidgets::kNavigationIconSize));
    edit->setAccessibleName(tr("Edit alarm") + u": "_s + description);
    actions_layout->addWidget(edit);

    QPushButton *remove = CreateButton(QString(), actions);
    remove->setFixedWidth(AndroidWidgets::kMinimumTouchSize);
    remove->setIcon(IconLoader::Load(u"edit-delete"_s));
    remove->setIconSize(QSize(AndroidWidgets::kNavigationIconSize, AndroidWidgets::kNavigationIconSize));
    remove->setAccessibleName(tr("Delete alarm") + u": "_s + description);
    actions_layout->addWidget(remove);

    QCheckBox *enabled = new QCheckBox(actions);
    enabled->setChecked(alarm.enabled);
    enabled->setAccessibleName(tr("Alarm on") + u": "_s + description);
    AndroidWidgets::AdaptCheckBox(enabled);
    enabled->setFixedWidth(AndroidWidgets::kMinimumTouchSize);
    actions_layout->addWidget(enabled);

    const QString id = alarm.id;
    QObject::connect(edit, &QPushButton::clicked, this, [this, id]() { ShowEditor(id); });
    QObject::connect(remove, &QPushButton::clicked, this, [this, alarm]() { ShowDeletePage(alarm); });
    QObject::connect(enabled, &QCheckBox::toggled, this, [this, id](const bool checked) {
      Alarm stored = AlarmBridge::FindAlarm(id);
      if (stored.id.isEmpty()) return;
      stored.enabled = checked;
      AlarmBridge::SaveAlarm(stored);
      if (checked) AlarmBridge::RequestRuntimePermissions();
      if (checked && AlarmClock::UsesTracks(stored) && stored.tracks.isEmpty()) alarm_clock_->UpdateTracks(id);
      // Not from within the check box's own signal, since refreshing deletes it.
      QTimer::singleShot(0, this, [this]() {
        if (pages_->currentWidget() == overview_) RefreshOverview();
      });
    });
    layout->addWidget(row);
  }

  QPushButton *add = CreateButton(tr("Add alarm"), contents);
  SetAddIcon(add);
  layout->addWidget(add);
  QObject::connect(add, &QPushButton::clicked, this, [this]() {
    Alarm alarm;
    alarm.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    AlarmBridge::SaveAlarm(alarm);
    AlarmBridge::RequestRuntimePermissions();
    alarm_clock_->UpdateTracks(alarm.id);
    ShowEditor(alarm.id);
  });

  // The next alarm includes snoozed alarms; a snoozed alarm that goes off first is shown with its countdown.
  const qint64 next = AlarmBridge::NextAlarmMsec();
  const QString next_time = QLocale().toString(QDateTime::fromMSecsSinceEpoch(next), u"ddd "_s + QLocale().timeFormat(QLocale::ShortFormat));
  if (next_snooze > 0 && next_snooze <= next) layout->addWidget(CreateWrappingLabel(tr("Next alarm: %1").arg(AlarmText::Snooze(next_time, AlarmText::MinutesLeft(now, next_snooze))), contents));
  else layout->addWidget(CreateWrappingLabel(next > 0 ? tr("Next alarm: %1").arg(next_time) : tr("No alarm set"), contents));
  // Refreshes the countdown while the overview is open; hiding the page stops the timer.
  if (next_snooze > 0) countdown_timer_->start(static_cast<int>(AlarmText::RefreshDelayMsec(now, next_snooze) + 50));
  else countdown_timer_->stop();

  QPushButton *settings = CreateButton(tr("Alarm settings"), contents);
  settings->setIconSize(QSize(AndroidWidgets::kMinimumTouchSize * 2 / 3, AndroidWidgets::kMinimumTouchSize * 2 / 3));
  AndroidWidgets::SetThemedIcon(settings, u":/icons/scalable/alarm-settings.svg"_s);
  layout->addWidget(settings);
  QObject::connect(settings, &QPushButton::clicked, this, &AlarmClockPage::ShowSettingsPage);

  layout->addStretch();
  area->setWidget(contents);
  overview_contents_ = contents;
  QTimer::singleShot(0, area, [area, scroll_position]() { area->verticalScrollBar()->setValue(scroll_position); });

}

void AlarmClockPage::ShowSettingsPage() {

  const AndroidWidgets::ScrollPage page = CreatePage();
  settings_ = page.area;
  SetPage(page.area, tr("Alarm settings"));
  RefreshSettings();

}

void AlarmClockPage::RefreshSettings() {

  QScrollArea *area = qobject_cast<QScrollArea*>(settings_.data());
  if (!area) return;
  const int scroll_position = area->verticalScrollBar()->value();

  QWidget *contents = new QWidget;
  contents->setBackgroundRole(QPalette::Base);
  contents->setAutoFillBackground(true);
  QVBoxLayout *layout = new QVBoxLayout(contents);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  ValueButton *auto_stop = AddValueButton(layout, tr("Stop automatically after"));
  const int auto_stop_minutes = AlarmBridge::AutoStopMinutes();
  auto_stop->SetValue(MinutesText(auto_stop_minutes));
  QObject::connect(auto_stop, &QPushButton::clicked, this, [this, auto_stop_minutes]() {
    ShowChoicePage(tr("Stop automatically after"), {{0, MinutesText(0)}, {10, MinutesText(10)}, {30, MinutesText(30)}, {60, MinutesText(60)}}, auto_stop_minutes, [](const int minutes) { AlarmBridge::SetAutoStopMinutes(minutes); });
  });

  AddTrackSettings(layout);

  layout->addStretch();
  area->setWidget(contents);
  QTimer::singleShot(0, area, [area, scroll_position]() { area->verticalScrollBar()->setValue(scroll_position); });

}

void AlarmClockPage::AddTrackSettings(QVBoxLayout *layout) {

  QWidget *contents = layout->parentWidget();
  const AlarmTrackSettings settings = AlarmClock::TrackSettings();
  QCheckBox *copy = new QCheckBox(tr("Copy tracks for offline alarms"), contents);
  copy->setChecked(settings.copy);
  AndroidWidgets::AdaptCheckBox(copy);
  layout->addWidget(copy);
  layout->addWidget(CreateWrappingLabel(settings.copy ? tr("Alarms play copies of their songs in the app's storage, also without network or NAS.") : tr("Alarms play the original songs. Songs on the NAS can't play this way, and online songs need network access; otherwise the default alarm sound plays."), contents));
  QObject::connect(copy, &QCheckBox::toggled, this, [this](const bool checked) {
    AlarmTrackSettings changed = AlarmClock::TrackSettings();
    changed.copy = checked;
    alarm_clock_->SetTrackSettings(changed);
    // Not from within the check box's own signal, since refreshing deletes it.
    QTimer::singleShot(0, this, [this]() {
      if (settings_ && pages_->currentWidget() == settings_) RefreshSettings();
    });
  });

  if (settings.copy) {
    ValueButton *tracks = AddValueButton(layout, tr("Tracks per alarm"));
    tracks->SetValue(QString::number(settings.tracks_per_alarm));
    QObject::connect(tracks, &QPushButton::clicked, this, [this, settings]() {
      QList<Choice> choices;
      for (const int count : AlarmTracks::TracksPerAlarmChoices()) choices << Choice(count, QString::number(count));
      ShowChoicePage(tr("Tracks per alarm"), choices, settings.tracks_per_alarm, [this](const int count) {
        AlarmTrackSettings changed = AlarmClock::TrackSettings();
        changed.tracks_per_alarm = count;
        alarm_clock_->SetTrackSettings(changed);
      });
    });
    ValueButton *total = AddValueButton(layout, tr("Maximum total size"));
    total->SetValue(SizeText(settings.maximum_total_mb));
    QObject::connect(total, &QPushButton::clicked, this, [this, settings]() {
      QList<Choice> choices;
      for (const int megabytes : AlarmTracks::MaximumTotalMbChoices()) choices << Choice(megabytes, SizeText(megabytes));
      ShowChoicePage(tr("Maximum total size"), choices, settings.maximum_total_mb, [this](const int megabytes) {
        AlarmTrackSettings changed = AlarmClock::TrackSettings();
        changed.maximum_total_mb = megabytes;
        alarm_clock_->SetTrackSettings(changed);
      });
    });
    layout->addWidget(CreateWrappingLabel(tr("Changed limits apply the next time the songs of an alarm are copied."), contents));
  }

  QPushButton *remove = CreateButton(tr("Delete copied tracks now"), contents);
  remove->setEnabled(AlarmClock::CopiesSize() > 0);
  layout->addWidget(remove);
  QObject::connect(remove, &QPushButton::clicked, this, &AlarmClockPage::ShowDeleteCopiesPage);

}

void AlarmClockPage::ShowDeleteCopiesPage() {

  const AndroidWidgets::ScrollPage page = CreatePage();
  const qint64 size = AlarmClock::CopiesSize();
  QString text = tr("Delete the copied tracks of all alarms? This frees %1.").arg(Utilities::PrettySize(static_cast<quint64>(size)));
  if (AlarmClock::TrackSettings().copy) text += u' ' + tr("Until the songs are copied again, the next time Strawberry starts or an alarm's music changes, alarms play the default alarm sound.");
  page.layout->addWidget(CreateWrappingLabel(text, page.contents));
  QPushButton *remove = CreateButton(tr("Delete copied tracks"), page.contents);
  page.layout->addWidget(remove);
  QPushButton *cancel = CreateButton(tr("Cancel"), page.contents);
  page.layout->addWidget(cancel);
  page.layout->addStretch();
  QObject::connect(cancel, &QPushButton::clicked, this, &AlarmClockPage::Back);
  QObject::connect(remove, &QPushButton::clicked, this, [this]() {
    alarm_clock_->DeleteCopies();
    Back();
  });
  SetPage(page.area, tr("Delete copied tracks"));

}

void AlarmClockPage::AddPermissionWarnings(QVBoxLayout *layout) {

  const int missing = AlarmBridge::MissingPermissions();
  const QString no_stop = tr("The alarm may then go off without any way to stop it.");
  const QList<std::pair<int, std::pair<QString, QString>>> permissions = {
    {AlarmBridge::kPermissionNotifications, {tr("Notifications are off for Strawberry, so an alarm that goes off cannot show its Snooze and Stop buttons. %1").arg(no_stop), tr("Allow notifications")}},
    {AlarmBridge::kPermissionChannel, {tr("Alarm clock notifications are switched off, so an alarm that goes off cannot show its Snooze and Stop buttons. %1").arg(no_stop), tr("Turn on alarm notifications")}},
    {AlarmBridge::kPermissionOverlay, {tr("While notifications are off, allowing Strawberry to display over other apps lets the alarm screen with its Stop button open anyway."), tr("Allow display over other apps")}},
    {AlarmBridge::kPermissionExactAlarms, {tr("Exact alarms are not allowed, so alarms may go off late and only with the default alarm sound until you open them."), tr("Allow exact alarms")}},
    {AlarmBridge::kPermissionFullScreen, {tr("Full-screen alarms are not allowed, so an alarm that goes off cannot show its alarm screen over the lock screen; stop it from the notification instead."), tr("Allow full-screen alarms")}},
  };
  for (const auto &permission : permissions) {
    if (!(missing & permission.first)) continue;
    // A framed banner, so the warning stands out from the alarm list in light and dark themes.
    QFrame *banner = new QFrame(layout->parentWidget());
    banner->setObjectName(u"alarm_permission_warning"_s);
    banner->setStyleSheet(u"QFrame#alarm_permission_warning { border: 2px solid %1; border-radius: 8px; }"_s.arg(banner->palette().color(QPalette::Link).name()));
    QVBoxLayout *banner_layout = new QVBoxLayout(banner);
    banner_layout->setContentsMargins(12, 12, 12, 12);
    QLabel *warning = CreateWrappingLabel(permission.second.first, banner);
    warning->setForegroundRole(QPalette::Link);
    QFont font = warning->font();
    font.setBold(true);
    warning->setFont(font);
    banner_layout->addWidget(warning);
    QPushButton *allow = CreateButton(permission.second.second, banner);
    const int bit = permission.first;
    QObject::connect(allow, &QPushButton::clicked, this, [bit]() { AlarmBridge::RequestPermission(bit); });
    banner_layout->addWidget(allow);
    layout->addWidget(banner);
  }

}

void AlarmClockPage::ShowEditor(const QString &id) {

  alarm_ = AlarmBridge::FindAlarm(id);
  enabled_changed_ = false;
  if (alarm_.id.isEmpty()) {
    RefreshOverview();
    return;
  }

  const AndroidWidgets::ScrollPage page = CreatePage();
  QWidget *contents = page.contents;
  QVBoxLayout *layout = page.layout;

  enabled_ = new QCheckBox(tr("Alarm on"), contents);
  AndroidWidgets::AdaptCheckBox(enabled_);
  layout->addWidget(enabled_);
  QObject::connect(enabled_, &QCheckBox::toggled, this, [this](const bool checked) {
    alarm_.enabled = checked;
    enabled_changed_ = true;
    if (checked) AlarmBridge::RequestRuntimePermissions();
    if (checked && AlarmClock::UsesTracks(alarm_) && alarm_.tracks.isEmpty()) {
      Save();
      alarm_clock_->UpdateTracks(alarm_.id);
    }
    else {
      ScheduleSave();
    }
  });

  time_button_ = AddValueButton(layout, tr("Time"));
  QObject::connect(time_button_, &QPushButton::clicked, this, &AlarmClockPage::ShowTimePage);

  QLabel *name_label = CreateWrappingLabel(tr("Name"), contents);
  layout->addWidget(name_label);
  name_ = new QLineEdit(contents);
  name_->setMinimumHeight(AndroidWidgets::kMinimumTouchSize);
  name_->setPlaceholderText(tr("Alarm"));
  name_->setAccessibleName(tr("Name"));
  name_label->setBuddy(name_);
  layout->addWidget(name_);
  QObject::connect(name_, &QLineEdit::textEdited, this, [this](const QString &text) {
    alarm_.name = text.trimmed();
    ScheduleSave();
  });

  days_button_ = AddValueButton(layout, tr("Repeat"));
  QObject::connect(days_button_, &QPushButton::clicked, this, &AlarmClockPage::ShowDaysPage);

  source_button_ = AddValueButton(layout, tr("Music"));
  QObject::connect(source_button_, &QPushButton::clicked, this, &AlarmClockPage::ShowSourcePage);

  volume_label_ = CreateWrappingLabel(QString(), contents);
  layout->addWidget(volume_label_);
  volume_ = new QSlider(Qt::Horizontal, contents);
  volume_->setRange(0, 100);
  volume_->setSingleStep(5);
  volume_->setPageStep(10);
  volume_->setMinimumHeight(AndroidWidgets::kMinimumTouchSize);
  volume_->setStyleSheet(kSliderStyle);
  volume_->setAccessibleName(tr("Volume"));
  volume_label_->setBuddy(volume_);
  layout->addWidget(volume_);
  QObject::connect(volume_, &QSlider::valueChanged, this, [this](const int value) {
    alarm_.volume = value;
    volume_label_->setText(tr("Volume: %1%").arg(value));
    ScheduleSave();
  });

  fade_in_button_ = AddValueButton(layout, tr("Fade in"));
  QObject::connect(fade_in_button_, &QPushButton::clicked, this, [this]() {
    QList<Choice> choices;
    for (const int seconds : {0, 30, 60, 120, 300}) choices << Choice(seconds, FadeInText(seconds));
    ShowChoicePage(tr("Fade in"), choices, alarm_.fade_in_seconds, [this](const int seconds) {
      alarm_.fade_in_seconds = seconds;
      Save();
    });
  });

  snooze_button_ = AddValueButton(layout, tr("Snooze for"));
  QObject::connect(snooze_button_, &QPushButton::clicked, this, [this]() {
    QList<Choice> choices;
    for (const int minutes : {5, 10, 15, 20, 30}) choices << Choice(minutes, MinutesText(minutes));
    ShowChoicePage(tr("Snooze for"), choices, alarm_.snooze_minutes, [this](const int minutes) {
      alarm_.snooze_minutes = minutes;
      Save();
    });
  });

  QPushButton *remove = CreateButton(tr("Delete alarm"), contents);
  layout->addSpacing(16);
  layout->addWidget(remove);
  QObject::connect(remove, &QPushButton::clicked, this, [this]() { ShowDeletePage(alarm_); });
  layout->addStretch();

  editor_ = page.area;
  SetPage(page.area, tr("Edit alarm"));
  RefreshEditor();

}

void AlarmClockPage::RefreshEditor() {

  if (!editor_ || alarm_.id.isEmpty()) return;
  // Only the parts the Java side changes, so edits that are not saved yet are kept.
  const Alarm stored = AlarmBridge::FindAlarm(alarm_.id);
  if (!stored.id.isEmpty() && !enabled_changed_) alarm_.enabled = stored.enabled;
  {
    const QSignalBlocker blocker(enabled_);
    enabled_->setChecked(alarm_.enabled);
  }
  time_button_->SetValue(TimeText(alarm_.hour, alarm_.minute));
  if (name_->text().trimmed() != alarm_.name) name_->setText(alarm_.name);
  days_button_->SetValue(AlarmText::Frequency(alarm_.days));
  source_button_->SetValue(SourceText(alarm_));
  {
    const QSignalBlocker blocker(volume_);
    volume_->setValue(alarm_.volume);
  }
  volume_label_->setText(tr("Volume: %1%").arg(alarm_.volume));
  fade_in_button_->SetValue(FadeInText(alarm_.fade_in_seconds));
  snooze_button_->SetValue(MinutesText(alarm_.snooze_minutes));

}

void AlarmClockPage::ShowTimePage() {

  // The dial needs every drag itself, so this page does not scroll.
  QWidget *page = new QWidget;
  page->setBackgroundRole(QPalette::Base);
  page->setAutoFillBackground(true);
  QVBoxLayout *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 8, 0, 0);

  TimePicker *picker = new TimePicker(TimePicker::DeviceUses24HourFormat(), page);
  picker->SetTime(alarm_.hour, alarm_.minute);
  layout->addWidget(picker);
  QObject::connect(picker, &TimePicker::TimeChanged, this, [this](const int hour, const int minute) {
    alarm_.hour = hour;
    alarm_.minute = minute;
    ScheduleSave();
  });
  QObject::connect(picker, &TimePicker::Done, this, &AlarmClockPage::Back);
  SetPage(page, tr("Time"));

}

void AlarmClockPage::ShowDaysPage() {

  const AndroidWidgets::ScrollPage page = CreatePage();
  QWidget *contents = page.contents;
  QVBoxLayout *layout = page.layout;
  layout->addWidget(CreateWrappingLabel(tr("The alarm goes off on the selected days. Without a day it goes off once."), contents));

  const QLocale locale;
  const int first = static_cast<int>(locale.firstDayOfWeek());
  for (int i = 0; i < 7; ++i) {
    const int day = (first - 1 + i) % 7 + 1;
    const int bit = Alarm::kMonday << (day - 1);
    QRadioButton *row = AndroidWidgets::CreateChoiceRow(locale.standaloneDayName(day, QLocale::LongFormat), contents);
    row->setAutoExclusive(false);
    row->setChecked(alarm_.days & bit);
    QObject::connect(row, &QRadioButton::toggled, this, [this, bit](const bool checked) {
      alarm_.days = checked ? (alarm_.days | bit) : (alarm_.days & ~bit);
      ScheduleSave();
    });
    layout->addWidget(row);
  }
  layout->addStretch();
  SetPage(page.area, tr("Repeat"));

}

void AlarmClockPage::ShowSourcePage() {

  const AndroidWidgets::ScrollPage page = CreatePage();
  QWidget *contents = page.contents;
  QVBoxLayout *layout = page.layout;

  const QList<Alarm::Source> simple_sources = {Alarm::Source::Random, Alarm::Source::LastPlayed, Alarm::Source::Sound};
  for (const Alarm::Source source : simple_sources) {
    Alarm example;
    example.source = source;
    QRadioButton *row = AndroidWidgets::CreateChoiceRow(SourceText(example), contents);
    row->setChecked(alarm_.source == source);
    QObject::connect(row, &QRadioButton::clicked, this, [this, source]() {
      SetSource(source);
      Back();
    });
    layout->addWidget(row);
  }

  // These open their own list, so they are navigation buttons rather than choices.
  ValueButton *playlist = new ValueButton(tr("Playlist"), contents);
  if (alarm_.source == Alarm::Source::Playlist) playlist->SetValue(alarm_.source_name);
  QObject::connect(playlist, &QPushButton::clicked, this, &AlarmClockPage::ShowPlaylistPage);
  layout->addWidget(playlist);
  ValueButton *song = new ValueButton(tr("Song"), contents);
  if (alarm_.source == Alarm::Source::Song) song->SetValue(alarm_.source_name);
  QObject::connect(song, &QPushButton::clicked, this, &AlarmClockPage::ShowSongPage);
  layout->addWidget(song);
  ValueButton *radio = new ValueButton(tr("Radio station"), contents);
  if (alarm_.source == Alarm::Source::Radio) radio->SetValue(alarm_.source_name);
  QObject::connect(radio, &QPushButton::clicked, this, &AlarmClockPage::ShowRadioPage);
  layout->addWidget(radio);

  layout->addSpacing(16);
  layout->addWidget(CreateWrappingLabel(tr("Songs are copied to the device, so the alarm also plays without a network connection. Radio needs a network connection. When the music cannot be played, the default alarm sound plays."), contents));
  layout->addStretch();
  SetPage(page.area, tr("Music"));

}

void AlarmClockPage::ShowPlaylistPage() {

  const AndroidWidgets::ScrollPage page = CreatePage();
  QPointer<QWidget> contents = page.contents;
  QVBoxLayout *layout = page.layout;
  QLabel *status = CreateWrappingLabel(tr("Loading playlists..."), contents);
  layout->addWidget(status);
  layout->addStretch();
  SetPage(page.area, tr("Playlist"));

  SharedPtr<PlaylistBackend> backend = alarm_clock_->playlist_backend();
  QFutureWatcher<PlaylistBackend::PlaylistList> *watcher = new QFutureWatcher<PlaylistBackend::PlaylistList>(this);
  QObject::connect(watcher, &QFutureWatcher<PlaylistBackend::PlaylistList>::finished, this, [this, watcher, contents, layout, status]() {
    watcher->deleteLater();
    if (!contents) return;
    const PlaylistBackend::PlaylistList playlists = watcher->result();
    status->setText(playlists.isEmpty() ? tr("There are no playlists.") : tr("Songs in the playlist that are not stored locally or on the NAS are skipped."));
    int index = 1;
    for (const PlaylistBackend::Playlist &playlist : playlists) {
      QRadioButton *row = AndroidWidgets::CreateChoiceRow(playlist.name, contents);
      const QString id = QString::number(playlist.id);
      row->setChecked(alarm_.source == Alarm::Source::Playlist && alarm_.source_id == id);
      const QString name = playlist.name;
      QObject::connect(row, &QRadioButton::clicked, this, [this, id, name]() {
        SetSource(Alarm::Source::Playlist, id, name);
        BackTo(editor_);
      });
      layout->insertWidget(index++, row);
    }
  });
  watcher->setFuture(QtConcurrent::run([backend]() {
    const PlaylistBackend::PlaylistList playlists = backend->GetAllPlaylists();
    backend->Close();
    return playlists;
  }));

}

void AlarmClockPage::ShowSongPage() {

  const AndroidWidgets::ScrollPage page = CreatePage();
  QPointer<QWidget> contents = page.contents;
  QVBoxLayout *layout = page.layout;
  QLineEdit *search = new QLineEdit(contents);
  search->setMinimumHeight(AndroidWidgets::kMinimumTouchSize);
  search->setPlaceholderText(tr("Title, artist or album"));
  search->setAccessibleName(tr("Search"));
  layout->addWidget(search);
  QPushButton *find = CreateButton(tr("Search"), contents);
  layout->addWidget(find);
  QLabel *status = CreateWrappingLabel(QString(), contents);
  layout->addWidget(status);
  QWidget *results = new QWidget(contents);
  QVBoxLayout *results_layout = new QVBoxLayout(results);
  results_layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(results);
  layout->addStretch();
  SetPage(page.area, tr("Song"));

  SharedPtr<CollectionBackend> backend = alarm_clock_->collection_backend();
  const auto run_search = [this, backend, contents, search, status, results, results_layout]() {
    const QString text = search->text().trimmed();
    if (text.isEmpty()) return;
    status->setText(tr("Searching..."));
    while (QLayoutItem *item = results_layout->takeAt(0)) {
      delete item->widget();
      delete item;
    }
    QFutureWatcher<SongList> *watcher = new QFutureWatcher<SongList>(this);
    QObject::connect(watcher, &QFutureWatcher<SongList>::finished, this, [this, watcher, contents, status, results, results_layout]() {
      watcher->deleteLater();
      if (!contents) return;
      const SongList songs = watcher->result();
      status->setText(songs.isEmpty() ? tr("No songs found.") : QString());
      for (const Song &song : songs) {
        QRadioButton *row = AndroidWidgets::CreateChoiceRow(SongText(song), results);
        const QString url = song.url().toString();
        const QString name = SongText(song);
        QObject::connect(row, &QRadioButton::clicked, this, [this, url, name]() {
          SetSource(Alarm::Source::Song, QString(), name, url);
          BackTo(editor_);
        });
        results_layout->addWidget(row);
      }
    });
    watcher->setFuture(QtConcurrent::run([backend, text]() {
      const SmartPlaylistSearchTerm::Operator contains = SmartPlaylistSearchTerm::Operator::Contains;
      const SmartPlaylistSearch query(SmartPlaylistSearch::SearchType::Or,
                                      {SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Title, contains, text),
                                       SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Artist, contains, text),
                                       SmartPlaylistSearchTerm(SmartPlaylistSearchTerm::Field::Album, contains, text)},
                                      SmartPlaylistSearch::SortType::FieldAsc, SmartPlaylistSearchTerm::Field::Artist, kSongSearchLimit);
      QVariantList bound_values;
      const QString sql = query.ToSql(backend->songs_table(), bound_values);
      const SongList songs = backend->ExecuteQuery(sql, bound_values);
      backend->Close();
      return songs;
    }));
  };
  QObject::connect(search, &QLineEdit::returnPressed, this, run_search);
  QObject::connect(find, &QPushButton::clicked, this, run_search);
  search->setFocus(Qt::OtherFocusReason);

}

void AlarmClockPage::ShowRadioPage() {

  const AndroidWidgets::ScrollPage page = CreatePage();
  QWidget *contents = page.contents;
  QVBoxLayout *layout = page.layout;

  QLabel *url_label = CreateWrappingLabel(tr("Stream URL"), contents);
  layout->addWidget(url_label);
  QLineEdit *url = new QLineEdit(contents);
  url->setMinimumHeight(AndroidWidgets::kMinimumTouchSize);
  url->setPlaceholderText(u"https://"_s);
  url->setInputMethodHints(Qt::ImhUrlCharactersOnly | Qt::ImhNoAutoUppercase);
  url_label->setBuddy(url);
  if (alarm_.source == Alarm::Source::Radio) url->setText(alarm_.source_url);
  layout->addWidget(url);
  QPushButton *use_url = CreateButton(tr("Use this URL"), contents);
  layout->addWidget(use_url);
  QObject::connect(use_url, &QPushButton::clicked, this, [this, url]() {
    const QUrl stream = QUrl::fromUserInput(url->text().trimmed());
    if (!stream.isValid() || (stream.scheme() != "http"_L1 && stream.scheme() != "https"_L1)) return;
    SetSource(Alarm::Source::Radio, QString(), stream.host(), stream.toString());
    BackTo(editor_);
  });

  QLabel *status = CreateWrappingLabel(QString(), contents);
  layout->addWidget(status);
  QWidget *channels = new QWidget(contents);
  QVBoxLayout *channels_layout = new QVBoxLayout(channels);
  channels_layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(channels);
  layout->addStretch();
  SetPage(page.area, tr("Radio station"));

  QAbstractItemModel *model = radio_services_->sort_model();
  const auto fill = [this, model, status, channels, channels_layout]() {
    while (QLayoutItem *item = channels_layout->takeAt(0)) {
      delete item->widget();
      delete item;
    }
    int count = 0;
    for (int service_row = 0; service_row < model->rowCount(); ++service_row) {
      const QModelIndex service = model->index(service_row, 0);
      QLabel *heading = CreateWrappingLabel(service.data().toString(), channels);
      QFont font = heading->font();
      font.setBold(true);
      heading->setFont(font);
      channels_layout->addWidget(heading);
      for (int row = 0; row < model->rowCount(service); ++row) {
        const QModelIndex channel = model->index(row, 0, service);
        // The model provides the stream URL only through its drag and drop data.
        std::unique_ptr<QMimeData> data(model->mimeData({channel}));
        const QUrl stream = data && !data->urls().isEmpty() ? data->urls().constFirst() : QUrl();
        if (!stream.isValid()) continue;
        const QString name = channel.data().toString();
        QRadioButton *choice = AndroidWidgets::CreateChoiceRow(name, channels);
        choice->setChecked(alarm_.source == Alarm::Source::Radio && alarm_.source_url == stream.toString());
        QObject::connect(choice, &QRadioButton::clicked, this, [this, name, stream]() {
          SetSource(Alarm::Source::Radio, QString(), name, stream.toString());
          BackTo(editor_);
        });
        channels_layout->addWidget(choice);
        ++count;
      }
    }
    status->setText(count == 0 ? tr("Loading radio stations...") : QString());
  };
  // The radio stations are loaded when the Radios tab is first shown; load them here if that did not happen yet.
  if (model->rowCount() == 0) radio_services_->GetChannels();
  fill();
  QTimer *refresh = new QTimer(channels);
  refresh->setSingleShot(true);
  refresh->setInterval(300);
  QObject::connect(refresh, &QTimer::timeout, channels, fill);
  QObject::connect(model, &QAbstractItemModel::rowsInserted, refresh, qOverload<>(&QTimer::start));
  QObject::connect(model, &QAbstractItemModel::modelReset, refresh, qOverload<>(&QTimer::start));

}

void AlarmClockPage::ShowChoicePage(const QString &title, const QList<Choice> &choices, const int current, const std::function<void(int)> &chosen) {

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

void AlarmClockPage::ShowDeletePage(const Alarm &alarm) {

  const AndroidWidgets::ScrollPage page = CreatePage();
  const QString name = alarm.name.isEmpty() ? TimeText(alarm.hour, alarm.minute) : alarm.name;
  const QString id = alarm.id;
  page.layout->addWidget(CreateWrappingLabel(tr("Are you sure you want to delete the alarm \"%1\"?").arg(name), page.contents));
  QPushButton *remove = CreateButton(tr("Delete alarm"), page.contents);
  page.layout->addWidget(remove);
  QPushButton *cancel = CreateButton(tr("Cancel"), page.contents);
  page.layout->addWidget(cancel);
  page.layout->addStretch();
  QObject::connect(cancel, &QPushButton::clicked, this, &AlarmClockPage::Back);
  QObject::connect(remove, &QPushButton::clicked, this, [this, id]() {
    save_timer_->stop();
    if (alarm_.id == id) alarm_ = Alarm();
    AlarmBridge::DeleteAlarm(id);
    alarm_clock_->RemoveTracks(id);
    BackTo(overview_);
  });
  SetPage(page.area, tr("Delete alarm"));

}

void AlarmClockPage::SetSource(const Alarm::Source source, const QString &source_id, const QString &source_name, const QString &source_url) {

  const bool changed = alarm_.source != source || alarm_.source_id != source_id || alarm_.source_url != source_url;
  alarm_.source = source;
  alarm_.source_id = source_id;
  alarm_.source_name = source_name;
  alarm_.source_url = source_url;
  if (!changed) {
    Save();
    return;
  }
  // The songs of the previous source must not play while the new ones are copied.
  alarm_.tracks.clear();
  alarm_.tracks_updated = 0;
  save_timer_->stop();
  Save();
  if (AlarmClock::UsesTracks(alarm_)) alarm_clock_->UpdateTracks(alarm_.id);
  else alarm_clock_->RemoveTracks(alarm_.id);

}

void AlarmClockPage::ScheduleSave() {
  save_timer_->start();
}

void AlarmClockPage::Save() {

  if (alarm_.id.isEmpty()) return;
  const Alarm stored = AlarmBridge::FindAlarm(alarm_.id);
  // The alarm was deleted, for example on another page.
  if (stored.id.isEmpty()) return;
  // The alarm service switches a one-off alarm off once it has gone off, so only an explicit change in the editor overrides the stored state.
  if (!enabled_changed_) alarm_.enabled = stored.enabled;
  enabled_changed_ = false;
  // The song copies are updated in the background, keep the latest ones.
  if (stored.source == alarm_.source && stored.source_id == alarm_.source_id && stored.source_url == alarm_.source_url) {
    alarm_.tracks = stored.tracks;
    alarm_.tracks_updated = stored.tracks_updated;
  }
  AlarmBridge::SaveAlarm(alarm_);

}

QString AlarmClockPage::TimeText(const int hour, const int minute) {
  return QLocale().toString(QTime(hour, minute), QLocale::ShortFormat);
}

QString AlarmClockPage::SourceText(const Alarm &alarm) {

  switch (alarm.source) {
    case Alarm::Source::Sound:
      return tr("Default alarm sound");
    case Alarm::Source::Random:
      return tr("Random songs from the collection");
    case Alarm::Source::LastPlayed:
      return tr("Recently played songs");
    case Alarm::Source::Playlist:
      return tr("Playlist: %1").arg(alarm.source_name);
    case Alarm::Source::Song:
      return tr("Song: %1").arg(alarm.source_name);
    case Alarm::Source::Radio:
      return tr("Radio: %1").arg(alarm.source_name);
  }
  return QString();

}

QString AlarmClockPage::SizeText(const int megabytes) {
  return megabytes >= 1024 && megabytes % 1024 == 0 ? tr("%1 GB").arg(megabytes / 1024) : tr("%1 MB").arg(megabytes);
}

QString AlarmClockPage::MinutesText(const int minutes) {
  return minutes <= 0 ? tr("Off") : tr("%n minute(s)", nullptr, minutes);
}

QString AlarmClockPage::FadeInText(const int seconds) {

  if (seconds <= 0) return tr("Off");
  if (seconds < 60) return tr("%n second(s)", nullptr, seconds);
  return MinutesText(seconds / 60);

}
