param([ValidateSet('deps', 'build')][string]$Stage)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
Set-Location (Resolve-Path "$PSScriptRoot/../..")
if ($Stage -eq 'deps') {
    cmake -S deps -B deps/build -G 'Visual Studio 17 2022' -A x64 `
        "-DDESTDIR=$PWD/deps/build/BambuStudio_dep" -DCMAKE_BUILD_TYPE=Release -DDEP_DEBUG=OFF
    cmake --build deps/build --config Release --target deps --parallel 1
} else {
    cmake -S . -B build -G 'Visual Studio 17 2022' -A x64 `
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_CONFIGURATION_TYPES=Release `
        -DBBL_RELEASE_TO_PUBLIC=1 -DBBL_INTERNAL_TESTING=0 `
        "-DCMAKE_PREFIX_PATH=$PWD/deps/build/BambuStudio_dep/usr/local" `
        "-DCMAKE_INSTALL_PREFIX=$PWD/build/BambuStudio" `
        -DSLIC3R_OPENAXIS=ON -DOPENAXIS_SOURCE_DIR= -DSLIC3R_PCH=ON
    cmake --build build --config Release --parallel 2
    cmake --install build --config Release
    cmake -S tests/openaxis -B build/openaxis-checks -G 'Visual Studio 17 2022' -A x64
    cmake --build build/openaxis-checks --config Release --parallel 2
    ctest --test-dir build/openaxis-checks -C Release --output-on-failure
    $smoke = Start-Process -FilePath "$PWD/build/BambuStudio/bambu-studio.exe" -ArgumentList '--help' `
        -WorkingDirectory "$PWD/build/BambuStudio" -WindowStyle Hidden -PassThru
    if (-not $smoke.WaitForExit(60000)) { $smoke.Kill(); throw 'Packaged application smoke check timed out' }
    if ($smoke.ExitCode -ne 0) { throw "Packaged application smoke check failed: $($smoke.ExitCode)" }
}
