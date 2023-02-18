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

#include <QWidget>
#include <QTimer>
#include <QIODevice>
#include <QFile>
#include <QMessageBox>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QPalette>
#include <QDateTime>
#include <QPropertyAnimation>
#include <QKeyEvent>
#include <QEnterEvent>

#include "utilities/colorutils.h"
#include "core/iconloader.h"
#include "playlist/playlist.h"
#include "playlist/playlistdelegates.h"
#include "smartplaylistsearchterm.h"
#include "smartplaylistsearchtermwidget.h"
#include "ui_smartplaylistsearchtermwidget.h"

// Exported by QtGui
void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0);

class SmartPlaylistSearchTermWidget::Overlay : public QWidget {  // clazy:exclude=missing-qobject-macro
 public:
  explicit Overlay(SmartPlaylistSearchTermWidget *parent);
  void Grab();
  void SetOpacity(const float opacity);
  float opacity() const { return opacity_; }

  static const int kSpacing;
  static const int kIconSize;

 protected:
  void paintEvent(QPaintEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void keyReleaseEvent(QKeyEvent *e) override;

 private:
  SmartPlaylistSearchTermWidget *parent_;

  float opacity_;
  QString text_;
  QPixmap pixmap_;
  QPixmap icon_;

};

const int SmartPlaylistSearchTermWidget::Overlay::kSpacing = 6;
const int SmartPlaylistSearchTermWidget::Overlay::kIconSize = 22;

SmartPlaylistSearchTermWidget::SmartPlaylistSearchTermWidget(CollectionBackend *collection, QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_SmartPlaylistSearchTermWidget),
      collection_(collection),
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
    ui_->field->addItem(SmartPlaylistSearchTerm::FieldName(static_cast<SmartPlaylistSearchTerm::Field>(i)));
    ui_->field->setItemData(i, i);
  }
  ui_->field->model()->sort(0);

  // Populate the date type combo box
  for (int i = 0; i < 5; ++i) {
    ui_->date_type->addItem(SmartPlaylistSearchTerm::DateName(static_cast<SmartPlaylistSearchTerm::DateType>(i), false));
    ui_->date_type->setItemData(i, i);

    ui_->date_type_relative->addItem(SmartPlaylistSearchTerm::DateName(static_cast<SmartPlaylistSearchTerm::DateType>(i), false));
    ui_->date_type_relative->setItemData(i, i);
  }

  // Icons on the buttons
  ui_->remove->setIcon(IconLoader::Load("list-remove"));

  // Set stylesheet
  QFile stylesheet_file(":/style/smartplaylistsearchterm.css");
  if (stylesheet_file.open(QIODevice::ReadOnly)) {
    QString stylesheet = QString::fromLatin1(stylesheet_file.readAll());
    stylesheet_file.close();
    const QColor base(222, 97, 97, 128);
    stylesheet.replace("%light2", Utilities::ColorToRgba(base.lighter(140)));
    stylesheet.replace("%light", Utilities::ColorToRgba(base.lighter(120)));
    stylesheet.replace("%dark", Utilities::ColorToRgba(base.darker(120)));
    stylesheet.replace("%base", Utilities::ColorToRgba(base));
    setStyleSheet(stylesheet);
  }

}

SmartPlaylistSearchTermWidget::~SmartPlaylistSearchTermWidget() { delete ui_; }

void SmartPlaylistSearchTermWidget::FieldChanged(int index) {

  SmartPlaylistSearchTerm::Field field = static_cast<SmartPlaylistSearchTerm::Field>(ui_->field->itemData(index).toInt());
  SmartPlaylistSearchTerm::Type type = SmartPlaylistSearchTerm::TypeOf(field);

  // Populate the operator combo box
  if (type != current_field_type_) {
    ui_->op->clear();
    for (SmartPlaylistSearchTerm::Operator op : SmartPlaylistSearchTerm::OperatorsForType(type)) {
      const int i = ui_->op->count();
      ui_->op->addItem(SmartPlaylistSearchTerm::OperatorText(type, op));
      ui_->op->setItemData(i, QVariant::fromValue(op));
    }
    current_field_type_ = type;
  }

  // Show the correct value editor
  QWidget *page = nullptr;
  SmartPlaylistSearchTerm::Operator op = static_cast<SmartPlaylistSearchTerm::Operator>(ui_->op->itemData(ui_->op->currentIndex()).toInt());
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
      new TagCompleter(collection_, Playlist::Column_Artist, ui_->value_text);
      break;

    case SmartPlaylistSearchTerm::Field::Album:
      new TagCompleter(collection_, Playlist::Column_Album, ui_->value_text);
      break;

    default:
      ui_->value_text->setCompleter(nullptr);
  }

  emit Changed();

}

