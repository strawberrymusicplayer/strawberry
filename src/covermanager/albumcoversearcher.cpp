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

#include <QWidget>
#include <QDialog>
#include <QStandardItemModel>
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QStandardItem>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include <QFont>
#include <QFontMetrics>
#include <QColor>
#include <QRect>
#include <QSize>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QKeySequence>
#include <QtEvents>

#include "utilities/strutils.h"
#include "utilities/mimeutils.h"
#include "widgets/busyindicator.h"
#include "widgets/forcescrollperpixel.h"
#include "widgets/groupediconview.h"
#include "widgets/searchfield.h"
#include "albumcoversearcher.h"
#include "albumcoverfetcher.h"
#include "albumcoverloader.h"
#include "albumcoverloaderresult.h"
#include "albumcoverimageresult.h"
#include "ui_albumcoversearcher.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kMargin = 4;
constexpr int kPaddingX = 3;
constexpr int kPaddingY = 1;
constexpr qreal kBorder = 5.0;
constexpr qreal kFontPointSize = 7.5;
constexpr int kBorderAlpha = 200;
constexpr int kBackgroundAlpha = 175;
}  // namespace

SizeOverlayDelegate::SizeOverlayDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void SizeOverlayDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &idx) const {

  QStyledItemDelegate::paint(painter, option, idx);

  if (!idx.data(AlbumCoverSearcher::Role_ImageFetchFinished).toBool()) {
    return;
  }

  const QSize size = idx.data(AlbumCoverSearcher::Role_ImageSize).toSize();
  const QString text = Utilities::PrettySize(size);

  QFont font(option.font);
  font.setPointSizeF(kFontPointSize);
  font.setBold(true);

  const QFontMetrics metrics(font);

  const int text_width = metrics.horizontalAdvance(text);

  const QRect icon_rect(option.rect.left(), option.rect.top(), option.rect.width(), option.rect.width());

  const QRect background_rect(icon_rect.right() - kMargin - text_width - kPaddingX * 2, icon_rect.bottom() - kMargin - metrics.height() - kPaddingY * 2, text_width + kPaddingX * 2, metrics.height() + kPaddingY * 2);
  const QRect text_rect(background_rect.left() + kPaddingX, background_rect.top() + kPaddingY, text_width, metrics.height());

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setPen(QColor(0, 0, 0, kBorderAlpha));
  painter->setBrush(QColor(0, 0, 0, kBackgroundAlpha));
  painter->drawRoundedRect(background_rect, kBorder, kBorder);

  painter->setPen(Qt::white);
  painter->setFont(font);
  painter->drawText(text_rect, text);
  painter->restore();

}

AlbumCoverSearcher::AlbumCoverSearcher(const QIcon &no_cover_icon, const SharedPtr<AlbumCoverLoader> albumcover_loader, QWidget *parent)
    : QDialog(parent),
      ui_(new Ui_AlbumCoverSearcher),
      albumcover_loader_(albumcover_loader),
      model_(new QStandardItemModel(this)),
      no_cover_icon_(no_cover_icon),
      fetcher_(nullptr),
      id_(0) {

  setWindowModality(Qt::WindowModal);
  ui_->setupUi(this);
  ui_->busy->hide();

  ui_->covers->set_header_text(tr("Covers from %1"));
  ui_->covers->AddSortSpec(Role_ImageDimensions, Qt::DescendingOrder);
  ui_->covers->setItemDelegate(new SizeOverlayDelegate(this));
  ui_->covers->setModel(model_);

  QObject::connect(&*albumcover_loader_, &AlbumCoverLoader::AlbumCoverLoaded, this, &AlbumCoverSearcher::AlbumCoverLoaded);
  QObject::connect(ui_->search, &QPushButton::clicked, this, &AlbumCoverSearcher::Search);
  QObject::connect(ui_->covers, &GroupedIconView::doubleClicked, this, &AlbumCoverSearcher::CoverDoubleClicked);

  new ForceScrollPerPixel(ui_->covers, this);

  ui_->buttonBox->button(QDialogButtonBox::Cancel)->setShortcut(QKeySequence::Close);

}

AlbumCoverSearcher::~AlbumCoverSearcher() {
  delete ui_;
}

void AlbumCoverSearcher::Init(AlbumCoverFetcher *fetcher) {

  fetcher_ = fetcher;
  QObject::connect(fetcher_, &AlbumCoverFetcher::SearchFinished, this, &AlbumCoverSearcher::SearchFinished, Qt::QueuedConnection);

}

