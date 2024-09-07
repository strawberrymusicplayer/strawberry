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

#include "config.h"
#include "version.h"

#include <QtGlobal>

#include <cstdlib>
#include <ctime>
#include <memory>

#ifdef Q_OS_UNIX
#  include <unistd.h>
#endif

#ifdef Q_OS_MACOS
#  include <sys/resource.h>
#  include <sys/sysctl.h>
#endif

#ifdef Q_OS_WIN32
#  include <windows.h>
#  include <iostream>
#endif  // Q_OS_WIN32

#include <glib.h>

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
#include <QSettings>
#include <QLoggingCategory>
#ifdef HAVE_TRANSLATIONS
#  include <QTranslator>
#endif

#include "main.h"

#include "core/logging.h"

#include "core/scoped_ptr.h"
#include "core/shared_ptr.h"
#include "core/settings.h"

#include "utilities/envutils.h"

#include <kdsingleapplication.h>

#ifdef HAVE_QTSPARKLE
#  include <qtsparkle-qt6/Updater>
#endif  // HAVE_QTSPARKLE

#ifdef Q_OS_MACOS
#  include "utilities/macosutils.h"
#  include "core/mac_startup.h"
#endif

#ifdef HAVE_DBUS
#  include "core/mpris2.h"
#endif
#include "core/metatypes.h"
#include "core/iconloader.h"
#include "core/mainwindow.h"
#include "core/commandlineoptions.h"
#include "core/application.h"
#include "core/networkproxyfactory.h"
#ifdef Q_OS_MACOS
#  include "core/macsystemtrayicon.h"
#else
#  include "core/qtsystemtrayicon.h"
#endif
#ifdef HAVE_TRANSLATIONS
#  include "core/translations.h"
#endif
#include "settings/behavioursettingspage.h"
#include "settings/appearancesettingspage.h"

#if defined(Q_OS_MACOS)
#  include "osd/osdmac.h"
#elif defined(HAVE_DBUS)
#  include "osd/osddbus.h"
#else
#  include "osd/osdbase.h"
#endif

using namespace Qt::StringLiterals;
using std::make_shared;

