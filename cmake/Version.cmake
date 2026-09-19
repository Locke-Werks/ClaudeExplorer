# Single source of truth for the version. Everything else derives from it:
# project(), both VERSIONINFO resources, and the tag check in CI.
#
# installer.toml carries the same number and CI asserts the two agree, because
# Forge stamps the installer from the TOML and cannot see this file.
set(CX_VERSION_MAJOR 1)
set(CX_VERSION_MINOR 0)
set(CX_VERSION_PATCH 0)

set(CX_VERSION "${CX_VERSION_MAJOR}.${CX_VERSION_MINOR}.${CX_VERSION_PATCH}")
set(CX_VERSION_RC "${CX_VERSION_MAJOR},${CX_VERSION_MINOR},${CX_VERSION_PATCH},0")
set(CX_VERSION_RC_STR "${CX_VERSION}.0")

set(CX_PRODUCT   "Claude Explorer")
set(CX_COMPANY   "Locke Werks")
set(CX_COPYRIGHT "Copyright (C) 2026 Locke Werks")
