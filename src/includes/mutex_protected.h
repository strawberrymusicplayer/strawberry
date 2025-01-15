/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MUTEX_PROTECTED_H
#define MUTEX_PROTECTED_H

#include <boost/noncopyable.hpp>

#include <QMutex>
#include <QMutexLocker>

template<typename T>
class mutex_protected : public boost::noncopyable {
 public:
  mutex_protected(const mutex_protected &value) : value_(value.value()) {}
  mutex_protected(const T value) : value_(value) {}
  ~mutex_protected() {}

  T value() const {
    QMutexLocker l(&mutex_);
    return value_;
  }

  bool operator==(const mutex_protected &value) const {
    QMutexLocker l(&mutex_);
    return value.value() == value_;
  }

  bool operator==(const T value) const {
    QMutexLocker l(&mutex_);
    return value == value_;
  }

  bool operator!=(const mutex_protected &value) const {
    QMutexLocker l(&mutex_);
    return value.value() != value_;
  }

  bool operator!=(const T value) const {
    QMutexLocker l(&mutex_);
    return value != value_;
  }

  bool operator>(const mutex_protected &value) const {
    QMutexLocker l(&mutex_);
    return value_ > value.value();
  }

  bool operator>(const T value) const {
    QMutexLocker l(&mutex_);
    return value_ > value;
  }

  bool operator>=(const mutex_protected &value) const {
    QMutexLocker l(&mutex_);
    return value_ >= value.value();
  }

  bool operator>=(const T value) const {
    QMutexLocker l(&mutex_);
    return value_ >= value;
  }

  bool operator<(const mutex_protected &value) const {
    QMutexLocker l(&mutex_);
    return value_ < value.value();
  }

  bool operator<(const T value) const {
    QMutexLocker l(&mutex_);
    return value_ < value;
  }

  bool operator<=(const mutex_protected &value) const {
    QMutexLocker l(&mutex_);
    return value_ <= value.value();
  }

  bool operator<=(const T value) const {
    QMutexLocker l(&mutex_);
    return value_ <= value;
  }

  void operator=(const mutex_protected &value) {
    QMutexLocker l(&mutex_);
    value_ = value.value();
  }

  void operator=(const T value) {
    QMutexLocker l(&mutex_);
    value_ = value;
  }

  void operator++() {
    QMutexLocker l(&mutex_);
    ++value_;
  }

  void operator+=(const T value) {
    QMutexLocker l(&mutex_);
    value_ += value;
  }

  void operator+=(const mutex_protected value) {
    QMutexLocker l(&mutex_);
    value_ += value.value();
  }

  void operator--() {
    QMutexLocker l(&mutex_);
    --value_;
  }

  void operator-=(const T value) {
    QMutexLocker l(&mutex_);
    value_ -= value;
  }

  void operator-=(const mutex_protected value) {
    QMutexLocker l(&mutex_);
    value_ -= value.value();
  }

 private:
  T value_;
  mutable QMutex mutex_;
};

#endif  // MUTEX_PROTECTED_H
