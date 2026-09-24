#pragma once

#include "services/engine_manager.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTimer;

namespace pldl::core {
class Settings;
class ThemeService;
} // namespace pldl::core
namespace pldl::web {
class RequestInterceptor;
}

namespace pldl::ui {

/// Settings (DESIGN.md section 3, FEATURES G1): a sidebar of six pages, each
/// a column of cards whose rows carry a label on the left and the control on
/// the right. Every control writes straight to core::Settings on change and
/// follows the settings' signals, so two views never disagree; nothing is
/// buffered behind OK or Apply. Destructive or window-level actions go out
/// as signals for MainWindow.
class SettingsDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SettingsDialog)

public:
    enum Page
    {
        General,
        Appearance,
        Downloads,
        Browser,
        Search,
        Advanced,
    };
    static constexpr int kPageCount = 6;

    SettingsDialog(core::Settings& settings, core::ThemeService& theme, services::EngineManager& engine,
                   web::RequestInterceptor& interceptor, bool trayAvailable, QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    void showPage(int index);
    /// Page by name ("general", "appearance", "downloads", "browser",
    /// "search", "advanced"); an unknown name opens General.
    void showPage(const QString& name);
    [[nodiscard]] static int pageIndex(const QString& name);
    [[nodiscard]] int currentPage() const;

    /// The Engine card's status line for a manager state: "Download engine
    /// <version>", "Setting up", "Update available (<version>)", "Not
    /// installed" or "Media converter missing". Pure, unit-tested.
    [[nodiscard]] static QString engineStatusText(const services::EngineManager::Status& status);
    /// The muted line under it: the install hint, the step, the check result; empty for nothing to add.
    [[nodiscard]] static QString engineStatusDetail(const services::EngineManager::Status& status);

Q_SIGNALS:
    void clearCacheRequested();
    void clearSessionRequested();
    void resetPermissionsRequested();
    void openLogFolderRequested();
    void copyDiagnosticsRequested();
    void engineSetupRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void setupUi();
    void applyNavIcons();
    QWidget* buildGeneral();
    QWidget* buildAppearance();
    QWidget* buildDownloads();
    QWidget* buildDownloadDefaults();
    QWidget* buildEngineCard();
    QWidget* buildBrowser();
    QWidget* buildIdentityPicker();
    QWidget* buildSearch();
    QWidget* buildAdvanced();
    void connectSettings();
    void loadValues();
    void loadDownloadValues();
    void loadBrowserValues();
    void refreshEngine(const services::EngineManager::Status& status);
    void refreshBlockedCount();
    void chooseDownloadFolder();
    void resetSettings();
    void installWheelGuards();

    core::Settings& m_settings;
    core::ThemeService& m_theme;
    services::EngineManager& m_engine;
    web::RequestInterceptor& m_interceptor;
    bool m_trayAvailable;
    bool m_loading = false;

    QListWidget* m_nav = nullptr;
    QStackedWidget* m_pages = nullptr;
    QTimer* m_blockedTimer = nullptr;

    // general
    QComboBox* m_startPage = nullptr;
    QComboBox* m_closeAction = nullptr;
    QCheckBox* m_keepHistory = nullptr;
    QCheckBox* m_showWhatsNew = nullptr;
    QCheckBox* m_notifyFinish = nullptr;
    // appearance
    QComboBox* m_themeChoice = nullptr;
    QSpinBox* m_scale = nullptr;
    // downloads
    QLineEdit* m_folder = nullptr;
    QCheckBox* m_playlistFolder = nullptr;
    QCheckBox* m_numberFiles = nullptr;
    QComboBox* m_kind = nullptr;
    QComboBox* m_quality = nullptr;
    QComboBox* m_container = nullptr;
    QComboBox* m_audioFormat = nullptr;
    QComboBox* m_audioBitrate = nullptr;
    QLineEdit* m_subtitles = nullptr;
    QCheckBox* m_embedThumbnail = nullptr;
    QCheckBox* m_embedMetadata = nullptr;
    QSpinBox* m_concurrent = nullptr;
    QSpinBox* m_speedLimit = nullptr;
    QCheckBox* m_skipExisting = nullptr;
    QCheckBox* m_sessionCookies = nullptr;
    // engine card
    QLabel* m_engineStatus = nullptr;
    QLabel* m_engineDetail = nullptr;
    QPushButton* m_engineCheck = nullptr;
    QPushButton* m_engineUpdate = nullptr;
    QPushButton* m_engineSetup = nullptr;
    QCheckBox* m_engineAutoUpdate = nullptr;
    QCheckBox* m_engineUseSystem = nullptr;
    QLineEdit* m_engineSystemPath = nullptr;
    // browser
    QLineEdit* m_browserStart = nullptr;
    QCheckBox* m_restoreTabs = nullptr;
    QCheckBox* m_blockAds = nullptr;
    QLabel* m_blockedCount = nullptr;
    QCheckBox* m_doNotTrack = nullptr;
    QComboBox* m_identity = nullptr;
    QLineEdit* m_identityText = nullptr;
    // search
    QSpinBox* m_resultsPerPage = nullptr;
    // advanced
    QComboBox* m_hardware = nullptr;
};

} // namespace pldl::ui
