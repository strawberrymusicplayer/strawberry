// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NASPAGE_ANDROID_H
#define NASPAGE_ANDROID_H

#include <functional>
#include <QObject>
#include <QStringList>
#include "core/nasfileengine.h"

class SettingsPage;

class NasPage : public QObject {
 public:
  NasPage(SettingsPage *page, std::function<void(const Nas::Location &)> add_path);
  void Load();
  void Open();
  bool Commit(const QStringList &paths, QString *error);

 private:
  void Edit(const Nas::Location &location);
  SettingsPage *page_;
  std::function<void(const Nas::Location &)> add_path_;
  QList<Nas::Location> pending_;
  QString load_error_;
};

#endif
