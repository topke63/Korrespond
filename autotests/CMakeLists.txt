# SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

include(ECMAddTests)

# Test for Account
ecm_add_test(
    accounttest.cpp
    TEST_NAME accounttest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)

# Test for AppSettings
ecm_add_test(
    appsettingstest.cpp
    TEST_NAME appsettingstest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)

# Test for FolderModel
ecm_add_test(
    foldermodeltest.cpp
    TEST_NAME foldermodeltest
    LINK_LIBRARIES plasmamail_models Qt6::Test
)

# Test for FolderUtils
ecm_add_test(
    folderutilstest.cpp
    TEST_NAME folderutilstest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)

# Test for MessageListModel
ecm_add_test(
    messagelistmodeltest.cpp
    TEST_NAME messagelistmodeltest
    LINK_LIBRARIES plasmamail_models Qt6::Test
)

# Test for message rendering
ecm_add_test(
    messagerenderertest.cpp
    TEST_NAME messagerenderertest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)

# Test for SecretStore (needs a keychain: run inside the Plasma session)
ecm_add_test(
    secretstoretest.cpp
    TEST_NAME secretstoretest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)

# Test for StorageSettings
ecm_add_test(
    storagesettingstest.cpp
    TEST_NAME storagesettingstest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)

# Test for MessageStore
ecm_add_test(
    messagestoretest.cpp
    TEST_NAME messagestoretest
    LINK_LIBRARIES plasmamail_engine Qt6::Test Qt6::Sql
)

# Test for retention
ecm_add_test(
    retentiontest.cpp
    TEST_NAME retentiontest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)

# Test for messageDate (Date header and Received fallback)
ecm_add_test(
    messagedatetest.cpp
    TEST_NAME messagedatetest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)

# Test for the Google OAuth client settings
ecm_add_test(
    oauthsettingstest.cpp
    TEST_NAME oauthsettingstest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)

# Test for MessageComposer
ecm_add_test(
    messagecomposertest.cpp
    TEST_NAME messagecomposertest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)

# Test for OutgoingMessage
ecm_add_test(
    outgoingmessagetest.cpp
    TEST_NAME outgoingmessagetest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)

# Test for NewMailTracker
ecm_add_test(
    newmailtrackertest.cpp
    TEST_NAME newmailtrackertest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)
