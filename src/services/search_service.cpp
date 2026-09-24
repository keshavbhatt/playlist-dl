#include "services/search_service.h"

#include "core/downloads/engine_spec.h"
#include "services/logging.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QProcess>
#include <QTimer>
#include <QUrl>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::services {

namespace {

// The site's own search filters, base64 of the protobuf the results page uses.
constexpr QLatin1StringView kPlaylistFilter{"EgIQAw%3D%3D"};
constexpr QLatin1StringView kChannelFilter{"EgIQAg%3D%3D"};
constexpr QLatin1StringView kSearchPage{"https://www.youtube.com/results"};

QString bestThumbnail(const QJsonObject& entry)
{
    const QString direct = entry.value(u"thumbnail"_s).toString();
    if (!direct.isEmpty()) {
        return direct;
    }
    const QJsonArray thumbnails = entry.value(u"thumbnails"_s).toArray();
    QString best;
    int bestWidth = -1;
    for (const auto& value : thumbnails) {
        const QJsonObject object = value.toObject();
        const QString url = object.value(u"url"_s).toString();
        if (url.isEmpty()) {
            continue;
        }
        const int width = object.value(u"width"_s).toInt();
        if (width >= bestWidth) {
            bestWidth = width;
            best = url;
        }
    }
    return best;
}

qint64 countOf(const QJsonValue& value)
{
    if (value.isDouble()) {
        return static_cast<qint64>(value.toDouble());
    }
    return -1;
}

} // namespace

QString searchKindName(SearchKind kind)
{
    switch (kind) {
    case SearchKind::Videos:
        return u"videos"_s;
    case SearchKind::Playlists:
        return u"playlists"_s;
    case SearchKind::Channels:
        return u"channels"_s;
    }
    return u"videos"_s;
}

SearchService::SearchService(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<pldl::services::SearchResult>();
    qRegisterMetaType<QList<pldl::services::SearchResult>>();
}

SearchService::~SearchService()
{
    cancel();
}

void SearchService::setPageSize(int count)
{
    m_pageSize = std::clamp(count, 1, 100);
}

QString SearchService::searchTarget(const QString& query, SearchKind kind, int lastItem)
{
    const QString trimmed = query.trimmed();
    if (kind == SearchKind::Videos) {
        return u"ytsearch"_s + QString::number(std::max(1, lastItem)) + u':' + trimmed;
    }
    // Built by hand: the filter is already percent-encoded and QUrl would
    // encode its percent signs a second time.
    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(trimmed));
    const QLatin1StringView filter = kind == SearchKind::Playlists ? kPlaylistFilter : kChannelFilter;
    return QString(kSearchPage) + u"?search_query="_s + encoded + u"&sp="_s + QString(filter);
}

QStringList SearchService::searchArguments(const core::EnginePaths& paths, const QString& query,
                                           SearchKind kind, int page, int pageSize)
{
    const int size = std::clamp(pageSize, 1, 100);
    const int first = std::max(0, page) * size + 1;
    const int last = first + size - 1;
    QStringList args = core::baseArguments(paths);
    args << u"--flat-playlist"_s << u"--dump-single-json"_s << u"--no-warnings"_s << u"--playlist-items"_s
         << QString::number(first) + u':' + QString::number(last) << u"--"_s
         << searchTarget(query, kind, last);
    return args;
}

QList<SearchResult> SearchService::parseResults(const QByteArray& json, SearchKind kind, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (document.isNull() || !document.isObject()) {
        if (error != nullptr) {
            *error = parseError.errorString();
        }
        return {};
    }
    const QJsonObject root = document.object();
    QList<SearchResult> results;
    const QJsonArray entries = root.value(u"entries"_s).toArray();
    results.reserve(entries.size());
    for (const auto& value : entries) {
        const QJsonObject entry = value.toObject();
        if (entry.isEmpty()) {
            continue;
        }
        SearchResult result;
        result.kind = kind;
        result.id = entry.value(u"id"_s).toString();
        result.title = entry.value(u"title"_s).toString();
        result.channel = entry.value(u"channel"_s).toString();
        if (result.channel.isEmpty()) {
            result.channel = entry.value(u"uploader"_s).toString();
        }
        result.durationSeconds = entry.value(u"duration"_s).toDouble();
        result.viewCount = countOf(entry.value(u"view_count"_s));
        result.uploadDate = entry.value(u"upload_date"_s).toString();
        result.itemCount = countOf(entry.value(u"playlist_count"_s));
        if (result.itemCount < 0) {
            result.itemCount = countOf(entry.value(u"n_entries"_s));
        }
        result.subscriberCount = countOf(entry.value(u"channel_follower_count"_s));
        result.thumbnailUrl = bestThumbnail(entry);
        result.url = entry.value(u"url"_s).toString();
        if (result.url.isEmpty()) {
            result.url = entry.value(u"webpage_url"_s).toString();
        }
        if (result.url.isEmpty() && !result.id.isEmpty() && kind == SearchKind::Videos) {
            result.url = u"https://www.youtube.com/watch?v="_s + result.id;
        }
        if (!result.isValid()) {
            continue;
        }
        if (result.thumbnailUrl.isEmpty() && kind == SearchKind::Videos && !result.id.isEmpty()) {
            result.thumbnailUrl = u"https://i.ytimg.com/vi/"_s + result.id + u"/hqdefault.jpg"_s;
        }
        results.append(result);
    }
    return results;
}

QStringList SearchService::countArguments(const core::EnginePaths& paths, const QString& url)
{
    QStringList args = core::baseArguments(paths);
    args << u"--flat-playlist"_s << u"--dump-single-json"_s << u"--no-warnings"_s << u"--playlist-items"_s << u"1"_s
         << u"--"_s << url;
    return args;
}

