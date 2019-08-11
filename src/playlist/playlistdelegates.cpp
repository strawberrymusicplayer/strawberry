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

#include <QtGlobal>
#include <QApplication>
#include <QObject>
#include <QWidget>
#include <QtConcurrentRun>
#include <QFuture>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QCompleter>
#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLocale>
#include <QMetaType>
#include <QVariant>
#include <QString>
#include <QStringBuilder>
#include <QUrl>
#include <QRegExp>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QLineEdit>
#include <QScrollBar>
#include <QToolTip>
#include <QTreeView>
#include <QWhatsThis>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QtDebug>
#include <QtEvents>
#include <QLinearGradient>

#include "core/closure.h"
#include "core/iconloader.h"
#include "core/logging.h"
#include "core/player.h"
#include "core/song.h"
#include "core/urlhandler.h"
#include "core/utilities.h"
#include "collection/collectionbackend.h"
#include "playlist/playlist.h"
#include "playlistdelegates.h"

#ifdef Q_OS_MACOS
#include "core/mac_utilities.h"
#endif  // Q_OS_MACOS

const int QueuedItemDelegate::kQueueBoxBorder = 1;
const int QueuedItemDelegate::kQueueBoxCornerRadius = 3;
const int QueuedItemDelegate::kQueueBoxLength = 30;
const QRgb QueuedItemDelegate::kQueueBoxGradientColor1 = qRgb(102, 150, 227);
const QRgb QueuedItemDelegate::kQueueBoxGradientColor2 = qRgb(77, 121, 200);
const int QueuedItemDelegate::kQueueOpacitySteps = 10;
const float QueuedItemDelegate::kQueueOpacityLowerBound = 0.4;

const int PlaylistDelegateBase::kMinHeight = 19;

QueuedItemDelegate::QueuedItemDelegate(QObject *parent, int indicator_column)
    : QStyledItemDelegate(parent), indicator_column_(indicator_column) {}

void QueuedItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {

  QStyledItemDelegate::paint(painter, option, index);

  if (index.column() == indicator_column_) {
    bool ok = false;
    const int queue_pos = index.data(Playlist::Role_QueuePosition).toInt(&ok);
    if (ok && queue_pos != -1) {
      float opacity = kQueueOpacitySteps - qMin(kQueueOpacitySteps, queue_pos);
      opacity /= kQueueOpacitySteps;
      opacity *= 1.0 - kQueueOpacityLowerBound;
      opacity += kQueueOpacityLowerBound;
      painter->setOpacity(opacity);

      DrawBox(painter, option.rect, option.font, QString::number(queue_pos + 1), kQueueBoxLength);

      painter->setOpacity(1.0);
    }
  }

}

void QueuedItemDelegate::DrawBox(QPainter *painter, const QRect &line_rect, const QFont &font, const QString &text, int width) const {

  QFont smaller = font;
  smaller.setPointSize(smaller.pointSize() - 1);
  smaller.setBold(true);

  if (width == -1) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    width = QFontMetrics(font).horizontalAdvance(text + "  ");
#else
    width = QFontMetrics(font).width(text + "  ");
#endif
  }

  QRect rect(line_rect);
  rect.setLeft(rect.right() - width - kQueueBoxBorder);
  rect.setWidth(width);
  rect.setTop(rect.top() + kQueueBoxBorder);
  rect.setBottom(rect.bottom() - kQueueBoxBorder - 1);

  QRect text_rect(rect);
  text_rect.setBottom(text_rect.bottom() + 1);

  QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
  gradient.setColorAt(0.0, kQueueBoxGradientColor1);
  gradient.setColorAt(1.0, kQueueBoxGradientColor2);

  // Turn on antialiasing
  painter->setRenderHint(QPainter::Antialiasing);

  // Draw the box
  painter->translate(0.5, 0.5);
  painter->setPen(QPen(Qt::white, 1));
  painter->setBrush(gradient);
  painter->drawRoundedRect(rect, kQueueBoxCornerRadius, kQueueBoxCornerRadius);

  // Draw the text
  painter->setFont(smaller);
  painter->drawText(rect, Qt::AlignCenter, text);
  painter->translate(-0.5, -0.5);

}

int QueuedItemDelegate::queue_indicator_size(const QModelIndex &index) const {

  if (index.column() == indicator_column_) {
    const int queue_pos = index.data(Playlist::Role_QueuePosition).toInt();
    if (queue_pos != -1) {
      return kQueueBoxLength + kQueueBoxBorder * 2;
    }
  }
  return 0;

}


