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

#include "androidwidgets.h"

#include <QAbstractButton>
#include <QApplication>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QComboBox>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPalette>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QVBoxLayout>

#include "core/iconloader.h"

using namespace Qt::Literals::StringLiterals;

namespace AndroidWidgets {

namespace {
constexpr int kPageTitlePointSize = 18;
constexpr qreal kNavigationTextScale = 1.15;

class CheckBoxPresentation : public QObject {
 public:
  explicit CheckBoxPresentation(QCheckBox *check)
      : QObject(check), check_(check), minimum_height_(qMax(kCheckBoxMinimumHeight, check->minimumHeight())) {
    check->setStyleSheet(check->styleSheet() + u" QCheckBox::indicator { width: %1px; height: %1px; }"_s.arg(kCheckBoxIndicatorSize));
    auto *layout = new QVBoxLayout(check);
    label_ = AddButtonLabel(layout, DisplayText());
    check->setMinimumWidth(0);
    check->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    UpdateLayout();
    check->installEventFilter(this);
  }

 protected:
  bool eventFilter(QObject *, QEvent *event) override {
    if (event->type() == QEvent::Resize || event->type() == QEvent::FontChange ||
        event->type() == QEvent::LayoutRequest || event->type() == QEvent::LayoutDirectionChange) {
      UpdateLayout();
    }
    else if (event->type() == QEvent::Paint) {
      const QString text = DisplayText();
      if (label_->text() != text) label_->setText(text);
      QStyleOptionButton option;
      option.initFrom(check_);
      if (check_->isDown()) option.state |= QStyle::State_Sunken;
      if (check_->checkState() == Qt::PartiallyChecked) option.state |= QStyle::State_NoChange;
      else option.state |= check_->isChecked() ? QStyle::State_On : QStyle::State_Off;
      QStylePainter painter(check_);
      painter.drawControl(QStyle::CE_CheckBox, option);
      return true;
    }
    else if (event->type() == QEvent::MouseButtonPress && check_->isEnabled()) {
      auto *mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton && check_->rect().contains(mouse->position().toPoint())) {
        check_->setDown(true);
        return true;
      }
    }
    else if (event->type() == QEvent::MouseButtonRelease && check_->isEnabled()) {
      auto *mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton) {
        const bool clicked = check_->isDown() && check_->rect().contains(mouse->position().toPoint());
        check_->setDown(false);
        if (clicked) check_->click();
        return true;
      }
    }
    return false;
  }

 private:
  QString DisplayText() const {
    QString text = check_->text();
    for (qsizetype i = 0; i < text.size(); ++i) {
      if (text.at(i) == u'&') text.remove(i, 1);
    }
    return text;
  }

  void UpdateLayout() {
    const int indicator_margin = kCheckBoxIndicatorSize + 8;
    check_->layout()->setContentsMargins(check_->isRightToLeft() ? 8 : indicator_margin, 8,
                                        check_->isRightToLeft() ? indicator_margin : 8, 8);
    check_->setMinimumHeight(qMax(minimum_height_, check_->layout()->totalHeightForWidth(check_->width())));
  }

  QCheckBox *check_;
  QLabel *label_;
  const int minimum_height_;
};

class CheckableGroupPresentation : public QObject {
 public:
  explicit CheckableGroupPresentation(QGroupBox *group) : QObject(group), group_(group), original_style_(group->styleSheet()) {
    UpdateStyle();
    group->installEventFilter(this);
  }

