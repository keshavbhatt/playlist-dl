find_package(Qt6 ${PLDL_QT_MIN_VERSION} REQUIRED COMPONENTS Test Qml)

set(PLDL_TEST_ENV
    "QT_QPA_PLATFORM=offscreen"
    "QT_LOGGING_RULES=pldl.*.debug=false"
    "QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu"
    "QT_FORCE_STDERR_LOGGING=1"
)

set(PLDL_SNAP_RUNTIME "" CACHE PATH "kf6-core24 runtime snap root for running tests (dev builds)")
set(PLDL_SNAP_RUNTIME_LIB_PATH "" CACHE STRING "LD_LIBRARY_PATH for the snap runtime (dev builds)")
if(PLDL_SNAP_RUNTIME)
    list(APPEND PLDL_TEST_ENV
        "LD_LIBRARY_PATH=${PLDL_SNAP_RUNTIME_LIB_PATH}"
        "QT_PLUGIN_PATH=${PLDL_SNAP_RUNTIME}/usr/lib/x86_64-linux-gnu/qt6/plugins"
        "QTWEBENGINEPROCESS_PATH=${PLDL_SNAP_RUNTIME}/usr/lib/qt6/libexec/QtWebEngineProcess"
        "QTWEBENGINE_RESOURCES_PATH=${PLDL_SNAP_RUNTIME}/usr/share/qt6/resources"
    )
endif()

function(pldl_add_test name source link_target)
    qt_add_executable(${name} ${source})
    set_target_properties(${name} PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
    pldl_set_warnings(${name})
    target_link_libraries(${name} PRIVATE ${link_target} Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "${PLDL_TEST_ENV}")
    if(ARGC GREATER 3 AND ARGV3 STREQUAL "GUI")
        set_tests_properties(${name} PROPERTIES LABELS "gui" TIMEOUT 120)
    else()
        set_tests_properties(${name} PROPERTIES LABELS "unit" TIMEOUT 30)
    endif()
endfunction()

# core
pldl_add_test(tst_settings            tst_settings.cpp            pldl_core)
pldl_add_test(tst_legacy_account      tst_legacy_account.cpp      pldl_core)
pldl_add_test(tst_daily_allowance     tst_daily_allowance.cpp     pldl_core)
pldl_add_test(tst_changelog           tst_changelog.cpp           pldl_core)
target_compile_definitions(tst_changelog PRIVATE
    PLDL_CHANGELOG_PATH="${PROJECT_SOURCE_DIR}/CHANGELOG.md"
    PLDL_VERSION="${PROJECT_VERSION}")
pldl_add_test(tst_youtube_url         tst_youtube_url.cpp         pldl_core)
pldl_add_test(tst_navigation_policy   tst_navigation_policy.cpp   pldl_core)
pldl_add_test(tst_block_list          tst_block_list.cpp          pldl_core)
pldl_add_test(tst_media_info          tst_media_info.cpp          pldl_core)
pldl_add_test(tst_download_options    tst_download_options.cpp    pldl_core)
pldl_add_test(tst_ytdlp_output        tst_ytdlp_output.cpp        pldl_core)
pldl_add_test(tst_download_queue      tst_download_queue.cpp      pldl_core)
pldl_add_test(tst_engine_spec         tst_engine_spec.cpp         pldl_core)
pldl_add_test(tst_zoom_policy         tst_zoom_policy.cpp         pldl_core)
pldl_add_test(tst_render_crash_policy tst_render_crash_policy.cpp pldl_core)
pldl_add_test(tst_graphics_fallback   tst_graphics_fallback.cpp   pldl_core)
pldl_add_test(tst_gpu_storm_detector  tst_gpu_storm_detector.cpp  pldl_core)
pldl_add_test(tst_log_sink            tst_log_sink.cpp            pldl_core)
pldl_add_test(tst_storage_and_flags   tst_storage_and_flags.cpp   pldl_core)
pldl_add_test(tst_theme_service       tst_theme_service.cpp       pldl_core GUI)
pldl_add_test(tst_notification_service tst_notification_service.cpp pldl_core GUI)
# services
pldl_add_test(tst_netscape_cookies    tst_netscape_cookies.cpp    pldl_services)
target_link_libraries(tst_netscape_cookies PRIVATE Qt6::Sql)
pldl_add_test(tst_search_service      tst_search_service.cpp      pldl_services)
set_tests_properties(tst_search_service PROPERTIES TIMEOUT 180)
pldl_add_test(tst_engine_manager      tst_engine_manager.cpp      pldl_services)
set_tests_properties(tst_engine_manager PROPERTIES TIMEOUT 900)
pldl_add_test(tst_search_suggestions  tst_search_suggestions.cpp  pldl_services)
pldl_add_test(tst_playlist_search     tst_playlist_search.cpp     pldl_services)
# platform
if(LINUX)
    pldl_add_test(tst_freedesktop_notifier tst_freedesktop_notifier.cpp pldl_platform GUI)
endif()
# web
pldl_add_test(tst_user_agent          tst_user_agent.cpp          pldl_web)
pldl_add_test(tst_web_scripts         tst_web_scripts.cpp         pldl_web GUI)
target_link_libraries(tst_web_scripts PRIVATE Qt6::Qml)
pldl_add_test(tst_web_view_lifetime   tst_web_view_lifetime.cpp   pldl_web GUI)
# app / ui
pldl_add_test(tst_cli_options         tst_cli_options.cpp         pldl_app)
pldl_add_test(tst_single_instance     tst_single_instance.cpp     pldl_app)
pldl_add_test(tst_smoke               tst_smoke.cpp               pldl_ui GUI)
pldl_add_test(tst_style_contrast      tst_style_contrast.cpp      pldl_ui)
pldl_add_test(tst_browser_page        tst_browser_page.cpp        pldl_ui GUI)
pldl_add_test(tst_search_page         tst_search_page.cpp         pldl_ui GUI)
pldl_add_test(tst_download_card_delegate tst_download_card_delegate.cpp pldl_ui GUI)
pldl_add_test(tst_downloads_controller tst_downloads_controller.cpp pldl_ui GUI)
pldl_add_test(tst_downloads_page      tst_downloads_page.cpp      pldl_ui GUI)
pldl_add_test(tst_settings_dialog     tst_settings_dialog.cpp     pldl_ui GUI)
pldl_add_test(tst_playlist_entry_delegate tst_playlist_entry_delegate.cpp pldl_ui GUI)
pldl_add_test(tst_playlist_page       tst_playlist_page.cpp       pldl_ui GUI)
pldl_add_test(tst_download_options_sheet tst_download_options_sheet.cpp pldl_ui GUI)
pldl_add_test(tst_playlist_file        tst_playlist_file.cpp        pldl_core)
pldl_add_test(tst_job_files            tst_job_files.cpp            pldl_core)
pldl_add_test(tst_playlist_items_sheet tst_playlist_items_sheet.cpp pldl_ui GUI)
