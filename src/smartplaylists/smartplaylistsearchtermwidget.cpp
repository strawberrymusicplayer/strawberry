/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QIODevice>
#include <QFile>
#include <QMessageBox>
#include <QDateTime>
#include <QMetaObject>
#include <QPropertyAnimation>
#include <QEvent>
#include <QEnterEvent>
#include <QShowEvent>
#include <QResizeEvent>

#include "includes/shared_ptr.h"
#include "core/iconloader.h"
#include "utilities/colorutils.h"
#include "playlist/playlist.h"
#include "playlist/playlistdelegates.h"
#include "smartplaylistsearchterm.h"
#include "smartplaylistsearchtermwidget.h"
#include "smartplaylistsearchtermwidgetoverlay.h"
#include "ui_smartplaylistsearchtermwidget.h"

using namespace Qt::Literals::StringLiterals;

SmartPlaylistSearchTermWidget::SmartPlaylistSearchTermWidget(SharedPtr<CollectionBackend> collection_backend, QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_SmartPlaylistSearchTermWidget),
      collection_backend_(collection_backend),
      overlay_(nullptr),
      animation_(new QPropertyAnimation(this, "overlay_opacity", this)),
      active_(true),
      initialized_(false),
      current_field_type_(SmartPlaylistSearchTerm::Type::Invalid) {

  ui_->setupUi(this);

  QObject::connect(ui_->field, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SmartPlaylistSearchTermWidget::FieldChanged);
  QObject::connect(ui_->op, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SmartPlaylistSearchTermWidget::OpChanged);
  QObject::connect(ui_->remove, &QToolButton::clicked, this, &SmartPlaylistSearchTermWidget::RemoveClicked);

  QObject::connect(ui_->value_date, &QDateEdit::dateChanged, this, &SmartPlaylistSearchTermWidget::Changed);
  QObject::connect(ui_->value_number, QOverload<int>::of(&QSpinBox::valueChanged), this, &SmartPlaylistSearchTermWidget::Changed);
  QObject::connect(ui_->value_text, &QLineEdit::textChanged, this, &SmartPlaylistSearchTermWidget::Changed);
  QObject::connect(ui_->value_time, &QTimeEdit::timeChanged, this, &SmartPlaylistSearchTermWidget::Changed);
  QObject::connect(ui_->value_date_numeric, QOverload<int>::of(&QSpinBox::valueChanged), this, &SmartPlaylistSearchTermWidget::Changed);
  QObject::connect(ui_->value_date_numeric1, QOverload<int>::of(&QSpinBox::valueChanged), this, &SmartPlaylistSearchTermWidget::RelativeValueChanged);
  QObject::connect(ui_->value_date_numeric2, QOverload<int>::of(&QSpinBox::valueChanged), this, &SmartPlaylistSearchTermWidget::RelativeValueChanged);
  QObject::connect(ui_->date_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SmartPlaylistSearchTermWidget::Changed);
  QObject::connect(ui_->date_type_relative, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SmartPlaylistSearchTermWidget::Changed);
  QObject::connect(ui_->value_rating, &RatingWidget::RatingChanged, this, &SmartPlaylistSearchTermWidget::Changed);

  ui_->value_date->setDate(QDate::currentDate());

  // Populate the combo boxes
  for (int i = 0; i < static_cast<int>(SmartPlaylistSearchTerm::Field::FieldCount); ++i) {
    const SmartPlaylistSearchTerm::Field field = static_cast<SmartPlaylistSearchTerm::Field>(i);
    ui_->field->addItem(SmartPlaylistSearchTerm::FieldName(field));
    ui_->field->setItemData(i, QVariant::fromValue(field));
  }
  ui_->field->model()->sort(0);

  // Populate the date type combo box
  for (int i = 0; i < 5; ++i) {
    const SmartPlaylistSearchTerm::DateType datetype = static_cast<SmartPlaylistSearchTerm::DateType>(i);
    ui_->date_type->addItem(SmartPlaylistSearchTerm::DateName(datetype, false));
    ui_->date_type->setItemData(i, QVariant::fromValue(datetype));

    ui_->date_type_relative->addItem(SmartPlaylistSearchTerm::DateName(datetype, false));
    ui_->date_type_relative->setItemData(i, QVariant::fromValue(datetype));
  }

  // Icons on the buttons
  ui_->remove->setIcon(IconLoader::Load(u"list-remove"_s));

  // Set stylesheet
  QFile stylesheet_file(u":/style/smartplaylistsearchterm.css"_s);
  if (stylesheet_file.open(QIODevice::ReadOnly)) {
    QString stylesheet = QString::fromLatin1(stylesheet_file.readAll());
    stylesheet_file.close();
    const QColor base(222, 97, 97, 128);
    stylesheet.replace("%light2"_L1, Utilities::ColorToRgba(base.lighter(140)));
    stylesheet.replace("%light"_L1, Utilities::ColorToRgba(base.lighter(120)));
    stylesheet.replace("%dark"_L1, Utilities::ColorToRgba(base.darker(120)));
    stylesheet.replace("%base"_L1, Utilities::ColorToRgba(base));
    setStyleSheet(stylesheet);
  }

}

