// SPDX-License-Identifier: GPL-3.0-or-later

#include <QByteArray>
#include <QFile>
#include <QSslCertificate>
#include <QString>
#include <QtGlobal>

#include "spotifytrust_android.h"

namespace SpotifyTrust {

Result Configure(QString &error) {

  for (const char *name : {"SSL_CERT_FILE", "SSL_CERT_DIR"}) {
    if (qEnvironmentVariableIsSet(name) && qEnvironmentVariableIsEmpty(name)) {
      error = QStringLiteral("%1 is set but empty").arg(QLatin1String(name));
      return Result::Error;
    }
  }
  if (qEnvironmentVariableIsSet("SSL_CERT_FILE") || qEnvironmentVariableIsSet("SSL_CERT_DIR")) return Result::ExistingOverride;

  const QByteArray bundle_path = qgetenv("CA_CERTIFICATES");
  if (bundle_path.isEmpty()) {
    error = QStringLiteral("GStreamer did not provide a CA certificate bundle");
    return Result::Error;
  }
  QFile bundle(QFile::decodeName(bundle_path));
  if (!bundle.open(QIODevice::ReadOnly)) {
    error = QStringLiteral("Cannot read the GStreamer CA certificate bundle: %1").arg(bundle.errorString());
    return Result::Error;
  }
  const QByteArray data = bundle.readAll();
  if (bundle.error() != QFileDevice::NoError) {
    error = QStringLiteral("Cannot read the GStreamer CA certificate bundle: %1").arg(bundle.errorString());
    return Result::Error;
  }
  if (QSslCertificate::fromData(data, QSsl::Pem).isEmpty()) {
    error = QStringLiteral("The GStreamer CA certificate bundle contains no readable PEM certificates");
    return Result::Error;
  }
  if (!qputenv("SSL_CERT_FILE", bundle_path)) {
    error = QStringLiteral("Cannot configure the Rust TLS certificate bundle");
    return Result::Error;
  }
  return Result::Bundled;

}

}  // namespace SpotifyTrust