PlaylistDelegateBase::PlaylistDelegateBase(QObject *parent, const QString &suffix)
    : QueuedItemDelegate(parent), view_(qobject_cast<QTreeView*>(parent)), suffix_(suffix)
{
}

QString PlaylistDelegateBase::displayText(const QVariant &value, const QLocale&) const {

  QString text;

  switch (static_cast<QMetaType::Type>(value.type())) {
    case QMetaType::Int: {
      int v = value.toInt();
      if (v > 0) text = QString::number(v);
      break;
    }

    case QMetaType::Float:
    case QMetaType::Double: {
      double v = value.toDouble();
      if (v > 0) text = QString::number(v);
      break;
    }

    default:
      text = value.toString();
      break;
  }

  if (!text.isNull() && !suffix_.isNull()) text += " " + suffix_;
  return text;

}

QSize PlaylistDelegateBase::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {

  QSize size = QueuedItemDelegate::sizeHint(option, index);
  if (size.height() < kMinHeight) size.setHeight(kMinHeight);
  return size;

}

void PlaylistDelegateBase::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {

  QueuedItemDelegate::paint(painter, Adjusted(option, index), index);

  // Stop after indicator
  if (index.column() == Playlist::Column_Title) {
    if (index.data(Playlist::Role_StopAfter).toBool()) {
      QRect rect(option.rect);
      rect.setRight(rect.right() - queue_indicator_size(index));

      DrawBox(painter, rect, option.font, tr("stop"));
    }
  }

}

QStyleOptionViewItem PlaylistDelegateBase::Adjusted(const QStyleOptionViewItem &option, const QModelIndex &index) const {

  if (!view_) return option;

  QPoint top_left(-view_->horizontalScrollBar()->value(), -view_->verticalScrollBar()->value());

  if (view_->header()->logicalIndexAt(top_left) != index.column())
    return option;

  QStyleOptionViewItem ret(option);

  if (index.data(Playlist::Role_IsCurrent).toBool()) {
    // Move the text in a bit on the first column for the song that's currently playing
    ret.rect.setLeft(ret.rect.left() + 20);
  }

  return ret;

}

bool PlaylistDelegateBase::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index) {

  // This function is copied from QAbstractItemDelegate, and changed to show displayText() in the tooltip, rather than the index's naked Qt::ToolTipRole text.

  Q_UNUSED(option);

  if (!event || !view) return false;

  QHelpEvent *he = static_cast<QHelpEvent*>(event);
  QString text = displayText(index.data(), QLocale::system());

  // Special case: we want newlines in the comment tooltip
  if (index.column() == Playlist::Column_Comment) {
    text = index.data(Qt::ToolTipRole).toString().toHtmlEscaped();
    text.replace("\\r\\n", "<br />");
    text.replace("\\n", "<br />");
    text.replace("\r\n", "<br />");
    text.replace("\n", "<br />");
  }

  if (text.isEmpty() || !he) return false;

  switch (event->type()) {
    case QEvent::ToolTip: {
      QSize real_text = sizeHint(option, index);
      QRect displayed_text = view->visualRect(index);
      bool is_elided = displayed_text.width() < real_text.width();
      if (is_elided) {
        QToolTip::showText(he->globalPos(), text, view);
      }
      else {  // in case that another text was previously displayed
        QToolTip::hideText();
      }
      return true;
    }

    case QEvent::QueryWhatsThis:
      return true;

    case QEvent::WhatsThis:
      QWhatsThis::showText(he->globalPos(), text, view);
      return true;

    default:
      break;
  }
  return false;

}


QString LengthItemDelegate::displayText(const QVariant &value, const QLocale&) const {

  bool ok = false;
  qint64 nanoseconds = value.toLongLong(&ok);

  if (ok && nanoseconds > 0) return Utilities::PrettyTimeNanosec(nanoseconds);
  return QString();

}


QString SizeItemDelegate::displayText(const QVariant &value, const QLocale&) const {

  bool ok = false;
  int bytes = value.toInt(&ok);

  if (ok) return Utilities::PrettySize(bytes);
  return QString();

}

QString DateItemDelegate::displayText(const QVariant &value, const QLocale &locale) const {

  bool ok = false;
  int time = value.toInt(&ok);

  if (!ok || time == -1)
    return QString();

  return QDateTime::fromTime_t(time).toString(QLocale::system().dateTimeFormat(QLocale::ShortFormat));

}

