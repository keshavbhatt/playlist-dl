#pragma once

#include <QDialog>

class QLineEdit;
class QListWidget;
class QLabel;
class QPlainTextEdit;

namespace pldl::core {
class Settings;
class ThemeService;
} // namespace pldl::core

namespace pldl::ui {

/// About box: brand hero, links, and a copyable diagnostics panel (FEATURES S7).
class AboutDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AboutDialog)

public:
    AboutDialog(const core::Settings& settings, core::ThemeService& theme, const QString& userAgent,
                const QString& engineSummary, QWidget* parent = nullptr);
    ~AboutDialog() override = default;

    /// The download engine's path: fills the supported sites list (FEATURES B7).
    void setEnginePath(const QString& path);
    /// Feeds the list directly (tests).
    void setSupportedSites(const QStringList& sites);
    [[nodiscard]] int supportedSiteCount() const;

private:
    void setupUi();
    void filterSites(const QString& text);
    void copyDiagnostics();
    void reportBug();

    const core::Settings& m_settings;
    core::ThemeService& m_theme;
    QString m_userAgent;
    QString m_engineSummary;
    QPlainTextEdit* m_debugText = nullptr;
    QLineEdit* m_siteFilter = nullptr;
    QListWidget* m_sites = nullptr;
    QLabel* m_siteCount = nullptr;
    QStringList m_allSites;
};

} // namespace pldl::ui
