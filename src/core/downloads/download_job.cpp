#include "core/downloads/download_job.h"

#include "core/downloads/media_info.h"

#include <QJsonArray>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::core {

bool isActiveState(DownloadState state)
{
    return state == DownloadState::Probing || state == DownloadState::Downloading ||
           state == DownloadState::Processing;
}

bool isFinishedState(DownloadState state)
{
    return state == DownloadState::Completed || state == DownloadState::Failed ||
           state == DownloadState::Cancelled;
}

QString stateLabel(DownloadState state)
{
    switch (state) {
    case DownloadState::Queued:
        return u"Queued"_s;
    case DownloadState::Probing:
        return u"Starting"_s;
    case DownloadState::Downloading:
        return u"Downloading"_s;
    case DownloadState::Processing:
        return u"Processing"_s;
    case DownloadState::Paused:
        return u"Paused"_s;
    case DownloadState::Completed:
        return u"Finished"_s;
    case DownloadState::Failed:
        return u"Failed"_s;
    case DownloadState::Cancelled:
        return u"Cancelled"_s;
    }
    return {};
}

double DownloadJob::progress() const
{
    if (state == DownloadState::Completed) {
        return 1.0;
    }
    if (totalBytes <= 0) {
        return -1.0;
    }
    return std::clamp(static_cast<double>(downloadedBytes) / static_cast<double>(totalBytes), 0.0, 1.0);
}

QString DownloadJob::statusLine() const
{
    switch (state) {
    case DownloadState::Queued:
        return u"Queued"_s;
    case DownloadState::Probing:
        return u"Reading video info…"_s;
    case DownloadState::Downloading: {
        QString line;
        if (totalBytes > 0) {
            line = u"%1 of %2"_s.arg(formatBytes(downloadedBytes), formatBytes(totalBytes));
        } else if (downloadedBytes > 0) {
            line = formatBytes(downloadedBytes);
        } else {
            line = u"Starting…"_s;
        }
        if (bytesPerSecond > 0) {
            line += u" · %1/s"_s.arg(formatBytes(static_cast<qint64>(bytesPerSecond)));
        }
        if (etaSeconds > 0) {
            line += u" · %1"_s.arg(formatDuration(etaSeconds));
        }
        if (itemCount > 1) {
            line += u" · %1 of %2"_s.arg(itemIndex).arg(itemCount);
        }
        return line;
    }
    case DownloadState::Processing:
        return stage.isEmpty() ? u"Processing…"_s : stage + u"…"_s;
    case DownloadState::Paused:
        if (totalBytes > 0) {
            return u"Paused · %1 of %2"_s.arg(formatBytes(downloadedBytes), formatBytes(totalBytes));
        }
        return u"Paused"_s;
    case DownloadState::Completed: {
        QString line = u"Finished"_s;
        if (totalBytes > 0) {
            line += u" · "_s + formatBytes(totalBytes);
        }
        if (itemCount > 1) {
            line += u" · %1 items"_s.arg(itemCount);
        }
        return line;
    }
    case DownloadState::Failed:
        return error.isEmpty() ? u"Failed"_s : u"Failed · "_s + error;
    case DownloadState::Cancelled:
        return u"Cancelled"_s;
    }
    return {};
}

QString DownloadJob::primaryFile() const
{
    return outputFiles.isEmpty() ? QString() : outputFiles.first();
}

QString DownloadJob::detailLine() const
{
    if (isPlaylist() && itemCount > 0) {
        if (isFinished()) {
            return u"%1 videos"_s.arg(itemCount);
        }
        const QString place = u"%1 of %2"_s.arg(std::max(1, itemIndex)).arg(itemCount);
        return currentItemTitle.isEmpty() ? place : currentItemTitle + u", "_s + place;
    }
    return uploader;
}

