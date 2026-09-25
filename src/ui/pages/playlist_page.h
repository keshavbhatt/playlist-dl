#pragma once

#include "services/engine_manager.h"
#include "core/downloads/media_info.h"
#include "services/search_service.h"
#include "ui/pages/page.h"

#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QResizeEvent;
class QSpacerItem;
class QLineEdit;
class QListView;
class QPushButton;
class QSortFilterProxyModel;
class QSpinBox;
class QStackedWidget;
class QToolButton;

namespace pldl::core {
class Settings;
}
namespace pldl::services {
class MediaProbe;
}

namespace pldl::ui {

class PlaylistEntryDelegate;
class PlaylistModel;
class RangeSlider;
class ThumbnailCache;

/// The Playlist page (DESIGN.md section 3, FEATURES P1 to P10): the header
/// with the playlist's picture, title, channel and count, Download, Play all
/// and Copy link; the toolbar with Select all, the range, the filter, the
/// sort and the skip toggle; the entry rows; the selection footer. The page
/// reads the playlist flat through the probe and remembers the last one it
/// showed, so the rail's Playlist button comes back to it. Downloading is
/// the window's: the page says what was chosen.
class PlaylistPage : public Page
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PlaylistPage)

public:
    enum class State
    {
        Idle,    ///< nothing opened yet
        Loading, ///< the probe is running (or waiting for the engine)
        Ready,
        Empty,
        Error,
    };
    Q_ENUM(State)

    enum class SortOrder
    {
        PlaylistOrder,
        Title,
        Duration,
    };
    Q_ENUM(SortOrder)

    PlaylistPage(core::Settings& settings, core::ThemeService& theme, services::MediaProbe& probe,
                 ThumbnailCache& thumbnails, QWidget* parent = nullptr);
    ~PlaylistPage() override;

    /// Shows `known` (a search card's data) at once and reads the playlist.
    /// A probe still running for the previous playlist is dropped.
    void open(const QUrl& playlistUrl, const std::optional<services::SearchResult>& known = std::nullopt);
    /// A playlist read elsewhere: fills the page without probing.
    void openInfo(const core::MediaInfo& info);
    /// The same for a link the window probed itself (any site).
    void openInfo(const QUrl& playlistUrl, const core::MediaInfo& info);
    /// Reads the current playlist (again): Retry, and the window's answer to engineNeeded.
    void reload();
    /// The error state with `reason` (the window, when the engine setup failed).
    void showError(const QString& reason);

    [[nodiscard]] QUrl currentUrl() const { return m_url; }
    [[nodiscard]] const core::MediaInfo& info() const { return m_info; }
    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] QList<int> selectedIndexes() const;
    [[nodiscard]] int selectedCount() const;

    void setAllChecked(bool checked);
    void setRange(int from, int to);
    void setFilter(const QString& text);
    void setSortOrder(SortOrder order);
    [[nodiscard]] SortOrder sortOrder() const { return m_sortOrder; }
    /// "Skip videos already downloaded" (FEATURES P5), page-local until the
    /// setting exists: on, entries with a file in the playlist's folder are
    /// left out of the selection.
    void setSkipDownloaded(bool skip);
    [[nodiscard]] bool skipDownloaded() const { return m_skipDownloaded; }
    /// The folder the playlist's files land in with the current defaults.
    [[nodiscard]] QString expectedFolder() const;

    [[nodiscard]] PlaylistModel* model() const { return m_model; }
    [[nodiscard]] QSortFilterProxyModel* proxy() const { return m_proxy; }
    [[nodiscard]] QListView* list() const { return m_list; }
    [[nodiscard]] PlaylistEntryDelegate* delegate() const { return m_delegate; }
    [[nodiscard]] QToolButton* backButton() const { return m_back; }
    [[nodiscard]] QLabel* titleLabel() const { return m_title; }
    [[nodiscard]] QLabel* channelLabel() const { return m_channel; }
    [[nodiscard]] QLabel* countLabel() const { return m_count; }
    [[nodiscard]] QPushButton* downloadButton() const { return m_download; }
    [[nodiscard]] QPushButton* playAllButton() const { return m_playAll; }
    [[nodiscard]] QPushButton* copyLinkButton() const { return m_copyLink; }
    [[nodiscard]] QCheckBox* selectAllBox() const { return m_selectAll; }
    [[nodiscard]] QSpinBox* fromSpin() const { return m_from; }
    [[nodiscard]] QSpinBox* toSpin() const { return m_to; }
    [[nodiscard]] RangeSlider* rangeSlider() const { return m_range; }
    [[nodiscard]] QLineEdit* filterField() const { return m_filter; }
    [[nodiscard]] QComboBox* sortCombo() const { return m_sort; }
    [[nodiscard]] QCheckBox* skipBox() const { return m_skip; }
    [[nodiscard]] QLabel* footerLabel() const { return m_footer; }
    [[nodiscard]] QLabel* statusLabel() const { return m_status; }
    [[nodiscard]] QPushButton* retryButton() const { return m_retry; }

    /// A canned playlist (12 entries, one private, one without a title) for the
    /// demo hooks and the tests.
    [[nodiscard]] static core::MediaInfo demoInfo();

