// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <memory>

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QImage>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>

#include "core/networkaccessmanager.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoversearcher.h"
#include "covermanager/coverdialogpage_android.h"
#include "covermanager/coverfromurldialog.h"
#include "tagreader/tagreaderclient.h"
#include "widgets/actiondialog.h"
#include "widgets/fancytabwidget.h"

namespace {

class CoverDialogPageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    window_.resize(360, 640);
    auto *layout = new QVBoxLayout(&window_);
    tabs_ = new FancyTabWidget(&window_);
    layout->addWidget(tabs_);
    content_ = new QWidget;
    auto *content_layout = new QVBoxLayout(content_);
    auto *button = new QPushButton(QStringLiteral("Parent selection"), content_);
    content_layout->addWidget(button);
    tabs_->AddTab(content_, QStringLiteral("Context"), QIcon(), QStringLiteral("Context"));
    ASSERT_TRUE(tabs_->EnableTab(content_));
    auto *collection = new QWidget;
    tabs_->AddTab(collection, QStringLiteral("Collection"), QIcon(), QStringLiteral("Collection"));
    ASSERT_TRUE(tabs_->EnableTab(collection));
    tabs_->SetMode(FancyTabWidget::Mode::LargeSidebar);
    tabs_->setCurrentIndex(0);
    window_.show();
    QCoreApplication::processEvents();
  }

  void ExpectButtonsFit(QDialog *dialog) {
    EXPECT_EQ(dialog->geometry(), dialog->parentWidget()->rect());
    EXPECT_TRUE(tabs_->currentWidget()->rect().contains(QRect(dialog->mapTo(tabs_->currentWidget(), QPoint()), dialog->size())));
    for (QPushButton *button : dialog->findChildren<QPushButton*>()) {
      if (!button->isVisible()) continue;
      EXPECT_GE(button->height(), 48);
      if (button->parentWidget() != dialog && !qobject_cast<QDialogButtonBox*>(button->parentWidget())) continue;
      EXPECT_TRUE(dialog->rect().contains(QRect(button->mapTo(dialog, QPoint()), button->size()))) << button->text().toStdString();
    }
  }

  QWidget window_;
  FancyTabWidget *tabs_ = nullptr;
  QWidget *content_ = nullptr;
};

TEST_F(CoverDialogPageTest, UrlPageFitsPortraitAndLandscapeAndRestoresParent) {
  QWidget controller(&window_);
  CoverFromURLDialog dialog(std::make_shared<NetworkAccessManager>(), &controller);
  CoverDialogPage page(&dialog);
  page.Open();
  QCoreApplication::processEvents();
  EXPECT_FALSE(content_->isVisible());
  ExpectButtonsFit(&dialog);
  window_.resize(640, 360);
  QCoreApplication::processEvents();
  ExpectButtonsFit(&dialog);
  page.ShowError(QStringLiteral("Not an image"));
  EXPECT_TRUE(dialog.findChild<QLabel*>(QStringLiteral("cover_error"))->isVisible());
  QLineEdit *url = dialog.findChild<QLineEdit*>(QStringLiteral("url"));
  ASSERT_NE(url, nullptr);
  url->setFocus();
  QTest::keyClick(url, Qt::Key_Back);
  QCoreApplication::processEvents();
  EXPECT_FALSE(dialog.isVisible());
  EXPECT_TRUE(content_->isVisible());
  page.Open();
  EXPECT_FALSE(dialog.findChild<QLabel*>(QStringLiteral("cover_error"))->isVisible());
  dialog.reject();
}

TEST_F(CoverDialogPageTest, ExecKeepsAcceptedAndRejectedResults) {
  QWidget controller(&window_);
  QDialog dialog(&controller);
  new QVBoxLayout(&dialog);
  CoverDialogPage page(&dialog);
  QTimer::singleShot(0, &dialog, &QDialog::accept);
  EXPECT_EQ(page.Exec(), QDialog::Accepted);
  EXPECT_TRUE(content_->isVisible());
  QTimer::singleShot(0, &dialog, &QDialog::reject);
  EXPECT_EQ(page.Exec(), QDialog::Rejected);
  EXPECT_TRUE(content_->isVisible());
}

TEST_F(CoverDialogPageTest, BackReturnsToCoverActionsThenContext) {
  auto *actions = new ActionDialog(content_, QStringLiteral("Cover"), QStringLiteral("Context > Album cover"), ActionDialog::TitlePosition::Top);
  QPushButton *action = actions->AddButton(QStringLiteral("Search"));
  actions->AddCancelButton(QStringLiteral("Back"));
  actions->Open();
  QWidget controller(&window_);
  QDialog editor(&controller);
  editor.setWindowTitle(QStringLiteral("Search for album covers"));
  new QVBoxLayout(&editor);
  CoverDialogPage page(&editor);
  page.Open();
  QCoreApplication::processEvents();
  EXPECT_EQ(editor.parentWidget(), actions);
  EXPECT_FALSE(action->isVisible());
  QTest::keyClick(&editor, Qt::Key_Back);
  QCoreApplication::processEvents();
  EXPECT_TRUE(actions->isVisible());
  EXPECT_TRUE(action->isVisible());
  EXPECT_EQ(editor.parentWidget(), &controller);
  EXPECT_FALSE(editor.isWindow());
  QTest::keyClick(actions, Qt::Key_Back);
  QCoreApplication::processEvents();
  EXPECT_TRUE(content_->isVisible());
}

