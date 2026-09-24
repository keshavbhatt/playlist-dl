# services: network-backed and process-backed services (engine provisioning,
# yt-dlp probing, in-app search). Links core + Qt6::Network (+ Qt6::Sql to read
# the profile's cookie database). No widgets, no WebEngine.

qt_add_library(pldl_services STATIC
    engine_manager.cpp
    engine_manager.h
    ktechpit_search.cpp
    ktechpit_search.h
    licensing/license_config.h
    licensing/license_service.cpp
    licensing/license_service.h
    logging.cpp
    logging.h
    media_probe.cpp
    media_probe.h
    netscape_cookies.cpp
    netscape_cookies.h
    playlist_search.cpp
    playlist_search.h
    search_service.cpp
    search_service.h
    search_suggestions.cpp
    search_suggestions.h
)
pldl_set_warnings(pldl_services)

target_link_libraries(pldl_services
    PUBLIC
        pldl_core
        account_license
        Qt6::Network
    PRIVATE
        Qt6::Sql
)
