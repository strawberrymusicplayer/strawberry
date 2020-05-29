/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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
#include "version.h"

#include <QtGlobal>

#include <glib.h>
#include <stdlib.h>
#include <memory>
#include <time.h>

#ifdef Q_OS_UNIX
#  include <unistd.h>
#endif

#ifdef Q_OS_MACOS
#  include <sys/resource.h>
#  include <sys/sysctl.h>
#endif

#ifdef Q_OS_WIN32
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
  #endif
  #include <windows.h>
  #include <iostream>
#endif  // Q_OS_WIN32

#include <QObject>
#include <QApplication>
#include <QCoreApplication>
#include <QSysInfo>
#include <QStandardPaths>
#include <QLibraryInfo>
#include <QFileDevice>
#include <QIODevice>
#include <QByteArray>
#include <QNetworkProxy>
#include <QFile>
#include <QDir>
#include <QString>
#include <QImage>
#include <QSettings>
#include <QLoggingCategory>
#include <QtDebug>
#ifdef HAVE_DBUS
#  include <QDBusArgument>
#endif
#ifdef HAVE_TRANSLATIONS
#  include <QTranslator>
#endif

#include "main.h"

#include "core/logging.h"

#include <singleapplication.h>
#include <singlecoreapplication.h>

#ifdef HAVE_DBUS
#  include "core/mpris.h"
#endif
#include "core/utilities.h"
#include "core/metatypes.h"
#include "core/iconloader.h"
#include "core/mainwindow.h"
#include "core/commandlineoptions.h"
#include "core/systemtrayicon.h"
#include "core/application.h"
#include "core/networkproxyfactory.h"
#include "core/scangiomodulepath.h"
#ifdef HAVE_TRANSLATIONS
#  include "core/translations.h"
#endif
#include "settings/behavioursettingspage.h"

#include "widgets/osd.h"

#ifdef HAVE_DBUS
  QDBusArgument &operator<<(QDBusArgument &arg, const QImage &image);
  const QDBusArgument &operator>>(const QDBusArgument &arg, QImage &image);
#endif

int main(int argc, char* argv[]) {

#ifdef Q_OS_MACOS
  // Do Mac specific startup to get media keys working.
  // This must go before QApplication initialisation.
  mac::MacMain();
#endif

#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  QCoreApplication::setApplicationName("Strawberry");
  QCoreApplication::setOrganizationName("Strawberry");
#else
  QCoreApplication::setApplicationName("strawberry");
  QCoreApplication::setOrganizationName("strawberry");
#endif
  QCoreApplication::setApplicationVersion(STRAWBERRY_VERSION_DISPLAY);
  QCoreApplication::setOrganizationDomain("strawberrymusicplayer.org");

  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

  // This makes us show up nicely in gnome-volume-control
  g_set_application_name(QCoreApplication::applicationName().toLocal8Bit());

  RegisterMetaTypes();

  // Initialise logging.  Log levels are set after the commandline options are parsed below.
  logging::Init();
  g_log_set_default_handler(reinterpret_cast<GLogFunc>(&logging::GLog), nullptr);

  CommandlineOptions options(argc, argv);
  {
    // Only start a core application now so we can check if there's another instance without requiring an X server.
    // This MUST be done before parsing the commandline options so QTextCodec gets the right system locale for filenames.
    SingleCoreApplication core_app(argc, argv, true, SingleCoreApplication::Mode::User | SingleCoreApplication::Mode::ExcludeAppVersion | SingleCoreApplication::Mode::ExcludeAppPath);
    // Parse commandline options - need to do this before starting the full QApplication so it works without an X server
    if (!options.Parse()) return 1;
    logging::SetLevels(options.log_levels());
    if (core_app.isSecondary()) {
      if (options.is_empty()) {
        qLog(Info) << "Strawberry is already running - activating existing window (1)";
      }
      core_app.sendMessage(options.Serialize(), 5000);
      return 0;
    }
  }

#ifdef Q_OS_MACOS
  // Must happen after QCoreApplication::setOrganizationName().
  setenv("XDG_CONFIG_HOME", QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation).toLocal8Bit().constData(), 1);
#endif

  // Output the version, so when people attach log output to bug reports they don't have to tell us which version they're using.
  qLog(Info) << "Strawberry" << STRAWBERRY_VERSION_DISPLAY;
  qLog(Info) << QString("%1 %2 - (%3 %4) [%5]").arg(QSysInfo::prettyProductName()).arg(QSysInfo::productVersion()).arg(QSysInfo::kernelType()).arg(QSysInfo::kernelVersion()).arg(QSysInfo::currentCpuArchitecture());

  // Seed the random number generators.
  time_t t = time(nullptr);
  srand(t);
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
  qsrand(t);
