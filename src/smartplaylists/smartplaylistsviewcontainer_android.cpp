// SPDX-License-Identifier: GPL-3.0-or-later

#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/iconloader.h"
#include "widgets/actiondialog.h"
#include "widgets/gridflowlayout.h"
#include "smartplaylistsviewcontainer.h"
#include "smartplaylistsmodel.h"
#include "smartplaylistsview.h"
#include "smartplaylistwizard.h"
#include "ui_smartplaylistsviewcontainer.h"

using namespace Qt::Literals::StringLiterals;

void SmartPlaylistsViewContainer::SetupAndroidBrowser() {

  auto *browser = new QWidget(this);
  browser->setLayout(layout());
  android_pages_ = new QStackedWidget(this);
  android_pages_->addWidget(browser);
  auto *pages_layout = new QVBoxLayout(this);
  pages_layout->setContentsMargins(0, 0, 0, 0);
  pages_layout->addWidget(android_pages_);
  auto *buttons = new GridFlowLayout(0, 3);
  for (QToolButton *button : {ui_->new_, ui_->edit_, ui_->delete_, ui_->restore_}) {
    ui_->horizontalLayout->removeWidget(button);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setMinimumSize(80, 64);
    button->setAccessibleName(button->toolTip());
    buttons->addWidget(button);
  }
  ui_->edit_->setText(tr("Edit"));
  ui_->delete_->setText(tr("Delete"));
  ui_->restore_->setText(tr("Restore defaults"));
  auto *play = new QToolButton(browser);
  play->setIcon(IconLoader::Load(u"media-playback-start"_s));
  play->setToolTip(tr("Open smart playlist"));
  play->setAccessibleName(play->toolTip());
  play->setText(tr("Open"));
  play->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  play->setMinimumSize(80, 64);
  buttons->addWidget(play);
  delete ui_->horizontalLayout->takeAt(0);
  ui_->horizontalLayout->addLayout(buttons);
  QObject::connect(play, &QToolButton::clicked, this, [this]() {
    if (ui_->view->currentIndex().isValid()) ItemDoubleClicked(ui_->view->currentIndex());
  });
  ui_->view->setStyleSheet(u"QListView::item { min-height: 48px; }"_s);
  ui_->view->setWordWrap(true);
  ui_->view->setDragEnabled(false);

}

void SmartPlaylistsViewContainer::OpenAndroidWizard(SmartPlaylistWizard *wizard) {

  wizard->setWindowFlags(Qt::Widget);
  wizard->setAttribute(Qt::WA_QuitOnClose, false);
  android_pages_->addWidget(wizard);
  wizard->SetupAndroid(android_pages_);
  QObject::connect(wizard, &QDialog::finished, this, [this, wizard]() {
    android_pages_->setCurrentIndex(0);
    android_pages_->removeWidget(wizard);
  });
  android_pages_->setCurrentWidget(wizard);
  wizard->show();

}

void SmartPlaylistsViewContainer::ConfirmAndroidRestoreDefaults() {

  ActionDialog *dialog = ActionDialog::CreateConfirmation(
      this, tr("Restore defaults"),
      tr("Are you sure you want to restore the default smart playlists? This will remove all custom smart playlists"),
      tr("Restore defaults"));
  QObject::connect(dialog, &QDialog::accepted, this, [this]() { model_->RestoreDefaults(); });
  dialog->Open();

}
