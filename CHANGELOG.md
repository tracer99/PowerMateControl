# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Version numbers follow the upstream [magouill/PowerMateControl](https://github.com/magouill/PowerMateControl) line so this fork stays aligned with that release history.

## [Unreleased]

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

[Unreleased]: https://github.com/tracer99/PowerMateControl/compare/v1.2.0...HEAD
[1.2.0]: https://github.com/tracer99/PowerMateControl/releases/tag/v1.2.0