QString LastPlayedItemDelegate::displayText(const QVariant &value, const QLocale &locale) const {

  bool ok = false;
  const int time = value.toInt(&ok);

  if (!ok || time == -1)
    return tr("Never");

  return Utilities::Ago(time, locale);

}

QString FileTypeItemDelegate::displayText(const QVariant &value, const QLocale &locale) const {

  bool ok = false;
  Song::FileType type = Song::FileType(value.toInt(&ok));

  if (!ok) return tr("Unknown");

  return Song::TextForFiletype(type);

}

QWidget *TextItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const {
  return new QLineEdit(parent);
}

TagCompletionModel::TagCompletionModel(CollectionBackend *backend, Playlist::Column column) : QStringListModel() {

  QString col = database_column(column);
  if (!col.isEmpty()) {
    setStringList(backend->GetAll(col));
  }

  if (QThread::currentThread() != backend->thread() && QThread::currentThread() != qApp->thread()) {
    backend->Close();
  }

}

QString TagCompletionModel::database_column(Playlist::Column column) {

  switch (column) {
    case Playlist::Column_Artist:       return "artist";
    case Playlist::Column_Album:        return "album";
    case Playlist::Column_AlbumArtist:  return "albumartist";
    case Playlist::Column_Composer:     return "composer";
    case Playlist::Column_Performer:    return "performer";
    case Playlist::Column_Grouping:     return "grouping";
    case Playlist::Column_Genre:        return "genre";
    default:
      qLog(Warning) << "Unknown column" << column;
      return QString();
  }

}

static TagCompletionModel *InitCompletionModel(CollectionBackend *backend, Playlist::Column column) {

  return new TagCompletionModel(backend, column);

}

TagCompleter::TagCompleter(CollectionBackend *backend, Playlist::Column column, QLineEdit *editor) : QCompleter(editor), editor_(editor) {

  QFuture<TagCompletionModel*> future = QtConcurrent::run(&InitCompletionModel, backend, column);
  NewClosure(future, this, SLOT(ModelReady(QFuture<TagCompletionModel*>)), future);

}

TagCompleter::~TagCompleter() {
  delete model();
}

void TagCompleter::ModelReady(QFuture<TagCompletionModel*> future) {

  TagCompletionModel *model = future.result();
  setModel(model);
  setCaseSensitivity(Qt::CaseInsensitive);
  editor_->setCompleter(this);

}

QWidget *TagCompletionItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem&, const QModelIndex&) const {

  QLineEdit *editor = new QLineEdit(parent);
  new TagCompleter(backend_, column_, editor);

  return editor;

}

QString NativeSeparatorsDelegate::displayText(const QVariant &value, const QLocale&) const {

  const QString string_value = value.toString();

  QUrl url;
  if (value.type() == QVariant::Url) {
    url = value.toUrl();
  }
  else if (string_value.contains("://")) {
    url = QUrl::fromEncoded(string_value.toLatin1());
  }
  else {
    return QDir::toNativeSeparators(string_value);
  }

  if (url.isLocalFile()) {
    return QDir::toNativeSeparators(url.toLocalFile());
  }
  return string_value;

}

SongSourceDelegate::SongSourceDelegate(QObject *parent) : PlaylistDelegateBase(parent) {}

QString SongSourceDelegate::displayText(const QVariant &value, const QLocale&) const {
  return QString();
}

QPixmap SongSourceDelegate::LookupPixmap(const Song::Source &source, const QSize &size) const {

  QPixmap pixmap;
  if (cache_.find(Song::TextForSource(source), &pixmap)) {
    return pixmap;
  }

  QIcon icon(Song::IconForSource(source));
  pixmap = icon.pixmap(size.height());
  cache_.insert(Song::TextForSource(source), pixmap);

  return pixmap;

}

void SongSourceDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {

  // Draw the background
  PlaylistDelegateBase::paint(painter, option, index);

  QStyleOptionViewItem option_copy(option);
  initStyleOption(&option_copy, index);

  const Song::Source &source = Song::Source(index.data().toInt());
  QPixmap pixmap = LookupPixmap(source, option_copy.decorationSize);

  QWidget *parent_widget = reinterpret_cast<QWidget*>(parent());
  int device_pixel_ratio = parent_widget->devicePixelRatio();

  // Draw the pixmap in the middle of the rectangle
  QRect draw_rect(QPoint(0, 0), option_copy.decorationSize / device_pixel_ratio);
  draw_rect.moveCenter(option_copy.rect.center());

  painter->drawPixmap(draw_rect, pixmap);

}

