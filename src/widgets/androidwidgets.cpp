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
#include <QBoxLayout>
#include <QButtonGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QVBoxLayout>

#include "core/iconloader.h"

using namespace Qt::Literals::StringLiterals;

namespace AndroidWidgets {

namespace {
constexpr int kPageTitlePointSize = 18;
constexpr qreal kNavigationTextScale = 1.15;
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
