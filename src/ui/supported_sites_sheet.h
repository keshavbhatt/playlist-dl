#pragma once

#include "services/supported_sites.h"

#include <QDialog>
#include <QList>
#include <QString>
#include <QUrl>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

/// The Supported sites sheet (ported 2026-09-25): a filter, the count and the engine version, one row per
/// entry grouped by site ("Vimeo", "Vimeo: album"). Open on a row (Enter, a
/// double click, or the Open link on the hovered or selected row) sends the
/// site's home page to the built-in browser.
class SupportedSitesSheet : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SupportedSitesSheet)

public:
    SupportedSitesSheet(const services::SupportedSites& sites, core::ThemeService& theme,
                        QWidget* parent = nullptr);
    ~SupportedSitesSheet() override = default;

    void setFilter(const QString& text);
    [[nodiscard]] QString filter() const;
    /// Rows shown for the current filter.
    [[nodiscard]] int shownCount() const;
    [[nodiscard]] QLineEdit* filterField() const { return m_filter; }
    [[nodiscard]] QListWidget* list() const { return m_list; }

Q_SIGNALS:
    void openSiteRequested(const QUrl& url);

private:
    void setupUi();
    void rebuild();
    void openRow(QListWidgetItem* item);
    bool eventFilter(QObject* watched, QEvent* event) override;

    const services::SupportedSites& m_sites;
    core::ThemeService& m_theme;
    QLineEdit* m_filter = nullptr;
    QLabel* m_count = nullptr;
    QLabel* m_version = nullptr;
    QListWidget* m_list = nullptr;
};

} // namespace pldl::ui
