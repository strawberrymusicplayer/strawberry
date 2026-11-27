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

#include <cmath>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include "equalizerjson.h"

namespace EqualizerJson {

namespace {

constexpr char kFormatKey[] = "format";
constexpr char kVersionKey[] = "version";
constexpr char kEnabledKey[] = "enabled";
constexpr char kSelectedPresetKey[] = "selected_preset";
constexpr char kCurrentKey[] = "current";
constexpr char kStereoBalancerKey[] = "stereo_balancer";
constexpr char kBalanceKey[] = "balance";
constexpr char kPresetsKey[] = "presets";
constexpr char kNameKey[] = "name";
constexpr char kPreampKey[] = "preamp";
constexpr char kGainsKey[] = "gains";
constexpr int kMinimumValue = -100;
constexpr int kMaximumValue = 100;

QString Tr(const char *text) {
  return QCoreApplication::translate("EqualizerJson", text);
}

QJsonObject ParamsToJson(const Equalizer::Params &params) {

  QJsonArray gains;
  for (const int gain : params.gain) gains.append(gain);
  QJsonObject object;
  object[QLatin1String(kPreampKey)] = params.preamp;
  object[QLatin1String(kGainsKey)] = gains;
  return object;

}

bool ReadValue(const QJsonValue &value, int *result) {

  if (!value.isDouble()) return false;
  const double number = value.toDouble();
  if (std::floor(number) != number || number < kMinimumValue || number > kMaximumValue) return false;
  *result = static_cast<int>(number);
  return true;

}

bool ReadParams(const QJsonValue &value, Equalizer::Params *params) {

  if (!value.isObject()) return false;
  const QJsonObject object = value.toObject();
  const QJsonValue gains = object.value(QLatin1String(kGainsKey));
  if (!ReadValue(object.value(QLatin1String(kPreampKey)), &params->preamp) || !gains.isArray() || gains.toArray().count() != Equalizer::kBands) return false;
  const QJsonArray gain_values = gains.toArray();
  for (int i = 0; i < Equalizer::kBands; ++i) {
    if (!ReadValue(gain_values.at(i), &params->gain[i])) return false;
  }
  return true;

}

bool Fail(QString *error, const QString &message) {

  if (error) *error = message;
  return false;

}

}  // namespace

QByteArray ToJson(const State &state) {

  QJsonArray presets;
  for (auto it = state.presets.cbegin(); it != state.presets.cend(); ++it) {
    QJsonObject preset = ParamsToJson(it.value());
    preset[QLatin1String(kNameKey)] = it.key();
    presets.append(preset);
  }
  QJsonObject stereo_balancer;
  stereo_balancer[QLatin1String(kEnabledKey)] = state.stereo_balancer_enabled;
  stereo_balancer[QLatin1String(kBalanceKey)] = state.stereo_balance;
  QJsonObject root;
  root[QLatin1String(kFormatKey)] = QLatin1String(kFormat);
  root[QLatin1String(kVersionKey)] = kVersion;
  root[QLatin1String(kEnabledKey)] = state.enabled;
  root[QLatin1String(kSelectedPresetKey)] = state.selected_preset;
  root[QLatin1String(kCurrentKey)] = ParamsToJson(state.current);
  root[QLatin1String(kStereoBalancerKey)] = stereo_balancer;
  root[QLatin1String(kPresetsKey)] = presets;
  return QJsonDocument(root).toJson(QJsonDocument::Indented);

}

bool FromJson(const QByteArray &json, State *state, QString *error) {

  const QString invalid = Tr("The file contains invalid equalizer settings.");
  if (json.size() > kMaximumFileSize) return Fail(error, Tr("The file is too large to be equalizer settings."));
  QJsonParseError parse_error;
  const QJsonDocument document = QJsonDocument::fromJson(json, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject()) return Fail(error, Tr("The file is not a valid JSON document."));
  const QJsonObject root = document.object();
  if (root.value(QLatin1String(kFormatKey)).toString() != QLatin1String(kFormat)) return Fail(error, Tr("The file does not contain Strawberry equalizer settings."));
  const QJsonValue version = root.value(QLatin1String(kVersionKey));
  if (!version.isDouble() || version.toDouble() != std::floor(version.toDouble()) || version.toDouble() < 1) return Fail(error, invalid);
  if (version.toDouble() > kVersion) return Fail(error, Tr("The file was created by a newer version of Strawberry and is not supported."));

  State result;
  const QJsonValue enabled = root.value(QLatin1String(kEnabledKey));
  const QJsonValue selected_preset = root.value(QLatin1String(kSelectedPresetKey));
  const QJsonValue stereo_balancer = root.value(QLatin1String(kStereoBalancerKey));
  const QJsonValue presets = root.value(QLatin1String(kPresetsKey));
  if (!enabled.isBool() || !selected_preset.isString() || !stereo_balancer.isObject() || !presets.isArray()) return Fail(error, invalid);
  if (!ReadParams(root.value(QLatin1String(kCurrentKey)), &result.current)) return Fail(error, invalid);
  result.enabled = enabled.toBool();
  result.selected_preset = selected_preset.toString();
  const QJsonObject balancer = stereo_balancer.toObject();
  const QJsonValue balancer_enabled = balancer.value(QLatin1String(kEnabledKey));
  if (!balancer_enabled.isBool() || !ReadValue(balancer.value(QLatin1String(kBalanceKey)), &result.stereo_balance)) return Fail(error, invalid);
  result.stereo_balancer_enabled = balancer_enabled.toBool();

  const QJsonArray preset_values = presets.toArray();
  if (preset_values.isEmpty()) return Fail(error, Tr("The file contains no equalizer presets."));
  for (const QJsonValue &value : preset_values) {
    Equalizer::Params params;
    const QString name = value.toObject().value(QLatin1String(kNameKey)).toString();
    if (name.trimmed().isEmpty() || name != name.trimmed() || !ReadParams(value, &params)) return Fail(error, invalid);
    if (result.presets.contains(name)) return Fail(error, Tr("The file contains the preset \"%1\" more than once.").arg(name));
    result.presets.insert(name, params);
  }
  if (!result.presets.contains(result.selected_preset)) return Fail(error, invalid);

  *state = result;
  return true;

}

QStringList ConflictingPresets(const QMap<QString, Equalizer::Params> &existing, const QMap<QString, Equalizer::Params> &imported) {

  QStringList conflicts;
  for (auto it = imported.cbegin(); it != imported.cend(); ++it) {
    if (existing.contains(it.key()) && existing.value(it.key()) != it.value()) conflicts << it.key();
  }
  return conflicts;

}

}  // namespace EqualizerJson
