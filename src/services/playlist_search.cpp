#include "services/playlist_search.h"

#include "core/settings/settings.h"
#include "services/logging.h"

#include <QTimer>

using namespace Qt::StringLiterals;

namespace pldl::services {

PlaylistSearch::PlaylistSearch(core::Settings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_service(new KtechpitSearch(this))
    , m_engine(new SearchService(this))
{
    qRegisterMetaType<pldl::services::PlaylistSearch::Source>();
    m_engine->setPageSize(m_settings.searchResultsPerPage());
    connect(&m_settings, &core::Settings::searchChanged, this,
            [this] { m_engine->setPageSize(m_settings.searchResultsPerPage()); });
    connect(m_service, &KtechpitSearch::finished, this, &PlaylistSearch::handleServiceFinished);
    connect(m_service, &KtechpitSearch::failed, this, &PlaylistSearch::handleServiceFailed);
    connect(m_engine, &SearchService::finished, this, &PlaylistSearch::handleEngineFinished);
    connect(m_engine, &SearchService::failed, this, &PlaylistSearch::handleEngineFailed);
}

PlaylistSearch::~PlaylistSearch()
{
    cancel();
}

bool PlaylistSearch::fallsBack(KtechpitSearch::Outcome outcome)
{
    switch (outcome) {
    case KtechpitSearch::Outcome::NetworkError:
    case KtechpitSearch::Outcome::Timeout:
    case KtechpitSearch::Outcome::BadPayload:
    case KtechpitSearch::Outcome::Empty:
        return true;
    }
    return true;
}

QString PlaylistSearch::describe(Source source)
{
    return source == Source::Engine ? tr("Search (engine)") : tr("Search");
}

QString PlaylistSearch::tooltip(Source source)
{
    return source == Source::Engine
               ? tr("The search service did not answer, so the download engine searched instead")
               : tr("Results from the search service");
}

PlaylistSearch::Mode PlaylistSearch::mode() const
{
    if (m_modeOverride.has_value()) {
        return *m_modeOverride;
    }
    return m_settings.searchMode() == core::SearchMode::EngineOnly ? Mode::EngineOnly : Mode::Automatic;
}

void PlaylistSearch::setEnginePaths(const core::EnginePaths& paths)
{
    m_engine->setEnginePaths(paths);
}

bool PlaylistSearch::isSearching() const
{
    return m_runningId != 0;
}

void PlaylistSearch::setSource(Source source)
{
    if (m_source == source) {
        return;
    }
    m_source = source;
    Q_EMIT sourceChanged(source);
}

quint64 PlaylistSearch::search(const QString& query, int page)
{
    cancel();
    const quint64 id = m_nextId++;
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        QTimer::singleShot(0, this, [this, id] { Q_EMIT finished(id, {}, false, m_source); });
        return id;
    }
    m_runningId = id;
    if (page <= 0) {
        m_query = trimmed;
        if (mode() == Mode::Automatic) {
            // The service is the first try; its source is confirmed when it answers.
            m_serviceId = m_service->search(trimmed);
            return id;
        }
        setSource(Source::Engine);
        runEngine(id, trimmed, 0);
        return id;
    }
    if (m_source == Source::Service) {
        // The service has no paging: nothing more to load.
        QTimer::singleShot(0, this, [this, id] {
            if (m_runningId == id) {
                m_runningId = 0;
                Q_EMIT finished(id, {}, false, Source::Service);
            }
        });
        return id;
    }
    runEngine(id, m_query, page);
    return id;
}

void PlaylistSearch::runEngine(quint64 id, const QString& query, int page)
{
    if (!m_engine->hasEngine()) {
        qCInfo(lcSearch) << "search" << id << "needs the engine; waiting for it";
        m_pending = Pending{id, query, page};
        Q_EMIT engineNeeded();
        return;
    }
    m_pending.reset();
    m_engineId = m_engine->search(query, SearchKind::Playlists, page);
}

void PlaylistSearch::retryPending()
{
    if (!m_pending.has_value()) {
        return;
    }
    const Pending pending = *m_pending;
    m_pending.reset();
    if (m_runningId != pending.id) {
        return; // cancelled or replaced meanwhile
    }
    if (!m_engine->hasEngine()) {
        m_runningId = 0;
        Q_EMIT failed(pending.id, tr("The download engine is not ready."));
        return;
    }
    runEngine(pending.id, pending.query, pending.page);
}

void PlaylistSearch::cancel()
{
    m_service->cancel();
    m_engine->cancel();
    m_pending.reset();
    m_runningId = 0;
    m_serviceId = 0;
    m_engineId = 0;
}

void PlaylistSearch::handleServiceFinished(quint64 serviceId, const QList<SearchResult>& results)
{
    if (serviceId != m_serviceId || m_runningId == 0) {
        return;
    }
    const quint64 id = m_runningId;
    m_runningId = 0;
    m_serviceId = 0;
    setSource(Source::Service);
    Q_EMIT finished(id, results, false, Source::Service);
}

void PlaylistSearch::handleServiceFailed(quint64 serviceId, KtechpitSearch::Outcome outcome)
{
    if (serviceId != m_serviceId || m_runningId == 0) {
        return;
    }
    m_serviceId = 0;
    if (!fallsBack(outcome)) {
        const quint64 id = m_runningId;
        m_runningId = 0;
        Q_EMIT failed(id, tr("The search did not come back with anything."));
        return;
    }
    qCInfo(lcSearch) << "search" << m_runningId << "falls back to the engine after" << outcome;
    setSource(Source::Engine);
    runEngine(m_runningId, m_query, 0);
}

void PlaylistSearch::handleEngineFinished(quint64 engineId, const QList<SearchResult>& results, bool hasMore)
{
    if (engineId != m_engineId || m_runningId == 0) {
        return;
    }
    const quint64 id = m_runningId;
    m_runningId = 0;
    m_engineId = 0;
    Q_EMIT finished(id, results, hasMore, Source::Engine);
}

void PlaylistSearch::handleEngineFailed(quint64 engineId, const QString& error)
{
    if (engineId != m_engineId || m_runningId == 0) {
        return;
    }
    const quint64 id = m_runningId;
    m_runningId = 0;
    m_engineId = 0;
    Q_EMIT failed(id, error);
}

} // namespace pldl::services
