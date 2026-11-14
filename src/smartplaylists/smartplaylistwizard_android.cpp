// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QInputMethod>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QTreeView>
#include <QWindow>
#include <QWizardPage>

#include "widgets/androidwidgets.h"
#include "smartplaylistwizard.h"

using namespace Qt::Literals::StringLiterals;

namespace {

void AdaptLayout(QLayout *layout) {

  if (!layout) return;
  if (auto *box = qobject_cast<QBoxLayout*>(layout)) {
    if (box->direction() == QBoxLayout::LeftToRight || box->direction() == QBoxLayout::RightToLeft) box->setDirection(QBoxLayout::TopToBottom);
  }
  for (int i = 0; i < layout->count(); ++i) {
    QLayoutItem *item = layout->itemAt(i);
    if (item->layout()) AdaptLayout(item->layout());
    if (item->widget()) AdaptLayout(item->widget()->layout());
  }

}

}  // namespace

void SmartPlaylistWizard::SetupAndroid(QStackedWidget *pages) {

  android_pages_ = pages;
  setWizardStyle(QWizard::ClassicStyle);
  setMinimumSize(0, 0);
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  setButtonLayout({CancelButton, Stretch, NextButton, FinishButton});
  for (const WizardButton which : {CancelButton, NextButton, FinishButton}) button(which)->setMinimumHeight(48);

  for (const int id : pageIds()) {
    QWizardPage *wizard_page = page(id);
    // QWizard's completeness handler hides its contents, losing keyboard focus when embedded.
    QObject::disconnect(wizard_page, SIGNAL(completeChanged()), this, nullptr);
    QObject::connect(wizard_page, &QWizardPage::completeChanged, this, [this, wizard_page]() {
      if (currentPage() != wizard_page) return;
      const bool complete = wizard_page->isComplete();
      const bool can_continue = nextId() != -1;
      button(NextButton)->setEnabled(can_continue && complete);
      button(FinishButton)->setEnabled((wizard_page->isFinalPage() || !can_continue) && complete);
    });
    const QString title = wizard_page->title();
    const QString description = wizard_page->subTitle();
    wizard_page->setProperty("androidSmartPlaylistTitle", title);
    wizard_page->setTitle(QString());
    wizard_page->setSubTitle(QString());
    if (!wizard_page->layout()) continue;
    auto *content = new QWidget(wizard_page);
    content->setLayout(wizard_page->layout());
    AdaptAndroidControls(content);
    auto *layout = new QVBoxLayout(wizard_page);
    auto *heading = new QLabel(tr("Smart playlists > %1").arg(title), wizard_page);
    heading->setTextFormat(Qt::PlainText);
    auto *back = AndroidWidgets::CreateBackButton(wizard_page);
    QObject::connect(back, &QPushButton::clicked, this, &SmartPlaylistWizard::AndroidBack);
    layout->addLayout(AndroidWidgets::CreatePageHeader(heading, back));
    const auto scroll = AndroidWidgets::CreateScrollPage(wizard_page);
    auto *explanation = new QLabel(description, scroll.contents);
    explanation->setWordWrap(true);
    scroll.layout->addWidget(explanation);
    scroll.layout->addWidget(content);
    scroll.layout->addStretch();
    layout->addWidget(scroll.area);
  }
  qApp->installEventFilter(this);
  QObject::connect(this, &QDialog::finished, this, [this]() {
    if (android_choices_) {
      android_pages_->removeWidget(android_choices_);
      android_choices_->deleteLater();
      android_choices_.clear();
    }
  });

}

