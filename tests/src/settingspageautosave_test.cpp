// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QLineEdit>
#include <QMap>
#include <QMetaObject>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "settings/settingspage.h"
#include "settings/streamingautosave_android.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class AutoSavePage : public SettingsPage {
 public:
  AutoSavePage() : SettingsPage(nullptr) {
    auto *layout = new QVBoxLayout(this);
    checkbox = new QCheckBox(this);
    combo = new QComboBox(this);
    combo->addItems({u"First"_s, u"Second"_s});
    field = new QLineEdit(this);
    navigation = new QPushButton(this);
    remove = new QPushButton(this);
    group = new QGroupBox(this);
    group->setCheckable(true);
    for (QWidget *widget : {static_cast<QWidget*>(checkbox), static_cast<QWidget*>(combo), static_cast<QWidget*>(field), static_cast<QWidget*>(navigation), static_cast<QWidget*>(remove), static_cast<QWidget*>(group)}) {
      layout->addWidget(widget);
    }
    QObject::connect(remove, &QPushButton::clicked, this, [this]() { set_changed(); });
  }
  void Load() override { Init(this); }
  int saves = 0;
  QCheckBox *checkbox;
  QComboBox *combo;
  QLineEdit *field;
  QPushButton *navigation;
  QPushButton *remove;
  QGroupBox *group;

 private:
  void Save() override { ++saves; }
};

// Mirrors the Android SettingsDialog: changes save after a delay, pending changes flush when the dialog is hidden, and pages_ is destroyed before ~QDialog hides a visible dialog.
class AutoSaveDialog : public QDialog {
 public:
  explicit AutoSaveDialog(int *flushes) : flushes_(flushes), page_(new AutoSavePage) {
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(page_);
    pages_.insert(0, page_);
    page_->Load();
    timer_.setSingleShot(true);
    timer_.setInterval(60000);
    QObject::connect(&timer_, &QTimer::timeout, this, [this]() { Flush(); });
    ConnectSettingsPageAutoSave(this, page_, [this]() { timer_.start(); });
    hide_filter_ = InstallFlushOnHide(this, [this]() { Flush(); });
  }
  ~AutoSaveDialog() override { delete hide_filter_; }
  AutoSavePage *page() const { return page_; }
  bool pending() const { return timer_.isActive(); }

 private:
  void Flush() {
    ++*flushes_;
    timer_.stop();
    for (AutoSavePage *page : std::as_const(pages_)) page->Apply();
  }

  int *flushes_;
  AutoSavePage *page_;
  QMap<int, AutoSavePage*> pages_;
  QTimer timer_;
  QObject *hide_filter_;
};

}  // namespace

TEST(SettingsPageAutoSaveTest, ChangesSaveThroughApplyWithoutDialogButtons) {

  AutoSavePage page;
  page.Load();
  ConnectSettingsPageAutoSave(&page, &page, [&page]() { page.Apply(); });

  page.checkbox->click();
  EXPECT_EQ(1, page.saves);
  page.field->setText(u"pattern"_s);
  EXPECT_EQ(1, page.saves);
  ASSERT_TRUE(QMetaObject::invokeMethod(page.field, "editingFinished"));
  EXPECT_EQ(2, page.saves);
  // A navigation button that changes nothing must not save.
  page.navigation->click();
  EXPECT_EQ(2, page.saves);
  EXPECT_FALSE(page.Apply());
  // Page actions such as Add folder only mark the page as changed; they are saved right away.
  page.remove->click();
  EXPECT_EQ(3, page.saves);
  page.group->setChecked(false);
  Q_EMIT page.group->clicked(false);
  EXPECT_EQ(4, page.saves);
  // Toggling back is a change compared with the last save.
  page.checkbox->click();
  EXPECT_EQ(5, page.saves);

}

TEST(SettingsPageAutoSaveTest, EventsThatChangeNothingDoNotSave) {

  AutoSavePage page;
  page.Load();
  ConnectSettingsPageAutoSave(&page, &page, [&page]() { page.Apply(); });

  // Return without edits, or editingFinished again after a flush.
  ASSERT_TRUE(QMetaObject::invokeMethod(page.field, "editingFinished"));
  EXPECT_EQ(0, page.saves);
  // Re-selecting the current combo item.
  Q_EMIT page.combo->activated(page.combo->currentIndex());
  EXPECT_EQ(0, page.saves);
  page.combo->setCurrentIndex(1);
  Q_EMIT page.combo->activated(1);
  EXPECT_EQ(1, page.saves);
  Q_EMIT page.combo->activated(1);
  EXPECT_EQ(1, page.saves);

}

TEST(SettingsDialogAutoSaveTest, ClosingFlushesPendingChanges) {

  int flushes = 0;
  AutoSaveDialog dialog(&flushes);
  dialog.show();
  dialog.page()->field->setText(u"typed"_s);
  dialog.page()->checkbox->click();
  EXPECT_TRUE(dialog.pending());
  EXPECT_EQ(0, dialog.page()->saves);

  dialog.close();
  EXPECT_FALSE(dialog.pending());
  EXPECT_EQ(1, flushes);
  EXPECT_EQ(1, dialog.page()->saves);
  // Nothing is left to save when the dialog is hidden again.
  dialog.show();
  dialog.hide();
  EXPECT_EQ(1, dialog.page()->saves);

}

TEST(SettingsDialogAutoSaveTest, QDialogDestructorHidesAVisibleDialog) {

  // Why the hide filter must be removed before the dialog's members are destroyed.
  int hides = 0;
  auto *dialog = new QDialog;
  InstallFlushOnHide(dialog, [&hides]() { ++hides; });
  dialog->show();
  delete dialog;
  EXPECT_EQ(1, hides);

}

TEST(SettingsDialogAutoSaveTest, DestroyingAVisibleDialogDoesNotFlushDestroyedPages) {

  int flushes = 0;
  auto *dialog = new AutoSaveDialog(&flushes);
  dialog->show();
  dialog->page()->checkbox->click();
  dialog->hide();
  EXPECT_EQ(1, flushes);
  dialog->show();
  delete dialog;
  EXPECT_EQ(1, flushes);

}

TEST(SettingsPageAutoSaveTest, SnapshotIncludesNestedEditorControls) {

  // Like Backend > Audio normalization > Replay Gain: a section inside a section, each moved into its own subpage.
  QWidget page;
  auto *outer = new QWidget(&page);
  auto *outer_check = new QCheckBox(outer);
  auto *inner_parent = new QWidget(outer);
  auto *inner = new QWidget(inner_parent);
  auto *preamp = new QSlider(inner);
  auto *other = new QWidget;
  auto *unrelated = new QSlider(other);

  QScrollArea outer_area;
  outer_area.setWidget(outer);
  QScrollArea inner_area;
  inner_area.setWidget(inner);
  QScrollArea other_area;
  other_area.setWidget(other);

  const QList<QWidget*> widgets = SubpageWidgetsOf(&page, {qMakePair(outer, &page), qMakePair(inner, inner_parent), qMakePair(other, static_cast<QWidget*>(&other_area))});
  EXPECT_TRUE(widgets.contains(outer_check));
  EXPECT_TRUE(widgets.contains(preamp));
  EXPECT_FALSE(widgets.contains(unrelated));

  // A nested editor of another page is not included either.
  QWidget other_page;
  EXPECT_TRUE(SubpageWidgetsOf(&other_page, {qMakePair(outer, &page), qMakePair(inner, inner_parent)}).isEmpty());

}
