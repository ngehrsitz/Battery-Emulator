#pragma once

// The build script (tools/identify_build.py) generates version_autogen.h
// with concrete #defines for BUILD_VERSION and the raw GIT_* / GITHUB_*
// macros.  Fallback gracefully if the file isn't present for some reason.

#if __has_include("version_autogen.h")
#include "version_autogen.h"
#endif

#ifndef BUILD_VERSION
#define BUILD_VERSION "unknown"
#endif
