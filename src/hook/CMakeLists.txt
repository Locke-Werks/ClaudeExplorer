cx_configure_rc(CX_HOOK_RC "cxhook" "cxhook.exe" "Claude Explorer hook receiver")

add_executable(cxhook
    main.cpp
    hook.cpp
    "${CX_HOOK_RC}"
)

# Console subsystem. WIN32_EXECUTABLE is forced off rather than left alone
# because qt_add_executable(... WIN32) in the viewer sets it ON for the whole
# tree, and a hook receiver built as a GUI app writes its diagnostics nowhere.
set_target_properties(cxhook PROPERTIES
    OUTPUT_NAME "cxhook"
    WIN32_EXECUTABLE OFF
)

target_link_libraries(cxhook PRIVATE cx_core)

cx_apply_warnings(cxhook)

if(CX_HARDENING)
    cx_apply_hardening(cxhook)
endif()

cx_embed_manifest(cxhook "${CX_MANIFEST}")

install(TARGETS cxhook RUNTIME DESTINATION .)