QString DownloadJob::formatLine() const
{
    switch (options.kind) {
    case DownloadOptions::Kind::Video:
        return qualityLabel(options.quality) + u", "_s + containerExtension(options.container).toUpper();
    case DownloadOptions::Kind::Audio: {
        const QString ext = audioFormatExtension(options.audioFormat);
        return u"Audio, "_s + (ext.isEmpty() ? u"best"_s : ext.toUpper());
    }
    case DownloadOptions::Kind::Custom:
        return u"Custom format"_s;
    }
    return {};
}

QJsonObject PlaylistEntry::toJson() const
{
    return {{u"id"_s, id}, {u"title"_s, title}, {u"file"_s, file}};
}

PlaylistEntry PlaylistEntry::fromJson(const QJsonObject& o)
{
    PlaylistEntry e;
    e.id = o.value(u"id"_s).toString();
    e.title = o.value(u"title"_s).toString();
    e.file = o.value(u"file"_s).toString();
    return e;
}

int DownloadJob::downloadedEntryCount() const
{
    return static_cast<int>(std::count_if(entries.cbegin(), entries.cend(),
                                          [](const PlaylistEntry& e) { return !e.file.isEmpty(); }));
}

QJsonObject DownloadJob::toJson() const
{
    QJsonArray files;
    for (const QString& f : outputFiles) {
        files.append(f);
    }
    QJsonArray items;
    for (const PlaylistEntry& e : entries) {
        items.append(e.toJson());
    }
    return {
        {u"id"_s, static_cast<qint64>(id)},
        {u"url"_s, url},
        {u"videoId"_s, videoId},
        {u"title"_s, title},
        {u"uploader"_s, uploader},
        {u"thumbnail"_s, thumbnail},
        {u"duration"_s, duration},
        {u"options"_s, options.toJson()},
        {u"state"_s, static_cast<int>(state)},
        {u"downloadedBytes"_s, static_cast<qint64>(downloadedBytes)},
        {u"totalBytes"_s, static_cast<qint64>(totalBytes)},
        {u"itemIndex"_s, itemIndex},
        {u"itemCount"_s, itemCount},
        {u"outputFiles"_s, files},
        {u"entries"_s, items},
        {u"error"_s, error},
        {u"createdAt"_s, createdAt.toString(Qt::ISODate)},
        {u"finishedAt"_s, finishedAt.toString(Qt::ISODate)},
    };
}

DownloadJob DownloadJob::fromJson(const QJsonObject& o)
{
    DownloadJob j;
    j.id = static_cast<quint64>(o.value(u"id"_s).toDouble());
    j.url = o.value(u"url"_s).toString();
    j.videoId = o.value(u"videoId"_s).toString();
    j.title = o.value(u"title"_s).toString();
    j.uploader = o.value(u"uploader"_s).toString();
    j.thumbnail = o.value(u"thumbnail"_s).toString();
    j.duration = o.value(u"duration"_s).toDouble();
    j.options = DownloadOptions::fromJson(o.value(u"options"_s).toObject());
    j.state = static_cast<DownloadState>(std::clamp(o.value(u"state"_s).toInt(), 0, 7));
    j.downloadedBytes = static_cast<qint64>(o.value(u"downloadedBytes"_s).toDouble());
    j.totalBytes = static_cast<qint64>(o.value(u"totalBytes"_s).toDouble());
    j.itemIndex = o.value(u"itemIndex"_s).toInt();
    j.itemCount = o.value(u"itemCount"_s).toInt();
    const QJsonArray files = o.value(u"outputFiles"_s).toArray();
    for (const auto& f : files) {
        j.outputFiles << f.toString();
    }
    const QJsonArray items = o.value(u"entries"_s).toArray();
    for (const auto& e : items) {
        j.entries << PlaylistEntry::fromJson(e.toObject());
    }
    j.error = o.value(u"error"_s).toString();
    j.createdAt = QDateTime::fromString(o.value(u"createdAt"_s).toString(), Qt::ISODate);
    j.finishedAt = QDateTime::fromString(o.value(u"finishedAt"_s).toString(), Qt::ISODate);
    return j;
}

} // namespace pldl::core
