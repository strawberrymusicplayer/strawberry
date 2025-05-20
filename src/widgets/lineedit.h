/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QString>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QCheckBox>

#include "ratingwidget.h"

class QToolButton;
class QPaintDevice;
class QPaintEvent;
class QResizeEvent;

class LineEditInterface {

 public:
  explicit LineEditInterface(QWidget *widget) : widget_(widget) {}

  QWidget *widget() const { return widget_; }

  virtual ~LineEditInterface() {}

  virtual void set_enabled(const bool enabled) = 0;
  virtual void set_font(const QFont &font) = 0;
  virtual void set_focus() = 0;

  virtual void clear() = 0;
  virtual QVariant value() const = 0;
  virtual void set_value(const QVariant &value) = 0;

  virtual QString hint() const = 0;
  virtual void set_hint(const QString &hint) = 0;
  virtual void clear_hint() = 0;

  virtual void set_partially() {}

 protected:
  QWidget *widget_;

 private:
  Q_DISABLE_COPY(LineEditInterface)
};

class ExtendedEditor : public LineEditInterface {

 public:
  explicit ExtendedEditor(QWidget *widget, int extra_right_padding = 0, bool draw_hint = true);
  ~ExtendedEditor() override {}

  virtual bool is_empty() const { return value().toString().isEmpty(); }

  QString hint() const override { return hint_; }
  void set_hint(const QString &hint) override;
  void clear_hint() override { set_hint(QString()); }

  bool has_clear_button() const { return has_clear_button_; }
  void set_clear_button(const bool visible);

  bool has_reset_button() const;
  void set_reset_button(const bool visible);

  qreal font_point_size() const { return font_point_size_; }
  void set_font_point_size(const qreal size) { font_point_size_ = size; }

 protected:
  void Paint(QPaintDevice *device);
  virtual void Resize();

 private:
  void UpdateButtonGeometry();

 protected:
  QString hint_;

  bool has_clear_button_;
  QToolButton *clear_button_;
  QToolButton *reset_button_;

  int extra_right_padding_;
  bool draw_hint_;
  qreal font_point_size_;
  bool is_rtl_;
};

class LineEdit : public QLineEdit, public ExtendedEditor {
  Q_OBJECT

  Q_PROPERTY(QString hint READ hint WRITE set_hint)
  Q_PROPERTY(qreal font_point_size READ font_point_size WRITE set_font_point_size)
  Q_PROPERTY(bool has_clear_button READ has_clear_button WRITE set_clear_button)
  Q_PROPERTY(bool has_reset_button READ has_reset_button WRITE set_reset_button)

 public:
  explicit LineEdit(QWidget *parent = nullptr);

  // ExtendedEditor
  void set_enabled(bool enabled) override { QLineEdit::setEnabled(enabled); }
  void set_font(const QFont &font) override { QLineEdit::setFont(font); }

  QVariant value() const override { return QLineEdit::text(); }
  void set_value(const QVariant &value) override { QLineEdit::setText(value.toString()); }

 public Q_SLOTS:
  void set_focus() override { QLineEdit::setFocus(); }
  void clear() override { QLineEdit::clear(); }

 protected:
  void paintEvent(QPaintEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

 private:
  bool is_rtl() const { return is_rtl_; }
  void set_rtl(bool rtl) { is_rtl_ = rtl; }

 private Q_SLOTS:
  void text_changed(const QString &text);

 Q_SIGNALS:
  void Reset();
};

class TextEdit : public QPlainTextEdit, public ExtendedEditor {
  Q_OBJECT
  Q_PROPERTY(QString hint READ hint WRITE set_hint)
  Q_PROPERTY(bool has_clear_button READ has_clear_button WRITE set_clear_button)
  Q_PROPERTY(bool has_reset_button READ has_reset_button WRITE set_reset_button)

 public:
  explicit TextEdit(QWidget *parent = nullptr);

