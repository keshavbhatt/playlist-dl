#pragma once

#include "core/downloads/download_options.h"
#include "services/search_service.h"

#include <QList>
#include <QObject>
#include <QString>

#include <optional>

namespace pldl::core {
class Settings;
}

namespace pldl::services {

/// The Search page's one search (ADR-003 as revised, FEATURES B1 to B3): the
/// download engine's playlist search, paged with "Load more". The engine is
/// asked for lazily: when it is needed and not there, `engineNeeded` fires
/// and the search waits for `retryPending()`.
class PlaylistSearch : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PlaylistSearch)

public:
    explicit PlaylistSearch(core::Settings& settings, QObject* parent = nullptr);
    ~PlaylistSearch() override;

    /// Page 0 starts over; a later page continues the same query. Returns
    /// the search's id. The search in flight is cancelled.
    quint64 search(const QString& query, int page = 0);
    void cancel();
    [[nodiscard]] bool isSearching() const;
    /// A search waiting for the engine (after `engineNeeded`).
    [[nodiscard]] bool hasPending() const { return m_pending.has_value(); }
    /// Resumes the search that waited for the engine; without an engine it
    /// fails with a message instead of waiting again.
    void retryPending();
    void setEnginePaths(const core::EnginePaths& paths);
    [[nodiscard]] bool hasEngine() const { return m_engine->hasEngine(); }
    [[nodiscard]] const QString& query() const { return m_query; }
    /// The engine's search, for the tests.
    [[nodiscard]] SearchService& engine() { return *m_engine; }

Q_SIGNALS:
    void finished(quint64 id, const QList<pldl::services::SearchResult>& results, bool hasMore);
    void failed(quint64 id, const QString& userMessage);
    /// The engine is needed and not provisioned: the search waits for
    /// `retryPending()` once the window has set it up.
    void engineNeeded();

private:
    struct Pending
    {
        quint64 id = 0;
        QString query;
        int page = 0;
    };
    void runEngine(quint64 id, const QString& query, int page);
    void handleEngineFinished(quint64 engineId, const QList<SearchResult>& results, bool hasMore);
    void handleEngineFailed(quint64 engineId, const QString& error);

    core::Settings& m_settings;
    SearchService* m_engine;
    QString m_query;
    quint64 m_nextId = 1;
    quint64 m_runningId = 0; ///< the search the page knows, 0 when idle
    quint64 m_engineId = 0;  ///< the engine request serving m_runningId
    std::optional<Pending> m_pending;
};

} // namespace pldl::services
