#pragma once

#include "core/downloads/media_info.h"

#include <QList>
#include <QStandardItemModel>
#include <QString>
#include <QStringList>

namespace pldl::ui {

/// The Playlist page's entries: one row per MediaEntry with the delegate's
/// roles, the check state that is the selection, and the rules around it
/// (FEATURES P3, P4, P5): unavailable entries are never checked, entries
/// already in the download folder are skipped while asked to. Pure of any
/// widget so the selection arithmetic is tested on its own.
class PlaylistModel : public QStandardItemModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PlaylistModel)

public:
    explicit PlaylistModel(QObject* parent = nullptr);
    ~PlaylistModel() override = default;

    /// Fills the rows; `downloadedFiles` are the names in the folder the
    /// playlist would land in. Available entries start checked, minus the
    /// downloaded ones when `skipDownloaded`.
    void setEntries(const QList<core::MediaEntry>& entries, const QStringList& downloadedFiles, bool skipDownloaded);
    [[nodiscard]] const QList<core::MediaEntry>& entries() const { return m_entries; }
    [[nodiscard]] core::MediaEntry entryAt(int row) const;
    [[nodiscard]] int availableCount() const;
    [[nodiscard]] int downloadedCount() const;
    /// The sum of the known durations, in seconds.
    [[nodiscard]] double totalDuration() const;

    /// The checked entries' 1-based playlist indexes, ascending.
    [[nodiscard]] QList<int> selectedIndexes() const;
    [[nodiscard]] int selectedCount() const;
    [[nodiscard]] bool isChecked(int row) const;
    [[nodiscard]] bool isUnavailable(int row) const;
    [[nodiscard]] bool isDownloaded(int row) const;
    /// Checks a row; unavailable rows stay unchecked.
    void setChecked(int row, bool checked);
    /// Checks (or clears) the rows of `rows`; with `skipDownloaded` the
    /// downloaded ones are left unchecked.
    void setRowsChecked(const QList<int>& rows, bool checked, bool skipDownloaded);
    /// Checks the entries whose playlist index lies in [from, to] and clears
    /// the others.
    void selectRange(int from, int to, bool skipDownloaded);
    /// Clears the check of every downloaded entry (the skip toggle turned on).
    void uncheckDownloaded();

    /// Whether a file in `files` looks like `title` (or carries "[id]"): the
    /// names are compared with everything but letters and digits removed, so
    /// the engine's own sanitising of the title does not matter (pure).
    [[nodiscard]] static bool looksDownloaded(const QStringList& files, const QString& id, const QString& title);
    /// Lower case letters and digits only (pure).
    [[nodiscard]] static QString normalised(const QString& text);

private:
    QList<core::MediaEntry> m_entries;
};

} // namespace pldl::ui
