#pragma once

#include "services/engine_manager.h"
#include "ui/downloads_filter_proxy.h"
#include "ui/pages/page.h"

#include <QList>
#include <QPointer>

class QLabel;
class QListView;
class QMenu;
class QPushButton;
class QStackedWidget;
class QToolButton;

namespace pldl::core {
class DownloadQueue;
class Settings;
} // namespace pldl::core

namespace pldl::ui {

class BadgeLabel;
class DownloadCardDelegate;
class DownloadsController;
class MessageSheet;

/// The Downloads page (DESIGN.md section 3): the engine chip, Pause all,
/// Resume all, the Clear menu and Open folder in the header; the All /
/// Active / Finished / Failed chips with the count line under them; the card
/// list over the controller's queue, or the empty state. Presentation only:
/// every action goes to the DownloadsController.
class DownloadsPage : public Page
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DownloadsPage)

public:
    using Filter = DownloadsFilterProxy::Filter;

    DownloadsPage(DownloadsController& controller, core::Settings& settings, core::ThemeService& theme,
                  QWidget* parent = nullptr);
    ~DownloadsPage() override = default;

    void setFilter(Filter filter);
    [[nodiscard]] Filter filter() const { return m_proxy->filter(); }
    /// The engine chip follows the engine's status; wired to the manager's
    /// statusChanged, callable directly (the tests).
    void setEngineStatus(const services::EngineManager::Status& status);
    /// Removes the finished, the failed and cancelled, or both.
    void clearFinished();
    void clearFailed();
    void clearAllFinished();

    [[nodiscard]] BadgeLabel* engineChip() const { return m_engineChip; }
    [[nodiscard]] QListView* list() const { return m_list; }
    [[nodiscard]] QToolButton* clearButton() const { return m_clear; }
    [[nodiscard]] QMenu* clearMenu() const { return m_clearMenu; }
    [[nodiscard]] QPushButton* pauseAllButton() const { return m_pauseAll; }
    [[nodiscard]] QPushButton* resumeAllButton() const { return m_resumeAll; }
    [[nodiscard]] QString countText() const;
    [[nodiscard]] bool isEmptyStateVisible() const;
    [[nodiscard]] int visibleCount() const;

Q_SIGNALS:
    /// The engine chip was clicked: the window opens the setup sheet.
    void engineSetupRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupHeader();
    void setupFilters();
    void setupList();
    void applyIcons();
    void onThemeChanged() override;
    void updateCounts();
    void openCurrent();
    void removeCurrent();
    void removeInStates(const QList<core::DownloadState>& states);

    DownloadsController& m_controller;
    core::DownloadQueue& m_queue;
    core::Settings& m_settings;
    DownloadsFilterProxy* m_proxy = nullptr;
    DownloadCardDelegate* m_delegate = nullptr;
    BadgeLabel* m_engineChip = nullptr;
    QPushButton* m_pauseAll = nullptr;
    QPushButton* m_resumeAll = nullptr;
    QToolButton* m_clear = nullptr;
    QMenu* m_clearMenu = nullptr;
    QAction* m_clearFinished = nullptr;
    QAction* m_clearFailed = nullptr;
    QAction* m_clearAll = nullptr;
    QPushButton* m_openFolder = nullptr;
    QList<QToolButton*> m_filters;
    QLabel* m_count = nullptr;
    QStackedWidget* m_stack = nullptr;
    QListView* m_list = nullptr;
    QLabel* m_emptyMark = nullptr;
    QLabel* m_emptyTitle = nullptr;
    QLabel* m_emptyBody = nullptr;
    QPointer<MessageSheet> m_removeSheet;
    services::EngineManager::Status m_engine;
};

} // namespace pldl::ui
