# Requires a matching MSVC developer environment (cl, rc, link on PATH).
# x64:   Launch-VsDevShell.ps1 -Arch amd64
# ARM64: Launch-VsDevShell.ps1 -Arch arm64
#        (add -HostArch amd64 when cross-compiling from x64 Windows)
param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",
    [ValidateSet("x64", "ARM64")]
    [string]$Architecture = "x64",
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "Get-ProjectVersion.ps1")

$RepoRoot = Get-RepoRoot
$Version = Get-ProjectVersion

$ArchFolder = $Architecture.ToLowerInvariant()
$Machine = @{
    x64   = "X64"
    ARM64 = "ARM64"
}[$Architecture]

$ExpectedTgtArch = @{
    x64   = "x64"
    ARM64 = "arm64"
}[$Architecture]

$DevShellHint = if ($Architecture -eq "ARM64") {
    "Launch-VsDevShell.ps1 -Arch arm64 (add -HostArch amd64 when cross-compiling from x64 Windows)"
} else {
    "Launch-VsDevShell.ps1 -Arch amd64"
}

if ($env:VSCMD_ARG_TGT_ARCH -and $env:VSCMD_ARG_TGT_ARCH -ne $ExpectedTgtArch) {
    throw "MSVC target arch is '$($env:VSCMD_ARG_TGT_ARCH)' but -Architecture $Architecture was requested. Use $DevShellHint."
}

$clCmd = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $clCmd) {
    throw "cl.exe is not on PATH. Open a matching VS developer shell first ($DevShellHint)."
}

# MSVC layout: ...\bin\Host<host>\<target>\cl.exe (target is x86, x64, or ARM64).
$clTarget = Split-Path (Split-Path $clCmd.Source -Parent) -Leaf
if ($clTarget -and $clTarget -ne $Machine) {
    throw "cl.exe is the $clTarget toolchain ($($clCmd.Source)) but -Architecture $Architecture was requested. Use $DevShellHint."
}

if (-not $OutDir) {
    $OutDir = Join-Path $RepoRoot "build\$ArchFolder"
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
    "trayIcon.cpp",
    "Settings.cpp",
    "AudioVolume.cpp",
    "LedController.cpp",
    "AboutDialog.cpp"
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
    "/MACHINE:$Machine",
    "setupapi.lib",
    "hid.lib",
    "shell32.lib",
    "user32.lib",
    "gdi32.lib",
    "advapi32.lib",
    "ole32.lib",
    "windowscodecs.lib"
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

Write-Host "Built $ExePath ($Configuration $Architecture) version $Version"