TEST_F(CoverDialogPageTest, SwitchingTabsFromNestedEditorRetainsReusableDialog) {
  auto *actions = new ActionDialog(content_, QStringLiteral("Cover"), QStringLiteral("Context > Album cover"), ActionDialog::TitlePosition::Top);
  actions->AddCancelButton(QStringLiteral("Back"));
  actions->Open();
  QWidget controller(&window_);
  QDialog editor(&controller);
  new QVBoxLayout(&editor);
  CoverDialogPage page(&editor);
  page.Open();
  tabs_->setCurrentIndex(1);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  EXPECT_FALSE(editor.isVisible());
  EXPECT_EQ(editor.parentWidget(), &controller);
  tabs_->setCurrentIndex(0);
  page.Open();
  EXPECT_TRUE(editor.isVisible());
  editor.reject();
}

TEST_F(CoverDialogPageTest, ChangingTabRejectsEditorAndPreservesParent) {
  QWidget controller(&window_);
  QDialog dialog(&controller);
  new QVBoxLayout(&dialog);
  CoverDialogPage page(&dialog);
  QSignalSpy finished(&dialog, &QDialog::finished);
  page.Open();
  tabs_->setCurrentIndex(1);
  QCoreApplication::processEvents();
  ASSERT_EQ(finished.count(), 1);
  EXPECT_EQ(finished.first().first().toInt(), QDialog::Rejected);
  tabs_->setCurrentIndex(0);
  EXPECT_TRUE(content_->isVisible());
}

TEST_F(CoverDialogPageTest, SearchFieldsAndButtonsFitNarrowPage) {
  QWidget controller(&window_);
  auto loader = std::make_shared<AlbumCoverLoader>(std::make_shared<TagReaderClient>());
  AlbumCoverSearcher dialog(QIcon(), loader, &controller);
  CoverDialogPage page(&dialog);
  page.Open();
  QCoreApplication::processEvents();
  ExpectButtonsFit(&dialog);
  QWidget *artist = dialog.findChild<QWidget*>(QStringLiteral("artist"));
  QWidget *album = dialog.findChild<QWidget*>(QStringLiteral("album"));
  ASSERT_NE(artist, nullptr);
  ASSERT_NE(album, nullptr);
  EXPECT_LT(artist->mapTo(&dialog, QPoint()).y(), album->mapTo(&dialog, QPoint()).y());
  window_.resize(640, 360);
  QCoreApplication::processEvents();
  ExpectButtonsFit(&dialog);
  dialog.reject();
}

TEST_F(CoverDialogPageTest, FullCoverStaysAboveParentAndBackClosesIt) {
  QWidget controller(&window_);
  QPixmap pixmap(2000, 1000);
  pixmap.fill(Qt::red);
  CoverDialogPage::ShowCover(&controller, QStringLiteral("Album cover"), pixmap);
  QCoreApplication::processEvents();
  QPointer<QDialog> dialog = tabs_->currentWidget()->findChild<QDialog*>();
  ASSERT_TRUE(dialog);
  ExpectButtonsFit(dialog);
  EXPECT_FALSE(content_->isVisible());
  QWidget *image = dialog->findChild<QWidget*>(QStringLiteral("full_cover_image"));
  ASSERT_NE(image, nullptr);
  const QImage rendered = image->grab().toImage();
  EXPECT_EQ(rendered.pixelColor(rendered.width() / 2, rendered.height() / 2), QColor(Qt::red));
  EXPECT_NE(rendered.pixelColor(rendered.width() / 2, 0), QColor(Qt::red));
  window_.update();
  QCoreApplication::processEvents();
  EXPECT_TRUE(dialog->isVisible());
  window_.resize(640, 360);
  QCoreApplication::processEvents();
  ExpectButtonsFit(dialog);
  QTest::keyClick(dialog, Qt::Key_Back);
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  EXPECT_TRUE(dialog.isNull());
  EXPECT_TRUE(content_->isVisible());
}

TEST_F(CoverDialogPageTest, NestedSearchFitsShortLandscapePage) {
  auto *actions = new ActionDialog(content_, QStringLiteral("Cover"), QStringLiteral("Context > Album cover"), ActionDialog::TitlePosition::Top);
  for (int i = 0; i < 7; ++i) actions->AddButton(QStringLiteral("Cover action"));
  actions->Open();
  QWidget controller(&window_);
  auto loader = std::make_shared<AlbumCoverLoader>(std::make_shared<TagReaderClient>());
  AlbumCoverSearcher dialog(QIcon(), loader, &controller);
  CoverDialogPage page(&dialog);
  page.Open();
  QCoreApplication::processEvents();
  window_.resize(640, 230);
  QCoreApplication::processEvents();
  ExpectButtonsFit(&dialog);
  dialog.reject();
}

}  // namespace
