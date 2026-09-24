#include "core/downloads/playlist_file.h"

#include "core/downloads/download_options.h"
#include "core/logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::core::playlist_file {

namespace {

QString folderOf(const DownloadJob& job)
{
    if (!job.outputFiles.isEmpty()) {
        return QFileInfo(job.outputFiles.first()).absolutePath();
    }
    return job.options.folder.isEmpty() ? job.options.outputDirectory
                                        : QDir(job.options.outputDirectory).filePath(job.options.folder);
}

QString titleOf(const QString& file)
{
    // "003 - Some title.mp4" -> "Some title"; the numbering prefix is the
    // download's, not the video's.
    QString base = QFileInfo(file).completeBaseName();
    static const QRegularExpression kIndexPrefix(u"^\\d{2,4} - "_s);
    return base.remove(kIndexPrefix);
}

} // namespace

QString pathFor(const DownloadJob& job)
{
    const QString name = sanitiseFolderName(job.title);
    return QDir(folderOf(job)).filePath((name.isEmpty() ? u"Playlist"_s : name) + u".m3u8"_s);
}

QList<PlaylistEntry> entriesOf(const DownloadJob& job)
{
    if (!job.entries.isEmpty()) {
        return job.entries;
    }
    QList<PlaylistEntry> out;
    for (const QString& file : job.outputFiles) {
        out.append(PlaylistEntry{{}, titleOf(file), file});
    }
    return out;
}

QByteArray m3uContent(const QList<PlaylistEntry>& entries, const QString& baseDir, const QString& title)
{
    QString text = u"#EXTM3U\n"_s;
    if (!title.trimmed().isEmpty()) {
        text += u"#PLAYLIST:"_s + title.trimmed() + u'\n';
    }
    const QDir base(baseDir);
    for (const PlaylistEntry& entry : entries) {
        if (entry.file.isEmpty()) {
            continue;
        }
        const QString name = entry.title.isEmpty() ? titleOf(entry.file) : entry.title;
        text += u"#EXTINF:-1,"_s + name + u'\n';
        text += base.relativeFilePath(entry.file) + u'\n';
    }
    return text.toUtf8();
}

bool write(const QString& path, const QByteArray& content)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcCore) << "playlist file: cannot open" << path << file.errorString();
        return false;
    }
    file.write(content);
    return file.commit();
}

bool writeFor(const DownloadJob& job)
{
    const QList<PlaylistEntry> entries = entriesOf(job);
    const bool any = std::any_of(entries.cbegin(), entries.cend(),
                                 [](const PlaylistEntry& e) { return !e.file.isEmpty(); });
    if (!any) {
        return false;
    }
    const QString path = pathFor(job);
    return write(path, m3uContent(entries, QFileInfo(path).absolutePath(), job.title));
}

} // namespace pldl::core::playlist_file
