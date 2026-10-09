# SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

# Engine: accounts and IMAP access, no UI dependencies.
add_library(plasmamail_engine STATIC
    engine/account.cpp
    engine/appsettings.cpp
    engine/folderutils.cpp
    engine/imapclient.cpp
    engine/messagecomposer.cpp
    engine/messagerenderer.cpp
    engine/messagestore.cpp
    engine/newmailtracker.cpp
    engine/oauthclient.cpp
    engine/oauthsettings.cpp
    engine/outgoingmessage.cpp
    engine/retention.cpp
    engine/smtpclient.cpp
    engine/syncmanager.cpp
    engine/secretstore.cpp
    engine/storagesettings.cpp
)
target_include_directories(plasmamail_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine)
target_link_libraries(plasmamail_engine PUBLIC
    Qt6::Core
    Qt6::Gui
    KF6::ConfigCore
    KF6::I18n
    KF6::Mime
    KPim6::IMAP
    KPim6::SMTP
    Qt6::Network
    Qt6::NetworkAuth
    Qt6Keychain::Qt6Keychain
    Qt6::Sql
)

# Models: list models for the UI (QtCore only, no QML).
add_library(plasmamail_models STATIC
    app/accountsession.cpp
    models/accountlistmodel.cpp
    models/foldermodel.cpp
    models/messagelistmodel.cpp
)
target_include_directories(plasmamail_models PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/models ${CMAKE_CURRENT_SOURCE_DIR}/app)
target_link_libraries(plasmamail_models PUBLIC
    plasmamail_engine
    KF6::I18n
)

# App: the controller QML talks to, and (from T19) the QML UI.
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)
add_executable(plasma-mail
    app/main.cpp
)
qt_add_qml_module(plasma-mail
    URI PlasmaMail
    VERSION 1.0
    SOURCES
        app/mailnotifier.cpp app/mailnotifier.h
        app/trayicon.cpp app/trayicon.h
        app/mailcontroller.cpp app/mailcontroller.h
    QML_FILES
        qml/Main.qml
        qml/AccountSetupPage.qml
        qml/MessageListPage.qml
        qml/MessagePage.qml
        qml/MoveFolderMenu.qml
        qml/PanelResizer.qml
        qml/StatusBar.qml
        qml/StorageSettingsPage.qml
        qml/NotificationSettingsPage.qml
        qml/FolderSidebar.qml
        qml/SenderAvatar.qml
        qml/AccountSettingsPage.qml
        qml/AccountDetailsPage.qml
        qml/ComposeWindow.qml
)
# The icon also lives in the executable, for runs from the build folder where the themed icon
# is not installed.
qt_add_resources(plasma-mail app_icon PREFIX "/icons" BASE ${CMAKE_SOURCE_DIR}/data/icons FILES ${CMAKE_SOURCE_DIR}/data/icons/sc-apps-plasma-mail.svg ${CMAKE_SOURCE_DIR}/data/icons/sc-apps-plasma-mail-unread.svg)
install(TARGETS plasma-mail ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
# The generated QML type registration includes headers by file name.
target_include_directories(plasma-mail PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app)
target_link_libraries(plasma-mail PRIVATE
    plasmamail_models
    Qt6::Qml
    Qt6::WebEngineQuick
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::I18n
    KF6::I18nQml
    KF6::CoreAddons
    KF6::Notifications
    KF6::NotifyConfig
    KF6::StatusNotifierItem
)
