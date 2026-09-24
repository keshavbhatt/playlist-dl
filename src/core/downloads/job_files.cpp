#include "core/downloads/job_files.h"

#include "core/downloads/playlist_file.h"
#include "core/logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace pldl::core::job_files {

QStringList existingFiles(const DownloadJob& job)
{
    QStringList files;
    auto add = [&files](const QString& path) {
        if (!path.isEmpty() && !files.contains(path) && QFileInfo(path).isFile()) {
            files << path;
        }
    };
    for (const QString& file : job.outputFiles) {
        add(file);
    }
    for (const PlaylistEntry& entry : job.entries) {
        add(entry.file);
    }
    if (job.isPlaylist()) {
        add(playlist_file::pathFor(job));
    }
    return files;
}

Removal removeJobFiles(const DownloadJob& job)
{
    Removal result;
    QStringList folders;
    for (const QString& file : existingFiles(job)) {
        if (QFile::remove(file)) {
            ++result.filesRemoved;
        } else {
            ++result.filesFailed;
            qCWarning(lcCore) << "could not delete" << file;
        }
        const QString folder = QFileInfo(file).absolutePath();
        if (!folders.contains(folder)) {
            folders << folder;
        }
    }
    // A playlist's own folder goes with its files, when it is now empty; the
    // shared download folder never does.
    if (job.isPlaylist() && !job.options.folder.isEmpty()) {
        const QString own = QDir(job.options.outputDirectory).filePath(job.options.folder);
        for (const QString& folder : folders) {
            QDir dir(folder);
            if (dir.absolutePath() == QDir(own).absolutePath() && dir.isEmpty()) {
                result.folderRemoved = QDir().rmdir(folder);
            }
        }
    }
    return result;
}

} // namespace pldl::core::job_files
