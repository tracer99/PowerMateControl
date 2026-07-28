#pragma once

// Keep in sync with the repo-root VERSION file (scripts/release.ps1 updates both).
#define PMC_VERSION_MAJOR 1
#define PMC_VERSION_MINOR 3
#define PMC_VERSION_PATCH 0
#define PMC_VERSION_BUILD 0

// RC has a limited preprocessor; keep the comma list as literals (not nested macros).
#define PMC_VERSION_COMMA 1,3,0,0
#define PMC_VERSION_STRING "1.3.0"