  // ExtendedEditor
  void set_enabled(bool enabled) override { QPlainTextEdit::setEnabled(enabled); }
  void set_font(const QFont &font) override { QPlainTextEdit::setFont(font); }

  QVariant value() const override { return QPlainTextEdit::toPlainText(); }
  void set_value(const QVariant &value) override { QPlainTextEdit::setPlainText(value.toString()); }

 public Q_SLOTS:
  void set_focus() override { QPlainTextEdit::setFocus(); }
  void clear() override { QPlainTextEdit::clear(); }

 protected:
  void paintEvent(QPaintEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

 Q_SIGNALS:
  void Reset();
};

class SpinBox : public QSpinBox, public ExtendedEditor {
  Q_OBJECT
  Q_PROPERTY(QString hint READ hint WRITE set_hint)
  Q_PROPERTY(bool has_clear_button READ has_clear_button WRITE set_clear_button)
  Q_PROPERTY(bool has_reset_button READ has_reset_button WRITE set_reset_button)

 public:
  explicit SpinBox(QWidget *parent = nullptr);

  // QSpinBox
  QString textFromValue(int val) const override;

  // ExtendedEditor
  void set_enabled(bool enabled) override { QSpinBox::setEnabled(enabled); }
  void set_font(const QFont &font) override { QSpinBox::setFont(font); }

  QVariant value() const override { return QSpinBox::value(); }
  void set_value(const QVariant &value) override { QSpinBox::setValue(value.toInt()); }
  bool is_empty() const override { return text().isEmpty() || text() == QStringLiteral("0"); }

 public Q_SLOTS:
  void set_focus() override { QSpinBox::setFocus(); }
  void clear() override { QSpinBox::clear(); }

 protected:
  void paintEvent(QPaintEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

 Q_SIGNALS:
  void Reset();
};

class CheckBox : public QCheckBox, public ExtendedEditor {
  Q_OBJECT
  Q_PROPERTY(QString hint READ hint WRITE set_hint)
  Q_PROPERTY(bool has_clear_button READ has_clear_button WRITE set_clear_button)
  Q_PROPERTY(bool has_reset_button READ has_reset_button WRITE set_reset_button)

 public:
  explicit CheckBox(QWidget *parent = nullptr);

  // ExtendedEditor
  void set_enabled(bool enabled) override { QCheckBox::setEnabled(enabled); }
  void set_font(const QFont &font) override { QCheckBox::setFont(font); }

  bool is_empty() const override { return text().isEmpty() || text() == QStringLiteral("0"); }
  QVariant value() const override { return QCheckBox::isChecked(); }
  void set_value(const QVariant &value) override { QCheckBox::setCheckState(value.toBool() ? Qt::Checked : Qt::Unchecked); }
  void set_partially() override { QCheckBox::setCheckState(Qt::PartiallyChecked); }

 protected:
  void Resize() override;

 public Q_SLOTS:
  void set_focus() override { QCheckBox::setFocus(); }
  void clear() override { QCheckBox::setChecked(false); }

 protected:
  void paintEvent(QPaintEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

 Q_SIGNALS:
  void Reset();
};

class RatingBox : public RatingWidget, public ExtendedEditor {
  Q_OBJECT

  Q_PROPERTY(QString hint READ hint WRITE set_hint)

 public:
  explicit RatingBox(QWidget *parent = nullptr);

  void set_enabled(bool enabled) override { RatingWidget::setEnabled(enabled); }
  void set_font(const QFont &font) override { RatingWidget::setFont(font); }

  QVariant value() const override { return RatingWidget::rating(); }
  void set_value(const QVariant &value) override { RatingWidget::set_rating(value.toFloat()); }

  void set_partially() override { RatingWidget::set_rating(0.0F); }

 protected:
  void Resize() override;

 public Q_SLOTS:
  void set_focus() override { RatingWidget::setFocus(); }
  void clear() override {}

 protected:
  void paintEvent(QPaintEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;

 Q_SIGNALS:
  void Reset();

};

#endif  // LINEEDIT_H
