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

#ifndef ANDROIDSTORAGE_H
#define ANDROIDSTORAGE_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace AndroidStorage {

// Asks for read access to audio files in shared storage, which the collection, tag reader and GStreamer need to open files by path.
void RequestAudioPermission();

// Android file dialogs return content:// document URIs; returns the file system path behind such a URI, or an empty string when it has none.
QString LocalPathForUrl(const QUrl &url);

// Returns the cleaned file system paths of the URLs from an Android file dialog.
// URLs without a file system path are added to unresolved as display strings.
QStringList LocalPathsForUrls(const QList<QUrl> &urls, QStringList *unresolved);

// The message to show for dialog selections without a file system path.
QString UnresolvedMessage(const QStringList &unresolved);

}  // namespace AndroidStorage

#endif  // ANDROIDSTORAGE_H
