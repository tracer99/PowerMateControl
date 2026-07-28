#pragma once

// Keep in sync with the repo-root VERSION file (scripts/release.ps1 updates both).
#define PMC_VERSION_MAJOR 1
#define PMC_VERSION_MINOR 2
#define PMC_VERSION_PATCH 1
#define PMC_VERSION_BUILD 0

// RC has a limited preprocessor; keep the comma list as literals (not nested macros).
#define PMC_VERSION_COMMA 1,2,1,0
#define PMC_VERSION_STRING "1.2.1"
