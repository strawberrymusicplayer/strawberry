#!/bin/sh
# Cross-builds the gst-plugin-spotify static archive for Strawberry's experimental Android Spotify plugin probe.
# The source archive, Rust toolchain and cargo-c versions are pinned; Cargo.lock pins all crates (--locked).
# Output: libgstspotify.a, a build manifest, and licenses/ with the licence texts and a list of all crates linked into the archive.

set -eu

SOURCE_VERSION=0.14.4
SOURCE_URL="https://codeload.github.com/GStreamer/gst-plugins-rs/tar.gz/refs/tags/${SOURCE_VERSION}"
SOURCE_SHA256=8fbc5e0f95fba4d51bb1efcca41db359e84300e200d6bf980c3f44c0692b7986
RUST_VERSION=1.85.0
CARGO_C_VERSION=0.10.13

usage() {
  cat <<EOF
Usage: $0 --ndk DIR --gstreamer DIR --output DIR [options]

Required:
  --ndk DIR          Android NDK root (for example ~/Android/Sdk/ndk/27.2.12479018)
  --gstreamer DIR    GStreamer Android SDK directory for the ABI (for example .../gstreamer-1.28.8/arm64)
  --output DIR       Directory receiving the archive, licences and manifest

Options:
  --abi ABI          arm64-v8a (default), armeabi-v7a, x86_64 or x86
  --api LEVEL        Minimum Android API level (default 28)
  --work DIR         Download/build directory (default: OUTPUT/work)
  --debug            Build the debug profile instead of release
  --jobs N           Parallel jobs (default 2)

Requires rustup (toolchain ${RUST_VERSION} with the Android target), cargo-c ${CARGO_C_VERSION}, curl, git, jq, pkg-config and sha256sum.
EOF
}

ndk=""
gstreamer=""
output=""
abi=arm64-v8a
api=28
work=""
profile=release
jobs=2

while [ $# -gt 0 ]; do
  case "$1" in
    --ndk) ndk="$2"; shift 2 ;;
    --gstreamer) gstreamer="$2"; shift 2 ;;
    --output) output="$2"; shift 2 ;;
    --abi) abi="$2"; shift 2 ;;
    --api) api="$2"; shift 2 ;;
    --work) work="$2"; shift 2 ;;
    --debug) profile=debug; shift ;;
    --jobs) jobs="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [ -z "${ndk}" ] || [ -z "${gstreamer}" ] || [ -z "${output}" ]; then
  usage >&2
  exit 2
fi

case "${abi}" in
  arm64-v8a) triple=aarch64-linux-android; clang_triple=aarch64-linux-android ;;
  armeabi-v7a) triple=armv7-linux-androideabi; clang_triple=armv7a-linux-androideabi ;;
  x86_64) triple=x86_64-linux-android; clang_triple=x86_64-linux-android ;;
  x86) triple=i686-linux-android; clang_triple=i686-linux-android ;;
  *) echo "Unsupported ABI: ${abi}" >&2; exit 2 ;;
esac

fail() {
  echo "Error: $*" >&2
  exit 1
}

for tool in rustup curl git jq pkg-config sha256sum tar; do
  command -v "${tool}" >/dev/null 2>&1 || fail "${tool} not found in PATH"
done

ndk_bin="${ndk}/toolchains/llvm/prebuilt/linux-x86_64/bin"
cc="${ndk_bin}/${clang_triple}${api}-clang"
[ -x "${cc}" ] || fail "NDK compiler not found: ${cc}"
[ -f "${gstreamer}/lib/pkgconfig/gstreamer-1.0.pc" ] || fail "GStreamer SDK pkg-config files not found in ${gstreamer}/lib/pkgconfig"

export RUSTUP_TOOLCHAIN="${RUST_VERSION}"
rustc_version="$(rustc --version)" || fail "Rust ${RUST_VERSION} is not installed; run: rustup toolchain install ${RUST_VERSION}"
case "${rustc_version}" in
  "rustc ${RUST_VERSION} "*) ;;
  *) fail "expected rustc ${RUST_VERSION}, found: ${rustc_version}" ;;
