#pragma once

#include "core/downloads/download_options.h"
#include "services/ktechpit_search.h"
#include "services/search_service.h"

#include <QList>
#include <QObject>
#include <QString>

#include <optional>

namespace pldl::core {
class Settings;
}

namespace pldl::services {

/// The Search page's one search (ADR-003, FEATURES B1 to B3): the search
/// service first, the engine's playlist search when the service fails, times
/// out or answers with nothing usable; "Load more" continues with whichever
/// source answered page 0 (the service has no paging). The engine is asked
/// for lazily: when it is needed and not there, `engineNeeded` fires and the
/// search waits for `retryPending()`.
class PlaylistSearch : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PlaylistSearch)

public:
    enum class Source
    {
        Service,
        Engine,
    };
    Q_ENUM(Source)

    enum class Mode
    {
        Automatic,
        EngineOnly,
    };
    Q_ENUM(Mode)

    explicit PlaylistSearch(core::Settings& settings, QObject* parent = nullptr);
    ~PlaylistSearch() override;

    /// Page 0 starts over; a later page continues with the source that
    /// answered page 0. Returns the search's id. The search in flight is cancelled.
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
    /// The mode from the settings, unless overridden (the debug hook forces
    /// the engine for one run).
    [[nodiscard]] Mode mode() const;
    void setModeOverride(std::optional<Mode> mode) { m_modeOverride = mode; }
    [[nodiscard]] Source source() const { return m_source; }
    [[nodiscard]] const QString& query() const { return m_query; }

    /// The two backends, for the tests to point at local servers.
    [[nodiscard]] KtechpitSearch& service() { return *m_service; }
    [[nodiscard]] SearchService& engine() { return *m_engine; }

    /// Whether a service outcome sends the query to the engine (pure): every
    /// one does; the service is only ever a first try.
    [[nodiscard]] static bool fallsBack(KtechpitSearch::Outcome outcome);
    /// The header chip's text: "Search" or "Search (engine)". Never a tool's name.
    [[nodiscard]] static QString describe(Source source);
    /// The chip's tooltip: why the results came from where they came from.
    [[nodiscard]] static QString tooltip(Source source);

Q_SIGNALS:
    void finished(quint64 id, const QList<pldl::services::SearchResult>& results, bool hasMore,
                  pldl::services::PlaylistSearch::Source source);
    void failed(quint64 id, const QString& userMessage);
    void sourceChanged(pldl::services::PlaylistSearch::Source source);
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
    void setSource(Source source);
    void handleServiceFinished(quint64 serviceId, const QList<SearchResult>& results);
    void handleServiceFailed(quint64 serviceId, KtechpitSearch::Outcome outcome);
    void handleEngineFinished(quint64 engineId, const QList<SearchResult>& results, bool hasMore);
    void handleEngineFailed(quint64 engineId, const QString& error);

    core::Settings& m_settings;
    KtechpitSearch* m_service;
    SearchService* m_engine;
    std::optional<Mode> m_modeOverride;
    Source m_source = Source::Service;
    QString m_query;
    quint64 m_nextId = 1;
    quint64 m_runningId = 0; ///< the search the page knows, 0 when idle
    quint64 m_serviceId = 0; ///< the service request serving m_runningId
    quint64 m_engineId = 0;  ///< the engine request serving m_runningId
    std::optional<Pending> m_pending;
};

} // namespace pldl::services

Q_DECLARE_METATYPE(pldl::services::PlaylistSearch::Source)
