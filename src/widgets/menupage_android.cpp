// SPDX-License-Identifier: GPL-3.0-or-later

#include "menupage_android.h"

#include <utility>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QStringList>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

#include "widgets/androidwidgets.h"
#include "widgets/fancytabwidget.h"
#include "widgets/scrollarea_android.h"

namespace {

constexpr int kSectionSpacing = 16;

QString ChoiceValue(const QList<QPointer<QAction>> &choices) {

  for (const QPointer<QAction> &choice : choices) {
    if (choice && choice->isCheckable() && choice->isChecked()) return choice->iconText();
  }
  return QString();

}

}  // namespace

MenuPage::MenuPage(QMenu *menu, QWidget *owner)
    : QWidget(owner),
      menu_(menu),
      owner_(owner),
      title_(new QLabel(this)),
      layout_(new QVBoxLayout(this)) {

  setBackgroundRole(QPalette::Window);
  setAutoFillBackground(true);
  setFocusPolicy(Qt::StrongFocus);
  title_->setTextFormat(Qt::PlainText);
  QPushButton *back = AndroidWidgets::CreateBackButton(this);
  QObject::connect(back, &QPushButton::clicked, this, [this]() { Back(); });
  layout_->addLayout(AndroidWidgets::CreatePageHeader(title_, back));
  hide();

}

MenuPage::~MenuPage() {

  if (host_) host_->removeEventFilter(this);
  for (const QPointer<QWidget> &child : std::as_const(hidden_)) {
    if (child) child->show();
  }

}

QString MenuPage::title() const { return title_->text(); }

void MenuPage::Open() {

  if (depth() > 0 || !menu_ || !owner_) return;

  focus_ = QApplication::focusWidget();
  host_ = owner_->window();
  parent_title_.clear();
  for (QWidget *widget = owner_; widget; widget = widget->parentWidget()) {
    if (FancyTabWidget *tabs = qobject_cast<FancyTabWidget*>(widget); tabs && tabs->currentWidget()) {
      host_ = tabs->currentWidget();
      parent_title_ = tabs->tabText(tabs->currentIndex()).remove(QLatin1Char('&'));
      // Tapping the open tab again returns to its content, like the Playlists and Tools tabs.
      tab_clicked_ = QObject::connect(tabs->tabBar(), &QTabBar::tabBarClicked, this, [this, tabs](const int index) {
        if (index == tabs->currentIndex()) Close();
      });
      break;
    }
  }

  setParent(host_, Qt::Widget);
  host_->installEventFilter(this);
  for (QWidget *child : host_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
    if (child != this && !child->isWindow() && !child->isHidden()) hidden_.append(child);
  }
  for (const QPointer<QWidget> &child : std::as_const(hidden_)) {
    if (child) child->hide();
  }
  setGeometry(host_->rect());
  Push(Level{ menu_->menuAction()->iconText(), menu_, {} });
  show();
  raise();
  setFocus(Qt::OtherFocusReason);

}

void MenuPage::Close() {

  if (depth() == 0) return;

  levels_.clear();
  stale_ = false;
  QObject::disconnect(tab_clicked_);
  if (host_) host_->removeEventFilter(this);
  for (const QPointer<QWidget> &child : std::as_const(hidden_)) {
    if (child) child->show();
  }
  hidden_.clear();
  host_.clear();
  hide();
  if (area_) area_->deleteLater();
  if (owner_) setParent(owner_, Qt::Widget);
  if (focus_ && focus_->isVisible()) focus_->setFocus(Qt::OtherFocusReason);
  focus_.clear();

}

void MenuPage::Back() {

  if (depth() <= 1) {
    Close();
    return;
  }

  levels_.removeLast();
  ShowLevel();

}

void MenuPage::Push(const Level &level) {

  if (!levels_.isEmpty() && area_) levels_.last().scroll = area_->verticalScrollBar()->value();
  levels_.append(level);
  ShowLevel();

}

void MenuPage::ShowLevel() {

  // The clicked button may belong to the old page, so delete it later.
  if (area_) {
    area_->hide();
    area_->deleteLater();
  }

  QStringList titles;
  if (!parent_title_.isEmpty()) titles << parent_title_;
  for (const Level &level : std::as_const(levels_)) titles << level.title;
  const QString last = titles.takeLast();
  // Start deeper titles on their own line, so the breadcrumb wraps predictably on narrow screens.
  title_->setText(titles.isEmpty() ? last : titles.join(QStringLiteral(" > ")) + (titles.size() >= 2 ? QStringLiteral(" >\n") : QStringLiteral(" > ")) + last);

  const Level &level = levels_.last();
  AndroidWidgets::ScrollPage page = AndroidWidgets::CreateScrollPage(this);
  area_ = page.area;
  if (level.menu) AddMenuRows(level.menu, page.layout);
  else AddChoiceRows(level.choices, page.layout);
  page.layout->addStretch();
  AndroidScrollArea::EnableForContents(area_);
  layout_->addWidget(area_, 1);
  area_->show();

  // The scroll range is only known once the new page has been laid out.
  const int scroll = level.scroll;
  QTimer::singleShot(0, area_, [area = area_, scroll]() {
    if (area) area->verticalScrollBar()->setValue(scroll);
  });
  setFocus(Qt::OtherFocusReason);

}