void SmartPlaylistWizard::AdaptAndroidControls(QWidget *content) {

  if (!android_pages_ || content->property("androidSmartPlaylistAdapted").toBool()) return;
  content->setProperty("androidSmartPlaylistAdapted", true);
  for (QFormLayout *form : content->findChildren<QFormLayout*>()) {
    auto *vertical = new QVBoxLayout;
    vertical->setContentsMargins(form->contentsMargins());
    vertical->setSpacing(form->spacing());
    while (form->count()) {
      QLayoutItem *item = form->takeAt(0);
      if (QLayout *child = item->layout()) {
        child->setParent(nullptr);
        vertical->addLayout(child);
      }
      else vertical->addItem(item);
    }
    QWidget *parent = form->parentWidget();
    delete form;
    parent->setLayout(vertical);
  }
  for (QScrollArea *area : content->findChildren<QScrollArea*>()) {
    if (area->objectName() != "terms_scroll_area"_L1 || !area->widget()) continue;
    QWidget *terms = area->takeWidget();
    delete area->parentWidget()->layout()->replaceWidget(area, terms);
    area->hide();
    area->parentWidget()->setMinimumHeight(0);
  }
  AdaptLayout(content->layout());
  for (QWidget *widget : content->findChildren<QWidget*>()) {
    AdaptLayout(widget->layout());
    widget->setMinimumWidth(0);
    if (auto *label = qobject_cast<QLabel*>(widget)) {
      label->setWordWrap(true);
      label->setTextFormat(Qt::PlainText);
    }
    if (qobject_cast<QAbstractButton*>(widget) || qobject_cast<QLineEdit*>(widget) || qobject_cast<QAbstractSpinBox*>(widget)) {
      widget->setMinimumHeight(48);
    }
    if (auto *tree = qobject_cast<QTreeView*>(widget)) {
      tree->setMinimumHeight(160);
      tree->setMaximumHeight(240);
    }
    if (auto *area = qobject_cast<QScrollArea*>(widget)) {
      area->setMinimumHeight(220);
      area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    if (auto *stack = qobject_cast<QStackedWidget*>(widget)) stack->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    widget->setProperty("androidSmartPlaylistAdapted", true);
  }
  for (QComboBox *combo : content->findChildren<QComboBox*>()) {
    if (combo->property("androidSmartPlaylistChoice").toBool()) continue;
    combo->setProperty("androidSmartPlaylistChoice", true);
    QString title = tr("Options");
    const QString name = combo->objectName();
    if (name == "field"_L1 || name == "field_value"_L1) title = tr("Field");
    else if (name == "op"_L1) title = tr("Condition");
    else if (name == "type"_L1) title = tr("Search mode");
    else if (name == "order"_L1) title = tr("Order");
    else if (name.startsWith("date_type"_L1)) title = tr("Period");
    auto *panel = new QWidget(combo->parentWidget());
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto value = AndroidWidgets::CreateLabeledValueButton(title, panel);
    layout->addWidget(value.label);
    layout->addWidget(value.button);
    delete combo->parentWidget()->layout()->replaceWidget(combo, panel);
    combo->hide();
    value.button->SetValue(combo->currentText());
    QObject::connect(combo, &QComboBox::currentTextChanged, value.button, [button = value.button](const QString &text) { button->SetValue(text); });
    QObject::connect(value.button, &QPushButton::clicked, this, [this, combo, title]() { OpenAndroidChoices(combo, title); });
    android_value_buttons_.insert(combo, value.button);
    QObject::connect(combo, &QObject::destroyed, this, [this, combo]() { android_value_buttons_.remove(combo); });
    value.button->setEnabled(combo->isEnabled());
  }
  for (QRadioButton *radio : content->findChildren<QRadioButton*>()) {
    if (radio->property("androidSmartPlaylistChoice").toBool()) continue;
    radio->setProperty("androidSmartPlaylistChoice", true);
    QRadioButton *row = AndroidWidgets::CreateChoiceRow(radio->text(), radio->parentWidget());
    row->setProperty("androidSmartPlaylistChoice", true);
    row->setAutoExclusive(false);
    row->setChecked(radio->isChecked());
    delete radio->parentWidget()->layout()->replaceWidget(radio, row);
    radio->hide();
    row->setEnabled(radio->isEnabled());
    android_value_buttons_.insert(radio, row);
    QObject::connect(radio, &QObject::destroyed, this, [this, radio]() { android_value_buttons_.remove(radio); });
    QObject::connect(row, &QRadioButton::clicked, radio, &QRadioButton::click);
    QObject::connect(radio, &QRadioButton::toggled, row, &QRadioButton::setChecked);
  }
  for (QStackedWidget *stack : content->findChildren<QStackedWidget*>()) {
    const auto resize = [stack]() {
      if (stack->currentWidget()) stack->setFixedHeight(qMax(48, stack->currentWidget()->sizeHint().height()));
    };
    QObject::connect(stack, &QStackedWidget::currentChanged, stack, resize);
    QTimer::singleShot(0, stack, resize);
  }

}

void SmartPlaylistWizard::OpenAndroidChoices(QComboBox *combo, const QString &title) {

  if (android_choices_) return;
  auto *choices = new QWidget(android_pages_);
  android_choices_ = choices;
  auto *layout = new QVBoxLayout(choices);
  auto *heading = new QLabel(tr("Smart playlists > %1\n%2").arg(currentPage()->property("androidSmartPlaylistTitle").toString(), title), choices);
  heading->setTextFormat(Qt::PlainText);
  auto *back = AndroidWidgets::CreateBackButton(choices);
  layout->addLayout(AndroidWidgets::CreatePageHeader(heading, back));
  const auto scroll = AndroidWidgets::CreateScrollPage(choices);
  layout->addWidget(scroll.area);
  QButtonGroup *group = AndroidWidgets::AddComboChoiceRows(combo, scroll.layout);
  scroll.layout->addStretch();
  QObject::connect(back, &QPushButton::clicked, this, &SmartPlaylistWizard::AndroidBack);
  QObject::connect(group, &QButtonGroup::idClicked, this, &SmartPlaylistWizard::AndroidBack);
  android_pages_->addWidget(choices);
  android_pages_->setCurrentWidget(choices);

}

void SmartPlaylistWizard::AndroidBack() {

  if (android_choices_) {
    QWidget *choices = android_choices_;
    android_pages_->setCurrentWidget(this);
    android_pages_->removeWidget(choices);
    android_choices_.clear();
    choices->deleteLater();
  }
  else if (visitedIds().size() > 1) back();
  else reject();

}

void SmartPlaylistWizard::CleanupAndroid() {

  qApp->removeEventFilter(this);
  for (QObject *control : android_value_buttons_.keys()) {
    QObject::disconnect(control, nullptr, this, nullptr);
  }

}

bool SmartPlaylistWizard::eventFilter(QObject *object, QEvent *event) {

  if (event->type() == QEvent::EnabledChange) {
    if (const auto button = android_value_buttons_.value(object)) {
      button->setEnabled(static_cast<QWidget*>(object)->isEnabled());
    }
  }
  if ((event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) && android_pages_ && android_pages_->isVisible() &&
      (android_pages_->currentWidget() == this || android_pages_->currentWidget() == android_choices_)) {
    const auto *key = static_cast<QKeyEvent*>(event);
    QWidget *widget = qobject_cast<QWidget*>(object);
    if (((widget && widget->window() == android_pages_->window()) || object == android_pages_->window()->windowHandle()) &&
        (key->key() == Qt::Key_Back || key->key() == Qt::Key_Escape)) {
      if (event->type() == QEvent::KeyPress) {
        if (QGuiApplication::inputMethod()->isVisible()) QGuiApplication::inputMethod()->hide();
        else AndroidBack();
      }
      event->accept();
      return true;
    }
  }
  return QWizard::eventFilter(object, event);

}
