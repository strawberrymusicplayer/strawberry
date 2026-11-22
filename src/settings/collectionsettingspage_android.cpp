// SPDX-License-Identifier: GPL-3.0-or-later

#include "collectionsettingspage.h"

#include <QDialog>
#include <QPushButton>

#include "collection/collectionbackend.h"
#include "settingsdialog.h"
#include "widgets/actiondialog.h"
#include "ui_collectionsettingspage.h"

using namespace Qt::Literals::StringLiterals;

void CollectionSettingsPage::SetupAndroidRemoveUnavailableSongs() {

  // Placed below the expiry setting, which removes the same songs after a number of days.
  auto *button = new QPushButton(tr("Remove unavailable songs now"), this);
  button->setObjectName(u"remove_unavailable_songs"_s);
  ui_->layout_updating->insertWidget(ui_->layout_updating->indexOf(ui_->widget) + 1, button);
  QObject::connect(button, &QPushButton::clicked, this, &CollectionSettingsPage::RemoveUnavailableSongs);

}

void CollectionSettingsPage::RemoveUnavailableSongs() {

  QWidget *parent = dialog() ? static_cast<QWidget*>(dialog()) : this;
  const int count = collection_backend_->UnavailableSongCount();
  if (count == 0) {
    auto *message = new ActionDialog(parent, tr("Remove unavailable songs"), tr("There are no unavailable songs in the collection."), ActionDialog::TitlePosition::Top);
    message->AddCancelButton(tr("Back"));
    message->Open();
    return;
  }

  const QString songs = count > 0 ? tr("%n song(s) marked unavailable", nullptr, count) : tr("all songs marked unavailable");
  auto *confirmation = ActionDialog::CreateConfirmation(parent, tr("Remove unavailable songs"), tr("Remove %1 from the collection now? Their play counts and ratings are lost, also when the files return later. This cannot be undone.").arg(songs), tr("Remove songs"));
  QObject::connect(confirmation, &QDialog::accepted, this, [this]() { collection_backend_->DeleteUnavailableSongsAsync(); });
  confirmation->Open();

}
