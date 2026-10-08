# SPDX-FileCopyrightText: 2026 Free Software Foundation Europe e.V. <https://fsfe.org>
#
# SPDX-License-Identifier: BSD-2-Clause

include(ECMAddTests)

# Test for Account
ecm_add_test(
    accounttest.cpp
    TEST_NAME accounttest
    LINK_LIBRARIES plasmamail_engine Qt6::Test
)