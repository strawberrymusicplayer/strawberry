/*
 * Strawberry Music Player
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

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>

#include "playlisttitleheader.h"

#ifdef Q_OS_ANDROID
#  include "androidwidgets.h"
#else
#  include "core/iconloader.h"
#endif

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kHeaderHeight = 56;
constexpr int kTitleMinimumHeight = 48;
}  // namespace

PlaylistTitleHeader::PlaylistTitleHeader(QWidget *parent)
    : QWidget(parent),
      title_(new QStackedWidget(this)),
      name_button_(new QPushButton(title_)),
      name_editor_(new QLineEdit(title_)),
#ifdef Q_OS_ANDROID
      back_button_(AndroidWidgets::CreateBackButton(this)),
#else
      back_button_(new QPushButton(IconLoader::Load(u"go-previous"_s), tr("Back"), this)),
#endif
      playlist_id_(-1),
      editing_playlist_id_(-1) {

  setFixedHeight(kHeaderHeight);

  title_->setFixedHeight(kHeaderHeight);
  title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

  name_button_->setMinimumHeight(kTitleMinimumHeight);
  name_button_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  name_button_->setAutoDefault(false);
  name_button_->setFlat(true);
  name_button_->setStyleSheet(u"text-align: left; padding-left: 8px;"_s);
  name_button_->setAccessibleName(tr("Rename playlist"));
  name_button_->setToolTip(tr("Rename playlist"));
#ifdef Q_OS_ANDROID
  AndroidWidgets::SetPageTitleFont(name_button_);
#endif

  name_editor_->setMinimumHeight(kTitleMinimumHeight);
  name_editor_->setFont(name_button_->font());

  title_->addWidget(name_button_);
  title_->addWidget(name_editor_);

  back_button_->setAccessibleName(tr("Back to playlists"));
  back_button_->setToolTip(tr("Back to playlists"));

  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(title_, 1);
  layout->addWidget(back_button_);

  QObject::connect(back_button_, &QPushButton::clicked, this, &PlaylistTitleHeader::BackClicked);
  QObject::connect(name_button_, &QPushButton::clicked, this, &PlaylistTitleHeader::StartRename);
  QObject::connect(name_editor_, &QLineEdit::returnPressed, this, &PlaylistTitleHeader::SaveRename);
  QObject::connect(name_editor_, &QLineEdit::editingFinished, this, &PlaylistTitleHeader::EditingFinished);

}

void PlaylistTitleHeader::SetPlaylist(const int id, const QString &name) {

  if (is_renaming() && id != editing_playlist_id_) CancelRename();

  playlist_id_ = id;
  playlist_name_ = id >= 0 ? name : QString();
  name_button_->setText(playlist_name_);

}

void PlaylistTitleHeader::StartRename() {

  if (playlist_id_ < 0) return;

  editing_playlist_id_ = playlist_id_;
  name_editor_->setText(playlist_name_);
  title_->setCurrentWidget(name_editor_);
  name_editor_->selectAll();
  name_editor_->setFocus(Qt::OtherFocusReason);

}

void PlaylistTitleHeader::CancelRename() {

  editing_playlist_id_ = -1;
  title_->setCurrentWidget(name_button_);
  name_button_->setFocus(Qt::OtherFocusReason);

}

void PlaylistTitleHeader::SaveRename() {

  if (!is_renaming()) return;

  const int playlist_id = editing_playlist_id_;
  const QString new_name = name_editor_->text().trimmed();
  CancelRename();
  if (playlist_id == playlist_id_ && !new_name.isEmpty() && new_name != playlist_name_) {
    Q_EMIT RenameRequested(playlist_id, new_name);
  }

}

void PlaylistTitleHeader::EditingFinished() {

  // Leaving the editor without confirming discards the new name.
  if (is_renaming()) CancelRename();

}
