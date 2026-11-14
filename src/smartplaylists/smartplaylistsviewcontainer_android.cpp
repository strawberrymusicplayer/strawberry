// SPDX-License-Identifier: GPL-3.0-or-later

#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/iconloader.h"
#include "widgets/gridflowlayout.h"
#include "smartplaylistsviewcontainer.h"
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
  auto *buttons = new GridFlowLayout;
  for (QToolButton *button : {ui_->new_, ui_->edit_, ui_->delete_, ui_->restore_}) {
    ui_->horizontalLayout->removeWidget(button);
    button->setMinimumSize(48, 48);
    button->setAccessibleName(button->toolTip());
    buttons->addWidget(button);
  }
  auto *play = new QToolButton(browser);
  play->setIcon(IconLoader::Load(u"media-playback-start"_s));
  play->setToolTip(tr("Open smart playlist"));
  play->setAccessibleName(play->toolTip());
  play->setMinimumSize(48, 48);
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