 protected:
  bool eventFilter(QObject *, QEvent *event) override {
    if (event->type() == QEvent::FontChange || event->type() == QEvent::LayoutDirectionChange) UpdateStyle();
    else if (event->type() == QEvent::Hide || (event->type() == QEvent::EnabledChange && !group_->isEnabled())) pressed_ = false;
    else if (event->type() == QEvent::MouseButtonPress && group_->isEnabled()) {
      auto *mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton && group_->rect().contains(mouse->position().toPoint()) && mouse->position().y() < header_height_) {
        pressed_ = true;
        group_->setFocus(Qt::MouseFocusReason);
        return true;
      }
    }
    else if (event->type() == QEvent::MouseButtonRelease && group_->isEnabled()) {
      auto *mouse = static_cast<QMouseEvent*>(event);
      if (mouse->button() == Qt::LeftButton && pressed_) {
        pressed_ = false;
        if (group_->rect().contains(mouse->position().toPoint()) && mouse->position().y() < header_height_) {
          group_->setChecked(!group_->isChecked());
          Q_EMIT group_->clicked(group_->isChecked());
        }
        return true;
      }
    }
    return false;
  }

 private:
  void UpdateStyle() {
    header_height_ = qMax(kCheckBoxMinimumHeight, group_->fontMetrics().height() + 28);
    const QString style = original_style_ + u" QGroupBox[androidTouchGroupBox=\"true\"] { margin-top: %1px; } "
        "QGroupBox[androidTouchGroupBox=\"true\"]::title { subcontrol-origin: margin; subcontrol-position: top %2; padding-top: 14px; padding-bottom: 14px; } "
        "QGroupBox[androidTouchGroupBox=\"true\"]::indicator { width: %3px; height: %3px; }"_s.arg(header_height_).arg(group_->isRightToLeft() ? u"right"_s : u"left"_s).arg(kCheckBoxIndicatorSize);
    if (group_->styleSheet() != style) group_->setStyleSheet(style);
  }

  QGroupBox *group_;
  QString original_style_;
  int header_height_ = kCheckBoxMinimumHeight;
  bool pressed_ = false;
};

class CheckBoxAdapter : public QObject {
 public:
  explicit CheckBoxAdapter(QApplication *application) : QObject(application) {
    application->installEventFilter(this);
  }

 protected:
  bool eventFilter(QObject *object, QEvent *event) override {
    if (event->type() == QEvent::Polish || event->type() == QEvent::Show) {
      if (auto *check = qobject_cast<QCheckBox*>(object)) AdaptCheckBox(check);
      else if (auto *group = qobject_cast<QGroupBox*>(object)) AdaptCheckableGroupBox(group);
    }
    return false;
  }
};
}

void InitializeCheckBoxes(QApplication *application) {

  new CheckBoxAdapter(application);

}

void AdaptCheckBox(QCheckBox *check) {

  if (check->property("androidTouchCheckBox").toBool()) return;
  check->setProperty("androidTouchCheckBox", true);
  new CheckBoxPresentation(check);

}

void AdaptCheckableGroupBox(QGroupBox *group) {

  if (!group->isCheckable() || group->property("androidTouchGroupBox").toBool()) return;
  group->setProperty("androidTouchGroupBox", true);
  new CheckableGroupPresentation(group);

}

void SetPageTitleFont(QWidget *widget) {

  QFont font = widget->font();
  font.setPointSize(kPageTitlePointSize);
  font.setBold(true);
  widget->setFont(font);

}

QPushButton *CreateBackButton(QWidget *parent) {

  QPushButton *back = new QPushButton(QCoreApplication::translate("AndroidWidgets", "Back"), parent);
  back->setIcon(IconLoader::Load(u"go-previous"_s));
  back->setIconSize(QSize(kNavigationIconSize, kNavigationIconSize));
  back->setMinimumHeight(kMinimumTouchSize);
  return back;

}

QHBoxLayout *CreatePageHeader(QLabel *title, QPushButton *back) {

  SetPageTitleFont(title);
  title->setWordWrap(true);
  QHBoxLayout *header = new QHBoxLayout;
  header->addWidget(title, 1);
  header->addWidget(back);
  return header;

}

void StyleNavigationButton(QAbstractButton *button, const int minimum_height) {

  QFont font = button->font();
  font.setPointSizeF(font.pointSizeF() * kNavigationTextScale);
  button->setFont(font);
  button->setStyleSheet(u"text-align: left; padding-left: 24px;"_s);
  button->setMinimumHeight(minimum_height);
  button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

}

void SetWindowBackground(QScrollArea *area) {

  area->setBackgroundRole(QPalette::Window);
  area->setAutoFillBackground(true);
  area->viewport()->setBackgroundRole(QPalette::Window);
  area->viewport()->setAutoFillBackground(true);

}

