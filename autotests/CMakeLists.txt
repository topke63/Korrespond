# SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later

include(ECMAddTests)

# Test for Account
ecm_add_test(
    accounttest.cpp
    TEST_NAME accounttest
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
