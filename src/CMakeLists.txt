# Engine: accounts and IMAP access, no UI dependencies.
add_library(plasmamail_engine STATIC
    engine/account.cpp
)
target_include_directories(plasmamail_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine)
target_link_libraries(plasmamail_engine PUBLIC
    Qt6::Core
    KF6::ConfigCore
)