#endif

  Utilities::IncreaseFDLimit();

  // important: Do not remove this.
  // This must also be done as a SingleApplication, in case SingleCoreApplication was compiled with a different appdata.
  SingleApplication a(argc, argv, true, SingleApplication::Mode::User | SingleApplication::Mode::ExcludeAppVersion | SingleApplication::Mode::ExcludeAppPath);
  if (a.isSecondary()) {
    if (options.is_empty()) {
      qLog(Info) << "Strawberry is already running - activating existing window (2)";
    }
    a.sendMessage(options.Serialize(), 5000);
    return 0;
  }

#if defined(USE_BUNDLE) && (defined(Q_OS_LINUX) || defined(Q_OS_MACOS))
  qLog(Debug) << "Looking for resources in" << QCoreApplication::applicationDirPath() + "/" + USE_BUNDLE_DIR;
  QCoreApplication::setLibraryPaths(QStringList() << QCoreApplication::applicationDirPath() + "/" + USE_BUNDLE_DIR);
#endif

#ifndef Q_OS_MACOS
  // Gnome on Ubuntu has menu icons disabled by default.  I think that's a bad idea, and makes some menus in Strawberry look confusing.
  QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);
#else
  QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, true);
  // Fixes focus issue with NSSearchField, see QTBUG-11401
  QCoreApplication::setAttribute(Qt::AA_NativeWindows, true);
#endif

  // Set the permissions on the config file on Unix - it can contain passwords for internet services so it's important that other users can't read it.
  // On Windows these are stored in the registry instead.
#ifdef Q_OS_UNIX
  {
    QSettings s;

    // Create the file if it doesn't exist already
    if (!QFile::exists(s.fileName())) {
      QFile file(s.fileName());
      file.open(QIODevice::WriteOnly);
    }

    // Set -rw-------
    QFile::setPermissions(s.fileName(), QFile::ReadOwner | QFile::WriteOwner);
  }
#endif

  // Resources
  Q_INIT_RESOURCE(data);
  Q_INIT_RESOURCE(icons);
#ifdef HAVE_TRANSLATIONS
  Q_INIT_RESOURCE(translations);
#endif

#ifdef DEBUG
  QLoggingCategory::defaultCategory()->setEnabled(QtDebugMsg, true);
#endif

  IconLoader::Init();

#ifdef HAVE_TRANSLATIONS
  QString override_language = options.language();
  if (override_language.isEmpty()) {
    QSettings s;
    s.beginGroup(BehaviourSettingsPage::kSettingsGroup);
    override_language = s.value("language").toString();
    s.endGroup();
  }

  QString system_language = QLocale::system().uiLanguages().empty() ? QLocale::system().name() : QLocale::system().uiLanguages().first();
  // uiLanguages returns strings with "-" as separators for language/region; however QTranslator needs "_" separators
  system_language.replace("-", "_");

  const QString language = override_language.isEmpty() ? system_language : override_language;

  std::unique_ptr<Translations> translations(new Translations);

  translations->LoadTranslation("qt", QLibraryInfo::location(QLibraryInfo::TranslationsPath), language);
  translations->LoadTranslation("strawberry", ":/translations", language);
  translations->LoadTranslation("strawberry", a.applicationDirPath(), language);
  translations->LoadTranslation("strawberry", QDir::currentPath(), language);
#endif

  Application app;

  // Network proxy
  QNetworkProxyFactory::setApplicationProxyFactory(NetworkProxyFactory::Instance());

  // Create the tray icon and OSD
  std::unique_ptr<SystemTrayIcon> tray_icon(SystemTrayIcon::CreateSystemTrayIcon());
  OSD osd(tray_icon.get(), &app);

#ifdef HAVE_DBUS
  mpris::Mpris mpris(&app);
#endif

  // Window
  MainWindow w(&app, tray_icon.get(), &osd, options);
#ifdef Q_OS_MACOS
  mac::EnableFullScreen(w);
#endif  // Q_OS_MACOS
#ifdef HAVE_GIO
  ScanGIOModulePath();
#endif
#ifdef HAVE_DBUS
  QObject::connect(&mpris, SIGNAL(RaiseMainWindow()), &w, SLOT(Raise()));
#endif
  QObject::connect(&a, SIGNAL(receivedMessage(quint32, QByteArray)), &w, SLOT(CommandlineOptionsReceived(quint32, QByteArray)));

  int ret = a.exec();

  return ret;
}
