# core: pure logic. Links Qt6::Core (+ Qt6::Gui for value types only).
# MUST NOT link Widgets, WebEngine, Network, or anything in ui/web/platform.

qt_add_library(pldl_core STATIC
    blocking/block_list.cpp
    blocking/block_list.h
    chromium_flags.cpp
    chromium_flags.h
    downloads/download_job.cpp
    downloads/download_job.h
    downloads/download_options.cpp
    downloads/download_options.h
    downloads/download_queue.cpp
    downloads/download_queue.h
    downloads/download_runner.cpp
    downloads/download_runner.h
    downloads/engine_spec.cpp
    downloads/engine_spec.h
    downloads/media_info.cpp
    downloads/playlist_file.cpp
    downloads/playlist_file.h
    downloads/media_info.h
    downloads/ytdlp_output.cpp
    downloads/ytdlp_output.h
    graphics_fallback.cpp
    graphics_fallback.h
    gpu_storm_detector.cpp
    gpu_storm_detector.h
    log_sink.cpp
    log_sink.h
    logging.cpp
    logging.h
    media_state.h
    navigation_policy.cpp
    navigation_policy.h
    notifications/notification.h
    notifications/notification_service.cpp
    notifications/notification_service.h
    render_crash_policy.cpp
    render_crash_policy.h
    settings/settings.cpp
    settings/settings.h
    settings/settings_keys.h
    storage_policy.cpp
    storage_policy.h
    theme/theme_service.cpp
    theme/theme_service.h
    changelog.cpp
    changelog.h
    licensing/daily_allowance.cpp
    licensing/daily_allowance.h
    licensing/legacy_account.cpp
    licensing/legacy_account.h
    youtube_url.cpp
    youtube_url.h
    zoom_policy.cpp
    zoom_policy.h
)
pldl_set_warnings(pldl_core)

target_include_directories(pldl_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(pldl_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
)
