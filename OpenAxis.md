# OpenAxis navigation

Based on upstream stable release `v02.08.02.61`, commit
`926a7192574bcb9b3a732e1ec59a46d79cb45466`.

Enable the optional native GUI integration with `-DSLIC3R_OPENAXIS=ON` when
configuring BambuStudio. CMake 3.24 or newer fetches the OpenAxis C++ SDK at
`cpp/v1.0.0-rc.1`; `-DOPENAXIS_SOURCE_DIR=/path/to/openaxis` uses a local checkout.
The normal BambuStudio dependencies are still required. With the option off,
the application does not fetch or link the SDK. The integration supplies the SDK
with BambuStudio's bundled JSON target so both use the same JSON types and ABI.

The SDK connects to Rotatrix on localhost and schedules navigation on the wx UI
thread. Each viewport owns its connection and cancels gestures when focus,
model, plate, dimensions, or scene revision change. Pose writes use the native
camera and projection calculation, returning the realized pose after clamping;
native camera changes are reconciled with the session.

Picks use BambuStudio mesh-raycasting helpers with scene-scoped owned mesh snapshots, without changing native hover state. Plate top surfaces participate in ordinary picks; toolbar icons and gizmos do not. Selection picks use selected model volumes. Plate shapes already include their world placement.

In Preview, ordinary picks also hit visible G-code extrusion surfaces using the
native renderer's layer, move-range and feature filters. Travel/wipe moves and
the nozzle marker are excluded. Empty areas still fall back to plate picking.

View > OpenAxis Diagnostics opens a compact connection, focus, and gesture panel.
Viewport evidence is enabled while the panel is open; the pivot remains visible
independently. Logs are available in the external Rotatrix log viewer.

## Verification

The dependency-free overlay checks cover clipping, perspective division, DPI,
and viewport offsets:

```
cmake -S tests/openaxis -B build_openaxis_checks
cmake --build build_openaxis_checks --config Release
ctest --test-dir build_openaxis_checks -C Release --output-on-failure
```

In a GUI build, verify rotation, pan, zoom, native mouse input after navigation,
plate switching, selection-only picking, and focus loss in Prepare and Preview.
Open and close the diagnostics panel and confirm viewport evidence follows it.

## Local Windows build

Use a Visual Studio 2022 x64 developer shell with CMake, native Perl, and
`pkg-config.exe` on PATH. PowerShell 7 is required by the build script.

```powershell
pwsh -File .github/scripts/openaxis-windows.ps1 deps
pwsh -File .github/scripts/openaxis-windows.ps1 build
```

The installed application is `build/BambuStudio/bambu-studio.exe`. The build
stage runs the viewport tests and launches the installed application with
`--help` as a loader smoke check. It does not exercise a connected Rotatrix
or interactive GUI navigation.

For a workspace-local Node/pnpm cache, configure with
`-DNODE_CACHE_DIR=<oss-forks>/tools/node/22.22.2`. The pinned platform
distributions remain in versioned subdirectories; the upstream default cache
location is unchanged. Select `<oss-forks>/tools/cmake/3.31.6/bin/cmake.exe`
explicitly, without changing global toolchains. Set `CI=true` in the build
process environment when running unattended so pnpm can recreate its modules
after a checkout/build-directory relocation without an interactive prompt.

Do not reuse a CMake build tree after moving its source checkout. Configure a
fresh build directory and inspect installed dependency metadata for absolute
paths before reusing the dependency prefix.
## Local macOS build and package

Use the workspace's CMake 3.31.6 on the build process PATH; do not change global
compiler or tool defaults. The upstream scripts retain their persistent
`deps/build/<arch>` and `build/<arch>` directories. For example, from this checkout:

```sh
export PATH="$OSS_FORKS/tools/cmake/3.31.6/bin:$PATH"
export OPENAXIS_MAC_ARCH=arm64 CMAKE_BUILD_PARALLEL_LEVEL=1
bash .github/scripts/openaxis-unix.sh macos deps
bash .github/scripts/openaxis-unix.sh macos build
python3 .github/scripts/openaxis-package.py macos-arm64
```

The installed build bundle is `build/arm64/BambuStudio/BambuStudio.app` (substitute
`x86_64` for Intel). The packaging step adds SDK license notices before ad-hoc
signing, archives exactly `BambuStudio.app` in
`dist/macos-app-<variant>-<full-source-SHA>.tar.gz`, then extracts the archive,
verifies its signature and runs the extracted executable with `--help`.
Executable permissions and internal symlinks are preserved. The bundle name,
`com.bambulab.bambu-studio` identifier, and upstream preference locations remain
unchanged. Test the extracted app without replacing a running upstream app.

CI uploads packages directly, without an outer ZIP. Release preparation promotes
existing build artifacts and uses the central macOS signing service.

Packaging contract checks (no signing identity required):

```sh
python3 -B tests/openaxis/test_release.py
python3 -B tests/openaxis/test_macos_package.py
```

### macOS dependency isolation

The macOS build selects Apple's OpenGL framework explicitly so an installed
XQuartz does not supply the application's OpenGL library. The dependency build
pins OpenEXR 2.5.5; application configuration disables discovery of the unrelated
Imath 3 package and uses the built IlmBase instead. This does not disable OpenVDB:
OpenVDB 8.2.0 and its static library remain enabled from the same dependency prefix.
The equivalent local configure options are:

```sh
-DCMAKE_DISABLE_FIND_PACKAGE_Imath=ON
-DOPENGL_gl_LIBRARY="$(xcrun --sdk macosx --show-sdk-path)/System/Library/Frameworks/OpenGL.framework"
```

The macOS dependency pipeline pins zstd 1.5.7 from its checksum-verified official
source archive. It builds shared zstd with multithreading for the original targets:
arm64 macOS 11.3 and x86_64 macOS 10.15. Boost, OpenVDB and the app explicitly use
that architecture's dependency prefix, including when reusing caches that formerly
selected Homebrew. After upstream bundle assembly, packaging uses CMake's
`BundleUtilities` to copy zstd into `Contents/Frameworks`, rewrite load paths, and
verify runtime closure. Its installed BSD license is copied from
`share/licenses/zstd/LICENSE` into Resources before signing. Unexpected
external runtime dependencies fail packaging for license/bundling review. The
extracted archive is still signature-verified and smoke-tested. This supplements
the upstream app assembly, which contains no runtime-dylib fixup step.

Static macOS builds also restrict GLEW config discovery to `CMAKE_PREFIX_PATH`
before the existing module fallback. Homebrew's GLEW config can embed framework
paths from a different SDK, causing Objective-C++ compilation to mix SDK headers.
When reusing a cache that already selected Homebrew GLEW, clear it once with
`-UGLEW_DIR`. The built `libGLEW.a` and include directory should resolve inside
the dependency prefix. This retains Retina support and availability warnings.

The wxWidgets link list is filtered on macOS to remove XQuartz GL/GLU entries;
the existing Apple OpenGL framework supplies those APIs. Assimp uses its bundled
unzip implementation with system zlib, avoiding host minizip discovery while
retaining the upstream OBJ, FBX and glTF importer set.

Packaging inspects every bundled Mach-O executable and dylib with `vtool` for the
selected architecture before signing. A minimum OS newer than the variant's target
fails packaging. `tests/openaxis/test_macos_deployment.py` exercises acceptance and
rejection using real compiled Mach-O fixtures. This catches the former Homebrew
bottle dependency (macOS 15 minimum) even when loader smoke succeeds on the host.
