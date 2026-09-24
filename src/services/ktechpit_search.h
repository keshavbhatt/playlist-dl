#pragma once

#include "services/search_service.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

namespace pldl::services {

/// The playlist search service 2.x used (FEATURES B1, ADR-003): one GET with
/// the query, an Invidious-shaped JSON array of playlists back. One search at
/// a time; a new one cancels the one in flight. Every failure is typed so the
/// orchestrator can decide whether to fall back to the engine. Nothing here
/// reaches the user: the page never names the host.
class KtechpitSearch : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(KtechpitSearch)

public:
    enum class Outcome
    {
        NetworkError, ///< no connection, a refused port, an HTTP error status
        Timeout,      ///< nothing came back within the transfer timeout
        BadPayload,   ///< the body is not a JSON array of objects
        Empty,        ///< a JSON array with nothing usable in it
    };
    Q_ENUM(Outcome)

    static constexpr int kDefaultTimeoutMs = 8000;

    explicit KtechpitSearch(QObject* parent = nullptr);
    ~KtechpitSearch() override;

    /// The endpoint without a query; the tests point it at a local server.
    void setEndpoint(const QUrl& endpoint) { m_endpoint = endpoint; }
    [[nodiscard]] const QUrl& endpoint() const { return m_endpoint; }
    /// The transfer timeout in milliseconds (the tests lower it).
    void setTimeout(int milliseconds);
    [[nodiscard]] int timeout() const { return m_timeoutMs; }

    /// Starts a search and returns its id; the search in flight is cancelled.
    quint64 search(const QString& query);
    void cancel();
    [[nodiscard]] bool isSearching() const { return m_reply != nullptr; }

    /// The GET URL for `query` (pure): `<endpoint>?query=<percent-encoded>`.
    [[nodiscard]] static QUrl requestUrl(const QUrl& endpoint, const QString& query);
    /// The body parsed into results (pure): nullopt when it is not a JSON
    /// array of objects; an empty list when the array holds nothing usable.
    [[nodiscard]] static std::optional<QList<SearchResult>> parse(const QByteArray& body);

Q_SIGNALS:
    void finished(quint64 id, const QList<pldl::services::SearchResult>& results);
    void failed(quint64 id, pldl::services::KtechpitSearch::Outcome outcome);

private:
    void handleReply(quint64 id);

    QNetworkAccessManager* m_network;
    QNetworkReply* m_reply = nullptr;
    QUrl m_endpoint;
    quint64 m_nextId = 1;
    quint64 m_runningId = 0;
    int m_timeoutMs = kDefaultTimeoutMs;
};

} // namespace pldl::services

Q_DECLARE_METATYPE(pldl::services::KtechpitSearch::Outcome)
