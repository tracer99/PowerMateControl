---
name: Doxygen src documentation
overview: Add Doxygen-style documentation across all meaningful `src/*` headers and translation units so classes, public APIs, and non-obvious internals are readable in IDE hover and (optionally) future HTML docs—without changing runtime behavior.
todos:
  - id: dox-headers
    content: Add @file + class/method Doxygen to all src/*.h (except heavy resource ID spam)
    status: completed
  - id: dox-cpp
    content: Document main.cpp and all src/*.cpp helpers/impls; fold existing useful // notes into @note
    status: completed
  - id: dox-verify
    content: "Sanity-check: no code changes beyond comments; Release build still compiles"
    status: completed
isProject: false
---

# Document `src/*` with Doxygen comments

## Convention (locked)

Use **Doxygen** block comments as selected:

```cpp
/**
 * @brief Short one-line summary.
 *
 * Longer detail when needed (threading, registry keys, HID layout).
 *
 * @param name Meaning of the parameter.
 * @return Meaning of the return value.
 * @note Side effects, threading, or Windows-specific constraints.
 */
```

Rules:

- **Public API** (`.h` declarations): full `@brief` / `@param` / `@return` / `@note` where useful.
- **`.cpp` helpers** (anonymous namespaces, dialog procs, static methods): `@brief` at least; add `@param`/`@return` when non-obvious.
- **File banner** at top of each `.h` / `.cpp` (after includes are fine; prefer before includes for headers, after `#include` guard / `#pragma once` and before the first declaration—standard Doxygen `@file`):

```cpp
/**
 * @file AudioVolume.h
 * @brief WASAPI master-volume read/write and change notifications.
 */
```

- Prefer documenting **why / contracts** (mutex, UI-thread rules, LED queueing) over restating the function name.
- Do **not** change logic, signatures, or formatting beyond inserting comments and replacing thin legacy `// Function to…` stubs with Doxygen where they already exist.
- Keep existing useful technical notes (HID report layout, overlapped I/O deadlock fix) by folding them into `@note` / detail paragraphs—don’t delete domain knowledge.

## Scope

| Include | Approach |
|---|---|
| All pairs: AboutDialog, AudioVolume, LedController, PowermateManager, ProfileManager, Settings, TriggerAction, trayIcon, main.cpp | Full Doxygen coverage as above |
| [`version.h`](src/version.h) | Brief `@file` only (macros stay self-explanatory; keep VERSION sync note) |
| [`resource.h`](src/resource.h) | Brief `@file` + short group comments for icon / About ID blocks (no comment-per-ID spam) |
| [`resource.rc`](src/resource.rc) | Skip (script, not C++) |

## Per-area focus

- **[`PowermateManager`](src/PowermateManager.h) / `.cpp`**: device open/read loop, overlapped cancel, pending LED apply on input thread, feature IDs / pulse encoding.
- **[`AudioVolume`](src/AudioVolume.h) / `.cpp`**: COM endpoint lifetime, notify callback → LED, step/mute vs OSD.
- **[`TrayIcon`](src/trayIcon.h) / `.cpp`**: tray HWND requirements, `TrayWndProc` messages, Run vs Settings Autostart sync.
- **[`LedController`](src/LedController.cpp)**: Scroll pulse / Volume solid / mute pulse mapping.
- **[`Settings`](src/Settings.h)**, **[`ProfileManager`](src/ProfileManager.h)**, **[`TriggerAction`](src/TriggerAction.h)**, **[`AboutDialog`](src/AboutDialog.h)**, **[`main.cpp`](src/main.cpp)**: class/`wWinMain` entry contract (mutex, COM STA, shutdown order).

## Process

1. Headers first (API surface + `@file`), then matching `.cpp` (helpers + non-exported behavior).
2. Replace obsolete `// Function to …` comments in [`trayIcon.cpp`](src/trayIcon.cpp) with Doxygen on the same declarations/definitions.
3. Leave a local Release build check for comment-only safety (comments must not break the build).
4. No version bump / changelog unless you ask afterward (docs-only).

Plan artifacts stay in [`.cursor/plans/`](.cursor/plans/) when creating this plan in-repo if the tool places them there; do not invent new product docs under `docs/` unless requested.