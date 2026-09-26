param([int]$Attempts = 3, [int]$RetryDelaySeconds = 10)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
    choco install strawberryperl pkgconfiglite --yes --no-progress
    $installExit = $LASTEXITCODE
    # Chocolatey can return zero after a feed timeout. Verify native tools, not
    # just its exit code, before spending time compiling the dependencies.
    $perl = Get-Command perl.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $perl -and (Test-Path 'C:\Strawberry\perl\bin\perl.exe')) {
        $perl = Get-Item 'C:\Strawberry\perl\bin\perl.exe'
    }
    $pkgConfig = Get-Command pkg-config.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $pkgConfig -and $env:ChocolateyInstall) {
        $packageRoot = Join-Path $env:ChocolateyInstall 'lib/pkgconfiglite'
        if (Test-Path -LiteralPath $packageRoot) {
            $pkgConfig = Get-ChildItem -LiteralPath $packageRoot -Filter pkg-config.exe -Recurse -File | Select-Object -First 1
        }
    }
    if ($perl -and $pkgConfig) {
        $perlPath = if ($perl.Source) { $perl.Source } else { $perl.FullName }
        $pkgConfigPath = if ($pkgConfig.Source) { $pkgConfig.Source } else { $pkgConfig.FullName }
        & $perlPath --version
        $perlExit = $LASTEXITCODE
        & $pkgConfigPath --version
        $pkgConfigExit = $LASTEXITCODE
        if ($perlExit -eq 0 -and $pkgConfigExit -eq 0) {
            @((Split-Path $perlPath), (Split-Path $pkgConfigPath)) |
                Out-File -FilePath $env:GITHUB_PATH -Append -Encoding utf8
            Write-Host "Verified Perl: $perlPath"
            Write-Host "Verified pkg-config: $pkgConfigPath"
            exit 0
        }
    }
    Write-Warning "Windows prerequisites incomplete after attempt $attempt/$Attempts (Chocolatey exit $installExit)."
    if ($attempt -lt $Attempts) { Start-Sleep -Seconds $RetryDelaySeconds }
}
throw "Native Perl and pkg-config.exe must be installed and runnable before building dependencies."
