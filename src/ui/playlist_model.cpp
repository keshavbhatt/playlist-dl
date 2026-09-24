#include "ui/playlist_model.h"

#include "core/youtube_url.h"
#include "ui/playlist_entry_delegate.h"

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::ui {

using Role = PlaylistEntryDelegate::Role;

PlaylistModel::PlaylistModel(QObject* parent)
    : QStandardItemModel(parent)
{
}

QString PlaylistModel::normalised(const QString& text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        if (c.isLetterOrNumber()) {
            out.append(c.toLower());
        }
    }
    return out;
}

bool PlaylistModel::looksDownloaded(const QStringList& files, const QString& id, const QString& title)
{
    const QString idMark = id.isEmpty() ? QString() : u"["_s + id + u"]"_s;
    const QString key = normalised(title);
    for (const QString& file : files) {
        if (!idMark.isEmpty() && file.contains(idMark)) {
            return true;
        }
        if (!key.isEmpty() && normalised(file).contains(key)) {
            return true;
        }
    }
    return false;
}

void PlaylistModel::setEntries(const QList<core::MediaEntry>& entries, const QStringList& downloadedFiles,
                               bool skipDownloaded)
{
    clear();
    m_entries = entries;
    int index = 0;
    for (const core::MediaEntry& entry : entries) {
        ++index;
        auto* item = new QStandardItem;
        const bool unavailable = PlaylistEntryDelegate::isUnavailableEntry(entry);
        const bool downloaded = !unavailable && looksDownloaded(downloadedFiles, entry.id, entry.title);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setData(index, Role::IndexRole);
        item->setData(entry.id, Role::IdRole);
        item->setData(entry.title, Role::TitleRole);
        item->setData(entry.uploader, Role::UploaderRole);
        item->setData(entry.thumbnail.isEmpty() && !entry.id.isEmpty() ? core::thumbnailUrl(entry.id).toString()
                                                                        : entry.thumbnail,
                      Role::ThumbnailRole);
        item->setData(entry.duration, Role::DurationRole);
        item->setData(entry.url, Role::UrlRole);
        item->setData(unavailable, Role::UnavailableRole);
        item->setData(downloaded, Role::DownloadedRole);
        item->setData(entry.isLive, Role::LiveRole);
        const bool checked = !unavailable && !(skipDownloaded && downloaded);
        item->setData(checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
        appendRow(item);
    }
}

core::MediaEntry PlaylistModel::entryAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row) : core::MediaEntry{};
}

int PlaylistModel::availableCount() const
{
    int count = 0;
    for (int row = 0; row < rowCount(); ++row) {
        if (!isUnavailable(row)) {
            ++count;
        }
    }
    return count;
}

int PlaylistModel::downloadedCount() const
{
    int count = 0;
    for (int row = 0; row < rowCount(); ++row) {
        if (isDownloaded(row)) {
            ++count;
        }
    }
    return count;
}

double PlaylistModel::totalDuration() const
{
    double total = 0;
    for (const core::MediaEntry& entry : m_entries) {
        if (entry.duration > 0) {
            total += entry.duration;
        }
    }
    return total;
}

bool PlaylistModel::isChecked(int row) const
{
    return index(row, 0).data(Qt::CheckStateRole).toInt() == Qt::Checked;
}

bool PlaylistModel::isUnavailable(int row) const
{
    return index(row, 0).data(Role::UnavailableRole).toBool();
}

bool PlaylistModel::isDownloaded(int row) const
{
    return index(row, 0).data(Role::DownloadedRole).toBool();
}

QList<int> PlaylistModel::selectedIndexes() const
{
    QList<int> out;
    for (int row = 0; row < rowCount(); ++row) {
        if (isChecked(row)) {
            out << index(row, 0).data(Role::IndexRole).toInt();
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

double PlaylistModel::selectedDuration() const
{
    double total = 0;
    for (int row = 0; row < rowCount(); ++row) {
        if (isChecked(row)) {
            total += std::max(0.0, m_entries.at(row).duration);
        }
    }
    return total;
}

int PlaylistModel::selectedCount() const
{
    int count = 0;
    for (int row = 0; row < rowCount(); ++row) {
        if (isChecked(row)) {
            ++count;
        }
    }
    return count;
}

void PlaylistModel::setChecked(int row, bool checked)
{
    if (row < 0 || row >= rowCount() || isUnavailable(row)) {
        return;
    }
    setData(index(row, 0), checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
}

void PlaylistModel::setRowsChecked(const QList<int>& rows, bool checked, bool skipDownloaded)
{
    for (const int row : rows) {
        setChecked(row, checked && !(skipDownloaded && isDownloaded(row)));
    }
}

void PlaylistModel::selectRange(int from, int to, bool skipDownloaded)
{
    for (int row = 0; row < rowCount(); ++row) {
        const int playlistIndex = index(row, 0).data(Role::IndexRole).toInt();
        const bool inRange = playlistIndex >= from && playlistIndex <= to;
        setChecked(row, inRange && !(skipDownloaded && isDownloaded(row)));
    }
}

void PlaylistModel::uncheckDownloaded()
{
    for (int row = 0; row < rowCount(); ++row) {
        if (isDownloaded(row)) {
            setChecked(row, false);
        }
    }
}

} // namespace pldl::ui
