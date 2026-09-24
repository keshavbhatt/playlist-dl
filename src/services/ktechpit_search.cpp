#include "services/ktechpit_search.h"

#include "services/logging.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::services {

namespace {
constexpr QLatin1StringView kDefaultEndpoint{"https://ktechpit.com/USS/Olivia/youtube/api.php"};
constexpr QLatin1StringView kPlaylistPage{"https://www.youtube.com/playlist?list="};
constexpr int kMinTimeoutMs = 50;

QString thumbnailOf(const QJsonObject& object)
{
    // The service answers with the large frame; the card shows a small one
    // and the medium frame is what the site serves fastest (2.x did the same).
    QString url = object.value(u"playlistThumbnail"_s).toString();
    url.replace(u"hqdefault"_s, u"mqdefault"_s);
    return url;
}
} // namespace

KtechpitSearch::KtechpitSearch(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_endpoint(QString(kDefaultEndpoint))
{
    qRegisterMetaType<pldl::services::KtechpitSearch::Outcome>();
    qRegisterMetaType<QList<pldl::services::SearchResult>>();
}

KtechpitSearch::~KtechpitSearch()
{
    cancel();
}

void KtechpitSearch::setTimeout(int milliseconds)
{
    m_timeoutMs = std::max(kMinTimeoutMs, milliseconds);
}

QUrl KtechpitSearch::requestUrl(const QUrl& endpoint, const QString& query)
{
    QUrl url = endpoint;
    // Encoded by hand and set strictly: the tolerant parser would turn a "+"
    // or a "&" in the query into a delimiter.
    url.setQuery(u"query="_s + QString::fromLatin1(QUrl::toPercentEncoding(query.trimmed())),
                 QUrl::StrictMode);
    return url;
}

std::optional<QList<SearchResult>> KtechpitSearch::parse(const QByteArray& body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isArray()) {
        return std::nullopt;
    }
    const QJsonArray array = document.array();
    QList<SearchResult> results;
    results.reserve(array.size());
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            return std::nullopt;
        }
        const QJsonObject object = value.toObject();
        SearchResult result;
        result.kind = SearchKind::Playlists;
        result.id = object.value(u"playlistId"_s).toString();
        if (result.id.isEmpty()) {
            continue;
        }
        result.title = object.value(u"title"_s).toString();
        result.channel = object.value(u"author"_s).toString();
        const QJsonValue count = object.value(u"videoCount"_s);
        result.itemCount = count.isDouble() ? static_cast<qint64>(count.toDouble()) : -1;
        result.thumbnailUrl = thumbnailOf(object);
        result.url = QString(kPlaylistPage) + result.id;
        results.append(result);
    }
    return results;
}

quint64 KtechpitSearch::search(const QString& query)
{
    cancel();
    const quint64 id = m_nextId++;
    QNetworkRequest request(requestUrl(m_endpoint, query));
    request.setTransferTimeout(m_timeoutMs);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork);
    m_runningId = id;
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, id] { handleReply(id); });
    qCInfo(lcSearch) << "service search" << id;
    return id;
}

void KtechpitSearch::cancel()
{
    if (m_reply == nullptr) {
        return;
    }
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    m_runningId = 0;
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
}

void KtechpitSearch::handleReply(quint64 id)
{
    if (m_reply == nullptr || m_runningId != id) {
        return;
    }
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    m_runningId = 0;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // The transfer timeout reports TimeoutError; cancel() disconnects
        // before it aborts, so an abort that reaches here is a timeout too.
        const bool timedOut = reply->error() == QNetworkReply::TimeoutError ||
                              reply->error() == QNetworkReply::OperationCanceledError;
        qCWarning(lcSearch) << "service search" << id << (timedOut ? "timed out" : "failed:")
                            << reply->errorString();
        Q_EMIT failed(id, timedOut ? Outcome::Timeout : Outcome::NetworkError);
        return;
    }
    const std::optional<QList<SearchResult>> results = parse(reply->readAll());
    if (!results.has_value()) {
        qCWarning(lcSearch) << "service search" << id << "answered with something that is not a result list";
        Q_EMIT failed(id, Outcome::BadPayload);
        return;
    }
    if (results->isEmpty()) {
        qCInfo(lcSearch) << "service search" << id << "found nothing";
        Q_EMIT failed(id, Outcome::Empty);
        return;
    }
    qCInfo(lcSearch) << "service search" << id << "found" << results->size() << "playlists";
    Q_EMIT finished(id, *results);
}

} // namespace pldl::services
