/*
 * Strawberry Music Player
 * Copyright 2026, Malte Zilinski <malte@zilinski.eu>
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

#ifndef RADIOBROWSERSEARCHVIEW_H
#define RADIOBROWSERSEARCHVIEW_H

#include <QWidget>
#include <QPair>
#include <QString>

#include "radiochannel.h"

class QTimer;
class QMimeData;
class QMenu;
class QAction;
class QShowEvent;
class QContextMenuEvent;

class RadioBrowserSearchModel;
class RadioBrowserService;

class Ui_RadioBrowserSearchView;

class RadioBrowserSearchView : public QWidget {
  Q_OBJECT

 public:
  explicit RadioBrowserSearchView(QWidget *parent = nullptr);
  ~RadioBrowserSearchView();

  void Init(RadioBrowserService *service);

 protected:
  void showEvent(QShowEvent *e) override;

 Q_SIGNALS:
  void AddToPlaylist(QMimeData *mimedata);

 private Q_SLOTS:
  void TextChanged(const QString &text);
  void SearchTriggered();
  void SearchFinished(const RadioChannelList &channels, const bool has_more);
  void SearchError(const QString &error);
  void LoadMore();
  void CountryChanged(const int index);
  void SortChanged(const int index);
  void CountriesLoaded(const QList<QPair<QString, QString>> &countries);
  void AddSelectedToPlaylist();
  void ItemDoubleClicked(const QModelIndex &index);
  void ShowContextMenu(const QPoint &pos);

 private:
  void DoSearch();

  Ui_RadioBrowserSearchView *ui_;
  RadioBrowserService *service_;
  RadioBrowserSearchModel *model_;
  QTimer *search_timer_;
  QMenu *context_menu_;
  QAction *action_add_to_playlist_;

  QString default_country_;
  int current_offset_;
  int search_limit_;
  bool has_more_;
  bool initialized_;
};

#endif  // RADIOBROWSERSEARCHVIEW_H
