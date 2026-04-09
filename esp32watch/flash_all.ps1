param(
    [string]$EnvName = 'esp32s3_n16r8_dev'
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

Write-Host "Uploading LittleFS image for $EnvName..."
& pio run -e $EnvName -t uploadfs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Cleaning build cache before firmware upload..."
& pio run -e $EnvName -t clean
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Uploading firmware for $EnvName..."
& pio run -e $EnvName -t upload
exit $LASTEXITCODE