int main(int argc, char *argv[]) {

#ifdef Q_OS_MACOS
  // Do Mac specific startup to get media keys working.
  // This must go before QApplication initialization.
  mac::MacMain();
#endif

#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  QCoreApplication::setApplicationName(QStringLiteral("Strawberry"));
  QCoreApplication::setOrganizationName(QStringLiteral("Strawberry"));
#else
  QCoreApplication::setApplicationName(QStringLiteral("strawberry"));
  QCoreApplication::setOrganizationName(QStringLiteral("strawberry"));
#endif
  QCoreApplication::setApplicationVersion(QStringLiteral(STRAWBERRY_VERSION_DISPLAY));
  QCoreApplication::setOrganizationDomain(QStringLiteral("strawberrymusicplayer.org"));

  // This makes us show up nicely in gnome-volume-control
  g_set_application_name("Strawberry");
  g_setenv("PULSE_PROP_application.icon_name", "strawberry", TRUE);
  g_setenv("PULSE_PROP_media.role", "music", TRUE);

  RegisterMetaTypes();

  // Initialize logging.  Log levels are set after the commandline options are parsed below.
  logging::Init();
  g_log_set_default_handler(reinterpret_cast<GLogFunc>(&logging::GLog), nullptr);

  CommandlineOptions options(argc, argv);
  {
    // Only start a core application now, so we can check if there's another instance without requiring an X server.
    // This MUST be done before parsing the commandline options so QTextCodec gets the right system locale for filenames.
    QCoreApplication core_app(argc, argv);
    KDSingleApplication single_app(QCoreApplication::applicationName(), KDSingleApplication::Option::IncludeUsernameInSocketName);
    // Parse commandline options - need to do this before starting the full QApplication, so it works without an X server
    if (!options.Parse()) return 1;
    logging::SetLevels(options.log_levels());
    if (!single_app.isPrimaryInstance()) {
      if (options.is_empty()) {
        qLog(Info) << "Strawberry is already running - activating existing window (1)";
      }
      if (!single_app.sendMessage(options.Serialize())) {
        qLog(Error) << "Could not send message to primary instance.";
      }
      return 0;
    }
  }

#ifdef Q_OS_MACOS
  // Must happen after QCoreApplication::setOrganizationName().
  Utilities::SetEnv("XDG_CONFIG_HOME", QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
#endif

  // Output the version, so when people attach log output to bug reports they don't have to tell us which version they're using.
  qLog(Info) << "Strawberry" << STRAWBERRY_VERSION_DISPLAY << "Qt" << QLibraryInfo::version().toString();
  qLog(Info) << QStringLiteral("%1 %2 - (%3 %4) [%5]").arg(QSysInfo::prettyProductName(), QSysInfo::productVersion(), QSysInfo::kernelType(), QSysInfo::kernelVersion(), QSysInfo::currentCpuArchitecture());

  // Seed the random number generators.
  time_t t = time(nullptr);
  srand(t);

#ifdef Q_OS_MACOS
  Utilities::IncreaseFDLimit();
#endif

  QGuiApplication::setApplicationDisplayName(QStringLiteral("Strawberry Music Player"));
  QGuiApplication::setDesktopFileName(QStringLiteral("org.strawberrymusicplayer.strawberry"));
  QGuiApplication::setQuitOnLastWindowClosed(false);

  QApplication a(argc, argv);
  KDSingleApplication single_app(QCoreApplication::applicationName(), KDSingleApplication::Option::IncludeUsernameInSocketName);
  if (!single_app.isPrimaryInstance()) {
    if (options.is_empty()) {
      qLog(Info) << "Strawberry is already running - activating existing window (2)";
    }
    if (!single_app.sendMessage(options.Serialize())) {
      qLog(Error) << "Could not send message to primary instance.";
    }
    return 0;
  }

  QThread::currentThread()->setObjectName(QStringLiteral("Main"));

  QGuiApplication::setWindowIcon(IconLoader::Load(QStringLiteral("strawberry")));

#if defined(USE_BUNDLE)
  qLog(Debug) << "Looking for resources in" << QCoreApplication::libraryPaths();
#endif

  // Gnome on Ubuntu has menu icons disabled by default.  I think that's a bad idea, and makes some menus in Strawberry look confusing.
  QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);

  {
    Settings s;
    s.beginGroup(AppearanceSettingsPage::kSettingsGroup);
    QString style = s.value(AppearanceSettingsPage::kStyle).toString();
    if (style.isEmpty()) {
      style = "default"_L1;
      s.setValue(AppearanceSettingsPage::kStyle, style);
    }
    s.endGroup();
    if (style != "default"_L1) {
      QApplication::setStyle(style);
    }
    if (QApplication::style()) qLog(Debug) << "Style:" << QApplication::style()->objectName();
  }

  // Set the permissions on the config file on Unix - it can contain passwords for streaming services, so it's important that other users can't read it.
  // On Windows these are stored in the registry instead.
#ifdef Q_OS_UNIX
  {
    Settings s;
    if (QFile::exists(s.fileName())) {
      if (!QFile::setPermissions(s.fileName(), QFile::ReadOwner | QFile::WriteOwner)) {
        qLog(Error) << "Could not set permissions for settingsfile" << s.fileName();
      }
    }
    else {
      qLog(Error) << "Missing settingsfile" << s.fileName();
    }
  }
#endif

  // Resources
  Q_INIT_RESOURCE(data);
  Q_INIT_RESOURCE(icons);
#if defined(HAVE_TRANSLATIONS) && !defined(INSTALL_TRANSLATIONS)
  Q_INIT_RESOURCE(translations);
#endif

  IconLoader::Init();

#ifdef HAVE_TRANSLATIONS
  QString override_language = options.language();
  if (override_language.isEmpty()) {
    Settings s;
    s.beginGroup(BehaviourSettingsPage::kSettingsGroup);
    override_language = s.value("language").toString();
    s.endGroup();
  }

  QString system_language = QLocale::system().uiLanguages().empty() ? QLocale::system().name() : QLocale::system().uiLanguages().first();
  // uiLanguages returns strings with "-" as separators for language/region; however QTranslator needs "_" separators
  system_language.replace(u'-', u'_');

  const QString language = override_language.isEmpty() ? system_language : override_language;

  ScopedPtr<Translations> translations(new Translations);

  translations->LoadTranslation(QStringLiteral("qt"), QLibraryInfo::path(QLibraryInfo::TranslationsPath), language);
  translations->LoadTranslation(QStringLiteral("strawberry"), QStringLiteral(":/translations"), language);
  translations->LoadTranslation(QStringLiteral("strawberry"), QStringLiteral(TRANSLATIONS_DIR), language);
  translations->LoadTranslation(QStringLiteral("strawberry"), QCoreApplication::applicationDirPath(), language);
  translations->LoadTranslation(QStringLiteral("strawberry"), QDir::currentPath(), language);

#  ifdef HAVE_QTSPARKLE
  //qtsparkle::LoadTranslations(language);
#  endif

#endif

  Application app;

  // Network proxy
  QNetworkProxyFactory::setApplicationProxyFactory(NetworkProxyFactory::Instance());

  // Create the tray icon and OSD
  SharedPtr<SystemTrayIcon> tray_icon = make_shared<SystemTrayIcon>();

#if defined(Q_OS_MACOS)
  OSDMac osd(tray_icon, &app);
#elif defined(HAVE_DBUS)
  OSDDBus osd(tray_icon, &app);
#else
  OSDBase osd(tray_icon, &app);
#endif

#ifdef HAVE_DBUS
  mpris::Mpris2 mpris2(&app);
#endif

  // Window
  MainWindow w(&app, tray_icon, &osd, options);

#ifdef Q_OS_MACOS
  mac::EnableFullScreen(w);
#endif  // Q_OS_MACOS

#ifdef HAVE_DBUS
  QObject::connect(&mpris2, &mpris::Mpris2::RaiseMainWindow, &w, &MainWindow::Raise);
#endif
  QObject::connect(&single_app, &KDSingleApplication::messageReceived, &w, QOverload<const QByteArray&>::of(&MainWindow::CommandlineOptionsReceived));

  int ret = QCoreApplication::exec();

  return ret;

}
