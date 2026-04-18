param(
    [string]$EnvName = 'esp32s3_n16r8_dev'
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

Write-Host "Building firmware for $EnvName..."
& pio run -e $EnvName
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Building LittleFS image for $EnvName..."
& pio run -e $EnvName -t buildfs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Uploading firmware for $EnvName..."
& pio run -e $EnvName -t upload
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Uploading LittleFS image for $EnvName..."
& pio run -e $EnvName -t uploadfs
exit $LASTEXITCODE
