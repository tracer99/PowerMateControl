#pragma once

/**
 * @file version.h
 * @brief Embedded product/file version macros kept in sync with the repo-root VERSION file.
 *
 * @note scripts/release.ps1 updates both VERSION and this header.
 *       RC has a limited preprocessor; PMC_VERSION_COMMA stays as literal commas.
 */

#define PMC_VERSION_MAJOR 1
#define PMC_VERSION_MINOR 4
#define PMC_VERSION_PATCH 2
#define PMC_VERSION_BUILD 0

#define PMC_VERSION_COMMA 1,4,2,0
#define PMC_VERSION_STRING "1.4.2"
