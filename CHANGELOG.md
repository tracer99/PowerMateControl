# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Version numbers follow the upstream [magouill/PowerMateControl](https://github.com/magouill/PowerMateControl) line so this fork stays aligned with that release history.

## [Unreleased]

## [1.4.2] - 2026-07-28

### Added

- Ko-fi tip link ([ko-fi.com/tracer_ca](https://ko-fi.com/tracer_ca)) in the About dialog and README

## [1.4.1] - 2026-07-28

### Changed

- CI: skip duplicate `main` build on `release:` commits; upgrade Actions to Node 24–compatible versions; replace `ilammy/msvc-dev-cmd` with `egor-tensin/vs-shell`; stop leftover `vctip` after build

## [1.4.0] - 2026-07-28

### Added

- Volume profile changes master volume via WASAPI (`VolumeStepUp` / `VolumeStepDown` / `SetMute`) instead of media keys
- Hardware LED pulse in **Scroll** profile and when muted (or volume at zero) in **Volume** profile
- Persist **Run at startup** preference as `Autostart` under `HKCU\Software\PowerMateControl` (still writes the Run key; recovers Run from Settings if needed)
- Tray **About...** pane with logo, version, maintainer, and links to the GitHub repo / issues

### Changed

- Volume knob no longer triggers the Windows volume OSD (direct endpoint control)
- Volume-profile LED still tracks solid brightness from master volume when unmuted

## [1.3.1] - 2026-07-28

### Fixed

- Tray menu and Exit no longer freeze until the PowerMate is turned (HID `ReadFile` / `SetFeature` deadlock on the UI thread)
- LED brightness updates are queued and applied on the input thread; overlapped reads allow clean shutdown
- Initialize COM as STA on the UI thread
- Tray owner window is a real top-level HWND (not `HWND_MESSAGE`) for reliable context menus

## [1.3.0] - 2026-07-28

### Added

- Persist the selected profile (`Scroll` / `Volume`) in `HKCU\Software\PowerMateControl`
- Volume-profile LED: PowerMate brightness tracks the system master volume (off when muted)
- Scroll-profile LED stays off; LED refreshes on connect, profile change, and OS volume changes

## [1.2.1] - 2026-07-28

### Fixed

- CI/local MSVC build: compile `resource.rc` with `rc.exe` and link the `.res` correctly
- Include `shellapi.h` so tray/`NOTIFYICONDATA` APIs build under `WIN32_LEAN_AND_MEAN`
- Embed a readable Windows file version (`VERSIONINFO` via `winver.h`)
- Use `freopen_s` for the `-debug` console to avoid CRT deprecation warnings

## [1.2.0] - 2026-07-28

Matches upstream [v1.2.0](https://github.com/magouill/PowerMateControl/releases/tag/v1.2.0). This repository is maintained independently (releases, issues, and contributions happen here).

### Added

- Windows system tray app for the Griffin PowerMate USB (VID `077D`, PID `0410`)
- **Scroll** and **Volume** profiles (scroll/click vs volume/mute)
- Button handling on release (fewer false triggers)
- System suspend / resume support
- Optional **Run at startup** via the tray menu
- `-debug` console logging
- Documented requirements and local build instructions
- GitHub Actions CI that builds on `main` and publishes tagged releases
- Shared `scripts/build.ps1` and `scripts/release.ps1` for build and versioning
- Semantic versioning (`VERSION`) and this changelog

### Notes

- Bluetooth PowerMate is not supported.

[Unreleased]: https://github.com/tracer99/PowerMateControl/compare/v1.4.2...HEAD
[1.4.2]: https://github.com/tracer99/PowerMateControl/compare/v1.4.1...v1.4.2
[1.4.1]: https://github.com/tracer99/PowerMateControl/compare/v1.4.0...v1.4.1
[1.4.0]: https://github.com/tracer99/PowerMateControl/compare/v1.3.1...v1.4.0
[1.3.1]: https://github.com/tracer99/PowerMateControl/compare/v1.3.0...v1.3.1
[1.3.0]: https://github.com/tracer99/PowerMateControl/compare/v1.2.1...v1.3.0
[1.2.1]: https://github.com/tracer99/PowerMateControl/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/tracer99/PowerMateControl/releases/tag/v1.2.0
