/*
 * Strawberry Music Player
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include <QtGlobal>

#ifdef Q_OS_ANDROID

#include <algorithm>
#include <functional>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QAction>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QMenu>
#include <QPalette>
#include <QPointer>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTouchEvent>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "androidsettingspage.h"
#include "settingsdialog.h"
#include "settingspage.h"

using namespace Qt::Literals::StringLiterals;

namespace {

QLabel *AddLabel(QBoxLayout *layout, const QString &text, bool value = false) {
  auto *label = new QLabel(text, layout->parentWidget());
  label->setWordWrap(true);
  label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  label->setTextFormat(Qt::PlainText);
  label->setAttribute(Qt::WA_TransparentForMouseEvents);
  QFont font = label->font();
  font.setBold(value);
  label->setFont(font);
  if (value) label->setForegroundRole(QPalette::Link);
  layout->addWidget(label);
  return label;
}

QPushButton *NavigationButton(QWidget *parent, const QString &title, QLabel **summary = nullptr) {
  auto *button = new AndroidSettingsButton<QPushButton>(parent);
  button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  button->setAccessibleName(title);
  button->setProperty("androidSettingsNavigation", true);
  auto *layout = new QVBoxLayout(button);
  AddLabel(layout, title + u" >"_s);
  if (summary) *summary = AddLabel(layout, QString(), true);
  return button;
}

bool FindControlLabel(QLayout *layout, QWidget *control, QString &title) {
  for (int i = 0; i < layout->count(); ++i) {
    QLayoutItem *item = layout->itemAt(i);
    if (item->widget() == control) return true;
    if (auto *label = qobject_cast<QLabel*>(item->widget())) {
      if (!label->text().isEmpty()) title = label->text();
    }
    if (item->layout() && FindControlLabel(item->layout(), control, title)) return true;
  }
  return false;
}

QString ControlTitle(QWidget *control) {
  if (!control->accessibleName().isEmpty()) return control->accessibleName();
  const auto labels = control->parentWidget()->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
  for (QLabel *label : labels) {
    if (label->buddy() == control) return label->text();
  }
  if (QLayout *layout = control->parentWidget()->layout()) {
    QString title;
    if (FindControlLabel(layout, control, title) && !title.isEmpty()) return title;
  }
  if (!control->parentWidget()->accessibleName().isEmpty()) return control->parentWidget()->accessibleName();
  return SettingsDialog::tr("Options");
}

class ChoicePresentation : public QObject {
 public:
  ChoicePresentation(QAbstractButton *button, bool radio) : QObject(button), button_(button), radio_(radio) {
    auto *layout = new QVBoxLayout(button);
    layout->setContentsMargins(radio ? 8 : 28, 8, 8, 8);
    label_ = AddLabel(layout, QString(button->text()).remove(u'&'));
    button->setMinimumWidth(0);
    button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    button->setMinimumHeight(48);
    if (radio) {
      button->setAutoFillBackground(true);
      button->setStyleSheet(u"QRadioButton::indicator { image: none; border: none; background: transparent; width: 0px; height: 0px; }"_s);
      QObject::connect(button, &QAbstractButton::toggled, this, [this]() { Update(); });
    }
    button->installEventFilter(this);
    Update();
  }

 protected:
  bool eventFilter(QObject *, QEvent *event) override {
    if (event->type() == QEvent::Resize || event->type() == QEvent::FontChange || event->type() == QEvent::LayoutRequest) {
      button_->setMinimumHeight(qMax(48, button_->layout()->totalHeightForWidth(button_->width())));
    }
    else if (event->type() == QEvent::Paint) {
      const QString text = QString(button_->text()).remove(u'&');
      if (label_->text() != text) label_->setText(text);
      QStyleOptionButton option;
      option.initFrom(button_);
      option.state |= button_->isChecked() ? QStyle::State_On : QStyle::State_Off;
      if (auto *check = qobject_cast<QCheckBox*>(button_)) {
        if (check->checkState() == Qt::PartiallyChecked) option.state |= QStyle::State_NoChange;
      }
      QStylePainter painter(button_);
      painter.drawControl(radio_ ? QStyle::CE_RadioButton : QStyle::CE_CheckBox, option);
      return true;
    }
    else if (event->type() == QEvent::MouseButtonPress) {
      auto *mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton) {
        button_->setDown(true);
        return true;
      }
    }
    else if (event->type() == QEvent::MouseButtonRelease) {
      auto *mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton) {
        const bool clicked = button_->isDown() && button_->rect().contains(mouse->position().toPoint());
        button_->setDown(false);
        if (clicked) button_->click();
        return true;
      }
    }
    return false;
  }

 private:
  void Update() {
    if (!radio_) return;
    const bool checked = button_->isChecked();
    button_->setBackgroundRole(checked ? QPalette::Highlight : QPalette::Window);
    label_->setForegroundRole(checked ? QPalette::HighlightedText : QPalette::WindowText);
    QFont font = label_->font();
    font.setBold(checked);
    label_->setFont(font);
  }

  QAbstractButton *button_;
  QLabel *label_;
  bool radio_;
};

class ComboPresentation : public QObject {
 public:
  ComboPresentation(QComboBox *combo, SettingsDialog *dialog) : QObject(combo), combo_(combo), dialog_(dialog) {
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(8);
    combo->setMinimumWidth(0);
    combo->setMaximumWidth(QWIDGETSIZE_MAX);
    combo->setMinimumHeight(48);
    combo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    combo->setAttribute(Qt::WA_AcceptTouchEvents, !combo->isEditable());
    QFont font = combo->font();
    font.setBold(true);
    combo->setFont(font);
    combo->installEventFilter(this);
  }

 protected:
  bool eventFilter(QObject *, QEvent *event) override {
    if (event->type() == QEvent::TouchBegin) {
      const auto *touch = static_cast<QTouchEvent*>(event);
      touch_pending_ = touch->points().size() == 1;
      if (touch_pending_) touch_start_ = touch->points().first().globalPosition();
      event->accept();
      return true;
    }
    if (event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd) {
      const auto *touch = static_cast<QTouchEvent*>(event);
      if (touch->points().size() != 1 ||
          (touch->points().first().globalPosition() - touch_start_).manhattanLength() > QApplication::startDragDistance()) {
        touch_pending_ = false;
      }
      const bool open = event->type() == QEvent::TouchEnd && touch_pending_ &&
                        combo_->rect().contains(touch->points().first().position().toPoint());
      if (event->type() == QEvent::TouchEnd) touch_pending_ = false;
      event->accept();
      if (open) Open();
      return true;
    }
    if (event->type() == QEvent::TouchCancel) {
      touch_pending_ = false;
      event->accept();
      return true;
    }
    if (event->type() == QEvent::MouseButtonPress) {
      if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        Open();
        return true;
      }
    }
    if (event->type() == QEvent::MouseButtonRelease) return true;
    if (event->type() == QEvent::KeyPress) {
      const int key = static_cast<QKeyEvent*>(event)->key();
      if (key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Down || key == Qt::Key_F4) {
        Open();
        return true;
      }
    }
    if (event->type() == QEvent::Wheel) return true;
    return false;
  }

 private:
  void Open() {
    auto *content = new QWidget(combo_);
    content->setProperty("androidTransientSubpage", true);
    auto *layout = new QVBoxLayout(content);
    auto *choices = new QListView(content);
    QFont font = combo_->font();
    font.setBold(false);
    choices->setFont(font);
    choices->setModel(combo_->model());
    choices->setModelColumn(combo_->modelColumn());
    choices->setRootIndex(combo_->rootModelIndex());
    choices->setWordWrap(true);
    choices->setResizeMode(QListView::Adjust);
    choices->setSpacing(4);
    choices->setEditTriggers(QAbstractItemView::NoEditTriggers);
    choices->setSelectionMode(QAbstractItemView::SingleSelection);
    choices->setItemDelegate(new ChoiceDelegate(choices));
    choices->setCurrentIndex(combo_->model()->index(combo_->currentIndex(), combo_->modelColumn(), combo_->rootModelIndex()));
    layout->addWidget(choices);
    if (combo_->count() == 0) AddLabel(layout, SettingsDialog::tr("No options available"));
    QObject::connect(choices, &QListView::clicked, this, [this](const QModelIndex &index) {
      if (!index.flags().testFlag(Qt::ItemIsEnabled) || !index.flags().testFlag(Qt::ItemIsSelectable)) return;
      combo_->setCurrentIndex(index.row());
      // Some pages commit selection in activated(), not currentIndexChanged().
      Q_EMIT combo_->activated(index.row());
      Q_EMIT combo_->textActivated(combo_->currentText());
      dialog_->CloseAndroidSubpage();
    });
    dialog_->OpenAndroidSubpage(content, ControlTitle(combo_));
    QTimer::singleShot(0, choices, [choices]() { choices->scrollTo(choices->currentIndex()); });
  }

  class ChoiceDelegate : public QStyledItemDelegate {
   public:
    explicit ChoiceDelegate(QListView *view) : QStyledItemDelegate(view), view_(view) {}

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
      const int width = qMax(1, view_->viewport()->width() - 24);
      const QRect text = option.fontMetrics.boundingRect(QRect(0, 0, width, 100000), Qt::TextWordWrap, index.data().toString());
      return QSize(width, qMax(48, text.height() + 16));
    }

   private:
    QListView *view_;
  };

  QComboBox *combo_;
  SettingsDialog *dialog_;
  QPointF touch_start_;
  bool touch_pending_ = false;
};

class MenuPresentation : public QObject {
 public:
  MenuPresentation(QToolButton *button, SettingsDialog *dialog) : QObject(button), button_(button), dialog_(dialog) {
    button->setMinimumHeight(48);
    button->setMinimumWidth(0);
    button->setMaximumWidth(QWIDGETSIZE_MAX);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QMenu *menu = button->menu();
    button->setMenu(nullptr);
    button->setPopupMode(QToolButton::DelayedPopup);
    QObject::connect(button, &QToolButton::clicked, this, [this, menu]() { Open(menu, 1); });
  }

 private:
  void Open(QMenu *menu, int levels) {
    Q_EMIT menu->aboutToShow();
    auto *content = new QWidget(button_);
    content->setProperty("androidTransientSubpage", true);
    auto *layout = new QVBoxLayout(content);
    layout->setAlignment(Qt::AlignTop);
    for (QAction *action : menu->actions()) {
      if (!action->isVisible() || action->isSeparator()) continue;
      auto *choice = new AndroidSettingsButton<QPushButton>(content);
      choice->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      choice->setEnabled(action->isEnabled());
      choice->setCheckable(action->isCheckable());
      choice->setChecked(action->isChecked());
      auto *row = new QVBoxLayout(choice);
      AddLabel(row, QString(action->text()).remove(u'&') + (action->menu() ? u" >"_s : QString()));
      choice->setAccessibleName(action->text());
      choice->setAccessibleDescription(action->toolTip());
      if (action->isChecked()) {
        choice->setAutoFillBackground(true);
        QPalette palette = choice->palette();
        palette.setColor(QPalette::Button, palette.color(QPalette::Highlight));
        palette.setColor(QPalette::WindowText, palette.color(QPalette::HighlightedText));
        choice->setPalette(palette);
      }
      layout->addWidget(choice);
      QObject::connect(choice, &QPushButton::clicked, this, [this, action, levels]() {
        if (action->menu()) {
          Open(action->menu(), levels + 1);
          return;
        }
        for (int level = 0; level < levels; ++level) dialog_->CloseAndroidSubpage();
        const bool button_action = button_->actions().contains(action);
        action->trigger();
        // QToolButton normally forwards menu actions only while its native popup is open.
        if (!button_action) Q_EMIT button_->triggered(action);
      });
    }
    if (layout->isEmpty()) AddLabel(layout, SettingsDialog::tr("No options available"));
    QString title = menu->title();
    if (title.isEmpty()) title = button_->toolTip();
    if (title.isEmpty()) title = ControlTitle(button_);
    dialog_->OpenAndroidSubpage(content, title);
  }

  QToolButton *button_;
  SettingsDialog *dialog_;
};

void AdaptLayout(QLayout *layout, SettingsPage *page);

void Summarize(const QList<QPointer<QWidget>> &controls, QLabel *summary, QPushButton *button) {
  QStringList values;
  QHash<QString, int> rows;
  const auto add_value = [&values, &rows](QWidget *control, const QString &value) {
    const QString title = ControlTitle(control);
    if (title == SettingsDialog::tr("Options")) values << value;
    else if (rows.contains(title)) values[rows.value(title)] += u' ' + value;
    else {
      rows.insert(title, values.size());
      values << title + u": "_s + value;
    }
  };
  int enabled = 0;
  int total = 0;
  auto ordered = controls;
  std::stable_sort(ordered.begin(), ordered.end(), [](const QPointer<QWidget> &left, const QPointer<QWidget> &right) {
    return qobject_cast<QAbstractSpinBox*>(left.data()) && !qobject_cast<QAbstractSpinBox*>(right.data());
  });
  for (const QPointer<QWidget> &control : ordered) {
    if (!control || control->isHidden()) continue;
    if (auto *combo = qobject_cast<QComboBox*>(control.data())) {
      if (!combo->isEditable() && combo->isEnabled() && !combo->currentText().isEmpty()) add_value(combo, combo->currentText());
    }
    else if (auto *radio = qobject_cast<QRadioButton*>(control.data())) {
      if (radio->isChecked()) values << QString(radio->text()).remove(u'&');
    }
    else if (auto *spin = qobject_cast<QSpinBox*>(control.data())) {
      if (spin->isEnabled()) add_value(spin, spin->text());
    }
    else if (auto *double_spin = qobject_cast<QDoubleSpinBox*>(control.data())) {
      if (double_spin->isEnabled()) add_value(double_spin, double_spin->text());
    }
    else if (auto *check = qobject_cast<QCheckBox*>(control.data())) {
      ++total;
      if (check->isChecked()) ++enabled;
    }
  }
  if (total) values << SettingsDialog::tr("%1 of %2 enabled").arg(enabled).arg(total);
  QStringList visible_values = values.mid(0, 3);
  if (values.size() > 3) visible_values << SettingsDialog::tr("%n more setting(s)", nullptr, values.size() - 3);
  summary->setText(visible_values.join(u"\n"_s));
  summary->setVisible(!values.isEmpty());
  button->setAccessibleDescription(values.join(u"\n"_s));
}

class SectionPresentation : public QObject {
 public:
  SectionPresentation(QWidget *section, const QString &title, SettingsPage *page) : QObject(section), section_(section) {
    section->setAccessibleName(title);
    content_ = new QWidget(section);
    content_->setAccessibleName(title);
    content_->setLayout(section->layout());
    AdaptLayout(content_->layout(), page);
    content_->hide();
    if (auto *group = qobject_cast<QGroupBox*>(section)) {
      group->setFlat(true);
      if (!group->isCheckable()) {
        group->setTitle(QString());
        group->setProperty("androidNavigationSection", true);
        group->setStyleSheet(u"QGroupBox[androidNavigationSection=\"true\"] { border: none; margin: 0px; padding: 0px; }"_s);
      }
      QObject::connect(group, &QGroupBox::toggled, content_, &QWidget::setEnabled);
    }
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    QLabel *summary = nullptr;
    auto *button = NavigationButton(section, title, &summary);
    layout->addWidget(button);
    QList<QPointer<QWidget>> controls;
    for (QWidget *control : content_->findChildren<QWidget*>()) controls << control;
    // Editors are temporarily reparented out of their source section during navigation.
    update_ = [controls, summary, button]() { Summarize(controls, summary, button); };
    auto update = [this]() { update_(); };
    for (const QPointer<QWidget> &control : controls) {
      if (qobject_cast<QComboBox*>(control.data()) || qobject_cast<QAbstractSpinBox*>(control.data()) ||
          qobject_cast<QCheckBox*>(control.data()) || qobject_cast<QRadioButton*>(control.data())) {
        control->installEventFilter(this);
      }
    }
    for (QComboBox *combo : content_->findChildren<QComboBox*>()) {
      QObject::connect(combo, &QComboBox::currentTextChanged, this, update);
    }
    for (QAbstractButton *control : content_->findChildren<QAbstractButton*>()) {
      QObject::connect(control, &QAbstractButton::toggled, this, update);
    }
    for (QSpinBox *spin : content_->findChildren<QSpinBox*>()) {
      QObject::connect(spin, &QSpinBox::valueChanged, this, update);
    }
    for (QDoubleSpinBox *spin : content_->findChildren<QDoubleSpinBox*>()) {
      QObject::connect(spin, &QDoubleSpinBox::valueChanged, this, update);
    }
    QObject::connect(button, &QPushButton::clicked, page, [this, page, title]() {
      content_->setEnabled(IsEnabled());
      page->dialog()->OpenAndroidSubpage(content_, title);
    });
    const auto radios = content_->findChildren<QRadioButton*>();
    if (!radios.isEmpty() && content_->findChildren<QComboBox*>().isEmpty() &&
        content_->findChildren<QCheckBox*>().isEmpty() && content_->findChildren<QLineEdit*>().isEmpty() &&
        content_->findChildren<QAbstractSpinBox*>().isEmpty() && content_->findChildren<QPushButton*>().isEmpty()) {
      for (QRadioButton *radio : radios) {
        QObject::connect(radio, &QRadioButton::clicked, page, [page]() { page->dialog()->CloseAndroidSubpage(); });
      }
    }
    section->installEventFilter(this);
    update();
  }

 protected:
  bool eventFilter(QObject *object, QEvent *event) override {
    if (object == section_ && event->type() == QEvent::EnabledChange) {
      content_->setEnabled(IsEnabled());
    }
    if (!update_pending_ && (event->type() == QEvent::EnabledChange || event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
      update_pending_ = true;
      QTimer::singleShot(0, this, [this]() {
        update_pending_ = false;
        update_();
      });
    }
    return false;
  }

 private:
  bool IsEnabled() const {
    const auto *group = qobject_cast<QGroupBox*>(section_);
    return section_->isEnabled() && (!group || !group->isCheckable() || group->isChecked());
  }

  QWidget *section_;
  QWidget *content_;
  std::function<void()> update_;
  bool update_pending_ = false;
};

void AdaptWidget(QWidget *widget, SettingsPage *page) {
  if (widget->property("androidSettingsAdapted").toBool()) return;
  widget->setProperty("androidSettingsAdapted", true);
  if (auto *label = qobject_cast<QLabel*>(widget)) {
    label->setWordWrap(true);
    label->setMinimumWidth(0);
    label->setMaximumWidth(QWIDGETSIZE_MAX);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  }
  else if (auto *group = qobject_cast<QGroupBox*>(widget)) {
    group->setMinimumWidth(0);
    if (group->layout() && !group->title().isEmpty()) new SectionPresentation(group, group->title(), page);
    else if (group->layout()) AdaptLayout(group->layout(), page);
  }
  else if (auto *combo = qobject_cast<QComboBox*>(widget)) {
    new ComboPresentation(combo, page->dialog());
  }
  else if (auto *radio = qobject_cast<QRadioButton*>(widget)) {
    new ChoicePresentation(radio, true);
  }
  else if (auto *check = qobject_cast<QCheckBox*>(widget)) {
    new ChoicePresentation(check, false);
  }
  else if (auto *tool = qobject_cast<QToolButton*>(widget); tool && tool->menu()) {
    new MenuPresentation(tool, page->dialog());
  }
  else if (auto *tabs = qobject_cast<QTabWidget*>(widget)) {
    auto *parent_layout = tabs->parentWidget()->layout();
    auto *overview = new QWidget(tabs->parentWidget());
    auto *layout = new QVBoxLayout(overview);
    layout->setContentsMargins(0, 0, 0, 0);
    delete parent_layout->replaceWidget(tabs, overview);
    tabs->setParent(overview);
    tabs->hide();
    tabs->tabBar()->hide();
    for (int i = 0; i < tabs->count(); ++i) {
      if (tabs->widget(i)->layout()) AdaptLayout(tabs->widget(i)->layout(), page);
      auto *button = NavigationButton(overview, tabs->tabText(i));
      button->setEnabled(tabs->isTabEnabled(i));
      layout->addWidget(button);
      QObject::connect(button, &QPushButton::clicked, page, [page, tabs, i]() {
        tabs->setCurrentIndex(i);
        page->dialog()->OpenAndroidSubpage(tabs, tabs->tabText(i));
      });
    }
  }
  else if (auto *area = qobject_cast<QScrollArea*>(widget)) {
    if (area->widget()) AdaptWidget(area->widget(), page);
  }
  else if (qobject_cast<QAbstractItemView*>(widget)) {
    widget->setMinimumWidth(0);
    widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
  }
  else if (qobject_cast<QAbstractButton*>(widget) || qobject_cast<QAbstractSpinBox*>(widget) ||
           qobject_cast<QLineEdit*>(widget) || qobject_cast<QSlider*>(widget)) {
    widget->setMinimumWidth(0);
    widget->setMaximumWidth(QWIDGETSIZE_MAX);
    widget->setMinimumHeight(48);
    widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  }
  else if (widget->layout()) {
    const QString title = widget->property("androidSettingsTitle").toString();
    if (title.isEmpty()) AdaptLayout(widget->layout(), page);
    else new SectionPresentation(widget, title, page);
  }
}

void AdaptLayout(QLayout *layout, SettingsPage *page) {
  if (auto *box = qobject_cast<QBoxLayout*>(layout)) {
    box->setDirection(QBoxLayout::TopToBottom);
    box->setAlignment(Qt::AlignTop);
    for (int i = 0; i < box->count(); ++i) box->setStretch(i, 0);
  }
  else if (auto *form = qobject_cast<QFormLayout*>(layout)) {
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    QList<QPair<int, QLayoutItem*>> items;
    while (form->count()) {
      int row;
      QFormLayout::ItemRole role;
      form->getItemPosition(0, &row, &role);
      items.append({row * 3 + static_cast<int>(role), form->takeAt(0)});
    }
    std::stable_sort(items.begin(), items.end(), [](const auto &left, const auto &right) {
      return left.first < right.first;
    });
    for (int row = 0; row < items.size(); ++row) form->setItem(row, QFormLayout::SpanningRole, items.at(row).second);
    form->setAlignment(Qt::AlignTop);
  }
  else if (auto *grid = qobject_cast<QGridLayout*>(layout)) {
    for (int row = 0; row < grid->rowCount(); ++row) {
      grid->setRowStretch(row, 0);
      grid->setRowMinimumHeight(row, 0);
    }
    for (int column = 0; column < grid->columnCount(); ++column) {
      grid->setColumnStretch(column, 0);
      grid->setColumnMinimumWidth(column, 0);
    }
    grid->setAlignment(Qt::AlignTop);
    struct GridItem {
      int row;
      int column;
      QLayoutItem *item;
    };
    QList<GridItem> items;
    while (grid->count()) {
      int row, column, row_span, column_span;
      grid->getItemPosition(0, &row, &column, &row_span, &column_span);
      items.append({row, column, grid->takeAt(0)});
    }
    std::stable_sort(items.begin(), items.end(), [](const GridItem &left, const GridItem &right) {
      return left.row == right.row ? left.column < right.column : left.row < right.row;
    });
    for (int row = 0; row < items.size(); ++row) grid->addItem(items.at(row).item, row, 0);
    grid->setColumnStretch(0, 1);
  }
  for (int i = 0; i < layout->count(); ++i) {
    QLayoutItem *item = layout->itemAt(i);
    if (item->widget()) AdaptWidget(item->widget(), page);
    else if (item->layout()) AdaptLayout(item->layout(), page);
    else if (item->spacerItem()) item->spacerItem()->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
  }
}

}  // namespace

void AndroidSettingsPage::Adapt(SettingsPage *page) {
  if (auto *proxy = page->findChild<QWidget*>(u"proxy_manual_container"_s)) {
    proxy->setProperty("androidSettingsTitle", SettingsDialog::tr("Manual proxy configuration"));
  }
  if (auto *folders = page->findChild<QBoxLayout*>(u"layout_collection_folders"_s)) {
    auto *root = qobject_cast<QBoxLayout*>(page->layout());
    int index = 0;
    while (index < root->count() && root->itemAt(index)->layout() != folders) ++index;
    root->takeAt(index);
    folders->setParent(nullptr);
    auto *group = new QGroupBox(SettingsDialog::tr("Music folders"), page);
    group->setLayout(folders);
    if (auto *intro = page->findChild<QLabel*>(u"label_collection_folders"_s)) {
      const int intro_index = root->indexOf(intro);
      root->removeWidget(intro);
      if (intro_index >= 0 && intro_index < index) --index;
      folders->insertWidget(0, intro);
    }
    root->insertWidget(index, group);
  }
  AdaptLayout(page->layout(), page);
}

#endif  // Q_OS_ANDROID
