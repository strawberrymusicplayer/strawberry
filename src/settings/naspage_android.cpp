// SPDX-License-Identifier: GPL-3.0-or-later

#include "naspage_android.h"

#include <utility>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include "settingspage.h"
#include "settingsdialog.h"
#include "widgets/androidwidgets.h"

namespace {

QLabel *Label(const QString &text, QVBoxLayout *layout) {
  auto *label = new QLabel(text);
  label->setTextFormat(Qt::PlainText);
  label->setWordWrap(true);
  layout->addWidget(label);
  return label;
}

QPushButton *Button(const QString &text, QVBoxLayout *layout) {
  auto *button = new AndroidWidgets::LayoutButton<QPushButton>(layout->parentWidget());
  auto *labels = new QVBoxLayout(button);
  AndroidWidgets::AddButtonLabel(labels, text);
  button->setAccessibleName(text);
  button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  button->setMinimumHeight(48);
  button->setAutoDefault(false);
  layout->addWidget(button);
  return button;
}

QWidget *Content(SettingsPage *page, QVBoxLayout **layout) {
  auto *content = new QWidget(page->dialog());
  content->setProperty("androidTransientSubpage", true);
  *layout = new QVBoxLayout(content);
  (*layout)->setAlignment(Qt::AlignTop);
  content->hide();
  return content;
}

}  // namespace

NasPage::NasPage(SettingsPage *page, std::function<void(const Nas::Location &)> add_path)
    : QObject(page), page_(page), add_path_(std::move(add_path)) {}

void NasPage::Load() {
  load_error_.clear();
  if (!Nas::Load(&load_error_)) return;
  pending_ = Nas::Locations();
}

void NasPage::Open() {
  QVBoxLayout *layout;
  QWidget *content = Content(page_, &layout);
  Label(tr("Add a NAS folder on your network using SMB2/SMB3. Files are read-only. Enter the server and share manually; automatic discovery is not available."), layout);
  QPushButton *info = Button(tr("(i) Access outside your home network"), layout);
  info->setAccessibleName(tr("Information about VPN access"));
  QObject::connect(info, &QPushButton::clicked, content, [this]() {
    QVBoxLayout *tip_layout;
    QWidget *tip = Content(page_, &tip_layout);
    Label(tr("Outside your home network? Connect your phone to your home network using a separately configured VPN. Strawberry does not set up a VPN. Do not expose SMB directly to the Internet. Synology QuickConnect is not supported."), tip_layout);
    page_->dialog()->OpenAndroidSubpage(tip, tr("VPN access"));
  });
  if (!load_error_.isEmpty()) Label(load_error_, layout);
  else {
    for (const Nas::Location &location : std::as_const(pending_)) {
      QPushButton *edit = Button(location.server + u'/' + location.share + u'/' + location.folder, layout);
      QObject::connect(edit, &QPushButton::clicked, content, [this, location]() { Edit(location); });
    }
    QPushButton *add = Button(tr("Add NAS folder"), layout);
    QObject::connect(add, &QPushButton::clicked, content, [this]() {
      Nas::Location location;
      location.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      Edit(location);
    });
  }
  page_->dialog()->OpenAndroidSubpage(content, tr("NAS locations"));
}

void NasPage::Edit(const Nas::Location &location) {
  QVBoxLayout *layout;
  QWidget *content = Content(page_, &layout);
  const auto field = [layout](const QString &title, const QString &value, bool password = false) {
    Label(title, layout);
    auto *edit = new QLineEdit(value);
    edit->setMinimumHeight(48);
    edit->setAccessibleName(title);
    edit->setEchoMode(password ? QLineEdit::Password : QLineEdit::Normal);
    edit->setInputMethodHints(password ? Qt::ImhHiddenText | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase : Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);
    layout->addWidget(edit);
    return edit;
  };
  QLineEdit *server = field(tr("Server name or IP address"), location.server);
  QLineEdit *share = field(tr("Share"), location.share);
  QLineEdit *folder = field(tr("Folder within the share (optional)"), location.folder);
  QLineEdit *user = field(tr("Username"), location.user);
  QLineEdit *password = field(tr("Password"), location.password, true);
  QLineEdit *domain = field(tr("Domain (optional)"), location.domain);
  QLabel *error = Label(QString(), layout);
  Label(tr("Use a NAS account with read-only access. Changes remain pending until Apply or OK in Collection settings."), layout);
  QPushButton *check = Button(tr("Check and use folder"), layout);
  QObject::connect(check, &QPushButton::clicked, content, [this, content, location, server, share, folder, user, password, domain, error, check]() {
    Nas::Location edited = location;
    edited.server = server->text().trimmed();
    edited.share = share->text().trimmed();
    edited.folder = folder->text().trimmed();
    edited.user = user->text();
    edited.password = password->text();
    edited.domain = domain->text();
    const QString invalid = Nas::Validate(edited);
    if (!invalid.isEmpty()) { error->setText(invalid); return; }
    error->setText(tr("Checking NAS folder..."));
    check->setEnabled(false);
    auto *watcher = new QFutureWatcher<QString>(content);
    QObject::connect(watcher, &QFutureWatcher<QString>::finished, content, [this, watcher, content, edited, error, check]() {
      const QString result = watcher->result();
      watcher->deleteLater();
      check->setEnabled(true);
      if (!content->isVisible() || !page_->IsActive()) return;
      if (!result.isEmpty()) { error->setText(result); return; }
      bool replaced = false;
      for (Nas::Location &item : pending_) {
        if (item.id == edited.id) { item = edited; replaced = true; break; }
      }
      if (!replaced) pending_.append(edited);
      add_path_(edited);
      page_->set_changed();
      page_->dialog()->CloseAndroidSubpage();
      page_->dialog()->CloseAndroidSubpage();
    });
    watcher->setFuture(QtConcurrent::run([edited]() { return Nas::Check(edited); }));
  });
  page_->dialog()->OpenAndroidSubpage(content, tr("NAS folder"));
}

bool NasPage::Commit(const QStringList &paths, QString *error) {
  if (!load_error_.isEmpty()) { *error = load_error_; return false; }
  QList<Nas::Location> kept;
  for (const Nas::Location &location : std::as_const(pending_)) {
    if (paths.contains(location.path())) kept.append(location);
  }
  return Nas::Save(kept, error);
}
