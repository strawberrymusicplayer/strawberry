// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>

#include "engine/spotifytrust_android.h"

namespace {

constexpr auto kCertificate = R"(-----BEGIN CERTIFICATE-----
MIIDGTCCAgGgAwIBAgIUOx7dtNT7ouppYe8hI5EigN4GFoUwDQYJKoZIhvcNAQEL
BQAwHDEaMBgGA1UEAwwRU3BvdGlmeSB0ZXN0IG9ubHkwHhcNMjYxMTE0MTkwNzI5
WhcNMzYxMTExMTkwNzI5WjAcMRowGAYDVQQDDBFTcG90aWZ5IHRlc3Qgb25seTCC
ASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAND4S0Ufknsy7wATDUP7xVuP
8nAWhzyv2Fo001Ka6i1fIOHqlV5kJ9vr72MlaU9z3wGlETyd+n18oJR/p7PavMWp
QpVKxCid2hVUc3MnLalIwcEBTD0HOO4kFCg3Gfmv78i039xFVhcegpJSO4vK5MAQ
jGsExO6zp+ujYeyXAbB6a8V6Kghc2VnKA3nVmCLUq/Zar2hE1nJUigusw9RKlIyg
2mXRzhn581REP4sOOTRQI3zhA7kkp2fy2+/DT0EnlhmLZcg8Uj/zxAQuu1+BXK0D
2mcVADKs96K+CJEC1kGXbD1aGZucgildJQvsG0BDdGItSoBm2PkR9speVp4BIVsC
AwEAAaNTMFEwHQYDVR0OBBYEFIjIChMCz4tFepajvowx0QDIwsSkMB8GA1UdIwQY
MBaAFIjIChMCz4tFepajvowx0QDIwsSkMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZI
hvcNAQELBQADggEBAJ3HcpAgNFj9RiZxWVcSqDfklZLMCQODM4lqxaORJpUaMq6I
+ELB2cmZGJVCUUJiH/PvBNiPrNs+99I5Jy/8UaGHL49bYdunyC8UvLAFQR9FvIVm
NnlmQzUdANs/fYBgUjYBO+8jeUzZvxmcXM5eo/FQrXG8BoHv0uw4taJOESTKOrC8
0JiYRm+xuFiXXrdx8SSE5UYIqZhJq7jFR3Kk/6EQK4GrMpJ0FQvIGNmx3EIXM9lD
n3UXlZsCCEM+E6IiH333GmiATsB2/C4HpGeHz46US22VDPjGSQ0P8pxyVJ6Bpak2
/x77wECpB6Zzt1jCnC9NjUVmJQBYh6IphIEu1/s=
-----END CERTIFICATE-----
)";

class SpotifyTrustTest : public ::testing::Test {
 protected:
  void SetUp() override {
    for (int i = 0; i < 3; ++i) {
      was_set_[i] = qEnvironmentVariableIsSet(names_[i]);
      values_[i] = qgetenv(names_[i]);
      qunsetenv(names_[i]);
    }
  }

  void TearDown() override {
    for (int i = 0; i < 3; ++i) {
      if (was_set_[i]) qputenv(names_[i], values_[i]);
      else qunsetenv(names_[i]);
    }
  }

  const char *names_[3] = {"SSL_CERT_FILE", "SSL_CERT_DIR", "CA_CERTIFICATES"};
  bool was_set_[3] = {};
  QByteArray values_[3];
};

TEST_F(SpotifyTrustTest, MissingBundleIsAnError) {
  QString error;
  EXPECT_EQ(SpotifyTrust::Configure(error), SpotifyTrust::Result::Error);
  EXPECT_FALSE(error.isEmpty());
  EXPECT_FALSE(qEnvironmentVariableIsSet("SSL_CERT_FILE"));
}

TEST_F(SpotifyTrustTest, UnreadableBundleIsAnError) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  qputenv("CA_CERTIFICATES", QFile::encodeName(directory.filePath(QStringLiteral("missing.pem"))));
  QString error;
  EXPECT_EQ(SpotifyTrust::Configure(error), SpotifyTrust::Result::Error);
  EXPECT_FALSE(error.isEmpty());
  EXPECT_FALSE(qEnvironmentVariableIsSet("SSL_CERT_FILE"));
}

