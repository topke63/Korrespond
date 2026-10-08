# SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Engine: accounts and IMAP access, no UI dependencies.
add_library(plasmamail_engine STATIC
    engine/account.cpp
    engine/folderutils.cpp
    engine/imapclient.cpp
)
target_include_directories(plasmamail_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine)
target_link_libraries(plasmamail_engine PUBLIC
    Qt6::Core
    KF6::ConfigCore
    KF6::Mime
    KPim6::IMAP
    Qt6::Network
    Qt6Keychain::Qt6Keychain
    Qt6::Sql
)

# Models: list models for the UI (QtCore only, no QML).
add_library(plasmamail_models STATIC
    models/foldermodel.cpp
    models/messagelistmodel.cpp
)
target_include_directories(plasmamail_models PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/models)
target_link_libraries(plasmamail_models PUBLIC
    plasmamail_engine
    KF6::I18n
)
