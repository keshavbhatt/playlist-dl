#include "services/search_suggestions.h"

#include "services/logging.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace pldl::services {

namespace {
// Two hosts of the same service; the first is the usual one, the second
// answered every probe while the first stalled (2026-09-24).
constexpr QLatin1StringView kDefaultEndpoint{"https://suggestqueries.google.com/complete/search"};
constexpr QLatin1StringView kFallbackEndpoint{"https://clients1.google.com/complete/search"};
}

SearchSuggestions::SearchSuggestions(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_debounce(new QTimer(this))
    , m_endpoints({QUrl(QString(kDefaultEndpoint)), QUrl(QString(kFallbackEndpoint))})
{
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, &SearchSuggestions::send);
}

SearchSuggestions::~SearchSuggestions()
{
    cancel();
}

QUrl SearchSuggestions::requestUrl(const QUrl& endpoint, const QString& text)
{
    QUrl url = endpoint;
    url.setQuery(u"client=firefox&ds=yt&q="_s + QString::fromLatin1(QUrl::toPercentEncoding(text.trimmed())),
                 QUrl::StrictMode);
    return url;
}

QStringList SearchSuggestions::parse(const QByteArray& body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isArray()) {
        return {};
    }
    const QJsonArray root = document.array();
    if (root.size() < 2 || !root.at(1).isArray()) {
        return {};
    }
    QStringList out;
    const QJsonArray list = root.at(1).toArray();
    for (const QJsonValue& value : list) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            out << text;
        }
    }
    return out;
}

void SearchSuggestions::setEndpoints(const QList<QUrl>& endpoints)
{
    m_endpoints = endpoints;
    m_attempt = 0;
}

void SearchSuggestions::request(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        cancel();
        return;
    }
    if (m_reply != nullptr) {
        // A newer text: the older answer would arrive stale.
        QNetworkReply* reply = m_reply;
        m_reply = nullptr;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    m_text = trimmed;
    m_attempt = 0;
    m_debounce->start();
}

void SearchSuggestions::cancel()
{
    m_debounce->stop();
    m_text.clear();
    if (m_reply == nullptr) {
        return;
    }
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
}

bool SearchSuggestions::isPending() const
{
    return m_debounce->isActive() || m_reply != nullptr;
}

void SearchSuggestions::send()
{
    if (m_text.isEmpty() || m_attempt >= m_endpoints.size()) {
        return;
    }
    QNetworkRequest request(requestUrl(m_endpoints.at(m_attempt), m_text));
    request.setTransferTimeout(kTimeoutMs);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this, &SearchSuggestions::handleReply);
}

void SearchSuggestions::handleReply()
{
    QNetworkReply* reply = m_reply;
    if (reply == nullptr) {
        return;
    }
    m_reply = nullptr;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        qCDebug(lcSearch) << "suggestions: endpoint" << m_attempt << "failed:" << reply->errorString();
        if (m_attempt + 1 < m_endpoints.size()) {
            ++m_attempt;
            send();
        }
        return;
    }
    m_attempt = 0;
    const QStringList list = parse(reply->readAll());
    if (list.isEmpty()) {
        qCDebug(lcSearch) << "suggestions: nothing for" << m_text;
        return;
    }
    Q_EMIT suggestions(list);
}

} // namespace pldl::services
