/* This file is part of Strawberry.
   Copyright 2011, David Sansome <me@davidsansome.com>
   Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef LOGGING_H
#define LOGGING_H

#include <chrono>

#include <QtGlobal>
#include <QIODevice>
#include <QString>
#include <QDebug>

#ifdef QT_NO_DEBUG_STREAM
#  define qLog(level) while (false) QNoDebug()
#  define qLogCat(level, category) while (false) QNoDebug()
#else
#  ifdef _MSC_VER
#    define qLog(level) logging::CreateLogger##level(__LINE__, __FUNCSIG__, nullptr)
#  else
#    define qLog(level) logging::CreateLogger##level(__LINE__, __PRETTY_FUNCTION__, nullptr)
#  endif  // _MSC_VER

// This macro specifies a separate category for message filtering.
// The default qLog will use the class name extracted from the function name for this purpose.
// The category is also printed in the message along with the class name.
#  ifdef _MSC_VER
#    define qLogCat(level, category) logging::CreateLogger##level(__LINE__, __FUNCSIG__, category)
#  else
#    define qLogCat(level, category) logging::CreateLogger##level(__LINE__, __PRETTY_FUNCTION__, category)
#  endif  // _MSC_VER

#endif  // QT_NO_DEBUG_STREAM

namespace logging {

class NullDevice : public QIODevice {
  Q_OBJECT

 public:
  NullDevice(QObject *parent = nullptr) : QIODevice(parent) {}

 protected:
  qint64 readData(char*, qint64) override { return -1; }
  qint64 writeData(const char*, qint64 len) override { return len; }
};

enum Level {
  Level_Fatal = -1,
  Level_Error = 0,
  Level_Warning,
  Level_Info,
  Level_Debug,
};

  void Init();
  void SetLevels(const QString &levels);

  void DumpStackTrace();

QDebug CreateLoggerFatal(const int line, const char *pretty_function, const char *category);
QDebug CreateLoggerError(const int line, const char *pretty_function, const char *category);

#ifdef QT_NO_INFO_OUTPUT
QNoDebug CreateLoggerInfo(const int line, const char *pretty_function, const char *category);
#else
QDebug CreateLoggerInfo(const int line, const char *pretty_function, const char *category);
#endif // QT_NO_INFO_OUTPUT

#ifdef QT_NO_WARNING_OUTPUT
QNoDebug CreateLoggerWarning(const int line, const char *pretty_function, const char *category);
#else
QDebug CreateLoggerWarning(const int line, const char *pretty_function, const char *category);
#endif // QT_NO_WARNING_OUTPUT

#ifdef QT_NO_DEBUG_OUTPUT
QNoDebug CreateLoggerDebug(const int line, const char *pretty_function, const char *category);
#else
QDebug CreateLoggerDebug(const int line, const char *pretty_function, const char *category);
#endif  // QT_NO_DEBUG_OUTPUT

void GLog(const char *domain, int level, const char *message, void *user_data);

extern const char *kDefaultLogLevels;

}  // namespace logging

QDebug operator<<(QDebug dbg, std::chrono::seconds secs);

#endif  // LOGGING_H
