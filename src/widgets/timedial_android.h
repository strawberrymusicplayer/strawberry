// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TIMEDIAL_ANDROID_H
#define TIMEDIAL_ANDROID_H

#include <QPointF>
#include <QSize>
#include <QWidget>

class QButtonGroup;
class QGridLayout;
class QLabel;
class QMouseEvent;
class QPaintEvent;
class QPushButton;
class QResizeEvent;

// A round clock face, like Android's time picker: tapping or dragging picks the hour, then the minute.
// Hours are 0 to 23; the 24-hour dial has an inner ring for 00 and 13 to 23, the 12-hour dial keeps the current AM or PM.
class TimeDial : public QWidget {
  Q_OBJECT

 public:
  enum class Mode { Hours, Minutes };

  explicit TimeDial(QWidget *parent = nullptr);

  void SetUse24Hour(const bool use_24_hour);
  void SetTime(const int hour, const int minute);
  void SetMode(const Mode mode);

  Mode mode() const { return mode_; }
  int hour() const { return hour_; }
  int minute() const { return minute_; }

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 Q_SIGNALS:
  void TimeChanged(const int hour, const int minute);
  // The finger was lifted after picking an hour.
  void HourPicked();

 protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

 private:
  struct Geometry {
    QPointF center;
    double radius;
    double outer_radius;
    double inner_radius;
    double number_radius;
  };

  Geometry DialGeometry() const;
  bool ShowsInnerRing() const;
  void Pick(const QPointF &position);

  Mode mode_;
  bool use_24_hour_;
  int hour_;
  int minute_;
  bool tracking_;
};

// The digital time with tappable hour and minute, the AM and PM buttons for the 12-hour clock, the dial and a Done button.
// Portrait places them below each other; landscape places the dial to the right.
class TimePicker : public QWidget {
  Q_OBJECT

 public:
  explicit TimePicker(const bool use_24_hour, QWidget *parent = nullptr);

  // Whether Android's date and time settings use the 24-hour format.
  static bool DeviceUses24HourFormat();

  void SetTime(const int hour, const int minute);
  int hour() const;
  int minute() const;

 Q_SIGNALS:
  void TimeChanged(const int hour, const int minute);
  void Done();

 protected:
  void resizeEvent(QResizeEvent *event) override;

 private:
  void ShowMode(const TimeDial::Mode mode);
  void UpdateTexts();
  void UpdateLayout();

  bool use_24_hour_;
  QGridLayout *layout_;
  QWidget *header_;
  TimeDial *dial_;
  QPushButton *hour_button_;
  QPushButton *minute_button_;
  QPushButton *am_button_;
  QPushButton *pm_button_;
  QPushButton *done_button_;
  QLabel *colon_;
  qreal base_point_size_;
  int landscape_;
};

#endif  // TIMEDIAL_ANDROID_H
