// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#include <functional>

#include <QLayout>
#include <QPushButton>
#include <QScopedValueRollback>

#include "core/logging.h"
#include "core/settings.h"
#include "credentialsmanager/credentialsmanager.h"
#include "settingsdialog.h"
#include "settingspage.h"
#include "streamingautosave_android.h"
#include "streaming/streamingservice.h"
#include "streaming/streamingservices.h"
#include "widgets/actiondialog.h"
#include "widgets/androidwidgets.h"
#include "widgets/loginstatewidget.h"

#ifdef HAVE_SUBSONIC
#include "constants/subsonicsettings.h"
#include "subsonic/subsonicservice.h"
#endif
#ifdef HAVE_TIDAL
#include "tidal/tidalservice.h"
#endif
#ifdef HAVE_SPOTIFY
#include "spotify/spotifyservice.h"
#endif
#ifdef HAVE_QOBUZ
#include "qobuz/qobuzservice.h"
#endif
#ifdef HAVE_PLEX
#include "plex/plexservice.h"
#endif
#ifdef HAVE_JELLYFIN
#include "constants/jellyfinsettings.h"
#include "jellyfin/jellyfinservice.h"
#endif

using namespace Qt::Literals::StringLiterals;

bool SettingsDialog::IsAndroidStreamingPage(const Page page) {

  switch (page) {
    case Page::Subsonic:
    case Page::Tidal:
    case Page::Spotify:
    case Page::Qobuz:
    case Page::Plex:
    case Page::Jellyfin:
    case Page::Radio:
      return true;
    default:
      return false;
  }

}

void SettingsDialog::SetupAndroidStreamingSettings(const SharedPtr<StreamingServices> streaming_services) {

  const auto show_error = [this](const QString &message) {
    qLog(Error) << message;
    auto *notification = new ActionDialog(this, tr("Settings could not be saved"), message, ActionDialog::TitlePosition::Top);
    notification->AddCancelButton(tr("Close"));
    notification->Open();
  };
  for (auto it = pages_.cbegin(); it != pages_.cend(); ++it) {
    if (!IsAndroidStreamingPage(it.key()) || it.key() == Page::Qobuz) continue;
    SettingsPage *page = it->page_;
    ConnectStreamingAutoSave(page, page, [this, page, show_error]() {
      if (loading_settings_ || !android_settings_loaded_ || !page->isEnabled()) return;
      const QScopedValueRollback<bool> saving(loading_settings_, true);
      page->set_changed();
      page->Apply();
      Settings settings;
      settings.sync();
      if (settings.status() != QSettings::NoError) {
        show_error(tr("Could not save %1 settings. Please check storage access and try again.").arg(page->windowTitle()));
        return;
      }
      Q_EMIT ReloadSettings();
    });
  }

  const auto add_reset = [this, show_error](const Page id, const StreamingServicePtr &service,
                                           const std::function<void()> &clear_session,
                                           const SharedPtr<CredentialsManager> credentials = nullptr,
                                           const QString &credentials_service = QString()) {
    SettingsPage *page = pages_.value(id).page_;
    auto *button = new QPushButton(tr("Reset service"), page);
    button->setObjectName(u"reset_service"_s);
    AndroidWidgets::StyleNavigationButton(button, AndroidWidgets::kMinimumTouchSize);
    page->layout()->addWidget(button);
    QObject::connect(button, &QPushButton::clicked, this, [this, page, service, clear_session, credentials, credentials_service, show_error]() {
      auto *confirmation = ActionDialog::CreateConfirmation(this, tr("Reset %1").arg(service->name()),
          tr("Sign out of %1, erase its saved credentials, and reset its preferences? This cannot be undone.").arg(service->name()), tr("Reset service"));
      QObject::connect(confirmation, &QDialog::accepted, this, [this, page, service, clear_session, credentials, credentials_service, show_error]() {
        page->setEnabled(false);
        const auto reset = [this, page, service, clear_session, show_error]() {
          const QScopedValueRollback<bool> resetting(loading_settings_, true);
          if (clear_session) clear_session();
          Settings settings;
          settings.remove(service->settings_group());
          settings.sync();
          if (settings.status() != QSettings::NoError) {
            page->setEnabled(true);
            show_error(tr("Could not reset %1 settings. Please check storage access and try again.").arg(service->name()));
            return;
          }
          Q_EMIT ReloadSettings();
          page->ResetAndroidSettings();
          for (LoginStateWidget *state : page->findChildren<LoginStateWidget*>()) {
            state->SetLoggedIn(LoginStateWidget::State::LoggedOut);
          }
          if (auto *login = page->findChild<QPushButton*>(u"button_login"_s)) {
            login->setText(tr("Login"));
            login->setEnabled(true);
          }
          page->setEnabled(true);
        };
        if (!credentials) {
          reset();
          return;
        }
        auto *operation = new QObject(this);
        const CredentialsReplyPtr reply = credentials->DeletePasswordAsync(credentials_service);
        QObject::connect(&*reply, &CredentialsReply::Finished, operation, [operation, reply, reset, page, show_error]() {
          if (reply->success()) reset();
          else {
            page->setEnabled(true);
            show_error(tr("Could not erase the saved password: %1").arg(reply->error()));
          }
          operation->deleteLater();
        });
      });
      confirmation->Open();
    });
  };

  (void)streaming_services;
  (void)add_reset;
#ifdef HAVE_SUBSONIC
  const auto subsonic = streaming_services->Service<SubsonicService>();
  add_reset(Page::Subsonic, subsonic, {}, subsonic->credentials_manager(), QLatin1String(SubsonicSettings::kCredentialsService));
#endif
#ifdef HAVE_TIDAL
  const auto tidal = streaming_services->Service<TidalService>();
  add_reset(Page::Tidal, tidal, [tidal]() { tidal->ClearSession(); });
#endif
#ifdef HAVE_SPOTIFY
  const auto spotify = streaming_services->Service<SpotifyService>();
  add_reset(Page::Spotify, spotify, [spotify]() { spotify->ClearSession(); });
#endif
#ifdef HAVE_QOBUZ
  const auto qobuz = streaming_services->Service<QobuzService>();
  add_reset(Page::Qobuz, qobuz, [qobuz]() { qobuz->ClearSession(); });
#endif
#ifdef HAVE_PLEX
  const auto plex = streaming_services->Service<PlexService>();
  add_reset(Page::Plex, plex, [plex]() { plex->Deauthenticate(); });
#endif
#ifdef HAVE_JELLYFIN
  const auto jellyfin = streaming_services->Service<JellyfinService>();
  add_reset(Page::Jellyfin, jellyfin, {}, jellyfin->credentials_manager(), QLatin1String(JellyfinSettings::kCredentialsService));
#endif

}
