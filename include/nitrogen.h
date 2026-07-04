/**
 * @file nitrogen.h
 * @brief Root header for the Nitrogen browser project.
 * @details One-stop include that pulls in every public module header so that
 *          application code needs only a single include.  Translators/compiles
 *          pull in headers in the dependency order: util → core → net → html →
 *          css → js → render → layout → devtools → browser.  Adding a new
 *          module requires adding both the include line here and the
 *          corresponding source entry in CMakeLists.txt.
 * @copyright 2026, Nitrogen Browser Project
 */

#ifndef NITROGEN_H
#define NITROGEN_H

// --- util: no internal deps ---
#include "util/String.h"
#include "util/Memory.h"
#include "util/Logging.h"
#include "util/Arena.h"

// --- core ---
#ifdef _WIN32
#include "core/Win32Window.h"
#endif

// --- net ---
#include "net/HttpClient.h"

// --- html (DOMNode must come before parser so the parser can return it) ---
#include "html/DOMNode.h"
#include "html/HTMLParser.h"

// --- css (ComputedStyle before CSSParser so selectors reference computed style) ---
#include "css/ComputedStyle.h"
#include "css/Selectors.h"
#include "css/CSSParser.h"

// --- js ---
#include "js/VM.h"

// --- render ---
#include "render/Painter.h"

// --- layout ---
#include "layout/Box.h"

// --- devtools ---
#include "devtools/DOMInspector.h"
#include "devtools/PaintProfiler.h"

// --- browser chrome ---
#include "browser/Tab.h"
#include "browser/Bookmarks.h"
#include "browser/URLBar.h"
#include "browser/SettingsPanel.h"
#include "browser/BrowserUI.h"

#endif // NITROGEN_H
