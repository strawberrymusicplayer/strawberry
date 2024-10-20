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

#include <QtGlobal>

#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <utility>
#include <memory>
#include <chrono>

#ifndef _MSC_VER
#  include <cxxabi.h>
#endif

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#endif
#include <glib.h>
#ifdef __clang__
#  pragma clang diagnostic pop
#endif

#ifdef HAVE_BACKTRACE
#  include <execinfo.h>
#endif

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QDateTime>
#include <QIODevice>
#include <QBuffer>
#include <QtMessageHandler>
#include <QMessageLogContext>
#include <QDebug>

#include "logging.h"

using namespace Qt::Literals::StringLiterals;

namespace logging {

static Level sDefaultLevel = Level_Debug;
static QMap<QString, Level> *sClassLevels = nullptr;
static QIODevice *sNullDevice = nullptr;

const char *kDefaultLogLevels = "*:3";

static constexpr char kMessageHandlerMagic[] = "__logging_message__";
static const size_t kMessageHandlerMagicLen = strlen(kMessageHandlerMagic);
static QtMessageHandler sOriginalMessageHandler = nullptr;

template<class T>
static T CreateLogger(Level level, const QString &class_name, int line, const char *category);

void GLog(const char *domain, int level, const char *message, void*) {

  switch (level) {
    case G_LOG_FLAG_RECURSION:
    case G_LOG_FLAG_FATAL:
    case G_LOG_LEVEL_ERROR:
    case G_LOG_LEVEL_CRITICAL:
      qLogCat(Error, domain) << message;
      break;
    case G_LOG_LEVEL_WARNING:
      qLogCat(Warning, domain) << message;
      break;
    case G_LOG_LEVEL_MESSAGE:
    case G_LOG_LEVEL_INFO:
      qLogCat(Info, domain) << message;
      break;
    case G_LOG_LEVEL_DEBUG:
    default:
      qLogCat(Debug, domain) << message;
      break;
  }

}

template<class T>
class DebugBase : public QDebug {
 public:
  DebugBase() : QDebug(sNullDevice) {}
  explicit DebugBase(QtMsgType t) : QDebug(t) {}
  T &space() { return static_cast<T&>(QDebug::space()); }
  T &nospace() { return static_cast<T&>(QDebug::nospace()); }
};

// Debug message will be stored in a buffer.
class BufferedDebug : public DebugBase<BufferedDebug> {
 public:
  BufferedDebug() = default;
  explicit BufferedDebug(QtMsgType msg_type) : buf_(new QBuffer, later_deleter) {

    Q_UNUSED(msg_type)

    buf_->open(QIODevice::WriteOnly);

    // QDebug doesn't have a method to set a new io device, but swap() allows the devices to be swapped between two instances.
    QDebug other(buf_.get());
    swap(other);
  }

  // Delete function for the buffer. Since a base class is holding a reference to the raw pointer,
  // it shouldn't be deleted until after the deletion of this object is complete.
  static void later_deleter(QBuffer *b) { b->deleteLater(); }

