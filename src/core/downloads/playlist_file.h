#pragma once

#include "core/downloads/download_job.h"

#include <QByteArray>
#include <QList>
#include <QString>

// The playlist file written next to a downloaded playlist (FEATURES E10): an
// extended M3U in UTF-8 (".m3u8"), one relative path per downloaded entry in
// the order given, so any media player plays the folder as a list. Pure
// except for `write`.
namespace pldl::core::playlist_file {

/// "<folder>/<title>.m3u8": the folder of the first downloaded file, else the
/// job's output folder; the title sanitised for a file name, "Playlist" when empty.
[[nodiscard]] QString pathFor(const DownloadJob& job);

/// The job's entries; a job that carries none (queued from a link before the
/// engine reported its items) gets one per output file, titled by file name.
[[nodiscard]] QList<PlaylistEntry> entriesOf(const DownloadJob& job);

/// The M3U text for the entries that have a file, paths relative to `baseDir`.
[[nodiscard]] QByteArray m3uContent(const QList<PlaylistEntry>& entries, const QString& baseDir,
                                     const QString& title);

/// Writes `content` to `path` (creating the folder); false when it cannot.
bool write(const QString& path, const QByteArray& content);

/// pathFor + entriesOf + m3uContent + write for the job's own order; false
/// when the job has no downloaded file or the write fails.
bool writeFor(const DownloadJob& job);

} // namespace pldl::core::playlist_file