esac
rustup target list --installed | grep -qx "${triple}" || fail "Rust target ${triple} is missing; run: rustup target add --toolchain ${RUST_VERSION} ${triple}"
cargo_c_version="$(cargo cbuild --version 2>/dev/null)" || fail "cargo-c is not installed; run: cargo +${RUST_VERSION} install --locked cargo-c --version ${CARGO_C_VERSION}"
case "${cargo_c_version}" in
  "cargo-c ${CARGO_C_VERSION}+"*|"cargo-c ${CARGO_C_VERSION}") ;;
  *) fail "expected cargo-c ${CARGO_C_VERSION}, found: ${cargo_c_version}" ;;
esac

mkdir -p "${output}"
output="$(cd "${output}" && pwd)"
[ -n "${work}" ] || work="${output}/work"
mkdir -p "${work}"
work="$(cd "${work}" && pwd)"

tarball="${work}/gst-plugins-rs-${SOURCE_VERSION}.tar.gz"
if [ ! -f "${tarball}" ] || ! echo "${SOURCE_SHA256}  ${tarball}" | sha256sum -c --status; then
  curl -fL --retry 3 -o "${tarball}.part" "${SOURCE_URL}"
  mv "${tarball}.part" "${tarball}"
fi
echo "${SOURCE_SHA256}  ${tarball}" | sha256sum -c --status || fail "checksum mismatch for ${tarball}"

source_dir="${work}/gst-plugins-rs-${SOURCE_VERSION}"
rm -rf "${source_dir}"
tar -xzf "${tarball}" -C "${work}"
[ -f "${source_dir}/Cargo.lock" ] || fail "unexpected source layout in ${tarball}"

triple_env="$(echo "${triple}" | tr 'a-z-' 'A-Z_')"
triple_cc_env="$(echo "${triple}" | tr '-' '_')"
export "CARGO_TARGET_${triple_env}_LINKER=${cc}"
export "CC_${triple_cc_env}=${cc}"
export "AR_${triple_cc_env}=${ndk_bin}/llvm-ar"
export PKG_CONFIG_ALLOW_CROSS=1
export PKG_CONFIG_LIBDIR="${gstreamer}/lib/pkgconfig"
unset PKG_CONFIG_PATH PKG_CONFIG_SYSROOT_DIR || true
export CARGO_BUILD_JOBS="${jobs}"
export CARGO_TARGET_DIR="${work}/target"
export SOURCE_DATE_EPOCH=0
# Android packaging strips debug symbols; omitting them also keeps local paths out of C objects.
export CARGO_PROFILE_RELEASE_DEBUG=false
# Keep local build paths out of the archive.
cargo_home="${CARGO_HOME:-${HOME}/.cargo}"
export RUSTFLAGS="--remap-path-prefix=${source_dir}=/gst-plugins-rs --remap-path-prefix=${cargo_home}=/cargo --remap-path-prefix=${CARGO_TARGET_DIR}=/target"
export "CFLAGS_${triple_cc_env}=-ffile-prefix-map=${source_dir}=/gst-plugins-rs -ffile-prefix-map=${cargo_home}=/cargo -ffile-prefix-map=${CARGO_TARGET_DIR}=/target -ffile-prefix-map=${ndk}=/ndk"

profile_arg=--release
[ "${profile}" = release ] || profile_arg=""

cd "${source_dir}"
# shellcheck disable=SC2086
cargo cbuild --locked ${profile_arg} -p gst-plugin-spotify --target "${triple}" --library-type staticlib

archive="${CARGO_TARGET_DIR}/${triple}/${profile}/libgstspotify.a"
[ -f "${archive}" ] || fail "build did not produce ${archive}"
cp "${archive}" "${output}/libgstspotify.a"

licenses="${output}/licenses"
rm -rf "${licenses}"
mkdir -p "${licenses}/gst-plugin-spotify-${SOURCE_VERSION}"
cp "${source_dir}/audio/spotify/LICENSE-MPL-2.0" "${licenses}/gst-plugin-spotify-${SOURCE_VERSION}/"

# Crates linked into the archive for the target; build scripts and proc macros only run on the host.
cargo tree --locked -p gst-plugin-spotify --target "${triple}" -e normal,no-proc-macro --prefix none --format '{p} | {l}' \
  | sed -e 's/ (\*)$//' -e "s# (${source_dir}[^)]*)##" -e 's/ | $/ | see the licence file in the crate source/' | sort -u > "${licenses}/gst-plugin-spotify-dependencies.txt"