Q_SIGNALS:
    void backRequested();
    /// The header title changed (the window title follows it).
    void titleChanged(const QString& title);
    /// Play all, or a row's play button: the Browser page opens `url`.
    void playRequested(const QUrl& url);
    /// The Download button: the selection's 1-based playlist indexes.
    void downloadRequested(const pldl::core::MediaInfo& info, const QList<int>& indexes);
    /// A row's download button: just that video.
    void videoDownloadRequested(const pldl::core::MediaEntry& entry);
    /// The probe needs the engine: the window sets it up and calls reload().
    void engineNeeded();
    void toast(const QString& text);
    /// A playlist is open (the rail's Playlist button follows).
    void hasPlaylistChanged(bool open);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void onThemeChanged() override;

private:
    void buildHeader();
    void buildToolbar();
    void buildList();
    void buildStatus();
    void applyIcons();
    void setState(State state);

public:
    /// The engine's progress while the page waits for it (State::Loading).
    void setEngineStatus(const services::EngineManager::Status& status);

private:
    /// "1 item" / "%1 items": the page never says videos (owner, 2026-09-25).
    [[nodiscard]] QString itemWord(int count) const;
    void fillHeader(const QString& title, const QString& channel, const QString& thumbnail, int count,
                    double totalDuration);
    void renderThumbnail();
    void elideTitle();
    void populate();
    void handleProbeFinished(quint64 id, const core::MediaInfo& info);
    void handleProbeFailed(quint64 id, const QString& error);
    void handleRowAction(const QModelIndex& index, int action);
    void applyRange();
    void syncRangeToSelection();
    void updateSelectionUi();
    [[nodiscard]] QList<int> visibleRows() const;
    [[nodiscard]] QStringList downloadedFiles() const;
    [[nodiscard]] QUrl playAllUrl() const;

    core::Settings& m_settings;
    services::MediaProbe& m_probe;
    ThumbnailCache& m_thumbnails;
    QUrl m_url;
    core::MediaInfo m_info;
    State m_state = State::Idle;
    SortOrder m_sortOrder = SortOrder::PlaylistOrder;
    quint64 m_probeId = 0;
    bool m_skipDownloaded = true;
    bool m_syncing = false;
    QString m_titleText;
    QString m_thumbnailUrl;

    QToolButton* m_back = nullptr;
    QLabel* m_thumb = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_channel = nullptr;
    QLabel* m_count = nullptr;
    QPushButton* m_download = nullptr;
    QPushButton* m_playAll = nullptr;
    QPushButton* m_copyLink = nullptr;
    QStackedWidget* m_stack = nullptr;
    QWidget* m_listPane = nullptr;
    QHBoxLayout* m_toolRow = nullptr;      ///< select all, the range, then (when wide) filter, sort, skip
    QWidget* m_toolRow2 = nullptr;         ///< filter, sort, skip when the page is narrow
    bool m_toolbarNarrow = false;
    QSpacerItem* m_toolStretch = nullptr; ///< keeps row one left-aligned while the filter is on row two
    void relayoutToolbar();
    QCheckBox* m_selectAll = nullptr;
    QSpinBox* m_from = nullptr;
    QSpinBox* m_to = nullptr;
    RangeSlider* m_range = nullptr;
    QLineEdit* m_filter = nullptr;
    QComboBox* m_sort = nullptr;
    QCheckBox* m_skip = nullptr;
    QListView* m_list = nullptr;
    PlaylistModel* m_model = nullptr;
    QSortFilterProxyModel* m_proxy = nullptr;
    PlaylistEntryDelegate* m_delegate = nullptr;
    QLabel* m_footer = nullptr;
    QWidget* m_statusPane = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_retry = nullptr;
};

} // namespace pldl::ui
