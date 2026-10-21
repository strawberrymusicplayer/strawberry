/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CREDENTIALSMANAGER_H
#define CREDENTIALSMANAGER_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QQueue>
#include <QString>

#include "includes/scoped_ptr.h"
#include "credentialsreply.h"
#include "credentialsresult.h"

class CredentialsBackendInterface;

// Stores passwords securely in the system keyring.
// On Linux and *BSD KWallet, GNOME Keyring (Secret Service D-Bus API) and libsecret are supported, on Windows Windows Credential Manager, and on macOS Keychain.
// If no secure backend is available, passwords are stored obfuscated in the settings file as a last resort.
// Passwords are unique per service, the service name should be a short lowercase identifier, for example "subsonic".
// All requests are asynchronous and run one at a time in the order they were made.
// KWallet, GNOME Keyring and libsecret use asynchronous APIs, backends without an asynchronous API run in a thread pool with QtConcurrent.
// Requests return a reply which emits Finished in the calling thread, the calling thread must have an event loop.
// Requests can be made from any thread, and may show a password prompt from the system keyring.
class CredentialsManager : public QObject {
  Q_OBJECT

 public:
  explicit CredentialsManager(QObject *parent = nullptr);
  ~CredentialsManager() override;

  enum class BackendType {
    Auto,
    KWallet,
    GnomeKeyring,
    LibSecret,
    WindowsCredentialManager,
    Keychain,
    Settings
  };

  static constexpr char kSettingsGroup[] = "CredentialsManager";
  static constexpr char kBackend[] = "backend";
  // The backend which stored a password in Auto mode, it's used first on the next start, so the passwords are not looked up in another backend.
  static constexpr char kAutoBackend[] = "auto_backend";

  static QString BackendToString(const BackendType backend);
  static BackendType BackendFromString(const QString &backend);

  // Returns the backends compiled in for this platform, excluding Auto.
  static QList<BackendType> SupportedBackends();

  // Re-reads the selected backend from the settings, the backend is initialized again before the next request.
  void ReloadSettings();

  // Finishes with Success and the password, NotFound if no password is stored for the service, or Error.
  [[nodiscard]] CredentialsReplyPtr ReadPasswordAsync(const QString &service);

  // Stores the password for the service, replacing any existing password.
  // Saving an empty password deletes the stored password.
  [[nodiscard]] CredentialsReplyPtr SavePasswordAsync(const QString &service, const QString &password);

  // Deletes the password for the service, finishes with Success if the password is deleted or did not exist.
  [[nodiscard]] CredentialsReplyPtr DeletePasswordAsync(const QString &service);

 protected:
  // Returns the automatically detected backends in order of preference.
  virtual QList<BackendType> AutoBackendTypes() const;

  virtual ScopedPtr<CredentialsBackendInterface> CreateBackend(const BackendType backend_type);

 private:
  enum class RequestType {
    Read,
    Save,
    Delete
  };

  struct Request {
    RequestType type;
    CredentialsReplyPtr reply;
    QString password;
  };

  enum class BackendState {
    Uninitialized,
    Initializing,
    Ready
  };

  void AddRequest(const Request &request);
  void StartNextRequest();
  // Checks whether the remembered backend which was not available is available now, and uses it then.
  void CheckRememberedBackend(const Request &request);
  void RememberedBackendChecked(const Request &request, const bool available);
  void RunRequest(const Request &request);
  void RequestFinished(const Request &request, const CredentialsResult &credentials_result);
  // Finishes the request, password_in_backend is true when the password is stored in the current backend.
  void FinishRequest(const Request &request, const CredentialsResult &credentials_result, const bool password_in_backend);

  // Reads a password not found in a secure backend in Auto mode from the settings file, and moves it to the secure backend.
  void MovePasswordFromSettings(const Request &request);
  void SettingsPasswordRead(const Request &request, const CredentialsResult &settings_result);
  void SettingsPasswordSaved(const Request &request, const CredentialsResult &settings_result, const CredentialsResult &save_result);
  // Deletes a password saved or deleted in a secure backend from the settings file.
  void DeletePasswordFromSettings(const Request &request, const CredentialsResult &credentials_result);

  // The backend selected in the settings, or in Auto mode the backend which stored passwords before.
  BackendType preferred_backend_type() const { return backend_type_ != BackendType::Auto ? backend_type_ : auto_backend_type_; }

  void InitializeBackend();
  void CheckNextBackend();
  void BackendChecked(const bool available);
  void BackendReady();

 private:
  BackendType backend_type_;
  BackendType auto_backend_type_;
  bool reset_backend_;
  BackendState backend_state_;
  ScopedPtr<CredentialsBackendInterface> backend_;
  // The settings file backend while a password is moved from it or deleted from it, or the remembered backend while it's checked.
  ScopedPtr<CredentialsBackendInterface> other_backend_;
  BackendType current_backend_type_;

  QList<BackendType> candidate_backend_types_;
  ScopedPtr<CredentialsBackendInterface> candidate_backend_;
  BackendType candidate_backend_type_;
  bool candidate_backend_is_selected_;
  // The backend remembered in Auto mode was not available when the backend was initialized, for example when the keyring daemon wasn't started yet.
  bool auto_backend_unavailable_;

  QQueue<Request> requests_;
  bool request_running_;
};

#endif  // CREDENTIALSMANAGER_H
