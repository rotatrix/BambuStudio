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