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

Help > OpenAxis Diagnostics opens a compact connection, focus, and gesture panel.
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

## Fork lifecycle and CI

Current development branch: `rotatrix/work/v02.08.02.61`. It is disposable work,
not a maintained release. No maintained branch or release tag has been created.
The work branch is temporarily the GitHub default so its CI and documentation
are visible. Promote a cleaned patch stack to `rotatrix/v02.08.02.61` when ready,
and then set that maintained branch as the default. Preserve its history after
publication; contributors target the applicable maintained branch.

The upstream `build_all.yml` remains the CI entry point and retains its upstream
jobs. Rotatrix refs call `openaxis-build.yml`, adapting the upstream build scripts
and packaging with OpenAxis enabled. The adapter retains the validated VS 2022
compiler for this release and pins CMake 3.31.6. It builds Windows x64, Intel and
ARM64 macOS (macOS 15 and 26 runners), and Ubuntu 22.04, 24.04, and 26.04 x64.
Dependency caches are isolated by platform and dependency-source hash.

Pushes to `rotatrix/**` and PRs into `rotatrix/*` run build, viewport tests, and
package smoke checks. Artifacts include the full source SHA in their names and
expire after 14 days. Manual runs can select an OS family. Normal builds never
create tags or GitHub releases.

Only pushes of `*-rotatrix.*` tags enter the release job. For this upstream base,
use `v02.08.02.61-rotatrix.N` or `v02.08.02.61-rotatrix.N-beta.N`. Tags are immutable;
existing release assets are never replaced. Final tags must identify a commit on
`rotatrix/v02.08.02.61`; beta tags may identify tested development work. The gate
verifies every package checksum, source SHA, SDK SHA, and workflow run ID before
staging a draft (beta tags are marked prerelease).

Release readiness is intentionally separate from work-branch CI: distribution
signing/notarization credentials are not configured. Current Windows artifacts
are unsigned and macOS artifacts are ad-hoc signed. Before publishing a final
release, configure signing/notarization, produce the signed distributions, and
complete GUI/device validation. Do not publish the unsigned draft as a final
release. The current development setup does not publish releases automatically.

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
paths before reusing the dependency prefix. The 2026-10-16 Windows preparation
uses `build_local`, retaining the older `build/BambuStudio` app untouched.
## Local macOS build and package

Use the workspace's CMake 3.31.6 on the build process PATH; do not change global
compiler or tool defaults. The upstream scripts retain their persistent
`deps/build/<arch>` and `build/<arch>` directories. For example, from this checkout:

```sh
export PATH="/Users/dliu/code/oss-forks/tools/cmake/3.31.6/bin:$PATH"
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

CI uploads the tar.gz directly with no outer ZIP. Checksums/source metadata and
build logs are separate artifacts; the release validator downloads these alongside
the archive. This transport change does not add distribution signing, notarization
or release publication. Intel, ARM64/macOS 15 and ARM64/macOS 26 variants remain.

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

On hosts with Homebrew zstd, Boost iostreams can retain a shared zstd dependency.
After upstream bundle assembly, packaging uses CMake's `BundleUtilities` to copy
it into `Contents/Frameworks`, rewrite load paths, and verify runtime closure.
Its installed BSD license is copied into Resources before signing. Unexpected
external runtime dependencies fail packaging for license/bundling review. The
extracted archive is still signature-verified and smoke-tested. This supplements
the upstream app assembly, which contains no runtime-dylib fixup step.

Static macOS builds also restrict GLEW config discovery to `CMAKE_PREFIX_PATH`
before the existing module fallback. Homebrew's GLEW config can embed framework
paths from a different SDK, causing Objective-C++ compilation to mix SDK headers.
When reusing a cache that already selected Homebrew GLEW, clear it once with
`-UGLEW_DIR`. The built `libGLEW.a` and include directory should resolve inside
the dependency prefix. This retains Retina support and availability warnings.

Lacroix validation environment: macOS 15.7.4 arm64, Xcode 26.3/Apple clang 17,
macOS SDK 26.2, CMake 3.31.6, Ninja 1.13.1, Release deployment target 11.3.
Dependencies were built with one worker; application validation used up to three
as competing builds subsided. No global toolchain defaults were changed.

The wxWidgets link list is filtered on macOS to remove XQuartz GL/GLU entries;
the existing Apple OpenGL framework supplies those APIs. Assimp uses its bundled
unzip implementation with system zlib, avoiding host minizip discovery while
retaining the upstream OBJ, FBX and glTF importer set.

Local validation completed on the above Lacroix environment: full arm64 build,
OpenAxis controller and Retina compilation, overlay test, runtime dependency
closure, ad-hoc signature verification, and extracted archive `--help` smoke.
The only non-system dylib is the bundled zstd. GUI/physical-device acceptance,
Intel builds, macOS 26 host testing, and downloaded CI artifact testing remain
separate checks; this local build does not establish them.
