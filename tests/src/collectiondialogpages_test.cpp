// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>

#include "collection/collectiondialogpages_android.h"
#include "collection/collectionmodel.h"
#include "collection/groupbydialog.h"
#include "covermanager/coverdialogpage_android.h"
#include "widgets/fancytabwidget.h"

namespace {

class CollectionDialogPagesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    window_.resize(360, 640);
    auto *layout = new QVBoxLayout(&window_);
    tabs_ = new FancyTabWidget(&window_);
    layout->addWidget(tabs_);
    content_ = new QWidget;
    auto *content_layout = new QVBoxLayout(content_);
    owner_ = new QPushButton(QStringLiteral("Options"), content_);
    content_layout->addWidget(owner_);
    tabs_->AddTab(content_, QStringLiteral("Collection"), QIcon(), QStringLiteral("Collection"));
    ASSERT_TRUE(tabs_->EnableTab(content_));
    tabs_->SetMode(FancyTabWidget::Mode::LargeSidebar);
    tabs_->setCurrentIndex(0);
    window_.show();
    QCoreApplication::processEvents();
  }

  QPushButton *ValueButton(QWidget *parent, const QString &title) {
    for (QPushButton *button : parent->findChildren<QPushButton*>()) {
      if (button->accessibleName() == title) return button;
    }
    return nullptr;
  }

  QString ButtonValue(QPushButton *button) {
    for (QLabel *label : button->findChildren<QLabel*>()) {
      if (!label->isHidden()) return label->text();
    }
    return QString();
  }

  QDialog *VisibleChoices() {
    for (QDialog *dialog : window_.findChildren<QDialog*>()) {
      if (dialog->isVisible() && !qobject_cast<GroupByDialog*>(dialog)) return dialog;
    }
    return nullptr;
  }

  QRadioButton *ChoiceRow(QWidget *page, const QString &text) {
    for (QRadioButton *row : page->findChildren<QRadioButton*>()) {
      if (row->accessibleName() == text) return row;
    }
    return nullptr;
  }

  QWidget window_;
  FancyTabWidget *tabs_ = nullptr;
  QWidget *content_ = nullptr;
  QPushButton *owner_ = nullptr;
};

TEST_F(CollectionDialogPagesTest, GroupingDialogUsesValueButtonsAndChoicePages) {
  GroupByDialog dialog(owner_);
  QSignalSpy accepted(&dialog, &GroupByDialog::Accepted);
  CollectionDialogPages::Show(&dialog);
  QCoreApplication::processEvents();

  EXPECT_TRUE(dialog.isVisible());
  EXPECT_FALSE(content_->isVisible());
  EXPECT_EQ(dialog.geometry(), dialog.parentWidget()->rect());
  for (QComboBox *combo : dialog.findChildren<QComboBox*>()) EXPECT_FALSE(combo->isVisible());

  QComboBox *first = dialog.findChild<QComboBox*>(QStringLiteral("combobox_first"));
  ASSERT_NE(first, nullptr);
  QPushButton *first_button = ValueButton(&dialog, QStringLiteral("First level"));
  ASSERT_NE(first_button, nullptr);
  EXPECT_TRUE(first_button->isVisible());
  EXPECT_GE(first_button->height(), 48);
  EXPECT_EQ(ButtonValue(first_button), first->currentText());

  QTest::mouseClick(first_button, Qt::LeftButton);
  QCoreApplication::processEvents();
  QDialog *choices = VisibleChoices();
  ASSERT_NE(choices, nullptr);
  EXPECT_FALSE(dialog.isVisible());
  bool titled = false;
  for (QLabel *label : choices->findChildren<QLabel*>()) {
    titled = titled || label->text() == QStringLiteral("Collection >\nCollection advanced grouping > First level");
  }
  EXPECT_TRUE(titled);

  const int artist_index = first->findText(QStringLiteral("Artist"));
  ASSERT_GE(artist_index, 0);
  QRadioButton *artist = ChoiceRow(choices, QStringLiteral("Artist"));
  ASSERT_NE(artist, nullptr);
  QTest::mouseClick(artist, Qt::LeftButton);
  QCoreApplication::processEvents();
  QCoreApplication::processEvents();
  EXPECT_EQ(VisibleChoices(), nullptr);
  EXPECT_TRUE(dialog.isVisible());
  EXPECT_EQ(first->currentIndex(), artist_index);
  EXPECT_EQ(ButtonValue(first_button), QStringLiteral("Artist"));

  dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
  QCoreApplication::processEvents();
  ASSERT_EQ(accepted.count(), 1);
  EXPECT_EQ(accepted.at(0).at(0).value<CollectionModel::Grouping>().first, CollectionModel::GroupBy::Artist);
  EXPECT_FALSE(dialog.isVisible());
  EXPECT_TRUE(content_->isVisible());
}

