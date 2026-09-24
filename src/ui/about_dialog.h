#pragma once

#include <QDialog>

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

    /// Feeds the list directly (tests).

private:
    void setupUi();
    void copyDiagnostics();
    void reportBug();

    const core::Settings& m_settings;
    core::ThemeService& m_theme;
    QString m_userAgent;
    QString m_engineSummary;
    QPlainTextEdit* m_debugText = nullptr;
};

} // namespace pldl::ui
