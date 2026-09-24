# ui: QtWidgets layer. May use everything. Nothing depends on it except app/.
# No .ui files, widgets are built in code (see CODING_STANDARDS.md section 5).

qt_add_library(pldl_ui STATIC
    a11y.cpp
    a11y.h
    about_dialog.cpp
    about_dialog.h
    account_dialog.cpp
    account_dialog.h
    actions.cpp
    actions.h
    badge_label.cpp
    badge_label.h
    browser_tab.cpp
    browser_tab.h
    bug_report_dialog.cpp
    bug_report_dialog.h
    busy_button.cpp
    busy_button.h
    diagnostics.cpp
    diagnostics.h
    download_card_delegate.cpp
    download_card_delegate.h
    downloads_controller.cpp
    downloads_controller.h
    downloads_filter_proxy.cpp
    downloads_filter_proxy.h
    engine_setup_dialog.cpp
    engine_setup_dialog.h
    icons.cpp
    icons.h
    keyboard.cpp
    keyboard.h
    kind_card.cpp
    kind_card.h
    license_gate.cpp
    license_gate.h
    links.h
    logging.cpp
    logging.h
    main_window.cpp
    main_window.h
    message_sheet.cpp
    message_sheet.h
    pages/browser_page.cpp
    pages/browser_page.h
    pages/downloads_page.cpp
    pages/downloads_page.h
    pages/page.cpp
    pages/page.h
    pages/search_page.cpp
    pages/search_page.h
    permission_prompt.cpp
    permission_prompt.h
    plans_dialog.cpp
    plans_dialog.h
    pldl_style.cpp
    pldl_style.h
    range_slider.cpp
    range_slider.h
    search_card_delegate.cpp
    search_card_delegate.h
    shortcuts_dialog.cpp
    shortcuts_dialog.h
    side_rail.cpp
    side_rail.h
    theme_applier.cpp
    theme_applier.h
    thumbnail_cache.cpp
    thumbnail_cache.h
    thumbnail_painter.cpp
    thumbnail_painter.h
    toast.cpp
    toast.h
    tray_controller.cpp
    tray_controller.h
    whats_new_dialog.cpp
    whats_new_dialog.h
)
pldl_set_warnings(pldl_ui)

set(PLDL_ICON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../resources/icons")
file(GLOB PLDL_UI_ICONS "${PLDL_ICON_DIR}/ui/*.svg")
qt_add_resources(pldl_ui "icons"
    PREFIX "/icons"
    BASE "${PLDL_ICON_DIR}"
    FILES
        "${PLDL_ICON_DIR}/pldl.svg"
        "${PLDL_ICON_DIR}/pldl-symbolic.svg"
        "${PLDL_ICON_DIR}/hicolor/16x16/apps/com.ktechpit.playlist-dl.png"
        "${PLDL_ICON_DIR}/hicolor/32x32/apps/com.ktechpit.playlist-dl.png"
        "${PLDL_ICON_DIR}/hicolor/48x48/apps/com.ktechpit.playlist-dl.png"
        "${PLDL_ICON_DIR}/hicolor/64x64/apps/com.ktechpit.playlist-dl.png"
        "${PLDL_ICON_DIR}/hicolor/128x128/apps/com.ktechpit.playlist-dl.png"
        "${PLDL_ICON_DIR}/hicolor/256x256/apps/com.ktechpit.playlist-dl.png"
        "${PLDL_ICON_DIR}/hicolor/512x512/apps/com.ktechpit.playlist-dl.png"
        ${PLDL_UI_ICONS}
)

# The changelog feeds the "What's new" dialog (:/text/CHANGELOG.md).
qt_add_resources(pldl_ui "text"
    PREFIX "/text"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES "${PROJECT_SOURCE_DIR}/CHANGELOG.md"
)

target_link_libraries(pldl_ui
    PUBLIC
        pldl_web
        pldl_services
        pldl_platform
        Qt6::Widgets
        Qt6::Svg
        Qt6::Concurrent
)