SmartPlaylistSearchTermWidget::~SmartPlaylistSearchTermWidget() { delete ui_; }

void SmartPlaylistSearchTermWidget::FieldChanged(int index) {

  const SmartPlaylistSearchTerm::Field field = ui_->field->itemData(index).value<SmartPlaylistSearchTerm::Field>();
  const SmartPlaylistSearchTerm::Type type = SmartPlaylistSearchTerm::TypeOf(field);

  // Populate the operator combo box
  if (type != current_field_type_) {
    ui_->op->clear();
    const SmartPlaylistSearchTerm::OperatorList operators = SmartPlaylistSearchTerm::OperatorsForType(type);
    for (const SmartPlaylistSearchTerm::Operator op : operators) {
      const int i = ui_->op->count();
      ui_->op->addItem(SmartPlaylistSearchTerm::OperatorText(type, op));
      ui_->op->setItemData(i, QVariant::fromValue(op));
    }
    current_field_type_ = type;
  }

  // Show the correct value editor
  QWidget *page = nullptr;
  const SmartPlaylistSearchTerm::Operator op = ui_->op->currentData().value<SmartPlaylistSearchTerm::Operator>();
  switch (type) {
    case SmartPlaylistSearchTerm::Type::Time:
      page = ui_->page_time;
      break;
    case SmartPlaylistSearchTerm::Type::Number:
      page = ui_->page_number;
      break;
    case SmartPlaylistSearchTerm::Type::Date:
      page = ui_->page_date;
      break;
    case SmartPlaylistSearchTerm::Type::Text:
      if (op == SmartPlaylistSearchTerm::Operator::Empty || op == SmartPlaylistSearchTerm::Operator::NotEmpty) {
        page = ui_->page_empty;
      }
      else {
        page = ui_->page_text;
      }
      break;
    case SmartPlaylistSearchTerm::Type::Rating:
      page = ui_->page_rating;
      break;
    case SmartPlaylistSearchTerm::Type::Invalid:
      page = nullptr;
      break;
  }
  ui_->value_stack->setCurrentWidget(page);

  // Maybe set a tag completer
  switch (field) {
    case SmartPlaylistSearchTerm::Field::Artist:
      new TagCompleter(collection_backend_, Playlist::Column::Artist, ui_->value_text);
      break;
    case SmartPlaylistSearchTerm::Field::ArtistSort:
      new TagCompleter(collection_backend_, Playlist::Column::ArtistSort, ui_->value_text);
      break;
    case SmartPlaylistSearchTerm::Field::Album:
      new TagCompleter(collection_backend_, Playlist::Column::Album, ui_->value_text);
      break;
    case SmartPlaylistSearchTerm::Field::AlbumSort:
      new TagCompleter(collection_backend_, Playlist::Column::AlbumSort, ui_->value_text);
      break;
    case SmartPlaylistSearchTerm::Field::AlbumArtist:
      new TagCompleter(collection_backend_, Playlist::Column::AlbumArtist, ui_->value_text);
      break;
    case SmartPlaylistSearchTerm::Field::AlbumArtistSort:
      new TagCompleter(collection_backend_, Playlist::Column::AlbumArtistSort, ui_->value_text);
      break;
    case SmartPlaylistSearchTerm::Field::ComposerSort:
      new TagCompleter(collection_backend_, Playlist::Column::ComposerSort, ui_->value_text);
      break;
    case SmartPlaylistSearchTerm::Field::PerformerSort:
      new TagCompleter(collection_backend_, Playlist::Column::PerformerSort, ui_->value_text);
      break;
    case SmartPlaylistSearchTerm::Field::TitleSort:
      new TagCompleter(collection_backend_, Playlist::Column::TitleSort, ui_->value_text);
      break;
    default:
      ui_->value_text->setCompleter(nullptr);
  }

  Q_EMIT Changed();

}

