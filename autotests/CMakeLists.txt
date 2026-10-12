# SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

include(ECMAddTests)

# Test for Account
ecm_add_test(
    accounttest.cpp
    TEST_NAME accounttest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for LegacyProfile (the move from the "Plasma Mail" names)
ecm_add_test(
    legacyprofiletest.cpp
    TEST_NAME legacyprofiletest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for AppSettings
ecm_add_test(
    appsettingstest.cpp
    TEST_NAME appsettingstest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for AccountListModel
ecm_add_test(
    accountlistmodeltest.cpp
    TEST_NAME accountlistmodeltest
    LINK_LIBRARIES korrespond_models Qt6::Test
)

# Test for FolderModel
ecm_add_test(
    foldermodeltest.cpp
    TEST_NAME foldermodeltest
    LINK_LIBRARIES korrespond_models Qt6::Test
)

# Test for FolderUtils
ecm_add_test(
    folderutilstest.cpp
    TEST_NAME folderutilstest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for MessageListModel
ecm_add_test(
    messagelistmodeltest.cpp
    TEST_NAME messagelistmodeltest
    LINK_LIBRARIES korrespond_models Qt6::Test
)

# Test for addresses seen in message headers
ecm_add_test(
    addresshistorytest.cpp
    TEST_NAME addresshistorytest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for the contact cache
ecm_add_test(
    contactstoretest.cpp
    TEST_NAME contactstoretest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for the contacts sync (fake People API server)
ecm_add_test(
    contactsynctest.cpp
    TEST_NAME contactsynctest
    LINK_LIBRARIES korrespond_engine Qt6::Test Qt6::Network
)

# Test for the Google People API client (fake HTTP server)
ecm_add_test(
    peopleclienttest.cpp
    TEST_NAME peopleclienttest
    LINK_LIBRARIES korrespond_engine Qt6::Test Qt6::Network
)

# Test for the open conversation's message model
ecm_add_test(
    conversationmodeltest.cpp
    TEST_NAME conversationmodeltest
    LINK_LIBRARIES korrespond_models Qt6::Test
)

# Test for message rendering
ecm_add_test(
    messagerenderertest.cpp
    TEST_NAME messagerenderertest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for SecretStore (needs a keychain: run inside the Plasma session)
ecm_add_test(
    secretstoretest.cpp
    TEST_NAME secretstoretest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for StorageSettings
ecm_add_test(
    storagesettingstest.cpp
    TEST_NAME storagesettingstest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for MessageStore
ecm_add_test(
    messagestoretest.cpp
    TEST_NAME messagestoretest
    LINK_LIBRARIES korrespond_engine Qt6::Test Qt6::Sql
)

# Test for retention
ecm_add_test(
    retentiontest.cpp
    TEST_NAME retentiontest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for ThreadBuilder (conversation grouping)
ecm_add_test(
    threadbuildertest.cpp
    TEST_NAME threadbuildertest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for messageDate (Date header and Received fallback)
ecm_add_test(
    messagedatetest.cpp
    TEST_NAME messagedatetest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for the Google OAuth client settings
ecm_add_test(
    oauthsettingstest.cpp
    TEST_NAME oauthsettingstest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for MessageComposer
ecm_add_test(
    messagecomposertest.cpp
    TEST_NAME messagecomposertest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for OutgoingMessage
ecm_add_test(
    outgoingmessagetest.cpp
    TEST_NAME outgoingmessagetest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for ComposeHtml
ecm_add_test(
    composehtmltest.cpp
    TEST_NAME composehtmltest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for NewMailTracker
ecm_add_test(
    newmailtrackertest.cpp
    TEST_NAME newmailtrackertest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for IdlePolicy
ecm_add_test(
    idlepolicytest.cpp
    TEST_NAME idlepolicytest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)

# Test for ImapIdleWatcher
ecm_add_test(
    imapidlewatchertest.cpp
    TEST_NAME imapidlewatchertest
    LINK_LIBRARIES korrespond_engine Qt6::Test
)
