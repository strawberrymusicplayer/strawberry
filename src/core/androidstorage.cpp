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
#include <sys/stat.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMap>
#include <QReadWriteLock>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "core/logging.h"
#include "androidstorage.h"
#ifdef HAVE_NAS
#  include "nasfileengine.h"
#endif

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr int kPermissionGranted = 0;
constexpr int kPermissionRequestCode = 4711;
constexpr char kExternalStorageAuthority[] = "com.android.externalstorage.documents";
constexpr char kDownloadsAuthority[] = "com.android.providers.downloads.documents";

struct DocumentGrant {
  QUrl uri;
  QString document_id;
  bool tree;
};

QReadWriteLock grants_lock;
QMap<QString, DocumentGrant> grants;

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

QString PathForDownloadsDocument(const QJniObject &uri) {

  QJniEnvironment env;
  const QJniObject resolver = Context().callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
  const QJniObject descriptor = resolver.callObjectMethod("openFileDescriptor", "(Landroid/net/Uri;Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;", uri.object(), QJniObject::fromString(u"r"_s).object<jstring>());
  if (env.checkAndClearExceptions() || !descriptor.isValid()) {
    qLog(Error) << "Could not open Android Downloads document";
    return QString();
  }
  // Resolve the provider's actual file, not its opaque document ID or display name.
  const int fd = descriptor.callMethod<jint>("getFd");
  struct stat info {};
  const bool regular_file = fd >= 0 && fstat(fd, &info) == 0 && S_ISREG(info.st_mode);
  const QString path = QFileInfo(QStringLiteral("/proc/self/fd/%1").arg(fd)).symLinkTarget();
  descriptor.callMethod<void>("close");
  if (env.checkAndClearExceptions() || !regular_file || !path.startsWith(u'/') || path.endsWith(" (deleted)"_L1)) {
    qLog(Error) << "Android Downloads document has no usable local backing file";
    return QString();
  }
  return path;

}

bool ReloadDocumentGrants() {

  QJniEnvironment env;
  const QJniObject resolver = Context().callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
  const QJniObject permissions = resolver.callObjectMethod("getPersistedUriPermissions", "()Ljava/util/List;");
  if (!permissions.isValid() || env.checkAndClearExceptions()) {
    qLog(Error) << "Could not load persisted Android document permissions";
    return false;
  }
  QMap<QString, DocumentGrant> updated;
  const int count = permissions.callMethod<jint>("size");
  for (int i = 0; i < count; ++i) {
    const QJniObject permission = permissions.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
    if (!permission.callMethod<jboolean>("isReadPermission")) continue;
    const QJniObject uri = permission.callObjectMethod("getUri", "()Landroid/net/Uri;");
    const QString authority = uri.callObjectMethod<jstring>("getAuthority").toString();
    if (authority != QLatin1String(kExternalStorageAuthority) && authority != QLatin1String(kDownloadsAuthority)) continue;
    const bool tree = QJniObject::callStaticMethod<jboolean>("android/provider/DocumentsContract", "isTreeUri", "(Landroid/net/Uri;)Z", uri.object());
    const QString id = QJniObject::callStaticObjectMethod("android/provider/DocumentsContract", tree ? "getTreeDocumentId" : "getDocumentId", "(Landroid/net/Uri;)Ljava/lang/String;", uri.object()).toString();
    QString path;
    if (authority == QLatin1String(kExternalStorageAuthority)) path = PathForExternalStorageDocument(id);
    else if (id.startsWith("raw:"_L1)) path = id.mid(4);
    else if (!tree) path = PathForDownloadsDocument(uri);
    if (!path.isEmpty()) updated.insert(QDir::cleanPath(path), {QUrl(uri.callObjectMethod<jstring>("toString").toString()), id, tree});
  }
  if (env.checkAndClearExceptions()) {
    qLog(Error) << "Could not read persisted Android document permissions";
    return false;
  }
  QWriteLocker locker(&grants_lock);
  grants = updated;
  return true;

}

bool PreserveDocumentGrant(const QJniObject &uri) {

  QJniEnvironment env;
  const QJniObject resolver = Context().callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
  // Retain read access only; Android local-file editing remains disabled.
  const jmethodID take_permission = env.findMethod(resolver.objectClass(), "takePersistableUriPermission", "(Landroid/net/Uri;I)V");
  if (take_permission) env->CallVoidMethod(resolver.object(), take_permission, uri.object(), 1);
  if (!take_permission || env.checkAndClearExceptions()) {
    qLog(Error) << "Could not retain Android document permission";
    return false;
  }
  return ReloadDocumentGrants();

}

}  // namespace

namespace AndroidStorage {

void InitializeFileAccess() {

#ifdef HAVE_NAS
  QString nas_error;
  Nas::Load(&nas_error);
#endif
  ReloadDocumentGrants();
  InstallDocumentFileEngine();

}

QUrl DocumentUrlForPath(const QString &path) {

  if (!path.startsWith(u'/')) return QUrl();
  const QString cleaned = QDir::cleanPath(path);
  QReadLocker locker(&grants_lock);
  const DocumentGrant *best = nullptr;
  QString root;
  for (auto it = grants.cbegin(); it != grants.cend(); ++it) {
    if ((cleaned == it.key() || (it->tree && cleaned.startsWith(it.key() + u'/'))) && it.key().size() > root.size()) {
      root = it.key();
      best = &it.value();
    }
  }
  if (!best) return QUrl();
  if (!best->tree) return best->uri;
  QString id = best->document_id;
  if (cleaned != root) {
    if (!id.endsWith(u':')) id += u'/';
    id += cleaned.mid(root.size() + 1);
  }
  const QJniObject tree_uri = QJniObject::callStaticObjectMethod("android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;", QJniObject::fromString(best->uri.toString(QUrl::FullyEncoded)).object<jstring>());
  const QJniObject document_uri = QJniObject::callStaticObjectMethod("android/provider/DocumentsContract", "buildDocumentUriUsingTree", "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;", tree_uri.object(), QJniObject::fromString(id).object<jstring>());
  return QUrl(document_uri.callObjectMethod<jstring>("toString").toString());

}

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
    if (!path.isEmpty() && !PreserveDocumentGrant(uri)) return QString();
  }
  else if (authority == QLatin1String(kDownloadsAuthority) && (document || tree)) {
    if (document_id.startsWith("raw:"_L1)) path = document_id.mid(4);
    else if (!tree) path = PathForDownloadsDocument(uri);
    if (!path.isEmpty() && !PreserveDocumentGrant(uri)) return QString();
  }
  else {
    // Media documents map to a MediaStore item, which knows the path of the file.
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

  return QCoreApplication::translate("AndroidStorage", "The selected item is not available as a local file or folder with retained access: %1. Select a folder or file stored on this device using Android's file picker.").arg(unresolved.join(", "_L1));

}

}  // namespace AndroidStorage