TEST_F(CollectionDialogPagesTest, BackOnChoicePageKeepsValue) {
  GroupByDialog dialog(owner_);
  CollectionDialogPages::Show(&dialog);
  QCoreApplication::processEvents();
  QComboBox *second = dialog.findChild<QComboBox*>(QStringLiteral("combobox_second"));
  const int index = second->currentIndex();

  QTest::mouseClick(ValueButton(&dialog, QStringLiteral("Second level")), Qt::LeftButton);
  QCoreApplication::processEvents();
  QDialog *choices = VisibleChoices();
  ASSERT_NE(choices, nullptr);
  QKeyEvent back(QEvent::KeyPress, Qt::Key_Back, Qt::NoModifier);
  QCoreApplication::sendEvent(choices, &back);
  QCoreApplication::processEvents();
  QCoreApplication::processEvents();
  EXPECT_EQ(VisibleChoices(), nullptr);
  EXPECT_TRUE(dialog.isVisible());
  EXPECT_EQ(second->currentIndex(), index);

  dialog.reject();
  QCoreApplication::processEvents();
  EXPECT_TRUE(content_->isVisible());

  CollectionDialogPages::Show(&dialog);
  QCoreApplication::processEvents();
  EXPECT_TRUE(dialog.isVisible());
  EXPECT_FALSE(content_->isVisible());
  int buttons = 0;
  for (QPushButton *button : dialog.findChildren<QPushButton*>()) {
    if (button->accessibleName() == QStringLiteral("Second level")) ++buttons;
  }
  EXPECT_EQ(buttons, 1);
  dialog.reject();
  QCoreApplication::processEvents();
  EXPECT_TRUE(content_->isVisible());
}

TEST_F(CollectionDialogPagesTest, GetTextReturnsEnteredTextOrEmptyWhenCancelled) {
  QTimer::singleShot(0, &window_, [this]() {
    QLineEdit *edit = nullptr;
    for (QLineEdit *candidate : window_.findChildren<QLineEdit*>()) {
      if (candidate->isVisible()) edit = candidate;
    }
    ASSERT_NE(edit, nullptr);
    EXPECT_FALSE(content_->isVisible());
    for (QLabel *label : window_.findChildren<QLabel*>()) {
      if (label->isVisible() && label->text() == QStringLiteral("Grouping name:")) {
        EXPECT_LT(edit->mapTo(&window_, QPoint()).y() - label->mapTo(&window_, QPoint()).y(), 100);
      }
    }
    edit->setText(QStringLiteral("My grouping"));
    for (QDialogButtonBox *buttons : window_.findChildren<QDialogButtonBox*>()) {
      if (buttons->isVisible()) buttons->button(QDialogButtonBox::Ok)->click();
    }
  });
  EXPECT_EQ(CoverDialogPage::GetText(owner_, QStringLiteral("Grouping Name"), QStringLiteral("Grouping name:")), QStringLiteral("My grouping"));
  EXPECT_TRUE(content_->isVisible());

  QTimer::singleShot(0, &window_, [this]() {
    for (QDialog *dialog : window_.findChildren<QDialog*>()) {
      if (dialog->isVisible()) {
        QKeyEvent back(QEvent::KeyPress, Qt::Key_Back, Qt::NoModifier);
        QCoreApplication::sendEvent(dialog, &back);
      }
    }
  });
  EXPECT_TRUE(CoverDialogPage::GetText(owner_, QStringLiteral("Grouping Name"), QStringLiteral("Grouping name:")).isEmpty());
  EXPECT_TRUE(content_->isVisible());
}

}  // namespace
