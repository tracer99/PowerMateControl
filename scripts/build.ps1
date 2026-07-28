# Requires an MSVC x64 developer environment (cl, rc, link on PATH).
param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "Get-ProjectVersion.ps1")

$RepoRoot = Get-RepoRoot
$Version = Get-ProjectVersion

if (-not $OutDir) {
    $OutDir = Join-Path $RepoRoot "build"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$SrcDir = Join-Path $RepoRoot "src"
$ExePath = Join-Path $OutDir "PowerMateControl.exe"
$ObjDir = Join-Path $OutDir "obj"
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

$CppSources = @(
    "main.cpp",
    "PowermateManager.cpp",
    "ProfileManager.cpp",
    "TriggerAction.cpp",
    "trayIcon.cpp"
)

$ResPath = Join-Path $ObjDir "resource.res"

# Trailing path separator required so cl writes objects into the directory.
$ObjPrefix = $ObjDir.TrimEnd('\', '/') + '\'

$CommonFlags = @(
    "/nologo",
    "/EHsc",
    "/std:c++17",
    "/W3",
    "/DUNICODE",
    "/D_UNICODE",
    "/DWIN32_LEAN_AND_MEAN",
    "/D_CRT_SECURE_NO_WARNINGS",
    "/Fo$ObjPrefix",
    "/Fe$ExePath"
)

$LinkFlags = @(
    $ResPath,
    "/link",
    "/SUBSYSTEM:WINDOWS",
    "/MACHINE:X64",
    "setupapi.lib",
    "hid.lib",
    "shell32.lib",
    "user32.lib",
    "gdi32.lib",
    "advapi32.lib"
)

if ($Configuration -eq "Debug") {
    $CompileFlags = @("/Zi", "/Od") + $CommonFlags
    $LinkFlags += @("/DEBUG")
} else {
    $CompileFlags = @("/O2") + $CommonFlags
}

Push-Location $SrcDir
try {
    & rc /nologo /fo $ResPath resource.rc
    if ($LASTEXITCODE -ne 0) {
        throw "rc failed with exit code $LASTEXITCODE"
    }

    & cl @CompileFlags @CppSources @LinkFlags
    if ($LASTEXITCODE -ne 0) {
        throw "cl failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

Write-Host "Built $ExePath ($Configuration) version $Version"
