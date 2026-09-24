# app: the QApplication subclass, single-instance guard, CLI parsing.
# Version metadata is generated from CMake, never hand-written.

configure_file(version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/app/version.h" @ONLY)

qt_add_library(pldl_app STATIC
    application.cpp
    application.h
    cli_options.cpp
    cli_options.h
    logging.cpp
    logging.h
    single_instance.cpp
    single_instance.h
)
pldl_set_warnings(pldl_app)

target_include_directories(pldl_app PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(pldl_app
    PUBLIC
        pldl_ui
        Qt6::Network
)
