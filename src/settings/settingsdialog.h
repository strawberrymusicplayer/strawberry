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
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QStyleOption>
#include <QMap>
#include <QSize>
#include <QString>

#include "core/shared_ptr.h"
#include "engine/enginebase.h"
#include "osd/osdbase.h"

class QMainWindow;
class QWidget;
class QModelIndex;
class QPainter;
class QTreeWidgetItem;
class QComboBox;
class QScrollArea;
class QAbstractButton;
class QShowEvent;
class QCloseEvent;

class Player;
class SCollection;
class CollectionBackend;
class CollectionModel;
class CollectionDirectoryModel;
class CoverProviders;
class LyricsProviders;
class AudioScrobbler;
class StreamingServices;
class GlobalShortcutsManager;
class SettingsPage;

class Ui_SettingsDialog;

class SettingsItemDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  explicit SettingsItemDelegate(QObject *parent);
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &idx) const override;
  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const override;
};


class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(SharedPtr<Player> player,
                          SharedPtr<DeviceFinders> device_finders,
                          SharedPtr<SCollection> collection,
                          SharedPtr<CoverProviders> cover_providers,
                          SharedPtr<LyricsProviders> lyrics_providers,
                          SharedPtr<AudioScrobbler> scrobbler,
                          SharedPtr<StreamingServices> streaming_services,
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
    Transcoding,
    Proxy,
    Appearance,
    Context,
    Notifications,
    GlobalShortcuts,
    Moodbar,
    Subsonic,
    Tidal,
    Qobuz,
    Spotify,
  };

  enum Role {
    Role_IsSeparator = Qt::UserRole
  };

#ifdef HAVE_GLOBALSHORTCUTS
  void SetGlobalShortcutManager(GlobalShortcutsManager *global_shortcuts_manager) { global_shortcuts_manager_ = global_shortcuts_manager; }
#endif

  bool is_loading_settings() const { return loading_settings_; }

  SharedPtr<Player> player() const { return player_; }
  SharedPtr<EngineBase> engine() const { return engine_; }
  SharedPtr<DeviceFinders> device_finders() const { return device_finders_; }
  SharedPtr<SCollection> collection() const { return collection_; }
  SharedPtr<CoverProviders> cover_providers() const { return cover_providers_; }
  SharedPtr<LyricsProviders> lyrics_providers() const { return lyrics_providers_; }
  SharedPtr<AudioScrobbler> scrobbler() const { return scrobbler_; }
  SharedPtr<StreamingServices> streaming_services() const { return streaming_services_; }
  OSDBase *osd() const { return osd_; }

#ifdef HAVE_GLOBALSHORTCUTS
  GlobalShortcutsManager *global_shortcuts_manager() const { return global_shortcuts_manager_; }
#endif

  void OpenAtPage(Page page);

 protected:
  void showEvent(QShowEvent *e) override;
  void closeEvent(QCloseEvent*) override;

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
  void NotificationPreview(const OSDBase::Behaviour, const QString&, const QString&);

 private Q_SLOTS:
  void CurrentItemChanged(QTreeWidgetItem *item);
  void DialogButtonClicked(QAbstractButton *button);

 private:
  static const char *kSettingsGroup;

  SharedPtr<Player> player_;
  SharedPtr<EngineBase> engine_;
  SharedPtr<DeviceFinders> device_finders_;
  SharedPtr<SCollection> collection_;
  SharedPtr<CoverProviders> cover_providers_;
  SharedPtr<LyricsProviders> lyrics_providers_;
  SharedPtr<AudioScrobbler> scrobbler_;
  SharedPtr<StreamingServices> streaming_services_;
#ifdef HAVE_GLOBALSHORTCUTS
  GlobalShortcutsManager *global_shortcuts_manager_;
#endif
  OSDBase *osd_;
  QMainWindow *mainwindow_;

  Ui_SettingsDialog *ui_;
  bool loading_settings_;

  QMap<Page, PageData> pages_;
};

#endif  // SETTINGSDIALOG_H
