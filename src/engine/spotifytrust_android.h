// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SPOTIFYTRUST_ANDROID_H
#define SPOTIFYTRUST_ANDROID_H

class QString;

namespace SpotifyTrust {

enum class Result {
  Error,
  ExistingOverride,
  Bundled,
};

Result Configure(QString &error);

}  // namespace SpotifyTrust

#endif  // SPOTIFYTRUST_ANDROID_H
