#pragma once

#include "core/downloads/download_options.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

// In-app search (FEATURES B5): the media engine's own search, read flat so a
// page of results costs one request and no per-item extraction.
namespace pldl::services {

/// What a search looks for. The engine searches videos natively
/// (`ytsearch<n>:`); playlists and channels come from the site's own search
/// page with its filter, which is why they carry less per item (see
/// SearchService).
enum class SearchKind
{
    Videos,
    Playlists,
    Channels,
};

[[nodiscard]] QString searchKindName(SearchKind kind);

struct SearchResult
{
    QString id;
    QString title;
    QString channel;
    double durationSeconds = 0; ///< 0 when the flat entry has none
    qint64 viewCount = -1;      ///< -1 when unknown
    QString uploadDate;         ///< yyyyMMdd, empty when unknown
    QString thumbnailUrl;
    QString url;
    SearchKind kind = SearchKind::Videos;
    qint64 itemCount = -1;       ///< playlists: number of videos, -1 when unknown
    qint64 subscriberCount = -1; ///< channels: followers, -1 when unknown

    [[nodiscard]] bool isValid() const { return !url.isEmpty(); }
};

/// One search at a time, through the media engine. `search()` cancels the
/// search still running, starts a flat query and answers once with the page's
/// results; "Load more" is the next page of the same query.
///
/// Limitation, playlists and channels: the engine has no `ytsearchplaylist:`
/// or `ytsearchchannel:` prefix, so those two go through the site's search
/// page with its own filter. Flat entries there carry a title, a URL and a
/// thumbnail but no duration, no view count and often no channel name, and
/// the result count per page is the site's, not ours, so a page can come back
/// shorter than `pageSize` and still have more behind it.
class SearchService : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SearchService)

public:
    static constexpr int kDefaultTimeoutMs = 60000;
    static constexpr int kDefaultPageSize = 20;

    explicit SearchService(QObject* parent = nullptr);
    ~SearchService() override;

    void setEnginePaths(const core::EnginePaths& paths) { m_paths = paths; }
    [[nodiscard]] bool hasEngine() const { return !m_paths.ytdlp.isEmpty(); }
    void setPageSize(int count);
    [[nodiscard]] int pageSize() const { return m_pageSize; }

    /// Starts a search and returns its id. `page` is 0-based: page 1 is the
    /// "Load more" page. Any search still running is cancelled first.
    quint64 search(const QString& query, SearchKind kind, int page = 0, int timeoutMs = kDefaultTimeoutMs);
    void cancel();
    [[nodiscard]] bool isSearching() const { return m_process != nullptr; }

    /// The flat search output carries no size for a playlist; this asks the
    /// engine for the playlist's own header (one item, cheap) and answers
    /// with `playlistCounted`. Queued, two at a time; a new search drops the
    /// queue. -1 when the engine could not say.
    void countPlaylist(const QString& url);
    void cancelCounts();
    [[nodiscard]] int pendingCounts() const { return static_cast<int>(m_countQueue.size() + m_counting.size()); }
    /// The argv for one count (pure) and the parse of its output (pure).
    [[nodiscard]] static QStringList countArguments(const core::EnginePaths& paths, const QString& url);
    [[nodiscard]] static qint64 parsePlaylistCount(const QByteArray& json);

    /// What the engine is pointed at for the query: `ytsearch<n>:<query>` for
    /// videos, the site's filtered search URL for playlists and channels (pure).
    [[nodiscard]] static QString searchTarget(const QString& query, SearchKind kind, int lastItem);
    /// The full argv for one page (pure).
    [[nodiscard]] static QStringList searchArguments(const core::EnginePaths& paths, const QString& query,
                                                     SearchKind kind, int page, int pageSize);
    /// Parses the engine's flat `-J` output into results (pure).
    [[nodiscard]] static QList<SearchResult> parseResults(const QByteArray& json, SearchKind kind,
                                                          QString* error = nullptr);

Q_SIGNALS:
    void finished(quint64 id, const QList<pldl::services::SearchResult>& results, bool hasMore);
    void failed(quint64 id, const QString& error);
    void playlistCounted(const QString& url, qint64 count);

private:
    void handleFinished(quint64 id);
    void runNextCount();

    core::EnginePaths m_paths;
    QProcess* m_process = nullptr;
    quint64 m_runningId = 0;
    quint64 m_nextId = 1;
    int m_pageSize = kDefaultPageSize;
    bool m_timedOut = false;
    SearchKind m_runningKind = SearchKind::Videos;
    QStringList m_countQueue;
    QHash<QProcess*, QString> m_counting;
    static constexpr int kCountParallel = 2;
    static constexpr int kCountTimeoutMs = 30000;
};

} // namespace pldl::services

Q_DECLARE_METATYPE(pldl::services::SearchResult)
Q_DECLARE_METATYPE(pldl::services::SearchKind)