AlbumCoverImageResult AlbumCoverSearcher::Exec(const QString &artist, const QString &album) {

  ui_->artist->setText(artist);
  ui_->album->setText(album);
  ui_->artist->setFocus();

  if (!artist.isEmpty() || !album.isEmpty()) {
    Search();
  }

  if (exec() == QDialog::Rejected) return AlbumCoverImageResult();

  QModelIndex selected = ui_->covers->currentIndex();
  if (!selected.isValid() || !selected.data(Role_ImageFetchFinished).toBool())
    return AlbumCoverImageResult();

  AlbumCoverImageResult result;
  result.image_data = selected.data(Role_ImageData).toByteArray();
  result.image = selected.data(Role_Image).value<QImage>();
  result.mime_type = Utilities::MimeTypeFromData(result.image_data);

  return result;

}

void AlbumCoverSearcher::Search() {

  model_->clear();
  cover_loading_tasks_.clear();

  if (ui_->album->isEnabled()) {
    id_ = fetcher_->SearchForCovers(ui_->artist->text(), ui_->album->text());
    ui_->search->setText(tr("Abort"));
    ui_->busy->show();
    ui_->artist->setEnabled(false);
    ui_->album->setEnabled(false);
    ui_->covers->setEnabled(false);
  }
  else {
    fetcher_->Clear();
    ui_->search->setText(tr("Search"));
    ui_->busy->hide();
    ui_->search->setEnabled(true);
    ui_->artist->setEnabled(true);
    ui_->album->setEnabled(true);
    ui_->covers->setEnabled(true);
  }

}

void AlbumCoverSearcher::SearchFinished(const quint64 id, const CoverProviderSearchResults &results) {

  if (id != id_) return;

  ui_->search->setEnabled(true);
  ui_->artist->setEnabled(true);
  ui_->album->setEnabled(true);
  ui_->covers->setEnabled(true);
  ui_->search->setText(tr("Search"));
  id_ = 0;

  for (const CoverProviderSearchResult &result : results) {

    if (result.image_url.isEmpty()) continue;

    AlbumCoverLoaderOptions cover_options(AlbumCoverLoaderOptions::Option::RawImageData | AlbumCoverLoaderOptions::Option::OriginalImage | AlbumCoverLoaderOptions::Option::ScaledImage | AlbumCoverLoaderOptions::Option::PadScaledImage);
    cover_options.desired_scaled_size = ui_->covers->iconSize(), ui_->covers->iconSize();
    quint64 new_id = albumcover_loader_->LoadImageAsync(cover_options, false, result.image_url, QUrl(), false);

    QStandardItem *item = new QStandardItem;
    item->setIcon(no_cover_icon_);
    item->setText(result.artist + u" - "_s + result.album);
    item->setData(result.image_url, Role_ImageURL);
    item->setData(new_id, Role_ImageRequestId);
    item->setData(false, Role_ImageFetchFinished);
    item->setData(QVariant(Qt::AlignTop | Qt::AlignHCenter), Qt::TextAlignmentRole);
    item->setData(result.provider, GroupedIconView::Role_Group);

    model_->appendRow(item);

    cover_loading_tasks_[new_id] = item;
  }

  if (cover_loading_tasks_.isEmpty()) ui_->busy->hide();

}

void AlbumCoverSearcher::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result) {

  if (!cover_loading_tasks_.contains(id)) return;
  QStandardItem *item = cover_loading_tasks_.take(id);

  if (cover_loading_tasks_.isEmpty()) ui_->busy->hide();

  if (!result.success || result.album_cover.image_data.isNull() || result.album_cover.image.isNull() || result.image_scaled.isNull()) {
    model_->removeRow(item->row());
    return;
  }

  const QPixmap pixmap = QPixmap::fromImage(result.image_scaled);
  if (pixmap.isNull()) {
    model_->removeRow(item->row());
    return;
  }

  const QIcon icon(pixmap);

  item->setData(true, Role_ImageFetchFinished);
  item->setData(result.album_cover.image_data, Role_ImageData);
  item->setData(result.album_cover.image, Role_Image);
  item->setData(result.album_cover.image.width() * result.album_cover.image.height(), Role_ImageDimensions);
  item->setData(result.album_cover.image.size(), Role_ImageSize);
  if (!icon.isNull()) item->setIcon(icon);

}

void AlbumCoverSearcher::keyPressEvent(QKeyEvent *e) {

  if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return) {
    e->ignore();
    return;
  }

  QDialog::keyPressEvent(e);

}

void AlbumCoverSearcher::CoverDoubleClicked(const QModelIndex &idx) {
  if (idx.isValid()) accept();
}
