#pragma once

/**
 * @file AboutDialog.h
 * @brief Modal About pane shown from the tray menu.
 */

#include <Windows.h>

/**
 * @brief Displays application branding, version, and external links.
 */
class AboutDialog {
public:
    /**
     * @brief Shows the About dialog modally.
     * @param owner Parent window (typically the tray owner HWND); may be nullptr.
     */
    static void Show(HWND owner);
};
