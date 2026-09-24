#pragma once

#include <QByteArray>
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
/// Silent on failure: a suggestion list that does not come is no error.
class SearchSuggestions : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SearchSuggestions)

public:
    static constexpr int kDebounceMs = 250;
    static constexpr int kTimeoutMs = 5000;

    explicit SearchSuggestions(QObject* parent = nullptr);
    ~SearchSuggestions() override;

    /// The endpoint without a query; the tests point it at a local server.
    void setEndpoint(const QUrl& endpoint) { m_endpoint = endpoint; }
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
    QUrl m_endpoint;
    QString m_text;
};

} // namespace pldl::services