void SmartPlaylistSearchTermWidget::OpChanged(int idx) {

  Q_UNUSED(idx);

  // Determine the currently selected operator
  SmartPlaylistSearchTerm::Operator op = static_cast<SmartPlaylistSearchTerm::Operator>(
    // This uses the operatorss index in the combobox to get its enum value
    ui_->op->itemData(ui_->op->currentIndex()).toInt());

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

  emit Changed();

}

void SmartPlaylistSearchTermWidget::SetActive(bool active) {

  active_ = active;

  if (overlay_) {
    delete overlay_;
    overlay_ = nullptr;
  }

  ui_->container->setEnabled(active);

  if (!active) {
    overlay_ = new Overlay(this);
  }

}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void SmartPlaylistSearchTermWidget::enterEvent(QEnterEvent*) {
#else
void SmartPlaylistSearchTermWidget::enterEvent(QEvent*) {
#endif

  if (!overlay_ || !isEnabled()) return;

  animation_->stop();
  animation_->setEndValue(1.0);
  animation_->setDuration(80);
  animation_->start();

}

void SmartPlaylistSearchTermWidget::leaveEvent(QEvent*) {

  if (!overlay_) return;

  animation_->stop();
  animation_->setEndValue(0.0);
  animation_->setDuration(160);
  animation_->start();

}

void SmartPlaylistSearchTermWidget::resizeEvent(QResizeEvent *e) {

  QWidget::resizeEvent(e);
  if (overlay_ && overlay_->isVisible()) {
    QTimer::singleShot(0, this, &SmartPlaylistSearchTermWidget::Grab);
  }

}

void SmartPlaylistSearchTermWidget::showEvent(QShowEvent *e) {

  QWidget::showEvent(e);
  if (overlay_) {
    QTimer::singleShot(0, this, &SmartPlaylistSearchTermWidget::Grab);
  }

}

void SmartPlaylistSearchTermWidget::Grab() { overlay_->Grab(); }

void SmartPlaylistSearchTermWidget::set_overlay_opacity(float opacity) {
  if (overlay_) overlay_->SetOpacity(opacity);
}

float SmartPlaylistSearchTermWidget::overlay_opacity() const {
  return overlay_ ? overlay_->opacity() : static_cast<float>(0.0);
}

void SmartPlaylistSearchTermWidget::SetTerm(const SmartPlaylistSearchTerm &term) {

  ui_->field->setCurrentIndex(ui_->field->findData(static_cast<int>(term.field_)));
  ui_->op->setCurrentIndex(ui_->op->findData(static_cast<int>(term.operator_)));

  // The value depends on the data type
  switch (SmartPlaylistSearchTerm::TypeOf(term.field_)) {
    case SmartPlaylistSearchTerm::Type::Text:
      if (ui_->value_stack->currentWidget() == ui_->page_empty) {
        ui_->value_text->setText("");
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
        ui_->date_type->setCurrentIndex(static_cast<int>(term.datetype_));
      }
      else if (ui_->value_stack->currentWidget() == ui_->page_date_relative) {
        ui_->value_date_numeric1->setValue(term.value_.toInt());
        ui_->value_date_numeric2->setValue(term.second_value_.toInt());
        ui_->date_type_relative->setCurrentIndex(static_cast<int>(term.datetype_));
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

  const int field = ui_->field->itemData(ui_->field->currentIndex()).toInt();
  const int op = ui_->op->itemData(ui_->op->currentIndex()).toInt();

  SmartPlaylistSearchTerm ret;
  ret.field_ = static_cast<SmartPlaylistSearchTerm::Field>(field);
  ret.operator_ = static_cast<SmartPlaylistSearchTerm::Operator>(op);

  // The value depends on the data type
  const QWidget *value_page = ui_->value_stack->currentWidget();
  if (value_page == ui_->page_text) {
    ret.value_ = ui_->value_text->text();
  }
  else if (value_page == ui_->page_empty) {
    ret.value_ = "";
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
    ret.datetype_ = static_cast<SmartPlaylistSearchTerm::DateType>(ui_->date_type->currentIndex());
    ret.value_ = ui_->value_date_numeric->value();
  }
  else if (value_page == ui_->page_date_relative) {
    ret.datetype_ = static_cast<SmartPlaylistSearchTerm::DateType>(ui_->date_type_relative->currentIndex());
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
    QMessageBox::warning(this, "Strawberry", tr("The second value must be greater than the first one!"));
  }
  // Emit the signal in any case, so the Next button will be disabled
  emit Changed();

}

SmartPlaylistSearchTermWidget::Overlay::Overlay(SmartPlaylistSearchTermWidget *parent)
    : QWidget(parent),
      parent_(parent),
      opacity_(0.0),
      text_(tr("Add search term")),
      icon_(IconLoader::Load("list-add").pixmap(kIconSize)) {

  raise();
  setFocusPolicy(Qt::TabFocus);

}

void SmartPlaylistSearchTermWidget::Overlay::SetOpacity(const float opacity) {

  opacity_ = opacity;
  update();

}

void SmartPlaylistSearchTermWidget::Overlay::Grab() {

  hide();

  // Take a "screenshot" of the window
  QPixmap pixmap = parent_->grab();
  QImage image = pixmap.toImage();

  // Blur it
  QImage blurred(image.size(), QImage::Format_ARGB32_Premultiplied);
  blurred.fill(Qt::transparent);

  QPainter blur_painter(&blurred);
  qt_blurImage(&blur_painter, image, 10.0, true, false);
  blur_painter.end();

  pixmap_ = QPixmap::fromImage(blurred);

  resize(parent_->size());
  show();
  update();

}

void SmartPlaylistSearchTermWidget::Overlay::paintEvent(QPaintEvent*) {

  QPainter p(this);

  // Background
  p.fillRect(rect(), palette().window());

  // Blurred parent widget
  p.setOpacity(0.25 + opacity_ * 0.25);
  p.drawPixmap(0, 0, pixmap_);

  // Draw a frame
  p.setOpacity(1.0);
  p.setPen(palette().color(QPalette::Mid));
  p.setRenderHint(QPainter::Antialiasing);
  p.drawRoundedRect(rect(), 5, 5);

  // Geometry

#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
  const QSize contents_size(kIconSize + kSpacing + fontMetrics().horizontalAdvance(text_), qMax(kIconSize, fontMetrics().height()));
#else
  const QSize contents_size(kIconSize + kSpacing + fontMetrics().width(text_), qMax(kIconSize, fontMetrics().height()));
#endif

  const QRect contents(QPoint((width() - contents_size.width()) / 2, (height() - contents_size.height()) / 2), contents_size);
  const QRect icon(contents.topLeft(), QSize(kIconSize, kIconSize));
  const QRect text(icon.right() + kSpacing, icon.top(), contents.width() - kSpacing - kIconSize, contents.height());

  // Icon and text
  p.setPen(palette().color(QPalette::Text));
  p.drawPixmap(icon, icon_);
  p.drawText(text, Qt::TextDontClip | Qt::AlignVCenter, text_);  // NOLINT(bugprone-suspicious-enum-usage)

}

void SmartPlaylistSearchTermWidget::Overlay::mouseReleaseEvent(QMouseEvent*) {
  emit parent_->Clicked();
}

void SmartPlaylistSearchTermWidget::Overlay::keyReleaseEvent(QKeyEvent *e) {
  if (e->key() == Qt::Key_Space) emit parent_->Clicked();
}
