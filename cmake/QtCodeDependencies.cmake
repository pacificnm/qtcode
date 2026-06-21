include(FindPkgConfig)

set(_qtcode_toolchain_doc "docs/toolchain-requirements.md")

function(qtcode_report_missing_dependency dependency purpose install_hint)
    message(FATAL_ERROR
        "Required dependency not found: ${dependency}\n"
        "Purpose: ${purpose}\n"
        "${install_hint}\n"
        "See ${_qtcode_toolchain_doc} for the full QTCode toolchain list.")
endfunction()

find_package(Qt6 QUIET COMPONENTS Core Widgets Sql Concurrent)
if(NOT Qt6_FOUND)
    qtcode_report_missing_dependency(
        "Qt 6 (Core, Widgets, Sql, Concurrent)"
        "native Qt/KDE application shell and UI widgets"
        "On Ubuntu/Debian: sudo apt install qt6-base-dev")
endif()

find_package(KF6CoreAddons QUIET)
if(NOT KF6CoreAddons_FOUND)
    qtcode_report_missing_dependency(
        "KF6CoreAddons"
        "KDE Frameworks utilities used by the native application shell"
        "On Ubuntu/Debian: sudo apt install libkf6coreaddons-dev")
endif()

find_package(KF6I18n QUIET)
if(NOT KF6I18n_FOUND)
    qtcode_report_missing_dependency(
        "KF6I18n"
        "KDE internationalization support for user-visible strings"
        "On Ubuntu/Debian: sudo apt install libkf6i18n-dev")
endif()

find_package(KF6XmlGui QUIET)
if(NOT KF6XmlGui_FOUND)
    qtcode_report_missing_dependency(
        "KF6XmlGui"
        "KDE action collections, menus, and standard help dialogs"
        "On Ubuntu/Debian: sudo apt install libkf6xmlgui-dev")
endif()

find_package(KF6TextEditor QUIET)
if(NOT KF6TextEditor_FOUND)
    qtcode_report_missing_dependency(
        "KF6TextEditor"
        "native KTextEditor workspace file editing"
        "On Ubuntu/Debian: sudo apt install libkf6texteditor-dev")
endif()

find_package(KF6Wallet QUIET)
if(NOT KF6Wallet_FOUND)
    qtcode_report_missing_dependency(
        "KF6Wallet"
        "secure storage for MCP server secrets through KDE Wallet"
        "On Ubuntu/Debian: sudo apt install libkf6wallet-dev")
endif()

find_package(PkgConfig QUIET)
if(NOT PkgConfig_FOUND)
    qtcode_report_missing_dependency(
        "pkg-config"
        "discover QTermWidget and other library build flags"
        "On Ubuntu/Debian: sudo apt install pkg-config")
endif()

pkg_check_modules(QTermWidget QUIET IMPORTED_TARGET qtermwidget6)
if(NOT QTermWidget_FOUND)
    qtcode_report_missing_dependency(
        "QTermWidget (qtermwidget6)"
        "embedded terminal tabs in the QTCode cockpit"
        "On Ubuntu/Debian: sudo apt install libqtermwidget6-2-dev libutf8proc-dev")
endif()

find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    qtcode_report_missing_dependency(
        "SQLite3"
        "local application state, settings, and agent session storage"
        "On Ubuntu/Debian: sudo apt install libsqlite3-dev")
endif()

find_package(libgit2 QUIET)
if(NOT libgit2_FOUND)
    qtcode_report_missing_dependency(
        "libgit2"
        "repository status, branch, and diff workflows"
        "On Ubuntu/Debian: sudo apt install libgit2-dev")
endif()

pkg_check_modules(LibGit2 QUIET IMPORTED_TARGET libgit2)
if(NOT LibGit2_FOUND)
    qtcode_report_missing_dependency(
        "libgit2 (pkg-config)"
        "repository status, branch, and diff workflows"
        "On Ubuntu/Debian: sudo apt install libgit2-dev")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_library(QtCodeExternalDependencies INTERFACE)
target_link_libraries(QtCodeExternalDependencies INTERFACE
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
    Qt6::Concurrent
    KF6::CoreAddons
    KF6::I18n
    KF6::XmlGui
    KF6::TextEditor
    KF6::Wallet
    PkgConfig::QTermWidget
    SQLite::SQLite3
    PkgConfig::LibGit2
)
add_library(QtCode::ExternalDependencies ALIAS QtCodeExternalDependencies)

message(STATUS "QTCode dependencies:")
message(STATUS "  Qt6 Core/Widgets ${Qt6Core_VERSION}")
message(STATUS "  KF6CoreAddons ${KF6CoreAddons_VERSION}")
message(STATUS "  KF6I18n ${KF6I18n_VERSION}")
message(STATUS "  KF6XmlGui ${KF6XmlGui_VERSION}")
message(STATUS "  KF6TextEditor ${KF6TextEditor_VERSION}")
message(STATUS "  KF6Wallet ${KF6Wallet_VERSION}")
message(STATUS "  QTermWidget ${QTermWidget_VERSION}")
message(STATUS "  SQLite3 ${SQLite3_VERSION}")
message(STATUS "  libgit2 ${libgit2_VERSION}")
