cx_configure_rc(CX_GUI_RC "ClaudeExplorer" "ClaudeExplorer.exe" "Claude Explorer")

qt_add_executable(ClaudeExplorer WIN32
    main.cpp
    explorer_window.cpp
    board_watcher.cpp
    node_explorer_panel.cpp
    node_graph.cpp
    agent_board.cpp
    claude_state.cpp
    claude_files.cpp
    theme_qt.cpp
    resources.qrc
    "${CX_GUI_RC}"
)

target_link_libraries(ClaudeExplorer PRIVATE
    cx_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

cx_apply_warnings(ClaudeExplorer)

if(CX_HARDENING)
    cx_apply_hardening(ClaudeExplorer)
endif()

cx_embed_manifest(ClaudeExplorer "${CX_MANIFEST}")
cx_deploy_qt(ClaudeExplorer)

install(TARGETS ClaudeExplorer RUNTIME DESTINATION .)
