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

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Network Sql Qml Quick QuickControls2 WebEngineQuick Test)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Kirigami I18n CoreAddons Config Mime)
find_package(KPim6IMAP REQUIRED)
find_package(Qt6Keychain REQUIRED)

add_definitions(-DTRANSLATION_DOMAIN=\"plasma-mail\")

# Only build the main library for now
add_subdirectory(src)
add_subdirectory(tools)

# Only build tests if they exist and BUILD_TESTING is enabled
if(BUILD_TESTING)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/autotests/CMakeLists.txt)
        add_subdirectory(autotests)
    endif()
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
