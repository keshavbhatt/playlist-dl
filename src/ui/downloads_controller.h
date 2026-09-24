#pragma once

#include "core/downloads/download_job.h"
#include "core/downloads/download_options.h"
#include "ui/download_card_delegate.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <optional>

class QTimer;
class QWidget;

namespace pldl::core {
class DownloadQueue;
class INotifier;
class NotificationService;
class Settings;
class ThemeService;
} // namespace pldl::core
namespace pldl::platform {
class ScreenInhibitor;
class TaskbarProgress;
} // namespace pldl::platform
namespace pldl::services {
class EngineManager;
class LicenseService;
class MediaProbe;
} // namespace pldl::services
namespace pldl::web {
class CookieExporter;
}

namespace pldl::ui {

class MessageSheet;
class ThumbnailCache;

/// Owns the download queue and everything around it (FEATURES E3 to E8):
/// the engine paths, the session cookies, the settings that drive the
/// scheduler, persistence, thumbnail keepsakes, the finish notifications,
/// the taskbar progress and the screen inhibit, and the free tier's daily
/// gate at admission. The pages ask it to enqueue jobs and hand it the card
/// actions; the window wires its signals to the rail, the tray and the toasts.
class DownloadsController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DownloadsController)

public:
    using Action = DownloadCardDelegate::Action;

    DownloadsController(core::Settings& settings, core::ThemeService& theme, services::EngineManager& engine,
                        services::MediaProbe& probe, services::LicenseService& license,
                        web::CookieExporter& cookies, QWidget* sheetParent, QObject* parent = nullptr);
    ~DownloadsController() override = default;

    /// Loads the persisted list (stale transfers come back paused) and keeps
    /// the thumbnails of what it restored.
    void start();
    /// Pauses running transfers and persists the list (the window's close).
    void shutdown();

    /// Admits `count` videos against the free tier's daily allowance and
    /// counts them. When refused, a sheet explains the limit and offers the
    /// plans (plansRequested); `parent` is the sheet's parent, null for the
    /// window the controller was given.
    bool admit(int count, QWidget* parent = nullptr);
    /// Admits one job and queues it. Returns the id, or 0 when refused. A
    /// job for a link that is already queued or running is not counted twice.
    quint64 enqueue(core::DownloadJob job);
    /// Admits all of `jobs` at once (one refusal for the whole batch), then
    /// queues them. Returns the ids, or an empty list when refused.
    QList<quint64> enqueueAll(QList<core::DownloadJob> jobs);
    /// The download options the settings' defaults describe (the sheet's
    /// starting point, the CLI's and the browser's whole choice).
    [[nodiscard]] core::DownloadOptions defaultOptions(bool playlist) const;
    /// Probes `url` (full for a YouTube video, flat for anything else) and
    /// queues one job with the default options: the CLI's --download and the
    /// browser's Download this until the options sheet exists. Needs the
    /// engine (the window's ensureEngine); requestSettled follows either way.
    void requestDownload(const QUrl& url);
    /// Drops the probes requestDownload started that have not answered yet.
    void cancelRequests();
    void openDownloadFolder();
    void handleCardAction(quint64 id, Action action);
    /// Retries every failed download in the list.
    void retryFailed();
    /// Removes the job from the list; with `deleteFiles` its files, playlist
    /// file and (when left empty) its own folder go too.
    void removeJob(quint64 id, bool deleteFiles);
    /// The sheet asking whether to keep the files, while it is open (tests).
    [[nodiscard]] MessageSheet* removeSheet() const { return m_removeSheet.data(); }
    /// Writes the playlist file (.m3u8) for a playlist job that has files, when
    /// the setting is on; false when nothing was written. Called as jobs finish.
    bool writePlaylistFileFor(quint64 id);

    [[nodiscard]] core::DownloadQueue& queue() { return *m_queue; }
    [[nodiscard]] ThumbnailCache& thumbnails() { return *m_thumbnails; }
    [[nodiscard]] services::EngineManager& engine() { return m_engine; }
    [[nodiscard]] services::LicenseService& license() { return m_license; }
    [[nodiscard]] core::NotificationService& notifications() { return *m_notifications; }
    [[nodiscard]] QString persistencePath() const { return m_persistencePath; }
    /// Where the list is saved; the default is DownloadQueue::defaultFilePath().
    void setPersistencePath(const QString& path) { m_persistencePath = path; }

Q_SIGNALS:
    /// Queued plus running plus paused: the rail badge and the tray tooltip.
    void activeCountChanged(int count);
    /// The gate sheet's "View plans".
    void plansRequested();
    void jobCompleted(quint64 id, const QString& title, const QString& path);
    /// Open on a playlist card (and a playlist's notification without its
    /// playlist file): the window shows the playlist's items.
    void playlistItemsRequested(quint64 id);
    /// A requestDownload has either queued a job or given up (the browser
    /// releases its busy button on this).
    void requestSettled(const QUrl& url, bool queued);
    void toast(const QString& text);

private:
    void wireEngine();
    void wireQueue();
    void wireNotifications();
    [[nodiscard]] std::optional<core::DownloadJob> notifiedJob(quint64 notificationId) const;
    void handleJobFinished(quint64 id, core::DownloadState state);
    void keepThumbnail(quint64 id);
    void schedulePersist();
    void persist();
    void syncActivity();
    void syncTaskbar();
    [[nodiscard]] static int itemsOf(const core::DownloadJob& job);
    [[nodiscard]] bool alreadyQueued(const core::DownloadJob& job) const;
    void showLimitSheet(int requested, int remaining, QWidget* parent);
    void confirmRemove(const core::DownloadJob& job);
    void applySpeedLimit(core::DownloadJob& job) const;

    core::Settings& m_settings;
    core::ThemeService& m_theme;
    services::EngineManager& m_engine;
    services::MediaProbe& m_probe;
    services::LicenseService& m_license;
    web::CookieExporter& m_cookies;
    QWidget* m_sheetParent;
    core::DownloadQueue* m_queue = nullptr;
    ThumbnailCache* m_thumbnails = nullptr;
    core::INotifier* m_notifier = nullptr;
    core::NotificationService* m_notifications = nullptr;
    platform::TaskbarProgress* m_taskbar = nullptr;
    platform::ScreenInhibitor* m_inhibitor = nullptr;
    QTimer* m_persistTimer = nullptr;
    QString m_persistencePath;
    QHash<quint64, quint64> m_notificationJobs;          ///< notification id to job id
    QHash<QString, QList<quint64>> m_awaitingThumbnail;  ///< url to jobs waiting for keep()
    QHash<quint64, QUrl> m_pendingProbes;                ///< probe id to the requested url
    QPointer<MessageSheet> m_limitSheet;
    QPointer<MessageSheet> m_removeSheet;
};

} // namespace pldl::ui
