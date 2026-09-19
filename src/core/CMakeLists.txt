# Everything both binaries share, and deliberately no Qt.
#
# The payoff is cxhook.exe: it links this and nothing else, so the binary Claude
# Code launches on every tool call is standalone rather than needing Qt6Core.dll
# beside it. CI asserts the absence rather than trusting it.
add_library(cx_core STATIC
    json.cpp
    strutil.cpp
    tool_summary.cpp
)

target_include_directories(cx_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(cx_core PUBLIC
    CX_VERSION_STRING="${CX_VERSION}"
    CX_PRODUCT_NAME="${CX_PRODUCT}"
)

cx_apply_warnings(cx_core)

if(CX_HARDENING)
    cx_apply_hardening(cx_core)
endif()

# SHGetKnownFolderPath and the CoTaskMemFree that goes with it.
target_link_libraries(cx_core PRIVATE shell32 ole32)