void SmartPlaylistSearchTermWidget::OpChanged(int idx) {

  Q_UNUSED(idx);

  // Determine the currently selected operator
  // This uses the operatorss index in the combobox to get its enum value
  const SmartPlaylistSearchTerm::Operator op = ui_->op->currentData().value<SmartPlaylistSearchTerm::Operator>();

  // We need to change the page only in the following case
  if ((ui_->value_stack->currentWidget() == ui_->page_text) || (ui_->value_stack->currentWidget() == ui_->page_empty)) {
    QWidget *page = nullptr;
    if (op == SmartPlaylistSearchTerm::Operator::Empty || op == SmartPlaylistSearchTerm::Operator::NotEmpty) {
      page = ui_->page_empty;
    }
    else {
      page = ui_->page_text;
    }
    ui_->value_stack->setCurrentWidget(page);
  }
  else if (
      (ui_->value_stack->currentWidget() == ui_->page_date) ||
      (ui_->value_stack->currentWidget() == ui_->page_date_numeric) ||
      (ui_->value_stack->currentWidget() == ui_->page_date_relative)
      ) {
    QWidget *page = nullptr;
    if (op == SmartPlaylistSearchTerm::Operator::NumericDate || op == SmartPlaylistSearchTerm::Operator::NumericDateNot) {
      page = ui_->page_date_numeric;
    }
    else if (op == SmartPlaylistSearchTerm::Operator::RelativeDate) {
      page = ui_->page_date_relative;
    }
    else {
      page = ui_->page_date;
    }
    ui_->value_stack->setCurrentWidget(page);
  }

  Q_EMIT Changed();

}

void SmartPlaylistSearchTermWidget::SetActive(bool active) {

  active_ = active;

  if (overlay_) {
    delete overlay_;
    overlay_ = nullptr;
  }

  ui_->container->setEnabled(active);

  if (!active) {
    overlay_ = new SmartPlaylistSearchTermWidgetOverlay(this);
  }

}

void SmartPlaylistSearchTermWidget::enterEvent(QEnterEvent *e) {

  Q_UNUSED(e)

  if (!overlay_ || !isEnabled()) return;

  animation_->stop();
  animation_->setEndValue(1.0);
  animation_->setDuration(80);
  animation_->start();

}

void SmartPlaylistSearchTermWidget::leaveEvent(QEvent *e) {

  Q_UNUSED(e);

  if (!overlay_) return;

  animation_->stop();
  animation_->setEndValue(0.0);
  animation_->setDuration(160);
  animation_->start();

}

void SmartPlaylistSearchTermWidget::resizeEvent(QResizeEvent *e) {

  QWidget::resizeEvent(e);

  if (overlay_ && overlay_->isVisible()) {
    QMetaObject::invokeMethod(this, &SmartPlaylistSearchTermWidget::Grab);
  }

}

void SmartPlaylistSearchTermWidget::showEvent(QShowEvent *e) {

  QWidget::showEvent(e);
  if (overlay_) {
    QMetaObject::invokeMethod(this, &SmartPlaylistSearchTermWidget::Grab);
  }

}

void SmartPlaylistSearchTermWidget::Grab() { overlay_->Grab(); }

void SmartPlaylistSearchTermWidget::set_overlay_opacity(const float opacity) {
  if (overlay_) overlay_->SetOpacity(opacity);
}

float SmartPlaylistSearchTermWidget::overlay_opacity() const {
  return overlay_ ? overlay_->opacity() : static_cast<float>(0.0);
}

