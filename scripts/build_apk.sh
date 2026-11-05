#!/usr/bin/env bash
# Build the xdesk android application without Gradle (aapt2, javac, d8, zipalign, apksigner)
#   output: android/build/xdesk.apk + android/build/version.txt (embedded in the binary by CMake when present)
# Environment:
#   ANDROID_HOME / ANDROID_SDK_ROOT   SDK folder (default ~/Android/Sdk)
#   XDESK_BUILD_TOOLS                 build-tools version (default: the newest installed)
#   XDESK_PLATFORM                    platform (default: the newest android-NN installed)
#   XDESK_KEYSTORE                    keystore (default android/release.jks, generated when missing)
#   XDESK_KEYSTORE_PASS               its password (default "xdesk-local")
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP="$ROOT/android"
OUT="$APP/build"
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}"
PASS="${XDESK_KEYSTORE_PASS:-xdesk-local}"
KEYSTORE="${XDESK_KEYSTORE:-$APP/release.jks}"

die() { echo "build_apk: $*" >&2; exit 1; }

# Tools
[ -d "$SDK" ] || die "Android SDK not found ($SDK): set ANDROID_HOME"
TOOLS_VERSION="${XDESK_BUILD_TOOLS:-$(find "$SDK/build-tools" -mindepth 1 -maxdepth 1 -printf '%f\n' | sort -V | tail -1)}"
TOOLS="$SDK/build-tools/$TOOLS_VERSION"
PLATFORM="${XDESK_PLATFORM:-$(find "$SDK/platforms" -mindepth 1 -maxdepth 1 -regex '.*/android-[0-9]+' -printf '%f\n' | sort -V | tail -1)}"
ANDROID_JAR="$SDK/platforms/$PLATFORM/android.jar"
[ -x "$TOOLS/aapt2" ] || die "aapt2 missing in $TOOLS"
[ -f "$ANDROID_JAR" ] || die "android.jar missing ($ANDROID_JAR)"
for tool in javac keytool zip; do command -v "$tool" > /dev/null || die "$tool is required"; done

# Version: project(... VERSION x.y.z) of the CMakeLists, code = x * 10000 + y * 100 + z
VERSION="$(sed -n 's/^project([^)]*VERSION \([0-9][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)"
[ -n "$VERSION" ] || die "version not found in CMakeLists.txt"
IFS=. read -r MAJOR MINOR PATCH <<< "$VERSION"
CODE=$(( ${MAJOR:-0} * 10000 + ${MINOR:-0} * 100 + ${PATCH:-0} ))
TARGET_SDK="${PLATFORM#android-}"
TARGET_SDK="${TARGET_SDK%%.*}"

echo "build_apk: xdesk $VERSION ($CODE), build-tools $TOOLS_VERSION, $PLATFORM"
rm -rf "$OUT"
command mkdir -p "$OUT/gen" "$OUT/classes" "$OUT/dex"

# Resources + manifest
"$TOOLS/aapt2" compile --dir "$APP/res" -o "$OUT/res.zip"
"$TOOLS/aapt2" link -I "$ANDROID_JAR" --manifest "$APP/AndroidManifest.xml" -o "$OUT/base.apk" \
    --java "$OUT/gen" --min-sdk-version 24 --target-sdk-version "$TARGET_SDK" \
    --version-code "$CODE" --version-name "$VERSION" "$OUT/res.zip"

# Java -> dex
find "$APP/src" "$OUT/gen" -name '*.java' > "$OUT/sources.txt"
javac --release 11 -encoding UTF-8 -classpath "$ANDROID_JAR" -d "$OUT/classes" @"$OUT/sources.txt"
find "$OUT/classes" -name '*.class' > "$OUT/classes.txt"
"$TOOLS/d8" --release --min-api 24 --lib "$ANDROID_JAR" --output "$OUT/dex" @"$OUT/classes.txt"
(cd "$OUT/dex" && zip -qj "$OUT/base.apk" classes.dex)

# Align + sign (local key generated once, never committed; the CI gives its own)
"$TOOLS/zipalign" -p -f 4 "$OUT/base.apk" "$OUT/aligned.apk"
if [ ! -f "$KEYSTORE" ]; then
    echo "build_apk: generating the signing key $KEYSTORE"
    keytool -genkeypair -keystore "$KEYSTORE" -storepass "$PASS" -keypass "$PASS" -alias xdesk \
        -keyalg EC -groupname secp256r1 -sigalg SHA256withECDSA -validity 10000 -dname "CN=xdesk" > /dev/null
    chmod 600 "$KEYSTORE"
fi
"$TOOLS/apksigner" sign --ks "$KEYSTORE" --ks-pass "pass:$PASS" --ks-key-alias xdesk --out "$OUT/xdesk.apk" "$OUT/aligned.apk"
"$TOOLS/apksigner" verify "$OUT/xdesk.apk"
echo "$VERSION" > "$OUT/version.txt"
rm -rf "$OUT/gen" "$OUT/classes" "$OUT/dex" "$OUT/res.zip" "$OUT/base.apk" "$OUT/aligned.apk" "$OUT/sources.txt" "$OUT/classes.txt" "$OUT"/*.idsig
echo "build_apk: $OUT/xdesk.apk ($(du -h "$OUT/xdesk.apk" | cut -f1))"
