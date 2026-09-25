#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
platform=${1:?Platform required}
stage=${2:?Stage required}
export CMAKE_BUILD_PARALLEL_LEVEL=${CMAKE_BUILD_PARALLEL_LEVEL:-2}
export SLIC3R_OPENAXIS=ON
if [[ "$platform" == macos ]]; then
  export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
  export PATH="$(brew --prefix)/opt/gettext/bin:$(brew --prefix)/opt/texinfo/bin:$PATH"
fi
if [[ "$stage" == deps ]]; then
  case "$platform" in
    linux) ./BuildLinux.sh -dfr ;;
    macos) ./BuildMac.sh -dx -a arm64 -t 11.3 ;;
    *) exit 2 ;;
  esac
elif [[ "$stage" == build ]]; then
  case "$platform" in
    linux)
      ./BuildLinux.sh -sfr
      ;;
    macos)
      ./BuildMac.sh -sx -a arm64 -t 11.3
      app=build/arm64/BambuStudio/BambuStudio.app
      codesign --force --deep --sign - "$app"
      codesign --verify --deep --strict "$app"
      "$app/Contents/MacOS/BambuStudio" --help
      ;;
    *) exit 2 ;;
  esac
  cmake -S tests/openaxis -B build/openaxis-checks -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build/openaxis-checks
  ctest --test-dir build/openaxis-checks --output-on-failure
else
  exit 2
fi
