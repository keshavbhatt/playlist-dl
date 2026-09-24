#pragma once

#include "core/downloads/download_job.h"

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

class QLabel;
class QListWidget;
class QPushButton;

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

/// A downloaded playlist's items (FEATURES E10; 2.x listed them too): every
/// entry with whether its file is there, Play for one, Play all through the
/// playlist file (written on the spot in the order shown, so it can be
/// arranged first: Move up, Move down, sort), Show in folder. The sheet
/// works on a copy of the job.
class PlaylistItemsSheet : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PlaylistItemsSheet)

public:
    static constexpr int kWidth = 680;

    PlaylistItemsSheet(const core::DownloadJob& job, core::ThemeService& theme, QWidget* parent = nullptr);
    ~PlaylistItemsSheet() override = default;

    /// The entries in the order shown.
    [[nodiscard]] const QList<core::PlaylistEntry>& entries() const { return m_entries; }
    /// The files of the entries that have one, in the order shown.
    [[nodiscard]] QStringList orderedFiles() const;
    [[nodiscard]] int downloadedCount() const;
    /// Where the playlist file goes (the job's folder).
    [[nodiscard]] QString playlistFilePath() const;
    /// Writes the playlist file in the order shown; false when nothing is downloaded or the write fails.
    bool writePlaylistFile();
    /// Moves the selected row by `delta` (-1 up, +1 down).
    void moveSelected(int delta);
    void sortByName();
    void sortByPlaylistOrder();
    [[nodiscard]] QListWidget* list() const { return m_list; }
    [[nodiscard]] QPushButton* playAllButton() const { return m_playAll; }

Q_SIGNALS:
    void toast(const QString& text);

private:
    void setupUi();
    void rebuildList();
    void refreshButtons();
    void playSelected();
    void revealSelected();
    void playAll();

    core::DownloadJob m_job;
    core::ThemeService& m_theme;
    QList<core::PlaylistEntry> m_entries;
    QList<core::PlaylistEntry> m_original; ///< the playlist's own order
    QLabel* m_summary = nullptr;
    QListWidget* m_list = nullptr;
    QPushButton* m_play = nullptr;
    QPushButton* m_reveal = nullptr;
    QPushButton* m_up = nullptr;
    QPushButton* m_down = nullptr;
    QPushButton* m_sortName = nullptr;
    QPushButton* m_sortPlaylist = nullptr;
    QPushButton* m_save = nullptr;
    QPushButton* m_playAll = nullptr;
};

} // namespace pldl::ui