qint64 SearchService::parsePlaylistCount(const QByteArray& json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return -1;
    }
    const QJsonObject root = doc.object();
    for (const QString& key : {u"playlist_count"_s, u"n_entries"_s}) {
        if (const QJsonValue v = root.value(key); v.isDouble() && v.toDouble() >= 0) {
            return static_cast<qint64>(v.toDouble());
        }
    }
    return -1;
}

void SearchService::countPlaylist(const QString& url)
{
    if (url.isEmpty() || !hasEngine() || m_countQueue.contains(url) || m_counting.values().contains(url)) {
        return;
    }
    m_countQueue << url;
    runNextCount();
}

void SearchService::cancelCounts()
{
    m_countQueue.clear();
    const QList<QProcess*> running = m_counting.keys();
    m_counting.clear();
    for (QProcess* process : running) {
        disconnect(process, nullptr, this, nullptr);
        if (process->state() != QProcess::NotRunning) {
            connect(process, &QProcess::finished, process, &QObject::deleteLater);
            process->kill();
        } else {
            process->deleteLater();
        }
    }
}

void SearchService::runNextCount()
{
    while (m_counting.size() < kCountParallel && !m_countQueue.isEmpty()) {
        const QString url = m_countQueue.takeFirst();
        auto* process = new QProcess(this);
        process->setProcessChannelMode(QProcess::SeparateChannels);
        process->setProcessEnvironment(core::engineProcessEnvironment());
        const QStringList args = countArguments(m_paths, url);
        m_counting.insert(process, url);
        const auto done = [this, process] {
            if (!m_counting.contains(process)) {
                return;
            }
            const QString countedUrl = m_counting.take(process);
            const qint64 count = process->exitStatus() == QProcess::NormalExit && process->exitCode() == 0
                                     ? parsePlaylistCount(process->readAllStandardOutput())
                                     : -1;
            process->deleteLater();
            Q_EMIT playlistCounted(countedUrl, count);
            runNextCount();
        };
        connect(process, &QProcess::finished, this, done);
        connect(process, &QProcess::errorOccurred, this, [done](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                done();
            }
        });
        QTimer::singleShot(kCountTimeoutMs, process, [process] {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
            }
        });
        process->start(m_paths.ytdlp, args, QIODevice::ReadOnly);
    }
}

quint64 SearchService::search(const QString& query, SearchKind kind, int page, int timeoutMs)
{
    cancel();
    cancelCounts();
    const quint64 id = m_nextId++;
    if (query.trimmed().isEmpty()) {
        QTimer::singleShot(0, this, [this, id] { Q_EMIT finished(id, {}, false); });
        return id;
    }
    if (!hasEngine()) {
        QTimer::singleShot(0, this,
                           [this, id] { Q_EMIT failed(id, tr("The download engine is not ready.")); });
        return id;
    }
    m_runningId = id;
    m_runningKind = kind;
    m_timedOut = false;
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setProcessEnvironment(core::engineProcessEnvironment());
    const QStringList args = searchArguments(m_paths, query, kind, page, m_pageSize);
    connect(m_process, &QProcess::finished, this, [this, id] { handleFinished(id); });
    connect(m_process, &QProcess::errorOccurred, this, [this, id](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            handleFinished(id);
        }
    });
    QTimer::singleShot(std::max(1000, timeoutMs), this, [this, id] {
        if (m_runningId == id && m_process != nullptr && m_process->state() != QProcess::NotRunning) {
            qCWarning(lcServices) << "search" << id << "timed out";
            m_timedOut = true;
            m_process->kill();
        }
    });
    qCInfo(lcServices) << "search" << id << searchKindName(kind) << "page" << page;
    m_process->start(m_paths.ytdlp, args, QIODevice::ReadOnly);
    return id;
}

void SearchService::cancel()
{
    if (m_process == nullptr) {
        return;
    }
    QProcess* process = m_process;
    m_process = nullptr;
    m_runningId = 0;
    disconnect(process, nullptr, this, nullptr);
    if (process->state() != QProcess::NotRunning) {
        process->kill();
        connect(process, &QProcess::finished, process, &QObject::deleteLater);
    } else {
        process->deleteLater();
    }
}

void SearchService::handleFinished(quint64 id)
{
    if (m_process == nullptr || m_runningId != id) {
        return;
    }
    QProcess* process = m_process;
    m_process = nullptr;
    m_runningId = 0;
    const bool timedOut = m_timedOut;
    disconnect(process, nullptr, this, nullptr);
    process->deleteLater();

    if (process->error() == QProcess::FailedToStart) {
        Q_EMIT failed(id, tr("Could not start the download engine."));
        return;
    }
    if (timedOut) {
        Q_EMIT failed(id, tr("The search did not answer in time."));
        return;
    }
    const QByteArray out = process->readAllStandardOutput();
    if (process->exitStatus() != QProcess::NormalExit || process->exitCode() != 0 ||
        out.trimmed().isEmpty()) {
        const QString stderrText = QString::fromUtf8(process->readAllStandardError()).trimmed();
        qCWarning(lcServices) << "search" << id << "failed:" << stderrText;
        Q_EMIT failed(id, tr("The search did not come back with anything."));
        return;
    }
    QString error;
    const QList<SearchResult> results = parseResults(out, m_runningKind, &error);
    if (results.isEmpty() && !error.isEmpty()) {
        Q_EMIT failed(id, tr("The search did not come back with anything."));
        return;
    }
    Q_EMIT finished(id, results, results.size() >= m_pageSize);
}

} // namespace pldl::services
