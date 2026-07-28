<p align="center">
  <img src="res/logo.png" alt="Logo" width="150" height="150">
  <h3 align="center">PowerMateControl</h3>

  <p align="center">
    Windows 11 tray application for the Griffin PowerMate USB device
    <br/>
    Control Windows volume or simulate mouse scroll action.
  </p>
</p>

---

PowerMateControl is a Windows system tray application designed for Windows 11.  
It provides functionality for the Griffin PowerMate USB device. There is no plans to support the Bluetooth version.
<table>
  <tr>
    <td>
      <img src="res/screenshot1.png" alt="screenshot1" width="170">
    </td>
    <td style="text-align: left; vertical-align: width: 45%;">
      <p>This program offers two distinct profiles:</p>
      <ul>
        <li><strong>Scroll</strong>: Sends mouse scroll actions.</li>
        <li><strong>Volume</strong>: Controls the system volume.</li>
      </ul>
    </td>
  </tr>
</table>

## Requirements

### Runtime

| Requirement | Details |
|---|---|
| OS | Windows 11 (Win32 APIs used may also work on Windows 10) |
| Architecture | x64 |
| Hardware | Griffin **PowerMate USB** only — VID `077D`, PID `0410` |
| Drivers | Standard Windows HID driver (no custom driver or Zadig install needed) |

Bluetooth PowerMate is **not** supported.

### Build toolchain

This repo ships source only (no Visual Studio solution or CMake project). CI builds an x64 GUI binary with MSVC on `windows-latest` (Visual Studio 2022).

Install locally:

