<#
.SYNOPSIS
  Prepare or tag a release using VERSION + CHANGELOG.md.

.EXAMPLE
  # Bump version metadata (VERSION, src/version.h) then edit CHANGELOG before tagging:
  .\scripts\release.ps1 -Bump minor

.EXAMPLE
  # Validate current VERSION against CHANGELOG and create annotated tag vX.Y.Z:
  .\scripts\release.ps1 -Tag

.EXAMPLE
  # Set an explicit version:
  .\scripts\release.ps1 -Version 1.1.0
#>
param(
    [ValidateSet("major", "minor", "patch")]
    [string]$Bump,

    [string]$Version,

    [switch]$Tag,

    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "Get-ProjectVersion.ps1")

function Get-NextVersion([string]$Current, [string]$Part) {
    $v = Split-ProjectVersion $Current
    switch ($Part) {
        "major" { return "{0}.0.0" -f ($v.Major + 1) }
        "minor" { return "{0}.{1}.0" -f $v.Major, ($v.Minor + 1) }
        "patch" { return "{0}.{1}.{2}" -f $v.Major, $v.Minor, ($v.Patch + 1) }
    }
}

if ($Bump -and $Version) {
    throw "Specify only one of -Bump or -Version"
}

$current = Get-ProjectVersion
$target = $current

if ($Bump) {
    $target = Get-NextVersion $current $Bump
} elseif ($Version) {
    if ($Version -notmatch '^\d+\.\d+\.\d+$') {
        throw "-Version must be MAJOR.MINOR.PATCH (got '$Version')"
    }
    $target = $Version
}

if ($target -ne $current) {
    Write-Host "Updating version $current -> $target"
    if (-not $DryRun) {
        Set-ProjectVersionFiles $target | Out-Null
    }
    Write-Host @"

Next steps:
  1. Move items from ## [Unreleased] into ## [$target] - $(Get-Date -Format yyyy-MM-dd) in CHANGELOG.md
  2. Update compare links at the bottom of CHANGELOG.md
  3. Commit the version bump
  4. Run: .\scripts\release.ps1 -Tag
"@
    if (-not $Tag) {
        return
    }
}

Test-ChangelogHasVersion $target
Write-Host "CHANGELOG.md contains section for $target"

$notes = Get-ChangelogSection $target
$tagName = "v$target"

if (-not $Tag) {
    Write-Host @"

Version $target is ready. To publish:
  git add VERSION src/version.h CHANGELOG.md
  git commit -m "release: $tagName"
  .\scripts\release.ps1 -Tag
  git push origin main $tagName
"@
    Write-Host "`n--- Release notes preview ---`n$notes"
    return
}

# Ensure working tree is clean enough for a release tag (ignore untracked).
$status = git status --porcelain
$dirty = $status | Where-Object { $_ -notmatch '^\?\?' }
if ($dirty) {
    throw "Working tree has uncommitted changes. Commit (or stash) before tagging.`n$($dirty -join "`n")"
}

$existing = git tag -l $tagName
if ($existing) {
    throw "Tag $tagName already exists"
}

$fileVersion = (Get-Content (Join-Path (Get-RepoRoot) "VERSION") -Raw).Trim()
$headerMatch = Select-String -Path (Join-Path (Get-RepoRoot) "src\version.h") -Pattern 'PMC_VERSION_STRING "([^"]+)"'
if (-not $headerMatch -or $headerMatch.Matches[0].Groups[1].Value -ne $fileVersion) {
    throw "VERSION ($fileVersion) does not match src/version.h. Re-run: .\scripts\release.ps1 -Version $fileVersion"
}
if ($fileVersion -ne $target) {
    throw "VERSION ($fileVersion) does not match release target $target"
}

$message = @"
Release $tagName

$notes
"@

if ($DryRun) {
    Write-Host ('[dry-run] would create annotated tag {0}' -f $tagName)
    Write-Host $message
    return
}

git tag -a $tagName -m $message
Write-Host "Created annotated tag $tagName"
Write-Host "Push with: git push origin main $tagName"
Write-Host "GitHub Actions will build and publish the release with these changelog notes."
