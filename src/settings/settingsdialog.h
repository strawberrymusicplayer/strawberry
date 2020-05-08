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
#include <QSettings>

#include "engine/engine_fwd.h"
#include "widgets/osd.h"

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

class Application;
class Player;
class Appearance;
class CollectionDirectoryModel;
class GlobalShortcuts;
class SettingsPage;

class Ui_SettingsDialog;


class SettingsItemDelegate : public QStyledItemDelegate {
 public:
  explicit SettingsItemDelegate(QObject *parent);
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};


class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(Application *app, QMainWindow *mainwindow, QWidget *parent = nullptr);
  ~SettingsDialog();

  enum Page {
    Page_Behaviour,
    Page_Collection,
    Page_Backend,
    Page_Playback,
    Page_Playlist,
    Page_Scrobbler,
    Page_Covers,
    Page_Lyrics,
    Page_Transcoding,
    Page_Proxy,
    Page_Appearance,
    Page_Context,
    Page_Notifications,
    Page_GlobalShortcuts,
    Page_Moodbar,
    Page_Subsonic,
    Page_Tidal,
  };

  enum Role {
    Role_IsSeparator = Qt::UserRole
  };

  void SetGlobalShortcutManager(GlobalShortcuts *manager) { manager_ = manager; }

  bool is_loading_settings() const { return loading_settings_; }

  Application *app() const { return app_; }
  Player *player() const { return player_; }
  EngineBase *engine() const { return engine_; }
  CollectionDirectoryModel *collection_directory_model() const { return model_; }
  GlobalShortcuts *global_shortcuts_manager() const { return manager_; }
  Appearance *appearance() const { return appearance_; }

  void OpenAtPage(Page page);

  void ComboBoxLoadFromSettings(const QSettings &s, QComboBox *combobox, const QString &setting, const QString &default_value);
  void ComboBoxLoadFromSettings(const QSettings &s, QComboBox *combobox, const QString &setting, const int default_value);

 protected:
  void showEvent(QShowEvent *e);
  void closeEvent(QCloseEvent*);

 private:
  struct PageData {
    QTreeWidgetItem *item_;
    QScrollArea *scroll_area_;
    SettingsPage *page_;
  };

  // QDialog
  void accept();
  void reject();

  void LoadGeometry();
  void SaveGeometry();

  QTreeWidgetItem *AddCategory(const QString &name);
  void AddPage(Page id, SettingsPage *page, QTreeWidgetItem *parent = nullptr);

  void Save();

 signals:
  void ReloadSettings();
  void NotificationPreview(OSD::Behaviour, QString, QString);

 private slots:
  void CurrentItemChanged(QTreeWidgetItem *item);
  void DialogButtonClicked(QAbstractButton *button);

 private:
  static const char *kSettingsGroup;

  QMainWindow *mainwindow_;
  Application *app_;
  Player *player_;
  EngineBase *engine_;
  CollectionDirectoryModel *model_;
  GlobalShortcuts *manager_;
  Appearance *appearance_;

  Ui_SettingsDialog *ui_;
  bool loading_settings_;

  QMap<Page, PageData> pages_;
};

#endif  // SETTINGSDIALOG_H
