cx_configure_rc(CX_GUI_RC "ClaudeExplorer" "ClaudeExplorer.exe" "Claude Explorer")

qt_add_executable(ClaudeExplorer WIN32
    main.cpp
    explorer_window.cpp
    board_watcher.cpp
    node_explorer_panel.cpp
    node_effects.cpp
    attention_watch.cpp
    node_graph.cpp
    conflict_edges.cpp
    agent_board.cpp
    claude_state.cpp
    claude_files.cpp
    transcript_usage.cpp
    theme_qt.cpp
    work_marks.cpp
    resources.qrc
    "${CX_GUI_RC}"
)

target_link_libraries(ClaudeExplorer PRIVATE
    cx_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    user32
)

cx_apply_warnings(ClaudeExplorer)

if(CX_HARDENING)
    cx_apply_hardening(ClaudeExplorer)
endif()

cx_embed_manifest(ClaudeExplorer "${CX_MANIFEST}")
cx_deploy_qt(ClaudeExplorer)

install(TARGETS ClaudeExplorer RUNTIME DESTINATION .)
