# SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.22)

project(plasma-mail VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.6)
set(KF_MIN_VERSION 6.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)
include(ECMAddTests)
# ECM_ENABLE_SANITIZERS="address;undefined" (set by the asan preset) builds with sanitizers.
include(ECMEnableSanitizers)
include(ECMInstallIcons)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Widgets Network Sql Qml Quick QuickControls2 WebEngineQuick NetworkAuth Test)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Kirigami I18n CoreAddons Config Mime Notifications StatusNotifierItem)
find_package(KPim6IMAP REQUIRED)
find_package(Qt6Keychain REQUIRED)

add_definitions(-DTRANSLATION_DOMAIN=\"plasma-mail\")

# Only build the main library for now
add_subdirectory(src)
add_subdirectory(tools)

# The app icon (file name sc-apps-<name>.svg → hicolor/scalable/apps) and the launcher entry.
ecm_install_icons(ICONS data/icons/sc-apps-plasma-mail.svg data/icons/sc-apps-plasma-mail-unread.svg DESTINATION ${KDE_INSTALL_ICONDIR} THEME hicolor)
install(FILES data/plasma-mail.desktop DESTINATION ${KDE_INSTALL_APPDIR})

# Only build tests if they exist and BUILD_TESTING is enabled
if(BUILD_TESTING)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/autotests/CMakeLists.txt)
        add_subdirectory(autotests)
    endif()
endif()

# KDE code style: `cmake --build build --target clang-format` formats every C++ file, and the
# pre-commit hook rejects commits whose changed lines aren't formatted. ECM generates
# .clang-format at configure time (git-ignored, as ECM recommends).
include(KDEClangFormat)
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/autotests/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/autotests/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tools/*.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
include(KDEGitCommitHooks)
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
