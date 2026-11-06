#!/bin/bash
# rpu-pinmame: VPX Standalone (macOS) with the RPU driver. VPX's PinMAME plugin only works with the
# libpinmame it was built against: build that PinMAME commit (platforms/config.sh PINMAME_SHA in the
# vpinball release) with pinmame-rpu.patch, put it in the app, re-sign the app for this Mac.
#   tools/install-vpx-macos.sh <PINMAME_SHA> [path/to/VPinballX_BGFX.app]
set -e
SHA=${1:?PINMAME_SHA from the VPX release (VPX 10.8.1-5436: 23321ec7de6dfd563a1c64153bc75f26e5059b9f)}
APP=$2
if [ -z "$APP" ]; then                                  # the app: ~/Applications, else /Applications
  for a in "$HOME/Applications/VPinballX_BGFX.app" /Applications/VPinballX_BGFX.app; do
    [ -d "$a" ] && { APP=$a; break; }
  done
  [ -n "$APP" ] || { echo "VPX app not found in ~/Applications or /Applications: give its path after the SHA"; exit 1; }
fi
[ -d "$APP" ] || { echo "VPX app not found at $APP"; exit 1; }
[ -f "$APP/Contents/Frameworks/libpinmame.3.7.0.dylib" ] ||
  { echo "$APP has no Contents/Frameworks/libpinmame.3.7.0.dylib: is it VPX Standalone 10.8.1?"; exit 1; }
[ -w "$APP/Contents/Frameworks" ] || { echo "can't write into $APP: run as a user who can, or with sudo"; exit 1; }
echo "VPX: $APP"
cd "$(dirname "$0")/.."
[ -d pinmame-vpx ] || git clone -q --filter=blob:none --no-checkout https://github.com/vpinball/pinmame.git pinmame-vpx
cd pinmame-vpx
git checkout -q -f "$SHA"
git apply ../pinmame-rpu.patch
cp cmake/libpinmame/CMakeLists.txt .
cmake -DCMAKE_BUILD_TYPE=Release -DPLATFORM=macos -DARCH=arm64 -DBUILD_STATIC=OFF -B build > /dev/null
cmake --build build --target pinmame_shared -- -j4
cp build/libpinmame.3.7.0.dylib "$APP/Contents/Frameworks/libpinmame.3.7.0.dylib"
codesign --force --deep -s - "$APP"
echo "installed into $APP"
