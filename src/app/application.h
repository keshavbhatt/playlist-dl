#pragma once

#include "app/cli_options.h"

#include <QApplication>

#include <memory>
#include <optional>

namespace pldl::core {
class Settings;
class ThemeService;
} // namespace pldl::core

namespace pldl::app {

class SingleInstance;

/// Application object. Sets identity (names/version, profile suffix), parses
/// the command line, owns Settings / ThemeService / SingleInstance and hands
/// them out by reference, the only sanctioned "global".
class Application : public QApplication
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Application)

public:
    Application(int& argc, char** argv);
    ~Application() override;

    [[nodiscard]] bool shouldExit() const { return m_exitCode.has_value(); }
    [[nodiscard]] int exitCode() const { return m_exitCode.value_or(0); }

    [[nodiscard]] const CliOptions& cliOptions() const { return m_cli; }
    [[nodiscard]] core::Settings& settings();
    [[nodiscard]] core::ThemeService& themeService();
    [[nodiscard]] SingleInstance& singleInstance();

    /// Dispatches a JSON command (from IPC or from our own CLI) to the UI.
    void dispatchCommand(const QJsonObject& command);
    /// Called once the window has stayed up long enough to deem the GPU stable.
    void markGpuStable();
    /// Starts a fresh copy with the same arguments and quits this one (a
    /// graphics fallback, the session clear). The single-instance lock is
    /// released first so the new copy becomes primary.
    void relaunch();
    /// Settings, "Sign out and clear session": leaves the marker the next
    /// start honours (the profile directories go before the web engine
    /// touches them) and relaunches.
    void clearSessionAndRelaunch();

Q_SIGNALS:
    void raiseRequested();
    void openRequested(const QString& url);
    void downloadRequested(const QString& url);
    void settingsRequested();
    void quitRequested();

private:
    void applyIdentity();
    void setupLogging();
    void applyChromiumFlags();
    void evaluateGpuStability();
    [[nodiscard]] QString gpuProbeMarkerPath() const;
    void honourClearSessionMarker();
    [[nodiscard]] bool forwardToPrimary();

    CliOptions m_cli;
    std::optional<int> m_exitCode;
    bool m_gpuTrialActive = false;
    std::unique_ptr<SingleInstance> m_instance;
    std::unique_ptr<core::Settings> m_settings;
    std::unique_ptr<core::ThemeService> m_theme;
};

} // namespace pldl::app
