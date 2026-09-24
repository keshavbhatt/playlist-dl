#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace pldl::services {

/// Search suggestions while typing (FEATURES B5): the suggestion endpoint's
/// JSON client, asked 250 ms after the last keystroke, one request at a time.
/// The endpoint stalls now and then for seconds on one connection, so a
/// request that fails or times out moves on to the next endpoint at once
/// (both answer the same shape). Silent when every endpoint fails: a
/// suggestion list that does not come is no error.
class SearchSuggestions : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SearchSuggestions)

public:
    static constexpr int kDebounceMs = 250;
    static constexpr int kTimeoutMs = 4000; ///< per endpoint

    explicit SearchSuggestions(QObject* parent = nullptr);
    ~SearchSuggestions() override;

    /// The endpoints without a query, tried in order; the tests point them
    /// at local servers.
    void setEndpoints(const QList<QUrl>& endpoints);
    void setEndpoint(const QUrl& endpoint) { setEndpoints({endpoint}); }
    [[nodiscard]] const QList<QUrl>& endpoints() const { return m_endpoints; }
    /// Asks for suggestions for `text` once typing pauses; an empty text
    /// cancels. A newer text drops the older request.
    void request(const QString& text);
    void cancel();
    [[nodiscard]] bool isPending() const;

    /// The GET URL for `text` (pure).
    [[nodiscard]] static QUrl requestUrl(const QUrl& endpoint, const QString& text);
    /// `[query, [suggestions...]]` parsed into the suggestions (pure); empty
    /// for anything else.
    [[nodiscard]] static QStringList parse(const QByteArray& body);

Q_SIGNALS:
    void suggestions(const QStringList& suggestions);

private:
    void send();
    void handleReply();

    QNetworkAccessManager* m_network;
    QTimer* m_debounce;
    QNetworkReply* m_reply = nullptr;
    QList<QUrl> m_endpoints;
    int m_attempt = 0; ///< index into m_endpoints of the request in flight
    QString m_text;
};

} // namespace pldl::services
