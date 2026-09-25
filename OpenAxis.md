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

## Preview builds

`.github/workflows/openaxis-build.yml` builds Windows x64, macOS ARM64, and
Linux x64 packages on pushes to `rotatrix/stable`. Version branches such as
`rotatrix/2.8.2.61` are built manually with `workflow_dispatch`, which can select a
single platform for debugging. Advance `rotatrix/stable` to a tested version
commit to start the release build; pushing version branches does not duplicate it. Installed dependencies are cached separately per
platform and dependency-source hash.

After all three builds, overlay checks, and package smoke checks pass, the
workflow creates a draft prerelease only for `rotatrix/stable`. Manual version
branch builds produce test artifacts without creating a release. The release gate checks each package's
checksum, full source commit, SDK revision, and workflow run ID; it refuses to
modify a published release. GUI and device testing are still required before
publishing. Windows packages are unsigned; macOS bundles are ad-hoc signed.

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