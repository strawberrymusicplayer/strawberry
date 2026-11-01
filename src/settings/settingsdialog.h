/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "config.h"

#include <QObject>
#include <QDialog>
#include <QMap>
#include <QString>

#include "includes/shared_ptr.h"
#include "constants/notificationssettings.h"
#include "osd/osdbase.h"

class QMainWindow;
class QWidget;
class QTreeWidgetItem;
class QComboBox;
class QScrollArea;
class QStackedWidget;
class QAbstractButton;
class QShowEvent;
class QCloseEvent;
class QKeyEvent;

class Player;
class AudioDeviceListers;
class CollectionManager;
class CoverProviders;
class LyricsProviders;
class AudioScrobbler;
class StreamingServices;
class Appearance;
class GlobalShortcutsManager;
class SettingsPage;

class Ui_SettingsDialog;

class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(const SharedPtr<Player> player,
                          const SharedPtr<AudioDeviceListers> audio_device_listers,
                          const SharedPtr<CollectionManager> collection,
                          const SharedPtr<CoverProviders> cover_providers,
                          const SharedPtr<LyricsProviders> lyrics_providers,
                          const SharedPtr<AudioScrobbler> scrobbler,
                          const SharedPtr<StreamingServices> streaming_services,
                          const SharedPtr<Appearance> appearance,
#ifdef HAVE_GLOBALSHORTCUTS
                          GlobalShortcutsManager *global_shortcuts_manager,
#endif
                          OSDBase *osd,
                          QMainWindow *mainwindow,
                          QWidget *parent = nullptr);
  ~SettingsDialog() override;

  enum class Page {
    Behaviour,
    Collection,
    Backend,
    Playback,
    Playlist,
    Scrobbler,
    Covers,
    Lyrics,
    Acoustid,
    Transcoding,
    Proxy,
    Appearance,
    Context,
    Notifications,
    GlobalShortcuts,
    Moodbar,
    Waveform,
    Subsonic,
    Tidal,
    Qobuz,
    Spotify,
    Plex,
    Jellyfin,
    Radio,
  };

  enum Role {
    Role_IsSeparator = Qt::UserRole
  };

  bool is_loading_settings() const { return loading_settings_; }

  void OpenAtPage(const Page page);
#ifdef Q_OS_ANDROID
  void OpenOverview();
  void AddAndroidHelpCategory(const QList<QAction*> &actions);
  void OpenAndroidSubpage(QWidget *content, const QString &title);
  void CloseAndroidSubpage();
  bool IsAndroidPageActive(const SettingsPage *page) const;
#endif

 protected:
#ifdef Q_OS_ANDROID
  bool event(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
#endif
  void showEvent(QShowEvent *e) override;
  void closeEvent(QCloseEvent *e) override;

 private:
  struct PageData {
    PageData() : item_(nullptr), scroll_area_(nullptr), page_(nullptr) {}
    QTreeWidgetItem *item_;
    QScrollArea *scroll_area_;
    SettingsPage *page_;
  };

  // QDialog
  void accept() override;
  void reject() override;

  void LoadGeometry();
  void SaveGeometry();

  QTreeWidgetItem *AddCategory(const QString &name);
  void AddPage(const Page id, SettingsPage *page, QTreeWidgetItem *parent = nullptr);

  void Apply();
  void Save();

 Q_SIGNALS:
  void ReloadSettings();
  void NotificationPreview(const OSDSettings::Type, const QString&, const QString&);

 private Q_SLOTS:
  void CurrentItemChanged(QTreeWidgetItem *item);
  void DialogButtonClicked(QAbstractButton *button);

 private:
  QMainWindow *mainwindow_;
  Ui_SettingsDialog *ui_;
  bool loading_settings_;
  QMap<Page, PageData> pages_;
#ifdef Q_OS_ANDROID
  void SetAndroidNavigationPage(QWidget *page);
  void AndroidBack();
  void SetAndroidTitle(const QString &category = QString(), const QString &page = QString(), const QString &subpage = QString());
  QStackedWidget *android_navigation_;
  QWidget *android_categories_page_;
  QMap<Page, QWidget*> android_category_pages_;
  QWidget *android_current_category_page_;
  struct AndroidSubpage {
    QScrollArea *area;
    QWidget *content_parent;
    QScrollArea *parent_area;
    int parent_scroll_value;
    QString title;
  };
  QList<AndroidSubpage> android_subpages_;
#endif
};

#endif  // SETTINGSDIALOG_H
