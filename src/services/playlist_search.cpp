#include "services/playlist_search.h"

#include "core/settings/settings.h"
#include "services/logging.h"

#include <QTimer>

using namespace Qt::StringLiterals;

namespace pldl::services {

PlaylistSearch::PlaylistSearch(core::Settings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_engine(new SearchService(this))
{
    m_engine->setPageSize(m_settings.searchResultsPerPage());
    connect(&m_settings, &core::Settings::searchChanged, this,
            [this] { m_engine->setPageSize(m_settings.searchResultsPerPage()); });
    connect(m_engine, &SearchService::finished, this, &PlaylistSearch::handleEngineFinished);
    connect(m_engine, &SearchService::failed, this, &PlaylistSearch::handleEngineFailed);
}

PlaylistSearch::~PlaylistSearch()
{
    cancel();
}

void PlaylistSearch::setEnginePaths(const core::EnginePaths& paths)
{
    m_engine->setEnginePaths(paths);
}

bool PlaylistSearch::isSearching() const
{
    return m_runningId != 0;
}

quint64 PlaylistSearch::search(const QString& query, int page)
{
    cancel();
    const quint64 id = m_nextId++;
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        QTimer::singleShot(0, this, [this, id] { Q_EMIT finished(id, {}, false); });
        return id;
    }
    m_runningId = id;
    if (page <= 0) {
        m_query = trimmed;
    }
    runEngine(id, m_query, std::max(0, page));
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
    m_engine->cancel();
    m_pending.reset();
    m_runningId = 0;
    m_engineId = 0;
}

void PlaylistSearch::handleEngineFinished(quint64 engineId, const QList<SearchResult>& results, bool hasMore)
{
    if (engineId != m_engineId || m_runningId == 0) {
        return;
    }
    const quint64 id = m_runningId;
    m_runningId = 0;
    m_engineId = 0;
    Q_EMIT finished(id, results, hasMore);
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
