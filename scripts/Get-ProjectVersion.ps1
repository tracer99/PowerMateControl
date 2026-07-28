# Shared helpers for reading/writing project version metadata.
$ErrorActionPreference = "Stop"

function Get-RepoRoot {
    Resolve-Path (Join-Path $PSScriptRoot "..")
}

function Get-ProjectVersion {
    $versionPath = Join-Path (Get-RepoRoot) "VERSION"
    $raw = (Get-Content -Path $versionPath -Raw).Trim()
    if ($raw -notmatch '^\d+\.\d+\.\d+$') {
        throw "VERSION must be semver MAJOR.MINOR.PATCH (got '$raw')"
    }
    return $raw
}

function Split-ProjectVersion([string]$Version) {
    $parts = $Version.Split('.')
    return [pscustomobject]@{
        Major = [int]$parts[0]
        Minor = [int]$parts[1]
        Patch = [int]$parts[2]
        String = $Version
    }
}

function Set-ProjectVersionFiles([string]$Version) {
    $v = Split-ProjectVersion $Version
    $root = Get-RepoRoot

    Set-Content -Path (Join-Path $root "VERSION") -Value "$Version`n" -NoNewline

    $header = @"
#pragma once

// Keep in sync with the repo-root VERSION file (scripts/release.ps1 updates both).
#define PMC_VERSION_MAJOR $($v.Major)
#define PMC_VERSION_MINOR $($v.Minor)
#define PMC_VERSION_PATCH $($v.Patch)
#define PMC_VERSION_BUILD 0

#define PMC_VERSION_COMMA PMC_VERSION_MAJOR,PMC_VERSION_MINOR,PMC_VERSION_PATCH,PMC_VERSION_BUILD
#define PMC_VERSION_STRING "$Version"
"@
    Set-Content -Path (Join-Path $root "src\version.h") -Value $header

    return $v
}

function Get-ChangelogSection([string]$Version) {
    $changelogPath = Join-Path (Get-RepoRoot) "CHANGELOG.md"
    $text = Get-Content -Path $changelogPath -Raw

    # Stop at the next ## heading or the reference-link footer ([label]: url).
    $pattern = "(?ms)^## \[$([regex]::Escape($Version))\][^\n]*\n(.*?)(?=^## \[|^\[[^\]]+\]:\s+\S|\z)"
    $match = [regex]::Match($text, $pattern)
    if (-not $match.Success) {
        throw "CHANGELOG.md has no '## [$Version]' section"
    }

    $body = $match.Groups[1].Value.Trim()
    if ([string]::IsNullOrWhiteSpace($body)) {
        throw "CHANGELOG.md section for $Version is empty"
    }

    return $body
}

function Test-ChangelogHasVersion([string]$Version) {
    $null = Get-ChangelogSection $Version
}