void MenuPage::AddMenuRows(QMenu *menu, QVBoxLayout *layout) {

  QWidget *contents = layout->parentWidget();
  const QList<QAction*> actions = menu->actions();
  for (qsizetype i = 0; i < actions.size(); ++i) {
    QAction *action = actions.at(i);
    if (!action->isVisible()) continue;
    if (action->isSeparator()) {
      if (layout->count() > 0) layout->addSpacing(kSectionSpacing);
      continue;
    }

    if (QMenu *submenu = action->menu()) {
      QList<QPointer<QAction>> entries;
      bool has_choices = false;
      for (QAction *entry : submenu->actions()) {
        if (!entry->isVisible()) continue;
        entries << entry;
        has_choices = has_choices || (entry->isCheckable() && !entry->isSeparator());
      }
      if (has_choices) {
        AddChoiceButton(action->iconText(), entries, action->isEnabled(), layout);
      }
      else {
        auto *button = new AndroidWidgets::ValueButton(action->iconText(), contents);
        button->setEnabled(action->isEnabled());
        QObject::connect(button, &QPushButton::clicked, this, [this, title = action->iconText(), submenu = QPointer<QMenu>(submenu)]() {
          if (submenu) Push(Level{ title, submenu, {} });
        });
        layout->addWidget(button);
      }
      continue;
    }

    QActionGroup *group = action->actionGroup();
    if (action->isCheckable() && group && group->isExclusive()) {
      QList<QPointer<QAction>> entries{ action };
      while (i + 1 < actions.size() && actions.at(i + 1)->actionGroup() == group) {
        ++i;
        if (actions.at(i)->isVisible()) entries << actions.at(i);
      }
      AddChoiceButton(QCoreApplication::translate("MenuPage", "Show"), entries, group->isEnabled(), layout);
      continue;
    }

    if (action->isCheckable()) {
      auto *check = new QCheckBox(action->iconText(), contents);
      AndroidWidgets::AdaptCheckBox(check);
      check->setChecked(action->isChecked());
      check->setEnabled(action->isEnabled());
      QObject::connect(check, &QCheckBox::clicked, action, &QAction::trigger);
      layout->addWidget(check);
      continue;
    }

    auto *button = new AndroidWidgets::ValueButton(action->iconText(), contents);
    button->setEnabled(action->isEnabled());
    QObject::connect(button, &QPushButton::clicked, this, [this, action = QPointer<QAction>(action)]() { TriggerAndClose(action); });
    layout->addWidget(button);
  }

}

void MenuPage::AddChoiceButton(const QString &title, const QList<QPointer<QAction>> &choices, const bool enabled, QVBoxLayout *layout) {

  const AndroidWidgets::LabeledValueButton value = AndroidWidgets::CreateLabeledValueButton(title, layout->parentWidget());
  value.button->SetValue(ChoiceValue(choices));
  value.button->setEnabled(enabled);
  value.label->setEnabled(enabled);
  QObject::connect(value.button, &QPushButton::clicked, this, [this, title, choices]() { Push(Level{ title, nullptr, choices }); });
  layout->addWidget(value.label);
  layout->addWidget(value.button);

}

void MenuPage::AddChoiceRows(const QList<QPointer<QAction>> &choices, QVBoxLayout *layout) {

  QWidget *contents = layout->parentWidget();
  for (const QPointer<QAction> &choice : choices) {
    if (!choice || !choice->isVisible()) continue;
    if (choice->isSeparator()) {
      if (layout->count() > 0) layout->addSpacing(kSectionSpacing);
      continue;
    }
    if (!choice->isCheckable()) {
      auto *button = new AndroidWidgets::ValueButton(choice->iconText(), contents);
      button->setEnabled(choice->isEnabled());
      QObject::connect(button, &QPushButton::clicked, this, [this, choice]() { TriggerAndClose(choice); });
      layout->addWidget(button);
      continue;
    }
    QRadioButton *row = AndroidWidgets::CreateChoiceRow(choice->iconText(), contents);
    row->setChecked(choice->isChecked());
    row->setEnabled(choice->isEnabled());
    QObject::connect(row, &QRadioButton::clicked, this, [this, choice]() {
      if (choice) choice->trigger();
      Back();
    });
    layout->addWidget(row);
  }

}

void MenuPage::TriggerAndClose(QAction *action) {

  const QPointer<QAction> guard(action);
  Close();
  if (guard) guard->trigger();

}

bool MenuPage::eventFilter(QObject *object, QEvent *event) {

  if (object == host_) {
    if (event->type() == QEvent::Resize) setGeometry(host_->rect());
    else if (event->type() == QEvent::Hide) Close();
  }

  return QWidget::eventFilter(object, event);

}

void MenuPage::keyPressEvent(QKeyEvent *event) {

  if (event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape) {
    event->accept();
    Back();
    return;
  }

  QWidget::keyPressEvent(event);

}

void MenuPage::showEvent(QShowEvent *event) {

  QWidget::showEvent(event);
  if (stale_ && depth() > 0) {
    stale_ = false;
    if (area_) levels_.last().scroll = area_->verticalScrollBar()->value();
    ShowLevel();
  }

}

void MenuPage::hideEvent(QHideEvent *event) {

  QWidget::hideEvent(event);
  if (depth() > 0) stale_ = true;

}
