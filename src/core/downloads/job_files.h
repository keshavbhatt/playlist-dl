#pragma once

#include "core/downloads/download_job.h"

#include <QStringList>

// What a download left on disk (FEATURES E3): the files it produced, its
// playlist file, and the folder it made for a playlist. Pure except for
// `removeJobFiles`.
namespace pldl::core::job_files {

/// The job's files that exist: its output files, its entries' files and the
/// playlist file, each once.
[[nodiscard]] QStringList existingFiles(const DownloadJob& job);

struct Removal
{
    int filesRemoved = 0;
    int filesFailed = 0;
    bool folderRemoved = false; ///< the playlist's own folder, gone because it was left empty
};

/// Deletes the job's files; then, for a playlist with its own folder, that
/// folder when nothing is left in it.
Removal removeJobFiles(const DownloadJob& job);

} // namespace pldl::core::job_files
