#!/bin/bash
# rpu-pinmame: VPX Standalone (macOS) with the RPU driver. VPX's PinMAME plugin only works with the
# libpinmame it was built against: build that PinMAME commit (platforms/config.sh PINMAME_SHA in the
# vpinball release) with pinmame-rpu.patch, put it in the app, re-sign the app for this Mac.
#   tools/install-vpx-macos.sh <PINMAME_SHA> [path/to/VPinballX_BGFX.app]
set -e
SHA=${1:?PINMAME_SHA from the VPX release (VPX 10.8.1-5436: 23321ec7de6dfd563a1c64153bc75f26e5059b9f)}
APP=${2:-$HOME/Applications/VPinballX_BGFX.app}
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