ScrollPage CreateScrollPage(QWidget *parent) {

  QScrollArea *area = new QScrollArea(parent);
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  area->setWidgetResizable(true);
  area->setFrameShape(QFrame::NoFrame);
  SetWindowBackground(area);

  QWidget *contents = new QWidget(area);
  contents->setBackgroundRole(QPalette::Window);
  contents->setAutoFillBackground(true);
  QVBoxLayout *layout = new QVBoxLayout(contents);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);
  area->setWidget(contents);

  return {area, contents, layout};

}

ValueButton::ValueButton(const QString &title, QWidget *parent) : LayoutButton<QPushButton>(parent) {

  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QVBoxLayout *layout = new QVBoxLayout(this);
  if (!title.isEmpty()) {
    setAccessibleName(title);
    AddButtonLabel(layout, title + u" >"_s);
  }
  value_ = AddButtonLabel(layout, QString());
  QFont font = value_->font();
  font.setBold(true);
  value_->setFont(font);
  // A foreground role, unlike a palette color, is kept when a style sheet applies to an ancestor.
  value_->setForegroundRole(QPalette::Link);
  value_->hide();

}

void ValueButton::SetValue(const QString &value, const QString &description) {

  value_->setText(value);
  value_->setVisible(!value.isEmpty());
  setAccessibleDescription(description.isEmpty() ? value : description);

}

void ValueButton::setVisible(const bool visible) {

  LayoutButton<QPushButton>::setVisible(visible);
  if (title_label_) title_label_->setVisible(visible);

}

LabeledValueButton CreateLabeledValueButton(const QString &title, QWidget *parent) {

  QLabel *label = new QLabel(title, parent);
  label->setWordWrap(true);
  label->setTextFormat(Qt::PlainText);
  ValueButton *button = new ValueButton(QString(), parent);
  button->setAccessibleName(title);
  button->title_label_ = label;
  label->setBuddy(button);
  return LabeledValueButton{ label, button };

}

QLabel *AddButtonLabel(QBoxLayout *layout, const QString &text) {

  QLabel *label = new QLabel(text, layout->parentWidget());
  label->setWordWrap(true);
  label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  label->setTextFormat(Qt::PlainText);
  label->setAttribute(Qt::WA_TransparentForMouseEvents);
  layout->addWidget(label);
  return label;

}

void StyleChoiceRow(QRadioButton *button, QLabel *label) {

  button->setStyleSheet(u"QRadioButton::indicator { image: none; border: none; background: transparent; width: 0px; height: 0px; }"_s);
  button->setAutoFillBackground(true);
  QWidget *text = label ? static_cast<QWidget*>(label) : button;
  const auto update = [button, text](const bool checked) {
    button->setBackgroundRole(checked ? QPalette::Highlight : QPalette::Window);
    text->setForegroundRole(checked ? QPalette::HighlightedText : QPalette::WindowText);
    QFont font = text->font();
    font.setBold(checked);
    text->setFont(font);
  };
  QObject::connect(button, &QRadioButton::toggled, text, update);
  update(button->isChecked());

}

QRadioButton *CreateChoiceRow(const QString &text, QWidget *parent) {

  LayoutButton<QRadioButton> *choice = new LayoutButton<QRadioButton>(parent);
  choice->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  choice->setAccessibleName(text);
  QVBoxLayout *layout = new QVBoxLayout(choice);
  layout->setContentsMargins(8, 8, 8, 8);
  StyleChoiceRow(choice, AddButtonLabel(layout, text));
  return choice;

}

QButtonGroup *AddComboChoiceRows(QComboBox *combo, QBoxLayout *layout) {

  QButtonGroup *group = new QButtonGroup(layout->parentWidget());
  for (int i = 0; i < combo->count(); ++i) {
    QRadioButton *choice = CreateChoiceRow(combo->itemText(i), layout->parentWidget());
    group->addButton(choice, i);
    choice->setChecked(combo->currentIndex() == i);
    layout->addWidget(choice);
  }
  QObject::connect(combo, &QComboBox::currentIndexChanged, group, [group](const int index) {
    if (QAbstractButton *choice = group->button(index)) choice->setChecked(true);
  });
  // Connected first, so the combo box is updated before the caller's idClicked() handlers run.
  QObject::connect(group, &QButtonGroup::idClicked, combo, &QComboBox::setCurrentIndex);
  return group;

}

}  // namespace AndroidWidgets