TEST_F(SpotifyTrustTest, InvalidPemIsAnError) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  QFile bundle(directory.filePath(QStringLiteral("invalid.pem")));
  ASSERT_TRUE(bundle.open(QIODevice::WriteOnly));
  ASSERT_GT(bundle.write("-----BEGIN CERTIFICATE-----\ninvalid\n-----END CERTIFICATE-----\n"), 0);
  bundle.close();
  qputenv("CA_CERTIFICATES", QFile::encodeName(bundle.fileName()));
  QString error;
  EXPECT_EQ(SpotifyTrust::Configure(error), SpotifyTrust::Result::Error);
  EXPECT_FALSE(error.isEmpty());
  EXPECT_FALSE(qEnvironmentVariableIsSet("SSL_CERT_FILE"));
}

TEST_F(SpotifyTrustTest, BundledCertificateIsConfigured) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  QFile bundle(directory.filePath(QStringLiteral("bundle.pem")));
  ASSERT_TRUE(bundle.open(QIODevice::WriteOnly));
  ASSERT_GT(bundle.write(kCertificate), 0);
  bundle.close();
  const QByteArray path = QFile::encodeName(bundle.fileName());
  qputenv("CA_CERTIFICATES", path);
  QString error;
  EXPECT_EQ(SpotifyTrust::Configure(error), SpotifyTrust::Result::Bundled);
  EXPECT_TRUE(error.isEmpty());
  EXPECT_EQ(qgetenv("SSL_CERT_FILE"), path);
  EXPECT_FALSE(qEnvironmentVariableIsSet("SSL_CERT_DIR"));
}

TEST_F(SpotifyTrustTest, ExistingOverridesArePreserved) {
  qputenv("SSL_CERT_FILE", "/custom/cert.pem");
  qputenv("SSL_CERT_DIR", "/custom/certs");
  QString error;
  EXPECT_EQ(SpotifyTrust::Configure(error), SpotifyTrust::Result::ExistingOverride);
  EXPECT_EQ(qgetenv("SSL_CERT_FILE"), QByteArray("/custom/cert.pem"));
  EXPECT_EQ(qgetenv("SSL_CERT_DIR"), QByteArray("/custom/certs"));
}

TEST_F(SpotifyTrustTest, DirectoryOnlyOverrideIsPreserved) {
  qputenv("SSL_CERT_DIR", "/custom/certs");
  QString error;
  EXPECT_EQ(SpotifyTrust::Configure(error), SpotifyTrust::Result::ExistingOverride);
  EXPECT_FALSE(qEnvironmentVariableIsSet("SSL_CERT_FILE"));
  EXPECT_EQ(qgetenv("SSL_CERT_DIR"), QByteArray("/custom/certs"));
}

TEST_F(SpotifyTrustTest, FileOnlyOverrideIsPreserved) {
  qputenv("SSL_CERT_FILE", "/custom/cert.pem");
  QString error;
  EXPECT_EQ(SpotifyTrust::Configure(error), SpotifyTrust::Result::ExistingOverride);
  EXPECT_EQ(qgetenv("SSL_CERT_FILE"), QByteArray("/custom/cert.pem"));
  EXPECT_FALSE(qEnvironmentVariableIsSet("SSL_CERT_DIR"));
}

TEST_F(SpotifyTrustTest, EmptyOverrideIsAnError) {
  qputenv("SSL_CERT_FILE", "/custom/cert.pem");
  qputenv("SSL_CERT_DIR", "");
  QString error;
  EXPECT_EQ(SpotifyTrust::Configure(error), SpotifyTrust::Result::Error);
  EXPECT_FALSE(error.isEmpty());
  EXPECT_EQ(qgetenv("SSL_CERT_FILE"), QByteArray("/custom/cert.pem"));
  EXPECT_TRUE(qEnvironmentVariableIsSet("SSL_CERT_DIR"));
}

TEST_F(SpotifyTrustTest, EmptyFileOverrideIsAnError) {
  qputenv("SSL_CERT_FILE", "");
  qputenv("SSL_CERT_DIR", "/custom/certs");
  QString error;
  EXPECT_EQ(SpotifyTrust::Configure(error), SpotifyTrust::Result::Error);
  EXPECT_FALSE(error.isEmpty());
  EXPECT_TRUE(qEnvironmentVariableIsSet("SSL_CERT_FILE"));
  EXPECT_EQ(qgetenv("SSL_CERT_DIR"), QByteArray("/custom/certs"));
}

}  // namespace
