# SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Engine: accounts and IMAP access, no UI dependencies.
add_library(korrespond_engine STATIC
    engine/account.cpp
    engine/addresshistory.cpp
    engine/appsettings.cpp
    engine/composehtml.cpp
    engine/contactstore.cpp
    engine/contactsync.cpp
    engine/folderutils.cpp
    engine/threadbuilder.cpp
    engine/idlepolicy.cpp
    engine/imapclient.cpp
    engine/imapidlewatcher.cpp
    engine/legacyprofile.cpp
    engine/messagecomposer.cpp
    engine/messagerenderer.cpp
    engine/messagestore.cpp
    engine/newmailtracker.cpp
    engine/oauthclient.cpp
    engine/oauthsettings.cpp
    engine/peopleclient.cpp
    engine/outgoingmessage.cpp
    engine/retention.cpp
    engine/smtpclient.cpp
    engine/syncmanager.cpp
    engine/secretstore.cpp
    engine/storagesettings.cpp
)
target_include_directories(korrespond_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine)
target_link_libraries(korrespond_engine PUBLIC
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
add_library(korrespond_models STATIC
    app/accountsession.cpp
    models/accountlistmodel.cpp
    models/emojimodel.cpp
    models/foldermodel.cpp
    models/formathandler.cpp
    models/messagelistmodel.cpp
    models/conversationmodel.cpp
)
target_include_directories(korrespond_models PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/models ${CMAKE_CURRENT_SOURCE_DIR}/app)
target_link_libraries(korrespond_models PUBLIC
    korrespond_engine
    Qt6::Quick
    KF6::I18n
)

# App: the controller QML talks to, and (from T19) the QML UI.
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)
add_executable(korrespond
    app/main.cpp
)
set_source_files_properties(qml/MimeTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(korrespond
    URI Korrespond
    VERSION 1.0
    # Lets the all_qmllint / korrespond_qmllint targets find Qt and KDE QML modules.
    IMPORT_PATH ${KDE_INSTALL_FULL_QMLDIR}
    SOURCES
        app/mailnotifier.cpp app/mailnotifier.h
        app/trayicon.cpp app/trayicon.h
        app/mailcontroller.cpp app/mailcontroller.h
        app/mailapplication.cpp app/mailapplication.h
        app/shortcutsproxymodel.cpp app/shortcutsproxymodel.h
        app/formathandlerforeign.h
    QML_FILES
        qml/MimeTheme.qml
        qml/Main.qml
        qml/AccountSetupPage.qml
        qml/MessageListPage.qml
        qml/MessagePage.qml
        qml/MoveFolderMenu.qml
        qml/PanelResizer.qml
        qml/StatusBar.qml
        qml/StorageSettingsPage.qml
        qml/NotificationSettingsPage.qml
        qml/SendingSettingsPage.qml
        qml/ContactsSettingsPage.qml
        qml/ViewSettingsPage.qml
        qml/ConversationView.qml
        qml/RecipientField.qml
        qml/ShortcutsHelp.qml
        qml/FolderSidebar.qml
        qml/SenderAvatar.qml
        qml/AccountSettingsPage.qml
        qml/AccountDetailsPage.qml
        qml/ComposeWindow.qml
)
# The icon also lives in the executable, for runs from the build folder where the themed icon
# is not installed.
qt_add_resources(korrespond app_icon PREFIX "/icons" BASE ${CMAKE_SOURCE_DIR}/data/icons FILES ${CMAKE_SOURCE_DIR}/data/icons/sc-apps-io.github.topke63.korrespond.svg ${CMAKE_SOURCE_DIR}/data/icons/sc-apps-io.github.topke63.korrespond.unread.svg)
if(KORRESPOND_DEV_HOOKS)
    # Dev hook only (KORRESPOND_DEMO_KEY_UNDO): QTest::keyClick runs the window's shortcut map.
    target_compile_definitions(korrespond PRIVATE KORRESPOND_DEV_HOOKS)
    target_link_libraries(korrespond PRIVATE Qt6::Test)
endif()
install(TARGETS korrespond ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
# The generated QML type registration includes headers by file name.
target_include_directories(korrespond PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app)
target_link_libraries(korrespond PRIVATE
    korrespond_models
    Qt6::Qml
    Qt6::WebEngineQuick
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::I18n
    KF6::I18nQml
    KF6::CoreAddons
    KF6::ColorScheme
    KF6::Notifications
    KF6::NotifyConfig
    KF6::StatusNotifierItem
    KF6::ConfigGui
    KirigamiAddonsStatefulApp
)
