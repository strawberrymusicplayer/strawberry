// SPDX-License-Identifier: GPL-3.0-or-later

#include <QScreen>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/logging.h"
#include "widgets/actiondialog.h"
#include "widgets/gridflowlayout.h"
#include "smartplaylistpreview_android.h"
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
  auto *buttons = new GridFlowLayout(0, 5);
  for (QToolButton *button : {ui_->new_, ui_->edit_, ui_->delete_, ui_->restore_}) {
    ui_->horizontalLayout->removeWidget(button);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setAutoRaise(true);
    button->setMinimumSize(48, 64);
    button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    button->setAccessibleName(button->toolTip());
    buttons->addWidget(button);
  }
  ui_->edit_->setText(tr("Edit"));
  ui_->delete_->setText(tr("Delete"));
  ui_->restore_->setText(tr("Restore defaults"));
  if (QScreen *current_screen = screen()) {
    QObject::connect(current_screen, &QScreen::geometryChanged, this, [this]() { UpdateAndroidToolbarOrientation(); });
  }
  UpdateAndroidToolbarOrientation();
  delete ui_->horizontalLayout->takeAt(0);
  ui_->horizontalLayout->addLayout(buttons);
  ui_->edit_->hide();
  ui_->delete_->hide();
  QObject::connect(ui_->view, &QAbstractItemView::clicked, this, &SmartPlaylistsViewContainer::OpenAndroidPreview);
  ui_->view->setStyleSheet(u"QListView::item { min-height: 48px; }"_s);
  ui_->view->setWordWrap(true);
  ui_->view->setDragEnabled(false);

}

void SmartPlaylistsViewContainer::OpenAndroidPreview(const QModelIndex &index) {

  if (!index.isValid()) return;
  PlaylistGeneratorPtr generator = model_->CreateGenerator(index);
  if (!generator) {
    qLog(Error) << "Cannot open smart playlist preview: invalid generator";
    return;
  }
  AndroidSmartPlaylistPreview::Open(playlist_manager_.get(), generator);
  Q_EMIT AndroidPreviewOpened();

}

void SmartPlaylistsViewContainer::UpdateAndroidToolbarOrientation() {

  const QScreen *current_screen = screen();
  if (!current_screen) return;
  const QRect screen_geometry = current_screen->geometry();
  const Qt::ToolButtonStyle style = screen_geometry.width() > screen_geometry.height()
                                        ? Qt::ToolButtonTextUnderIcon
                                        : Qt::ToolButtonIconOnly;
  for (QToolButton *button : {ui_->new_, ui_->edit_, ui_->delete_, ui_->restore_}) {
    button->setToolButtonStyle(style);
  }

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
