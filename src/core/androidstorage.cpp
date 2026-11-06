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

#include <jni.h>

#include <QCoreApplication>
#include <QDir>
#include <QJniEnvironment>
#include <QJniObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "core/logging.h"
#include "androidstorage.h"

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr int kPermissionGranted = 0;
constexpr int kPermissionRequestCode = 4711;
constexpr char kExternalStorageAuthority[] = "com.android.externalstorage.documents";
constexpr char kDownloadsAuthority[] = "com.android.providers.downloads.documents";

QJniObject Context() {
  return QNativeInterface::QAndroidApplication::context();
}

jobjectArray NewStringArray(QJniEnvironment &env, const QStringList &strings) {

  jobjectArray array = env->NewObjectArray(static_cast<jsize>(strings.count()), env.findClass("java/lang/String"), nullptr);
  for (qsizetype i = 0; i < strings.count(); ++i) {
    env->SetObjectArrayElement(array, static_cast<jsize>(i), QJniObject::fromString(strings[i]).object<jstring>());
  }
  return array;

}

QString PathForExternalStorageDocument(const QString &document_id) {

  // Document IDs of the external storage provider are "<volume>:<path relative to the volume>".
  const qsizetype separator = document_id.indexOf(u':');
  if (separator < 0) return QString();

  const QString volume = document_id.left(separator);
  const QString relative_path = document_id.mid(separator + 1);

  QString root;
  if (volume == "primary"_L1) {
    root = QJniObject::callStaticObjectMethod("android/os/Environment", "getExternalStorageDirectory", "()Ljava/io/File;").callObjectMethod<jstring>("getAbsolutePath").toString();
  }
  else {
    root = u"/storage/"_s + volume;
  }
  if (root.isEmpty()) return QString();

  return relative_path.isEmpty() ? root : root + u'/' + relative_path;

}

QString DataColumnForUri(const QJniObject &uri) {

  QJniEnvironment env;
  const QJniObject resolver = Context().callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
  if (!resolver.isValid()) return QString();

  jobjectArray projection = NewStringArray(env, QStringList() << u"_data"_s);
  QJniObject cursor = resolver.callObjectMethod("query", "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;", uri.object(), projection, static_cast<jstring>(nullptr), static_cast<jobjectArray>(nullptr), static_cast<jstring>(nullptr));
  env->DeleteLocalRef(projection);
  if (!cursor.isValid()) return QString();

  QString path;
  if (cursor.callMethod<jboolean>("moveToFirst") && !cursor.callMethod<jboolean>("isNull", "(I)Z", 0)) {
    path = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", 0).toString();
  }
  cursor.callMethod<void>("close");

  return path;

}

}  // namespace

namespace AndroidStorage {

void RequestAudioPermission() {

  const QString permission = QNativeInterface::QAndroidApplication::sdkVersion() >= 33 ? u"android.permission.READ_MEDIA_AUDIO"_s : u"android.permission.READ_EXTERNAL_STORAGE"_s;

  QNativeInterface::QAndroidApplication::runOnAndroidMainThread([permission]() {
    QJniObject activity = Context();
    if (!activity.isValid()) return;
    QJniEnvironment env;
    // A failed QJniObject call returns 0, which equals PERMISSION_GRANTED, so call through QJniEnvironment to detect the failure.
    const jmethodID check_permission = env.findMethod(activity.objectClass(), "checkSelfPermission", "(Ljava/lang/String;)I");
    const QJniObject permission_string = QJniObject::fromString(permission);
    const jint state = check_permission ? env->CallIntMethod(activity.object(), check_permission, permission_string.object<jstring>()) : kPermissionGranted;
    if (!check_permission || env.checkAndClearExceptions()) {
      // Android shows no prompt when the permission was already granted, so requesting it is safe.
      qLog(Error) << "Could not check permission" << permission << "- requesting it";
    }
    else if (state == kPermissionGranted) {
      return;
    }
    jobjectArray permissions = NewStringArray(env, QStringList() << permission);
    activity.callMethod<void>("requestPermissions", "([Ljava/lang/String;I)V", permissions, kPermissionRequestCode);
    env->DeleteLocalRef(permissions);
  });

}

QString LocalPathForUrl(const QUrl &url) {

  if (url.isLocalFile()) return url.toLocalFile();
  if (url.scheme() != "content"_L1) return QString();

  const QJniObject context = Context();
  const QJniObject uri = QJniObject::callStaticObjectMethod("android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;", QJniObject::fromString(url.toString(QUrl::FullyEncoded)).object<jstring>());
  if (!context.isValid() || !uri.isValid()) return QString();

  const QString authority = uri.callObjectMethod<jstring>("getAuthority").toString();
  const bool document = QJniObject::callStaticMethod<jboolean>("android/provider/DocumentsContract", "isDocumentUri", "(Landroid/content/Context;Landroid/net/Uri;)Z", context.object(), uri.object());
  const bool tree = QJniObject::callStaticMethod<jboolean>("android/provider/DocumentsContract", "isTreeUri", "(Landroid/net/Uri;)Z", uri.object());

  QString document_id;
  if (document) {
    document_id = QJniObject::callStaticObjectMethod("android/provider/DocumentsContract", "getDocumentId", "(Landroid/net/Uri;)Ljava/lang/String;", uri.object()).toString();
  }
  else if (tree) {
    document_id = QJniObject::callStaticObjectMethod("android/provider/DocumentsContract", "getTreeDocumentId", "(Landroid/net/Uri;)Ljava/lang/String;", uri.object()).toString();
  }

  QString path;
  if (authority == QLatin1String(kExternalStorageAuthority) && !document_id.isEmpty()) {
    path = PathForExternalStorageDocument(document_id);
  }
  else if (authority == QLatin1String(kDownloadsAuthority) && document_id.startsWith("raw:"_L1)) {
    path = document_id.mid(4);
  }
  else {
    // Media and downloads documents map to a MediaStore item, which knows the path of the file.
    QJniObject media_uri = uri;
    if (document && QNativeInterface::QAndroidApplication::sdkVersion() >= 29) {
      media_uri = QJniObject::callStaticObjectMethod("android/provider/MediaStore", "getMediaUri", "(Landroid/content/Context;Landroid/net/Uri;)Landroid/net/Uri;", context.object(), uri.object());
    }
    if (media_uri.isValid()) {
      path = DataColumnForUri(media_uri);
    }
  }

  if (path.isEmpty()) {
    qLog(Error) << "Could not find a file system path for" << url;
  }

  return path;

}

QStringList LocalPathsForUrls(const QList<QUrl> &urls, QStringList *unresolved) {

  QStringList paths;
  for (const QUrl &url : urls) {
    const QString path = LocalPathForUrl(url);
    if (path.isEmpty()) {
      if (unresolved) unresolved->append(url.toDisplayString());
    }
    else {
      paths << QDir::cleanPath(path);
    }
  }
  return paths;

}

QString UnresolvedMessage(const QStringList &unresolved) {

  return QCoreApplication::translate("AndroidStorage", "Not stored on this device, so it can't be used: %1").arg(unresolved.join(", "_L1));

}

}  // namespace AndroidStorage
