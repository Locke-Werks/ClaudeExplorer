cmake_minimum_required(VERSION 3.28)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Version)

project(ClaudeExplorer VERSION ${CX_VERSION} LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Claude Explorer targets Windows only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Claude Explorer is x64 only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(CX_WERROR    "Treat warnings as errors" OFF)
option(CX_HARDENING "Apply the hardening flags" ON)
option(CX_BUILD_GUI "Build the viewer" ON)

include(Warnings)
include(Hardening)
include(Manifest)
include(DeployQt)

# The CRT is deliberately left at the default dynamic /MD. cxhook.exe would
# happily link the static one, but ClaudeExplorer.exe loads the official Qt
# binaries, which are themselves built against /MD, and mixing the two produces
# a duplicate heap and LNK2038. One setting for both keeps cx_core linkable
# into either without a second build of it.

if(CX_BUILD_GUI)
    find_package(Qt6 6.5 QUIET COMPONENTS Core Gui Widgets)
    if(NOT Qt6_FOUND)
        message(FATAL_ERROR
            "Qt6 Widgets not found. Point CMake at a Qt kit with "
            "-DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64, or set the "
            "QT_ROOT_DIR environment variable, or configure with "
            "-DCX_BUILD_GUI=OFF to build only the hook.")
    endif()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
endif()

# RC wants forward slashes in a path, which is what CMake hands out anyway.
set(CX_ICON_PATH   "${CMAKE_CURRENT_SOURCE_DIR}/assets/claudeexplorer.ico")
set(CX_RC_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/res/claudeexplorer.rc.in")
set(CX_MANIFEST    "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest")

# One VERSIONINFO per target, because Explorer shows the description and the
# original filename and two binaries sharing one set of strings is how an exe
# ends up claiming to be the other one.
function(cx_configure_rc out_var internal_name original_filename description)
    set(CX_RC_INTERNAL_NAME     "${internal_name}")
    set(CX_RC_ORIGINAL_FILENAME "${original_filename}")
    set(CX_RC_DESCRIPTION       "${description}")

    set(_out "${CMAKE_CURRENT_BINARY_DIR}/${internal_name}.rc")
    configure_file("${CX_RC_TEMPLATE}" "${_out}" @ONLY)
    set(${out_var} "${_out}" PARENT_SCOPE)
endfunction()

add_subdirectory(src/core)
add_subdirectory(src/hook)

if(CX_BUILD_GUI)
    add_subdirectory(src/gui)
endif()

message(STATUS "")
message(STATUS "Claude Explorer ${CX_VERSION}")
if(CX_BUILD_GUI)
    message(STATUS "  Qt         ${Qt6_VERSION} (${Qt6_DIR})")
else()
    message(STATUS "  Qt         off")
endif()
message(STATUS "  hardening  ${CX_HARDENING}")
message(STATUS "  warnings   /W4, /WX ${CX_WERROR}")
message(STATUS "")