void SmartPlaylistSearchTermWidget::SetTerm(const SmartPlaylistSearchTerm &term) {

  ui_->field->setCurrentIndex(ui_->field->findData(QVariant::fromValue(term.field_)));
  ui_->op->setCurrentIndex(ui_->op->findData(QVariant::fromValue(term.operator_)));

  // The value depends on the data type
  switch (SmartPlaylistSearchTerm::TypeOf(term.field_)) {
    case SmartPlaylistSearchTerm::Type::Text:
      if (ui_->value_stack->currentWidget() == ui_->page_empty) {
        ui_->value_text->setText(""_L1);
      }
      else {
        ui_->value_text->setText(term.value_.toString());
      }
      break;

    case SmartPlaylistSearchTerm::Type::Number:
      ui_->value_number->setValue(term.value_.toInt());
      break;

    case SmartPlaylistSearchTerm::Type::Date:
      if (ui_->value_stack->currentWidget() == ui_->page_date_numeric) {
        ui_->value_date_numeric->setValue(term.value_.toInt());
        ui_->date_type->setCurrentIndex(ui_->date_type->findData(QVariant::fromValue(term.datetype_)));
      }
      else if (ui_->value_stack->currentWidget() == ui_->page_date_relative) {
        ui_->value_date_numeric1->setValue(term.value_.toInt());
        ui_->value_date_numeric2->setValue(term.second_value_.toInt());
        ui_->date_type_relative->setCurrentIndex(ui_->date_type_relative->findData(QVariant::fromValue(term.datetype_)));
      }
      else if (ui_->value_stack->currentWidget() == ui_->page_date) {
        ui_->value_date->setDateTime(QDateTime::fromSecsSinceEpoch(term.value_.toInt()));
      }
      break;

    case SmartPlaylistSearchTerm::Type::Time:
      ui_->value_time->setTime(QTime(0, 0).addSecs(term.value_.toInt()));
      break;

    case SmartPlaylistSearchTerm::Type::Rating:
      ui_->value_rating->set_rating(term.value_.toFloat());
      break;

    case SmartPlaylistSearchTerm::Type::Invalid:
      break;
  }

}

SmartPlaylistSearchTerm SmartPlaylistSearchTermWidget::Term() const {

  const SmartPlaylistSearchTerm::Field field = ui_->field->currentData().value<SmartPlaylistSearchTerm::Field>();
  const SmartPlaylistSearchTerm::Operator op = ui_->op->currentData().value<SmartPlaylistSearchTerm::Operator>();

  SmartPlaylistSearchTerm ret;
  ret.field_ = field;
  ret.operator_ = op;

  // The value depends on the data type
  const QWidget *value_page = ui_->value_stack->currentWidget();
  if (value_page == ui_->page_text) {
    ret.value_ = ui_->value_text->text();
  }
  else if (value_page == ui_->page_empty) {
    ret.value_ = ""_L1;
  }
  else if (value_page == ui_->page_number) {
    ret.value_ = ui_->value_number->value();
  }
  else if (value_page == ui_->page_date) {
    ret.value_ = ui_->value_date->dateTime().toSecsSinceEpoch();
  }
  else if (value_page == ui_->page_time) {
    ret.value_ = QTime(0, 0).secsTo(ui_->value_time->time());
  }
  else if (value_page == ui_->page_date_numeric) {
    ret.datetype_ = ui_->date_type->currentData().value<SmartPlaylistSearchTerm::DateType>();
    ret.value_ = ui_->value_date_numeric->value();
  }
  else if (value_page == ui_->page_date_relative) {
    ret.datetype_ = ui_->date_type_relative->currentData().value<SmartPlaylistSearchTerm::DateType>();
    ret.value_ = ui_->value_date_numeric1->value();
    ret.second_value_ = ui_->value_date_numeric2->value();
  }
  else if (value_page == ui_->page_rating) {
    ret.value_ = ui_->value_rating->rating();
  }

  return ret;

}

void SmartPlaylistSearchTermWidget::RelativeValueChanged() {

  // Don't check for validity when creating the widget
  if (!initialized_) {
    initialized_ = true;
    return;
  }
  // Explain the user why he can't proceed
  if (ui_->value_date_numeric1->value() >= ui_->value_date_numeric2->value()) {
    QMessageBox::warning(this, u"Strawberry"_s, tr("The second value must be greater than the first one!"));
  }
  // Emit the signal in any case, so the Next button will be disabled
  Q_EMIT Changed();

}
