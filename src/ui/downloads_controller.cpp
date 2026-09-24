#include "ui/downloads_controller.h"

#include "core/downloads/download_queue.h"
#include "core/downloads/job_files.h"
#include "core/downloads/playlist_file.h"
#include "core/notifications/notification_service.h"
#include "core/settings/settings.h"
#include "core/youtube_url.h"
#include "platform/file_manager.h"
#include "platform/notifier_factory.h"
#include "platform/screen_inhibitor.h"
#include "platform/taskbar_progress.h"
#include "services/engine_manager.h"
#include "services/licensing/license_service.h"
#include "services/media_probe.h"
#include "ui/logging.h"
#include "ui/message_sheet.h"
#include "ui/thumbnail_cache.h"
#include "web/cookie_exporter.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTemporaryFile>
#include <QTimer>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kPersistDelayMs = 500;
constexpr auto kDesktopId = "com.ktechpit.playlist-dl";
constexpr auto kOpenKey = "open";
constexpr auto kFolderKey = "folder";
constexpr auto kRetryKey = "retry";
} // namespace

DownloadsController::DownloadsController(core::Settings& settings, core::ThemeService& theme,
                                         services::EngineManager& engine, services::MediaProbe& probe,
                                         services::LicenseService& license, web::CookieExporter& cookies,
                                         QWidget* sheetParent, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_theme(theme)
    , m_engine(engine)
    , m_probe(probe)
    , m_license(license)
    , m_cookies(cookies)
    , m_sheetParent(sheetParent)
    , m_queue(new core::DownloadQueue(this))
    , m_thumbnails(new ThumbnailCache(this))
    , m_persistTimer(new QTimer(this))
    , m_persistencePath(core::DownloadQueue::defaultFilePath())
{
    // Platform pieces: parented to the controller, the factories' handles released.
    m_notifier = platform::createPlatformNotifier(this).release();
    m_notifications = new core::NotificationService(settings, m_notifier, nullptr, this);
    const QString desktopId = QGuiApplication::desktopFileName().isEmpty() ? QString::fromLatin1(kDesktopId)
                                                                           : QGuiApplication::desktopFileName();
    m_taskbar = platform::TaskbarProgress::create(desktopId, this).release();
    m_inhibitor = platform::ScreenInhibitor::create(this).release();

    m_persistTimer->setSingleShot(true);
    m_persistTimer->setInterval(kPersistDelayMs);
    connect(m_persistTimer, &QTimer::timeout, this, &DownloadsController::persist);

    m_queue->setMaxConcurrent(settings.concurrentDownloads());
    m_queue->setThumbnailDirectory(ThumbnailCache::keepsakeDirectory());
    connect(&settings, &core::Settings::concurrentDownloadsChanged, m_queue, &core::DownloadQueue::setMaxConcurrent);

    auto cookiesProvider = [this]() -> std::unique_ptr<QTemporaryFile> {
        if (!m_settings.useSessionCookies()) {
            return nullptr;
        }
        return m_cookies.writeTempFile();
    };
    m_queue->setCookiesProvider(cookiesProvider);
    m_probe.setCookiesProvider(cookiesProvider);

    wireEngine();
    wireQueue();
    wireNotifications();

    connect(&m_probe, &services::MediaProbe::finished, this, [this](quint64 id, const core::MediaInfo& info) {
        const auto it = m_pendingProbes.constFind(id);
        if (it == m_pendingProbes.constEnd()) {
            return; // another page's probe
        }
        const QUrl url = it.value();
        m_pendingProbes.erase(it);
        core::DownloadJob job;
        const core::YouTubeUrlInfo shape = core::classifyYouTubeUrl(url);
        const bool playlist = info.isPlaylist() || shape.kind == core::YouTubeUrlKind::Playlist ||
                              shape.kind == core::YouTubeUrlKind::Channel;
        // A watch link inside a playlist downloads that one video.
        const std::optional<QUrl> canonical = playlist ? std::nullopt : core::canonicalVideoUrl(url);
        job.url = canonical ? canonical->toString() : url.toString();
        job.videoId = playlist ? QString() : (info.id.isEmpty() ? shape.videoId : info.id);
        job.title = info.title;
        job.uploader = info.uploader;
        job.thumbnail = info.thumbnail;
        job.duration = info.duration;
        job.itemCount = playlist ? std::max(info.entryCount, static_cast<int>(info.entries.size())) : 0;
        job.options = defaultOptions(playlist);
        job.options.isChannel = shape.kind == core::YouTubeUrlKind::Channel;
        const quint64 jobId = enqueue(job);
        Q_EMIT requestSettled(url, jobId != 0);
    });
    connect(&m_probe, &services::MediaProbe::failed, this, [this](quint64 id, const QString& error) {
        const auto it = m_pendingProbes.constFind(id);
        if (it == m_pendingProbes.constEnd()) {
            return;
        }
        const QUrl url = it.value();
        m_pendingProbes.erase(it);
        qCWarning(lcUi) << "download request: could not read" << url << error;
        Q_EMIT toast(tr("Could not read that link: %1").arg(error));
        Q_EMIT requestSettled(url, false);
    });
}

// ---- wiring ----------------------------------------------------------------

void DownloadsController::wireEngine()
{
    connect(&m_engine, &services::EngineManager::ready, this,
            [this](const core::EnginePaths& paths) { m_queue->setEnginePaths(paths); });
    if (m_engine.isReady()) {
        m_queue->setEnginePaths(m_engine.paths());
    }
}

void DownloadsController::wireQueue()
{
    connect(m_queue, &core::DownloadQueue::changed, this, &DownloadsController::schedulePersist);
    connect(m_queue, &core::DownloadQueue::activeCountChanged, this, [this](int count) {
        syncActivity();
        Q_EMIT activeCountChanged(count);
    });
    connect(m_queue, &core::DownloadQueue::jobFinished, this, &DownloadsController::handleJobFinished);
    connect(m_queue, &core::DownloadQueue::jobAdded, this, &DownloadsController::keepThumbnail);
    connect(m_queue, &core::DownloadQueue::jobRemoved, m_thumbnails, &ThumbnailCache::forget);
    connect(m_queue, &QAbstractItemModel::dataChanged, this, [this] { syncTaskbar(); });
    connect(m_thumbnails, &ThumbnailCache::ready, this, [this](const QString& url) {
        const QList<quint64> waiting = m_awaitingThumbnail.take(url);
        for (const quint64 id : waiting) {
            keepThumbnail(id);
        }
    });
}

std::optional<core::DownloadJob> DownloadsController::notifiedJob(quint64 notificationId) const
{
    const auto it = m_notificationJobs.constFind(notificationId);
    return it == m_notificationJobs.constEnd() ? std::nullopt : m_queue->job(it.value());
}

void DownloadsController::wireNotifications()
{
    connect(m_notifications, &core::NotificationService::activated, this, [this](quint64 id) {
        if (const auto job = notifiedJob(id); job && !job->primaryFile().isEmpty()) {
            platform::openFile(job->primaryFile());
        }
    });
    connect(m_notifications, &core::NotificationService::actionInvoked, this, [this](quint64 id, const QString& key) {
        const auto job = notifiedJob(id);
        if (!job) {
            return;
        }
        if (key == QLatin1StringView(kOpenKey) && job->isPlaylist()) {
            // A playlist plays as a whole through its playlist file when it exists.
            const QString list = core::playlist_file::pathFor(*job);
            if (QFileInfo::exists(list)) {
                platform::openFile(list);
            } else {
                Q_EMIT playlistItemsRequested(job->id);
            }
        } else if (key == QLatin1StringView(kOpenKey) && !job->primaryFile().isEmpty()) {
            platform::openFile(job->primaryFile());
        } else if (key == QLatin1StringView(kFolderKey) && !job->primaryFile().isEmpty()) {
            platform::revealInFileManager(job->primaryFile());
        } else if (key == QLatin1StringView(kRetryKey)) {
            m_queue->retry(job->id);
        }
    });
    connect(m_notifications, &core::NotificationService::closed, this,
            [this](quint64 id) { m_notificationJobs.remove(id); });
}

// ---- lifecycle -------------------------------------------------------------

void DownloadsController::start()
{
    // load() turns whatever was queued or running when the app last ran
    // into Paused: a transfer never survives a restart, --continue picks
    // the .part up when the user resumes.
    m_queue->load(m_persistencePath);
    for (const core::DownloadJob& job : m_queue->jobs()) {
        keepThumbnail(job.id);
    }
    syncActivity();
}

void DownloadsController::shutdown()
{
    m_persistTimer->stop();
    cancelRequests();
    m_queue->pauseAll();
    persist();
    m_inhibitor->setInhibited(false, QString());
    m_taskbar->clear();
}

// ---- admission -------------------------------------------------------------

int DownloadsController::itemsOf(const core::DownloadJob& job)
{
    if (!job.isPlaylist()) {
        return 1;
    }
    if (job.itemCount > 0) {
        return job.itemCount;
    }
    // "1,3-5": count the ranges of the item spec when the total is unknown.
    int items = 0;
    const QStringList parts = job.options.playlistItems.split(u',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString from = part.section(u'-', 0, 0).trimmed();
        const QString to = part.section(u'-', 1, 1).trimmed();
        items += (part.contains(u'-') && !to.isEmpty()) ? std::max(1, to.toInt() - from.toInt() + 1) : 1;
    }
    return std::max(1, items);
}

bool DownloadsController::alreadyQueued(const core::DownloadJob& job) const
{
    for (const core::DownloadJob& existing : m_queue->jobs()) {
        if (existing.url == job.url && existing.options.kind == job.options.kind && !existing.isFinished()) {
            return true;
        }
    }
    return false;
}

bool DownloadsController::admit(int count, QWidget* parent)
{
    if (m_license.canDownload(count)) {
        m_license.spendDownloads(count);
        return true;
    }
    showLimitSheet(count, std::max(0, m_license.downloadsRemainingToday()), parent != nullptr ? parent : m_sheetParent);
    return false;
}

void DownloadsController::showLimitSheet(int requested, int remaining, QWidget* parent)
{
    if (MessageSheet* open = m_limitSheet.data(); open != nullptr && open->isVisible()) {
        open->raise();
        open->activateWindow();
        return;
    }
    const int limit = services::LicenseService::kFreeDownloadsPerDay;
    QString body = tr("The free version downloads up to %n video(s) a day.", nullptr, limit);
    if (remaining > 0) {
        body += u' ' + tr("%n more today, and this needs %1.", nullptr, remaining).arg(requested);
    } else {
        body += u' ' + tr("Today's %n are used up.", nullptr, limit);
    }
    body += u' ' + tr("The count starts again tomorrow. Pro has no daily limit.");
    auto* sheet = new MessageSheet(parent, MessageSheet::Tone::Info, tr("Daily download limit reached"), body);
    sheet->setAttribute(Qt::WA_DeleteOnClose);
    sheet->addButton(tr("Not now"));
    sheet->addButton(tr("View plans"), MessageSheet::Role::Primary);
    connect(sheet, &QDialog::finished, this, [this, sheet](int) {
        if (sheet->clickedIndex() == 1) {
            Q_EMIT plansRequested();
        }
    });
    m_limitSheet = sheet;
    sheet->open();
}

void DownloadsController::applySpeedLimit(core::DownloadJob& job) const
{
    job.options.speedLimitKbps = m_settings.speedLimitKbps();
}

quint64 DownloadsController::enqueue(core::DownloadJob job)
{
    if (alreadyQueued(job)) {
        return m_queue->add(job); // folds into the existing entry, nothing to count
    }
    if (!admit(itemsOf(job))) {
        return 0;
    }
    applySpeedLimit(job);
    const quint64 id = m_queue->add(job);
    qCInfo(lcUi) << "queued download" << id << job.url;
    Q_EMIT toast(tr("Added to queue"));
    return id;
}

QList<quint64> DownloadsController::enqueueAll(QList<core::DownloadJob> jobs)
{
    int items = 0;
    for (const core::DownloadJob& job : jobs) {
        if (!alreadyQueued(job)) {
            items += itemsOf(job);
        }
    }
    if (items > 0 && !admit(items)) {
        return {};
    }
    QList<quint64> ids;
    for (core::DownloadJob& job : jobs) {
        applySpeedLimit(job);
        ids << m_queue->add(job);
    }
    if (!ids.isEmpty()) {
        qCInfo(lcUi) << "queued" << ids.size() << "downloads";
        Q_EMIT toast(ids.size() == 1 ? tr("Added to queue") : tr("Added %n videos to the queue", nullptr, items));
    }
    return ids;
}

core::DownloadOptions DownloadsController::defaultOptions(bool playlist) const
{
    core::DownloadOptions o;
    switch (m_settings.lastDownloadKind()) {
    case core::DownloadKind::Audio:
        o.kind = core::DownloadOptions::Kind::Audio;
        break;
    case core::DownloadKind::Video:
    case core::DownloadKind::Custom: // custom needs format ids nobody chose yet
        o.kind = core::DownloadOptions::Kind::Video;
        break;
    }
    o.quality = m_settings.defaultQuality();
    o.container = m_settings.defaultContainer();
    o.audioFormat = m_settings.defaultAudioFormat();
    o.embedThumbnail = m_settings.embedThumbnail();
    o.embedMetadata = m_settings.embedMetadata();
    o.subtitleLanguages = m_settings.subtitleLanguages();
    o.embedSubtitles = true;
    o.filenamePattern = m_settings.filenamePattern();
    o.outputDirectory = m_settings.downloadDirectory();
    o.isPlaylist = playlist;
    o.folder = m_settings.organiseDownloads() ? core::downloadFolder(o.kind, playlist, false) : QString();
    o.speedLimitKbps = m_settings.speedLimitKbps();
    return o;
}

// ---- requests --------------------------------------------------------------

void DownloadsController::requestDownload(const QUrl& url)
{
    if (!core::isDownloadable(url)) {
        qCInfo(lcUi) << "download request: not a link the engine can read" << url;
        Q_EMIT toast(tr("That is not a link to a page with media."));
        Q_EMIT requestSettled(url, false);
        return;
    }
    if (!m_probe.hasEngine()) {
        qCWarning(lcUi) << "download request before the engine is ready" << url;
        Q_EMIT toast(tr("The download engine is not ready yet."));
        Q_EMIT requestSettled(url, false);
        return;
    }
    // A YouTube video is read in full; anything else flat, which lists a
    // playlist's entries and still answers a single item with its formats.
    const core::YouTubeUrlInfo shape = core::classifyYouTubeUrl(url);
    const bool flat = shape.kind != core::YouTubeUrlKind::Video;
    const quint64 id = m_probe.probe(url, flat);
    m_pendingProbes.insert(id, url);
    qCInfo(lcUi) << "download request: probing" << url << (flat ? "(flat)" : "");
}

void DownloadsController::cancelRequests()
{
    const QList<quint64> ids = m_pendingProbes.keys();
    for (const quint64 id : ids) {
        const QUrl url = m_pendingProbes.take(id);
        m_probe.cancel(id);
        Q_EMIT requestSettled(url, false);
    }
}

void DownloadsController::retryFailed()
{
    QList<quint64> failed;
    for (int row = 0; row < m_queue->rowCount(); ++row) {
        const QModelIndex index = m_queue->index(row, 0);
        if (index.data(core::DownloadQueue::StateRole).toInt() == static_cast<int>(core::DownloadState::Failed)) {
            failed << index.data(core::DownloadQueue::IdRole).toULongLong();
        }
    }
    for (const quint64 id : failed) {
        m_queue->retry(id);
    }
    if (!failed.isEmpty()) {
        Q_EMIT toast(failed.size() == 1 ? tr("Retrying 1 download") : tr("Retrying %1 downloads").arg(failed.size()));
    }
}

void DownloadsController::openDownloadFolder()
{
    const QString dir = m_settings.downloadDirectory();
    QDir().mkpath(dir);
    platform::openDirectory(dir);
}

void DownloadsController::handleCardAction(quint64 id, Action action)
{
    const auto job = m_queue->job(id);
    if (!job) {
        return;
    }
    switch (action) {
    case Action::PauseResume:
        if (job->state == core::DownloadState::Paused) {
            m_queue->resume(id); // counted when it was queued
        } else {
            m_queue->pause(id);
        }
        break;
    case Action::Cancel:
        m_queue->cancel(id);
        break;
    case Action::Retry:
        m_queue->retry(id);
        break;
    case Action::Open:
        if (job->isPlaylist()) {
            Q_EMIT playlistItemsRequested(id); // the items, Play all and the playlist file
        } else if (!job->primaryFile().isEmpty()) {
            platform::openFile(job->primaryFile());
        }
        break;
    case Action::ShowInFolder:
        if (!job->primaryFile().isEmpty()) {
            platform::revealInFileManager(job->primaryFile());
        } else {
            const QString folder = job->options.folder.isEmpty()
                                       ? job->options.outputDirectory
                                       : QDir(job->options.outputDirectory).filePath(job->options.folder);
            platform::openDirectory(QDir(folder).exists() ? folder : job->options.outputDirectory);
        }
        break;
    case Action::Remove:
        // Nothing on disk and nothing running: gone at once. Otherwise ask,
        // and offer to delete what the download left behind (owner request).
        if (core::job_files::existingFiles(*job).isEmpty() && !job->isActive() &&
            job->state != core::DownloadState::Queued) {
            m_queue->remove(id);
        } else {
            confirmRemove(*job);
        }
        break;
    }
}

void DownloadsController::confirmRemove(const core::DownloadJob& job)
{
    if (m_removeSheet != nullptr) {
        return;
    }
    const QStringList files = core::job_files::existingFiles(job);
    const bool running = job.isActive() || job.state == core::DownloadState::Queued;
    QString body;
    if (running) {
        body = tr("It has not finished. Removing it stops the transfer.");
    }
    if (!files.isEmpty()) {
        const QString folder = QFileInfo(files.first()).absolutePath();
        const QString count = files.size() == 1 ? tr("1 file is") : tr("%1 files are").arg(files.size());
        body += (body.isEmpty() ? QString() : u" "_s) + tr("%1 in %2.").arg(count, folder);
    }
    const quint64 id = job.id;
    auto* sheet = new MessageSheet(m_sheetParent, MessageSheet::Tone::Danger, tr("Remove this download?"), body);
    sheet->setObjectName(u"removeSheet"_s);
    sheet->setAttribute(Qt::WA_DeleteOnClose);
    sheet->addButton(tr("Keep"));
    sheet->addButton(files.isEmpty() ? tr("Remove") : tr("Remove from list"), files.isEmpty()
                                                                                    ? MessageSheet::Role::Destructive
                                                                                    : MessageSheet::Role::Normal);
    if (!files.isEmpty()) {
        sheet->addButton(tr("Delete the files too"), MessageSheet::Role::Destructive);
    }
    connect(sheet, &QDialog::finished, this, [this, sheet, id](int) {
        const int clicked = sheet->clickedIndex();
        if (clicked == 1) {
            removeJob(id, false);
        } else if (clicked == 2) {
            removeJob(id, true);
        }
    });
    m_removeSheet = sheet;
    sheet->open();
}

void DownloadsController::removeJob(quint64 id, bool deleteFiles)
{
    const auto job = m_queue->job(id);
    if (!job) {
        return;
    }
    // Stop the transfer first so nothing is written after the delete.
    m_queue->remove(id);
    if (!deleteFiles) {
        Q_EMIT toast(tr("Removed from the list"));
        return;
    }
    const core::job_files::Removal removal = core::job_files::removeJobFiles(*job);
    qCInfo(lcUi) << "removed job" << id << "files deleted" << removal.filesRemoved << "failed" << removal.filesFailed
                 << "folder" << removal.folderRemoved;
    if (removal.filesFailed > 0) {
        Q_EMIT toast(tr("Removed; %1 of %2 files could not be deleted")
                         .arg(removal.filesFailed)
                         .arg(removal.filesRemoved + removal.filesFailed));
    } else if (removal.filesRemoved == 1) {
        Q_EMIT toast(tr("Removed and 1 file deleted"));
    } else {
        Q_EMIT toast(tr("Removed and %1 files deleted").arg(removal.filesRemoved));
    }
}

// ---- queue events ----------------------------------------------------------

void DownloadsController::keepThumbnail(quint64 id)
{
    const auto job = m_queue->job(id);
    if (!job || job->thumbnail.isEmpty() || job->thumbnail.startsWith(u'/')) {
        return;
    }
    const QString local = m_thumbnails->keep(job->thumbnail, id);
    if (local.isEmpty()) {
        m_awaitingThumbnail[job->thumbnail].append(id); // ready(url) calls again
        return;
    }
    m_queue->setThumbnail(id, local);
}

bool DownloadsController::writePlaylistFileFor(quint64 id)
{
    if (!m_settings.writePlaylistFile()) {
        return false;
    }
    const auto job = m_queue->job(id);
    if (!job || !job->isPlaylist()) {
        return false;
    }
    const bool written = core::playlist_file::writeFor(*job);
    if (written) {
        qCInfo(lcUi) << "playlist file written for" << id << core::playlist_file::pathFor(*job);
    }
    return written;
}

void DownloadsController::handleJobFinished(quint64 id, core::DownloadState state)
{
    if (const auto finished = m_queue->job(id); finished && finished->thumbnail.startsWith(u'/')) {
        if (const QString card = m_thumbnails->compact(finished->thumbnail); card != finished->thumbnail) {
            m_queue->setThumbnail(id, card);
        }
    }
    syncActivity();
    const auto job = m_queue->job(id);
    if (!job) {
        return;
    }
    if (job->isPlaylist()) {
        writePlaylistFileFor(id); // whatever landed, even after a cancel or a failure
    }
    // A finished job is saved at once: the debounce would lose the last file
    // and the final state to a quit or a crash in the next half second.
    persist();
    if (state == core::DownloadState::Completed) {
        Q_EMIT jobCompleted(id, job->title, job->primaryFile());
    }
    if (!m_settings.notifyOnDownloadFinish() || state == core::DownloadState::Cancelled) {
        return;
    }
    core::Notification n;
    n.image = m_thumbnails->get(job->thumbnail).toImage();
    if (state == core::DownloadState::Completed) {
        n.title = tr("Download finished");
        n.body = job->title.isEmpty() ? QFileInfo(job->primaryFile()).fileName() : job->title;
        n.category = u"transfer.complete"_s;
        if (!job->primaryFile().isEmpty()) {
            n.actions.append({QString::fromLatin1(kOpenKey), tr("Open")});
            n.actions.append({QString::fromLatin1(kFolderKey), tr("Show in folder")});
        }
    } else {
        n.title = tr("Download failed");
        n.body = (job->title.isEmpty() ? job->url : job->title) + u"\n"_s + job->error;
        n.category = u"transfer.error"_s;
        n.actions.append({QString::fromLatin1(kRetryKey), tr("Retry")});
    }
    if (const quint64 notificationId = m_notifications->notify(n); notificationId != 0) {
        m_notificationJobs.insert(notificationId, id);
    }
}

void DownloadsController::syncActivity()
{
    const bool running = m_queue->runningCount() > 0;
    m_inhibitor->setInhibited(running, tr("Downloading"));
    syncTaskbar();
}

void DownloadsController::syncTaskbar()
{
    double sum = 0;
    int counted = 0;
    for (const core::DownloadJob& job : m_queue->jobs()) {
        if (!job.isActive()) {
            continue;
        }
        sum += std::max(0.0, job.progress());
        ++counted;
    }
    if (counted == 0) {
        m_taskbar->clear();
        return;
    }
    m_taskbar->setProgress(sum / counted);
}

// ---- persistence -----------------------------------------------------------

void DownloadsController::schedulePersist()
{
    m_persistTimer->start();
}

void DownloadsController::persist()
{
    m_queue->save(m_persistencePath);
}

} // namespace pldl::ui