1. [Visual Studio 2022](https://visualstudio.microsoft.com/) (Community is fine), **or** [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
2. Workload: **Desktop development with C++**
3. Components (defaults are usually enough):
   - MSVC v143 C++ x64/x86 build tools
   - Windows 10/11 SDK
   - C++ ATL / Windows SDK support for HID headers (`hidsdi.h`, `setupapi.h`)

No third-party libraries are required. The app links only Windows system libraries:

- `hid` — HID device enumeration (`HidD_GetHidGuid`)
- `setupapi` — device interface discovery
- `shell32` — system tray (`Shell_NotifyIcon`)
- `user32`, `gdi32`, `advapi32` — UI, icons, registry (startup)

C++ standard: **C++17** or later (`std::atomic`, `std::thread`, `std::mutex`).

## Quick start (prebuilt)

1. Download `PowerMateControl-<version>-windows-x64.zip` from this repo’s [Releases](https://github.com/tracer99/PowerMateControl/releases) page (current: **1.2.1**).
2. Extract and run `PowerMateControl.exe`.
3. Plug in the PowerMate USB. A tray icon appears (connected / disconnected).
4. Right-click the tray icon to choose a profile, enable **Run at startup**, or **Exit**.

CI also uploads a versioned Windows x64 artifact on every push/PR to `main` if you need a build before a tagged release.

## Build from source

### 1. Clone

```powershell
git clone https://github.com/tracer99/PowerMateControl.git
cd PowerMateControl
```

### 2. Open a Visual Studio developer shell

Use an **x64 Native Tools** environment so `cl`, `rc`, and `link` are on `PATH`:

- Start menu → **x64 Native Tools Command Prompt for VS 2022**, or
- In PowerShell:

```powershell
& "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64
```

(Adjust `Community` to `Professional`, `Enterprise`, or `BuildTools` as installed.)

### 3. Compile

From the repo root (same script CI uses):

```powershell
.\scripts\build.ps1 -Configuration Release
```

Output: `build\PowerMateControl.exe`

Debug:

```powershell
.\scripts\build.ps1 -Configuration Debug
.\build\PowerMateControl.exe -debug
```

(`-debug` allocates a console for log output.)

### Visual Studio IDE (optional)

There is no `.sln` / `.vcxproj` in the tree. To work in the IDE:

1. **File → New → Project from Existing Code** (or create an empty C++ Windows Desktop project).
2. Add all files under `src\`.
3. Configuration: **x64**, Character Set **Unicode**, SubSystem **Windows**.
4. C++ Language Standard: **ISO C++17**.
5. Linker → Input → Additional Dependencies: `setupapi.lib;hid.lib;shell32.lib;%(AdditionalDependencies)`.
6. Ensure the resource compiler can find `res\connected.ico` and `res\disconnected.ico` (paths in `resource.rc` are relative to `src\`).

## Versioning and releases

This project uses [Semantic Versioning](https://semver.org/) (`MAJOR.MINOR.PATCH`) and [Keep a Changelog](https://keepachangelog.com/).

| File | Role |
|---|---|
| `VERSION` | Canonical version string (e.g. `1.2.1`) |
| `CHANGELOG.md` | Human-readable release notes per version |
| `src/version.h` | Embedded in the binary / Windows file properties |

GitHub Actions builds on every push and PR to `main`. Pushing an annotated tag `vX.Y.Z` that matches `VERSION` publishes a GitHub Release whose body is taken from that version’s changelog section, with asset `PowerMateControl-X.Y.Z-windows-x64.zip`.

### Cut a release

1. Collect changes under `## [Unreleased]` in `CHANGELOG.md` while you work.
2. Bump the version (updates `VERSION` and `src/version.h`):

```powershell
.\scripts\release.ps1 -Bump patch   # or -Bump minor / -Bump major
# or: .\scripts\release.ps1 -Version 1.1.0
```

3. Move `[Unreleased]` items into a new `## [X.Y.Z] - YYYY-MM-DD` section and refresh the compare links at the bottom of `CHANGELOG.md`.
4. Commit, tag, and push:

```powershell
git add VERSION src/version.h CHANGELOG.md
git commit -m "release: vX.Y.Z"
.\scripts\release.ps1 -Tag
git push origin main vX.Y.Z
```

The tag step fails if the changelog section is missing, `VERSION` / `version.h` disagree, or the working tree has uncommitted changes.

## Usage

| Action | Scroll profile | Volume profile |
|---|---|---|
| Rotate left | Scroll down | Volume up |
| Rotate right | Scroll up | Volume down |
| Button release | Mouse double-click | Mute / unmute |

Tray menu:

- Connection status
- Profile selection (**Scroll** / **Volume**)
- **Run at startup** — writes `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\PowerMateControl`
- **Exit**

Only one instance can run (named mutex `UniqueAppMutexName`). Hot-plug, unplug, and sleep/resume are handled via `WM_DEVICECHANGE` / `WM_POWERBROADCAST`.

## Project layout

```
PowerMateControl/
├── .github/workflows/      # CI build + tagged releases
├── CHANGELOG.md            # Keep a Changelog release notes
├── VERSION                 # Semver source of truth (1.2.1)
├── LICENSE
├── README.md
├── scripts/
│   ├── build.ps1           # Shared MSVC build (local + CI)
│   ├── release.ps1         # Bump version / create vX.Y.Z tag
│   └── Get-ProjectVersion.ps1
├── res/                    # Icons, logo, screenshot assets
└── src/
    ├── main.cpp            # wWinMain, single-instance mutex, -debug
    ├── version.h           # PMC_VERSION_* (synced with VERSION)
    ├── PowermateManager.*  # HID open/read loop (VID 077D / PID 0410)
    ├── ProfileManager.*    # Scroll / Volume profile selection
    ├── TriggerAction.*     # SendInput for scroll, volume, mute, click
    ├── trayIcon.*          # Tray UI, menu, autostart, device notify
    ├── resource.h
    └── resource.rc         # Tray icons + VERSIONINFO
```

## License

MIT — see [LICENSE](LICENSE). Forked from [magouill/PowerMateControl](https://github.com/magouill/PowerMateControl); use this repository’s releases and issues.
