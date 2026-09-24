# web: everything that touches Qt WebEngine. Depends on core, services (cookie
# export), platform. Injected JavaScript lives in scripts/ as resource files -
# never as C++ strings.

qt_add_library(pldl_web STATIC
    bridge.cpp
    bridge.h
    cookie_exporter.cpp
    cookie_exporter.h
    error_page.cpp
    error_page.h
    full_screen_hint.cpp
    full_screen_hint.h
    logging.cpp
    logging.h
    permission_controller.cpp
    permission_controller.h
    popup_window.cpp
    popup_window.h
    script_dialogs.h
    request_interceptor.cpp
    request_interceptor.h
    script_bundle.cpp
    script_bundle.h
    user_agent.cpp
    user_agent.h
    web_page.cpp
    web_page.h
    web_profile.cpp
    web_profile.h
    web_view.cpp
    web_view.h
)
pldl_set_warnings(pldl_web)

qt_add_resources(pldl_web "scripts"
    PREFIX "/scripts"
    BASE "scripts"
    FILES
        scripts/adblock.js
        scripts/bootstrap.js
        scripts/hooks.js
        scripts/page-media.js
)

target_link_libraries(pldl_web
    PUBLIC
        pldl_core
        pldl_services
        pldl_platform
        Qt6::WebEngineWidgets
        Qt6::WebChannel
        Qt6::Network
)
