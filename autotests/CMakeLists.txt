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