# Some published crates omit their licence file; use the upstream file from the commit recorded in the crate.
upstream_license() {
  case "$1" in
    governor-0.10.2) echo "https://raw.githubusercontent.com/boinkor-net/governor/b28d8b0c123714082fca5275941d8d557f7bf66a/LICENSE 2248fdedb215c73b6431d4c0419c6476c0aa19621fa94041aa7c5a3ce952e5cc" ;;
    librespot-*-0.8.0) echo "https://raw.githubusercontent.com/librespot-org/librespot/d36f9f1907e8cc9d68a93f8ebc6b627b1bf7267d/LICENSE b6e979ad38c429c5811e9eb6930040a984619786f55ebc60660db05df5325ed6" ;;
    symphonia-0.5.5|symphonia-*-0.5.5) echo "https://raw.githubusercontent.com/pdeljanov/Symphonia/6d533f26150953a882a6a111ebd13f0abf7129d5/LICENSE c76f740d1521b9bed9ca7a04ad526c310493c62621b1341d623b431736533b30" ;;
  esac
}

metadata="${work}/metadata.json"
cargo metadata --locked --format-version 1 --filter-platform "${triple}" > "${metadata}"
rm -f "${work}/missing-licenses.txt"
cargo tree --locked -p gst-plugin-spotify --target "${triple}" -e normal,no-proc-macro --prefix none --format '{p}' \
  | sed 's/ (.*//' | sort -u | while read -r name version; do
  [ "${name}" = gst-plugin-spotify ] && continue
  version="${version#v}"
  manifest="$(jq -r --arg n "${name}" --arg v "${version}" 'first(.packages[] | select(.name == $n and .version == $v) | .manifest_path)' "${metadata}")"
  [ -n "${manifest}" ] && [ "${manifest}" != null ] || fail "no metadata for ${name} ${version}"
  crate_dir="$(dirname "${manifest}")"
  destination="${licenses}/${name}-${version}"
  mkdir -p "${destination}"
  find "${crate_dir}" -maxdepth 1 \( -type f -o -type l \) \( -iname 'licen[cs]e*' -o -iname 'copying*' -o -iname 'notice*' -o -iname 'copyright*' -o -iname 'unlicense*' -o -iname 'mpl-*' -o -iname 'lgpl-*' \) \
    -exec cp -L {} "${destination}/" \;
  if [ -z "$(ls -A "${destination}")" ]; then
    upstream="$(upstream_license "${name}-${version}")"
    if [ -z "${upstream}" ]; then
      echo "${name} ${version}" >> "${work}/missing-licenses.txt"
      continue
    fi
    url="${upstream% *}"
    sha256="${upstream#* }"
    cached="${work}/upstream-licenses/${sha256}"
    if [ ! -f "${cached}" ]; then
      mkdir -p "${work}/upstream-licenses"
      curl -fsSL --retry 3 -o "${cached}.part" "${url}"
      mv "${cached}.part" "${cached}"
    fi
    echo "${sha256}  ${cached}" | sha256sum -c --status || fail "checksum mismatch for ${url}"
    cp "${cached}" "${destination}/LICENSE"
  fi
done
if [ -s "${work}/missing-licenses.txt" ]; then
  missing="$(cat "${work}/missing-licenses.txt")"
  rm -f "${work}/missing-licenses.txt"
  fail "no licence text found for: ${missing}"
fi

archive_sha256="$(sha256sum "${output}/libgstspotify.a" | cut -d' ' -f1)"
cat > "${output}/gst-plugin-spotify-build.txt" <<EOF
source: ${SOURCE_URL}
source_sha256: ${SOURCE_SHA256}
rustc: ${rustc_version}
cargo-c: ${cargo_c_version}
abi: ${abi} (${triple}, API ${api})
profile: ${profile}
ndk: $(basename "${ndk}")
gstreamer: $(pkg-config --modversion gstreamer-1.0)
archive_sha256: ${archive_sha256}
EOF

echo "Built ${output}/libgstspotify.a (${archive_sha256})"
echo "Configure Strawberry with -DENABLE_ANDROID_SPOTIFY_PLUGIN_PROBE=ON -DANDROID_SPOTIFY_PLUGIN_ARCHIVE=${output}/libgstspotify.a"