  std::shared_ptr<QBuffer> buf_;
};

// Debug message will be logged immediately.
class LoggedDebug : public DebugBase<LoggedDebug> {
 public:
  LoggedDebug() = default;
  explicit LoggedDebug(QtMsgType t) : DebugBase(t) { nospace() << kMessageHandlerMagic; }
};

static void MessageHandler(QtMsgType type, const QMessageLogContext &message_log_context, const QString &message) {

  Q_UNUSED(message_log_context)

  if (message.startsWith(QLatin1String(kMessageHandlerMagic))) {
    QByteArray message_data = message.toUtf8();
    fprintf(type == QtCriticalMsg || type == QtFatalMsg ? stderr : stdout, "%s\n", message_data.constData() + kMessageHandlerMagicLen);
    fflush(type == QtCriticalMsg || type == QtFatalMsg ? stderr : stdout);
    return;
  }

  Level level = Level_Debug;
  switch (type) {
    case QtFatalMsg:
    case QtCriticalMsg:
      level = Level_Error;
      break;
    case QtWarningMsg:
      level = Level_Warning;
      break;
    case QtDebugMsg:
    default:
      level = Level_Debug;
      break;
  }

  const QStringList lines = message.split(u'\n');
  for (const QString &line : lines) {
    BufferedDebug d = CreateLogger<BufferedDebug>(level, u"unknown"_s, -1, nullptr);
    d << line.toLocal8Bit().constData();
    if (d.buf_) {
      d.buf_->close();
      fprintf(type == QtCriticalMsg || type == QtFatalMsg ? stderr : stdout, "%s\n", d.buf_->buffer().constData());
      fflush(type == QtCriticalMsg || type == QtFatalMsg ? stderr : stdout);
    }
  }

  if (type == QtFatalMsg) {
    abort();
  }

}

void Init() {

  delete sClassLevels;
  delete sNullDevice;

  sClassLevels = new QMap<QString, Level>();
  sNullDevice = new NullDevice;
  sNullDevice->open(QIODevice::ReadWrite);

  // Catch other messages from Qt
  if (!sOriginalMessageHandler) {
    sOriginalMessageHandler = qInstallMessageHandler(MessageHandler);
  }

}

void SetLevels(const QString &levels) {

  if (!sClassLevels) return;

  const QStringList items = levels.split(u',');
  for (const QString &item : items) {
    const QStringList class_level = item.split(u':');

    QString class_name;
    bool ok = false;
    int level = Level_Error;

    if (class_level.count() == 1) {
      level = class_level.last().toInt(&ok);
    }
    else if (class_level.count() == 2) {
      class_name = class_level.first();
      level = class_level.last().toInt(&ok);
    }

    if (!ok || level < Level_Error || level > Level_Debug) {
      continue;
    }

    if (class_name.isEmpty() || class_name == u'*') {
      sDefaultLevel = static_cast<Level>(level);
    }
    else {
      sClassLevels->insert(class_name, static_cast<Level>(level));
    }
  }

}

static QString ParsePrettyFunction(const char *pretty_function) {

  // Get the class name out of the function name.
  QString class_name = QLatin1String(pretty_function);
  const qint64 paren = class_name.indexOf(u'(');
  if (paren != -1) {
    const qint64 colons = class_name.lastIndexOf("::"_L1, paren);
    if (colons != -1) {
      class_name = class_name.left(colons);
    }
    else {
      class_name = class_name.left(paren);
    }
  }

  const qint64 space = class_name.lastIndexOf(u' ');
  if (space != -1) {
    class_name = class_name.mid(space + 1);
  }

  return class_name;

}

template <class T>
static T CreateLogger(Level level, const QString &class_name, int line, const char *category) {

  // Map the level to a string
  const char *level_name = nullptr;
  switch (level) {
    case Level_Debug:   level_name = " DEBUG "; break;
    case Level_Info:    level_name = " INFO  "; break;
    case Level_Warning: level_name = " WARN  "; break;
    case Level_Error:   level_name = " ERROR "; break;
    case Level_Fatal:   level_name = " FATAL "; break;
  }

  QString filter_category = (category != nullptr) ? QLatin1String(category) : class_name;
  // Check the settings to see if we're meant to show or hide this message.
  Level threshold_level = sDefaultLevel;
  if (sClassLevels && sClassLevels->contains(filter_category)) {
    threshold_level = sClassLevels->value(filter_category);
  }

  if (level > threshold_level) {
    return T();
  }

  QString function_line = class_name;
  if (line != -1) {
    function_line += QLatin1Char(':') + QString::number(line);
  }
  if (category) {
    function_line += QLatin1Char('(') + QLatin1String(category) + QLatin1Char(')');
  }

  QtMsgType type = QtDebugMsg;
  if (level == Level_Fatal) {
    type = QtFatalMsg;
  }

  T ret(type);
  ret.nospace() << QDateTime::currentDateTime().toString(u"hh:mm:ss.zzz"_s).toLatin1().constData() << level_name << function_line.leftJustified(32).toLatin1().constData();

  return ret.space();

}

#ifdef Q_OS_UNIX
QString CXXDemangle(const QString &mangled_function);
QString CXXDemangle(const QString &mangled_function) {

  int status = 0;
  char *demangled_function = abi::__cxa_demangle(mangled_function.toLatin1().constData(), nullptr, nullptr, &status);
  if (status == 0) {
    QString ret = QString::fromLatin1(demangled_function);
    free(demangled_function);
    return ret;
  }
  return mangled_function;  // Probably not a C++ function.

}
#endif  // Q_OS_UNIX

#ifdef Q_OS_LINUX
QString LinuxDemangle(const QString &symbol);
QString LinuxDemangle(const QString &symbol) {

  static const QRegularExpression regex_symbol(u"\\(([^+]+)"_s);
  QRegularExpressionMatch match = regex_symbol.match(symbol);
  if (!match.hasMatch()) {
    return symbol;
  }
  QString mangled_function = match.captured(1);
  return CXXDemangle(mangled_function);

}
#endif  // Q_OS_LINUX

#ifdef Q_OS_MACOS
QString DarwinDemangle(const QString &symbol);
QString DarwinDemangle(const QString &symbol) {

  const QStringList split = symbol.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  QString mangled_function = split[3];
  return CXXDemangle(mangled_function);

}
#endif  // Q_OS_MACOS

QString DemangleSymbol(const QString &symbol);
QString DemangleSymbol(const QString &symbol) {

#ifdef Q_OS_MACOS
  return DarwinDemangle(symbol);
#elif defined(Q_OS_LINUX)
  return LinuxDemangle(symbol);
#else
  return symbol;
#endif

}

void DumpStackTrace() {

#ifdef HAVE_BACKTRACE
  void *callstack[128];
  int callstack_size = backtrace(reinterpret_cast<void**>(&callstack), sizeof(callstack));
  char **symbols = backtrace_symbols(reinterpret_cast<void**>(&callstack), callstack_size);
  // Start from 1 to skip ourself.
  for (int i = 1; i < callstack_size; ++i) {
    std::cerr << DemangleSymbol(QString::fromLatin1(symbols[i])).toStdString() << std::endl;
  }
  free(symbols);
#else
  qLog(Debug) << "FIXME: Implement printing stack traces on this platform";
#endif

}

// These are the functions that create loggers for the rest of Strawberry.
// It's okay that the LoggedDebug instance is copied to a QDebug in these. It doesn't override any behavior that should be needed after return.
#define qCreateLogger(line, pretty_function, category, level) logging::CreateLogger<LoggedDebug>(logging::Level_##level, logging::ParsePrettyFunction(pretty_function), line, category)

QDebug CreateLoggerFatal(const int line, const char *pretty_function, const char *category) { return qCreateLogger(line, pretty_function, category, Fatal); }
QDebug CreateLoggerError(const int line, const char *pretty_function, const char *category) { return qCreateLogger(line, pretty_function, category, Error); }

#ifdef QT_NO_INFO_OUTPUT
QNoDebug CreateLoggerInfo(const int line, const char *pretty_function, const char *category) {

  Q_UNUSED(line)
  Q_UNUSED(pretty_function)
  Q_UNUSED(category)

  return QNoDebug();

}
#else
QDebug CreateLoggerInfo(const int line, const char *pretty_function, const char *category) { return qCreateLogger(line, pretty_function, category, Info); }
#endif // QT_NO_INFO_OUTPUT

#ifdef QT_NO_WARNING_OUTPUT
QNoDebug CreateLoggerWarning(const int line, const char *pretty_function, const char *category) {

  Q_UNUSED(line)
  Q_UNUSED(pretty_function)
  Q_UNUSED(category)

  return QNoDebug();

}
#else
QDebug CreateLoggerWarning(const int line, const char *pretty_function, const char *category) { return qCreateLogger(line, pretty_function, category, Warning); }
#endif // QT_NO_WARNING_OUTPUT

#ifdef QT_NO_DEBUG_OUTPUT
QNoDebug CreateLoggerDebug(const int line, const char *pretty_function, const char *category) {

  Q_UNUSED(line)
  Q_UNUSED(pretty_function)
  Q_UNUSED(category)

  return QNoDebug();

}
#else
QDebug CreateLoggerDebug(const int line, const char *pretty_function, const char *category) { return qCreateLogger(line, pretty_function, category, Debug); }
#endif // QT_NO_DEBUG_OUTPUT

}  // namespace logging

namespace {

template<typename T>
QString print_duration(T duration, const std::string &unit) {
  return QStringLiteral("%1%2").arg(duration.count()).arg(QString::fromStdString(unit));
}

}  // namespace

QDebug operator<<(QDebug dbg, std::chrono::seconds secs) {
  dbg.nospace() << print_duration(secs, "s");
  return dbg.space();
}
